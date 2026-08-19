
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h" // SCEVAddRecExpr
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"				   // getLoadStorePointerOperand
#include "llvm/Transforms/Utils/BasicBlockUtils.h" // ReplaceInstWithInst
#include "llvm/Transforms/Utils/Local.h"		   // isInstructionTriviallyDead
#include "llvm/ADT/STLExtras.h"					   // make_early_inc_range

using namespace llvm;

namespace
{

	//===------------------------------------------------------------------===//
	// Sezione Helper
	//===------------------------------------------------------------------===//

	// Entry block "logico" del loop: il blocco che contiene il guard branch se
	// il loop è guarded, altrimenti il preheader.
	BasicBlock *getLoopEntryBlock(Loop *L)
	{
		if (BranchInst *guard = L->getLoopGuardBranch())
			return guard->getParent();
		return L->getLoopPreheader();
	}

	// Primo blocco del corpo di L: il successore dell'header interno al loop.
	// Su un loop in forma ruotata con corpo tutto nell'header coincide con il
	// latch, ed è corretto così (il corpo viene spostato in headerL0).
	BasicBlock *getFirstBodyBlock(Loop *L)
	{
		BranchInst *branchHeader = dyn_cast<BranchInst>(L->getHeader()->getTerminator());
		if (!branchHeader)
			return nullptr;

		for (BasicBlock *succ : branchHeader->successors())
			if (L->contains(succ))
				return succ;

		return nullptr;
	}

	// Successore del guard branch che scavalca il loop: l'altro è il preheader.
	BasicBlock *getGuardSkipSuccessor(BranchInst *guard, BasicBlock *preheader)
	{
		return guard->getSuccessor(0) == preheader ? guard->getSuccessor(1)
												   : guard->getSuccessor(0);
	}

	//===------------------------------------------------------------------===//
	// 1) Adiacenza
	//===------------------------------------------------------------------===//

	// L0 e L1 sono adiacenti se fra l'uscita di L0 e l'ingresso di L1 non
	// viene eseguito nulla. Due topologie:
	//   - non guarded: exit(L0) == preheader(L1), blocco vuoto
	//   - guarded:     exit(L0) -> guard(L1), e anche il ramo che salta L0
	bool isAdjacent(Loop *L0, Loop *L1)
	{
		BasicBlock *exitL0 = L0->getExitBlock();
		BasicBlock *preheaderL1 = L1->getLoopPreheader();

		// Loop non guarded: size() == 1 => solo il terminatore.
		if (exitL0 && preheaderL1 && exitL0 == preheaderL1)
			return exitL0->size() == 1;

		// Loop guarded: il successore non-loop del guard branch di L0 deve
		// essere l'entry block di L1, cioè il blocco del guard di L1.
		BranchInst *guardL0 = L0->getLoopGuardBranch();
		BranchInst *guardL1 = L1->getLoopGuardBranch();
		if (guardL0 && guardL1)
		{
			BasicBlock *guardBlockL1 = guardL1->getParent();
			BasicBlock *preheaderL0 = L0->getLoopPreheader();

			// (a) L0 eseguito: exit(L0) vuoto e cade sul guard di L1.
			if (!exitL0 || exitL0->size() != 1)
				return false;
			if (exitL0->getUniqueSuccessor() != guardBlockL1)
				return false;

			// (b) L0 saltato: il ramo che scavalca L0 va al guard di L1.
			if (getGuardSkipSuccessor(guardL0, preheaderL0) != guardBlockL1)
				return false;

			// (c) guard(L1) sparisce con la fusione: ammesso solo il calcolo
			//     della condizione, niente side effects.
			for (Instruction &I : *guardBlockL1)
				if (I.mayHaveSideEffects())
					return false;

			return true;
		}

		return false;
	}

	//===------------------------------------------------------------------===//
	// 2) Stesso numero di iterazioni
	//===------------------------------------------------------------------===//

	bool haveSameTripCount(Loop *L0, Loop *L1, ScalarEvolution &SE)
	{
		// Deduce il numero di iterazioni per Loop
		const SCEV *tripCountL0 = SE.getBackedgeTakenCount(L0);
		const SCEV *tripCountL1 = SE.getBackedgeTakenCount(L1);

		if (isa<SCEVCouldNotCompute>(tripCountL0) || isa<SCEVCouldNotCompute>(tripCountL1))
			return false;

		// Gli SCEV sono uniquati: espressioni identiche => stesso puntatore.
		// Conservativo (forme equivalenti ma diverse falliscono), mai sbagliato.
		return tripCountL0 == tripCountL1;
	}

	//===------------------------------------------------------------------===//
	// 3) Control flow equivalence
	//===------------------------------------------------------------------===//

	bool areControlFlowEquivalent(Loop *L0, Loop *L1, DominatorTree &DT, PostDominatorTree &PDT)
	{
		// Entry block così la condizione vale
		// sia per i loop guarded sia per quelli non guarded.
		BasicBlock *entryL0 = getLoopEntryBlock(L0);
		BasicBlock *entryL1 = getLoopEntryBlock(L1);

		if (!entryL0 || !entryL1)
			return false;

		// L0 domina L1  -> per arrivare a L1 devo passare da L0
		// L1 postdomina L0 -> eseguito L0 arrivo per forza a L1
		return DT.dominates(entryL0, entryL1) && PDT.dominates(entryL1, entryL0);
	}

	//===------------------------------------------------------------------===//
	// 4) Nessuna dipendenza a distanza negativa
	//===------------------------------------------------------------------===//

	bool hasNegativeDistanceDep(Loop *L0, Loop *L1, ScalarEvolution &SE, DependenceInfo &DI)
	{
		SmallVector<Instruction *, 8> loadsStoresL0, loadsStoresL1;

		// Raccolgo  tutti gli accessi a memoria di L0 e L1.
		for (BasicBlock *BB : L0->blocks())
			for (Instruction &I : *BB)
				if (isa<LoadInst>(I) || isa<StoreInst>(I))
					loadsStoresL0.push_back(&I);

		for (BasicBlock *BB : L1->blocks())
			for (Instruction &I : *BB)
				if (isa<LoadInst>(I) || isa<StoreInst>(I))
					loadsStoresL1.push_back(&I);

		// Confronta ogni accesso di L0 con ogni accesso di L1: basta una coppia
		// a distanza negativa (o di segno ignoto) per rifiutare la fusione.
		for (Instruction *I0 : loadsStoresL0)
		{
			for (Instruction *I1 : loadsStoresL1)
			{
				// Due load non generano mai una dipendenza.
				if (isa<LoadInst>(I0) && isa<LoadInst>(I1))
					continue;

				// Pre-filtro: se la Dependence Analysis dimostra che le due
				// istruzioni sono indipendenti non serve andare oltre.
				if (!DI.depends(I0, I1, true))
					continue;

				// Puntatore su cui insiste ciascun accesso.
				Value *ptrI0 = getLoadStorePointerOperand(I0);
				Value *ptrI1 = getLoadStorePointerOperand(I1);
				if (!ptrI0 || !ptrI1)
					return true; // conservativo

				// Indirizzo simbolico dell'accesso, valutato nel loop di appartenenza.
				const SCEV *scevI0 = SE.getSCEVAtScope(ptrI0, L0);
				const SCEV *scevI1 = SE.getSCEVAtScope(ptrI1, L1);

				/// Basi diverse (A e B) => nessuna dipendenza.
				if (SE.getPointerBase(scevI0) != SE.getPointerBase(scevI1))
					continue;

				const SCEVAddRecExpr *addRecI0 = dyn_cast<SCEVAddRecExpr>(scevI0);
				const SCEVAddRecExpr *addRecI1 = dyn_cast<SCEVAddRecExpr>(scevI1);

				// Accessi non affini rispetto alla IV: distanza non calcolabile.
				if (!addRecI0 || !addRecI1)
					return true; // conservativo

				// Passi diversi: la distanza non è costante fra le iterazioni.
				if (addRecI0->getStepRecurrence(SE) != addRecI1->getStepRecurrence(SE))
					return true; // conservativo

				const SCEV *distance = SE.getMinusSCEV(addRecI0->getStart(),
													   addRecI1->getStart());

				// Blocca sia la distanza negativa sia quella di segno ignoto.
				if (!SE.isKnownNonNegative(distance))
					return true;
			}
		}

		return false;
	}

	//===------------------------------------------------------------------===//
	// Precondizioni strutturali della trasformazione
	//===------------------------------------------------------------------===//

	// Tutto ciò di cui `fuseLoops` ha bisogno viene validato qui, prima di toccare l'IR.
	bool canTransform(Loop *L0, Loop *L1, ScalarEvolution &SE)
	{
		// il passo presuppone la forma ruotata
		if (!L0->isRotatedForm() || !L1->isRotatedForm())
			return false;

		// Loop annidati non gestiti: getFirstBodyBlock(L1) restituirebbe l'header del loop interno, le cui PHI il rewiring non aggiorna.
		if (!L0->getSubLoops().empty() || !L1->getSubLoops().empty())
			return false;

		// getExitBlock() e' nullptr se le uscite sono piu' di una.
		if (!L1->getExitBlock())
			return false;

		// Primo blocco del corpo di L1: e' cio' che verra' agganciato a L0.
		if (!getFirstBodyBlock(L1))
			return false;

		PHINode *IV0 = L0->getInductionVariable(SE);
		PHINode *IV1 = L1->getInductionVariable(SE);
		if (!IV0 || !IV1)
			return false;

		// Le due IV devono percorrere la stessa sequenza di valori, altrimenti gli indici cambiano.
		if (IV0->getType() != IV1->getType())
			return false;

		// SCEV della IV in forma {start,+,step}: stesso valore iniziale e stesso passo.
		// Il trip count uguale non basta: i<100,i++ e i<200,i+=2 fanno entrambi 100 giri.
		const SCEVAddRecExpr *addRecIV0 = dyn_cast<SCEVAddRecExpr>(SE.getSCEV(IV0));
		const SCEVAddRecExpr *addRecIV1 = dyn_cast<SCEVAddRecExpr>(SE.getSCEV(IV1));

		if (!addRecIV0 || !addRecIV1)
			return false;
		if (addRecIV0->getStart() != addRecIV1->getStart())
			return false;
		if (addRecIV0->getStepRecurrence(SE) != addRecIV1->getStepRecurrence(SE))
			return false;

		// PHI extra nell'header di L1 (riduzioni) non gestite
		for (PHINode &PN : L1->getHeader()->phis())
			if (&PN != IV1)
				return false;

		return true;
	}

	//===------------------------------------------------------------------===//
	// canFuse
	//===------------------------------------------------------------------===//

	bool canFuse(Loop *L0, Loop *L1, DominatorTree &DT, ScalarEvolution &SE,
				 PostDominatorTree &PDT, DependenceInfo &DI)
	{
		if (!L0->isLoopSimplifyForm() || !L1->isLoopSimplifyForm())
			return false;

		if (!isAdjacent(L0, L1))
			return false;

		if (!haveSameTripCount(L0, L1, SE))
			return false;

		if (!areControlFlowEquivalent(L0, L1, DT, PDT))
			return false;

		if (hasNegativeDistanceDep(L0, L1, SE, DI))
			return false;

		// Precondizioni della trasformazione: verificate qui in modo che
		// `fuseLoops` non possa fallire a metà
		if (!canTransform(L0, L1, SE))
			return false;

		return true;
	}

	//===------------------------------------------------------------------===//
	// Trasformazione
	//===------------------------------------------------------------------===//

	bool fuseLoops(Loop *L0, Loop *L1, ScalarEvolution &SE)
	{
		BasicBlock *headerL0 = L0->getHeader();
		BasicBlock *headerL1 = L1->getHeader();
		BasicBlock *latchL0 = L0->getLoopLatch();
		BasicBlock *latchL1 = L1->getLoopLatch();

		PHINode *IV0 = L0->getInductionVariable(SE);
		PHINode *IV1 = L1->getInductionVariable(SE);

		BasicBlock *firstBodyL1 = getFirstBodyBlock(L1);

		// Già garantiti da canTransform(); l'assert documenta l'invariante.
		assert(IV0 && IV1 && firstBodyL1 && "precondizioni non verificate da canFuse");

		// Guard e preheader vanno letti ora: derivano dal CFG, che sotto cambia.
		BranchInst *guardL0 = L0->getLoopGuardBranch();
		BranchInst *guardL1 = L1->getLoopGuardBranch();
		BasicBlock *preheaderL0 = L0->getLoopPreheader();
		BasicBlock *preheaderL1 = L1->getLoopPreheader();

		// Incremento di L1: sarà quello del loop fuso, il cui latch è latchL1.
		Value *latchL1BackedgeValue = IV1->getIncomingValueForBlock(latchL1);

		// 1) Una sola IV: in SSA IV0 e IV1 sono variabili distinte.
		IV1->replaceAllUsesWith(IV0);
		IV1->eraseFromParent();

		// 2) Corpo di L1 in coda a quello di L0 (in forma ruotata sta nell'header).
		Instruction *insertPoint = headerL0->getTerminator();
		for (Instruction &I : make_early_inc_range(*headerL1))
			if (!isa<PHINode>(&I) && !isa<BranchInst>(&I))
				I.moveBefore(insertPoint);

		// 3) Rewiring del CFG.
		// latchL0 non è più un latch: diventa un blocco di passaggio verso L1.
		// Il test di uscita resta uno solo, in latchL1.
		ReplaceInstWithInst(latchL0->getTerminator(), BranchInst::Create(firstBodyL1));

		// latchL1 è il latch del loop fuso: backedge verso headerL0.
		BranchInst *branchLatchL1 = cast<BranchInst>(latchL1->getTerminator());
		for (unsigned i = 0; i < branchLatchL1->getNumSuccessors(); ++i)
			if (branchLatchL1->getSuccessor(i) == headerL1)
				branchLatchL1->setSuccessor(i, headerL0);

		// Caso guarded: stesso trip count, quindi se L0 non esegue non deve
		// eseguire nemmeno L1. Senza questo, sul ramo "L0 saltato" si entrerebbe in
		// latchL1 senza passare da headerL0 e la IV non sarebbe definita.
		if (guardL0 && guardL1)
		{
			// Destinazione del guard di L1 quando L1 non va eseguito.
			BasicBlock *skipL1 = getGuardSkipSuccessor(guardL1, preheaderL1);

			// Il predecessore di skipL1 cambia: era il guard di L1, ora quello di L0.
			skipL1->replacePhiUsesWith(guardL1->getParent(), guardL0->getParent());
			guardL0->setSuccessor(guardL0->getSuccessor(0) == preheaderL0 ? 1 : 0, skipL1);
		}

		// 4) PHI dell'header: il backedge arriva da latchL1.
		for (PHINode &PN : headerL0->phis())
		{
			int idx = PN.getBasicBlockIndex(latchL0);
			if (idx < 0)
				continue;
			if (&PN == IV0)
				PN.setIncomingValue(idx, latchL1BackedgeValue);
			PN.setIncomingBlock(idx, latchL1);
		}

		// 5) Pulizia: il vecchio test di uscita di L0 e il suo incremento non hanno più usi.
		// isInstructionTriviallyDead evita di rimuovere istruzioni con side
		// effect (una store inutilizzata ha use_empty() ma non va toccata).
		for (Instruction &I : make_early_inc_range(reverse(*latchL0)))
			if (!I.isTerminator() && isInstructionTriviallyDead(&I))
				I.eraseFromParent();

		return true;
	}

	//===------------------------------------------------------------------===//
	// Il passo
	//===------------------------------------------------------------------===//

	struct LoopFusionPassV2 : public PassInfoMixin<LoopFusionPassV2>
	{

		PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM)
		{
			bool Changed = false;
			bool MergedSomething = true;

			while (MergedSomething)
			{
				MergedSomething = false;

				// dopo una fusione le analisi vengono
				//  invalidate e il gestore le ricalcola alla prossima richiesta.
				LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
				DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
				ScalarEvolution &SE = AM.getResult<ScalarEvolutionAnalysis>(F);
				PostDominatorTree &PDT = AM.getResult<PostDominatorTreeAnalysis>(F);
				DependenceInfo &DI = AM.getResult<DependenceAnalysis>(F);

				// La fusione invalida gli iteratori su LoopInfo: si lavora su uno
				// snapshot e si riparte da capo dopo ogni trasformazione.
				SmallVector<Loop *, 8> Loops(LI.begin(), LI.end());

				for (size_t i = 0; i < Loops.size() && !MergedSomething; i++)
				{
					for (size_t j = i + 1; j < Loops.size() && !MergedSomething; j++)
					{
						Loop *L0 = Loops[i];
						Loop *L1 = Loops[j];

						// L'ordine in LoopInfo non è quello del programma: si prova
						// in entrambe le direzioni, l'adiacenza passa solo in quella
						// giusta.
						if (canFuse(L0, L1, DT, SE, PDT, DI))
							MergedSomething = fuseLoops(L0, L1, SE);
						else if (canFuse(L1, L0, DT, SE, PDT, DI))
							MergedSomething = fuseLoops(L1, L0, SE);

						if (!MergedSomething)
							continue;

						Changed = true;

						EliminateUnreachableBlocks(F); // vecchio exit di L0 e headerL1

						// Il CFG è cambiato: tutte le analisi sono stale.
						AM.invalidate(F, PreservedAnalyses::none());
					}
				}
			}

			return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
		}
	};
}

llvm::PassPluginLibraryInfo getLoopFusionPassV2PluginInfo()
{
	return {LLVM_PLUGIN_API_VERSION, "LoopFusionPassV2", LLVM_VERSION_STRING,
			[](PassBuilder &PB)
			{
				PB.registerPipelineParsingCallback(
					[](StringRef Name, FunctionPassManager &FPM,
					   ArrayRef<PassBuilder::PipelineElement>)
					{
						if (Name == "lf-pass")
						{
							FPM.addPass(LoopFusionPassV2());
							return true;
						}
						return false;
					});
			}};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo()
{
	return getLoopFusionPassV2PluginInfo();
}
