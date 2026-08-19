//===----------------------------------------------------------------------===//
// Assignment 3 - Loop-Invariant Code Motion
//
// Sposta nel preheader le istruzioni il cui valore non cambia tra le iterazioni,
// cosi' vengono eseguite una volta sola invece che a ogni giro del loop.
//===----------------------------------------------------------------------===//

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Dominators.h"
#include "llvm/ADT/SmallPtrSet.h"

using namespace llvm;

namespace {

	// Invariante se nessun operando e' definito dentro il loop. Gli operandi gia'
	// marcati per lo spostamento contano come esterni: cosi' si seguono le catene.
	bool isLoopInvariant(Instruction *I, Loop *L, SmallPtrSetImpl<Instruction *> &ToHoistSet) {
		for (Use &U : I->operands()) {
			if (Instruction *OpDef = dyn_cast<Instruction>(U))
				if (L->contains(OpDef->getParent()) && !ToHoistSet.count(OpDef))
					return false;
		}
		return true;
	}

	struct LoopICMPass : public PassInfoMixin<LoopICMPass> {

		// Ordine dall'interno verso l'esterno: un invariante sollevato nel preheader
		// del loop interno puo' poi essere sollevato ancora in quello esterno.
		void getLoopsPostorder(Loop *L, std::vector<Loop *> &PostOrderLoops) {
			for (Loop *SubLoop : *L) {
				getLoopsPostorder(SubLoop, PostOrderLoops);
			}
			PostOrderLoops.push_back(L);
		}

		PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {

			LoopInfo &LI = FAM.getResult<LoopAnalysis>(F);
			DominatorTree &DT = FAM.getResult<DominatorTreeAnalysis>(F);

			if (LI.empty())
				return PreservedAnalyses::all();

			// Ogni loop del nido, ordinato dal piu' interno.
			std::vector<Loop *> PostOrderLoops;
			for (Loop *TopLevelLoop : LI) {
				getLoopsPostorder(TopLevelLoop, PostOrderLoops);
			}

			for (Loop *L : PostOrderLoops) {
				// Serve la forma normale: garantisce un preheader unico dove spostare.
				if (!L->isLoopSimplifyForm())
					continue;

				BasicBlock *Preheader = L->getLoopPreheader();
				if (!Preheader) continue;

				// Blocchi fuori dal loop raggiungibili dalle sue uscite.
				SmallVector<BasicBlock *, 4> ExitBlocks;
				L->getExitBlocks(ExitBlocks);

				// Un load e' invariante solo se nessuno scrive quella memoria nel
				// loop: gli operandi non bastano, il valore dipende anche dalla memoria.
				bool LoopWritesMemory = false;
				for (BasicBlock *BB : L->blocks())
					for (Instruction &I : *BB)
						if (I.mayWriteToMemory())
							LoopWritesMemory = true;

				// Candidati raccolti nell'ordine in cui verranno spostati.
				SmallVector<Instruction *, 16> ToHoist;
				SmallPtrSet<Instruction *, 16> ToHoistSet;

				for (BasicBlock *BB : L->blocks()) {
					for (Instruction &I : *BB) {
						// Terminatori e PHI sono legati alla struttura del CFG: non si spostano.
						if (I.isTerminator() || isa<PHINode>(I))
							continue;

						if (!isLoopInvariant(&I, L, ToHoistSet))
							continue;

						// Nel preheader l'istruzione viene eseguita anche se il loop non parte:
						// deve essere innocua (niente divisioni per zero, niente trap).
						if (!isSafeToSpeculativelyExecute(&I))
							continue;

						if (I.mayReadFromMemory() && LoopWritesMemory)
							continue;

						// Condizione (a): il blocco domina tutte le uscite, cioe' l'istruzione
						// viene eseguita di sicuro almeno una volta se il loop parte.
						bool dominatesAllExits = true;
						for (BasicBlock *Exit : ExitBlocks) {
							if (!DT.dominates(BB, Exit)) {
								dominatesAllExits = false;
								break;
							}
						}

						// Se non le domina, si sposta lo stesso purche' il valore sia morto
						// all'uscita: chi lo usa sta tutto dentro il loop.
						if (!dominatesAllExits) {
							bool usedOutsideLoop = false;
							for (User *U : I.users()) {
								if (Instruction *UI = dyn_cast<Instruction>(U)) {
									if (!L->contains(UI->getParent())) {
										usedOutsideLoop = true;
										break;
									}
								}
							}
							if (usedOutsideLoop)
								continue;
						}

						// Condizione (c): la definizione deve dominare tutti i suoi usi,
						// altrimenti spostandola si romperebbe la forma SSA.
						bool dominatesAllUses = true;
						for (Use &U : I.uses()) {
							if (Instruction *UI = dyn_cast<Instruction>(U.getUser())) {
								if (L->contains(UI->getParent())) {
									if (!DT.dominates(&I, U)) {
										dominatesAllUses = false;
										break;
									}
								}
							}
						}
						if (!dominatesAllUses) continue;

						ToHoist.push_back(&I);
						ToHoistSet.insert(&I);
					}
				}

				// Spostamento vero e proprio, in fondo al preheader e nell'ordine di
				// raccolta, cosi' le dipendenze restano definite prima dell'uso.
				for (Instruction *I : ToHoist) {
					outs() << "Hoisting: " << *I << "\n";
					I->moveBefore(Preheader->getTerminator());
				}
			}

			return PreservedAnalyses::none();
		}
	};
}

llvm::PassPluginLibraryInfo getLoopICMPassPluginInfo() {
	return {LLVM_PLUGIN_API_VERSION, "LoopICMPass", LLVM_VERSION_STRING,
		[](PassBuilder &PB) {
			PB.registerPipelineParsingCallback(
				[](StringRef Name, FunctionPassManager &FPM,
					ArrayRef<PassBuilder::PipelineElement>) {
						if (Name == "loop-icm-pass") {
							FPM.addPass(LoopICMPass());
							return true;
						}
						return false;
					}
			);
		}
	};
}


extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
	return getLoopICMPassPluginInfo();
}
