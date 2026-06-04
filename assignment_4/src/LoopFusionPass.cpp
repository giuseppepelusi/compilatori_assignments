#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/IR/Instruction.h"

using namespace llvm;

namespace {

	bool isAdjacent(Loop* L0, Loop* L1) {
		BasicBlock* exitL0 = L0 -> getExitBlock();
		BasicBlock* preheaderL1 = L1 -> getLoopPreheader();

		if (exitL0 && preheaderL1 && exitL0 == preheaderL1)
			return exitL0->size() == 1;

		BranchInst* guardL0 = L0 -> getLoopGuardBranch();
		BranchInst* guardL1 = L1 -> getLoopGuardBranch();
		if (guardL0 && guardL1) {
			BasicBlock* target = guardL1 -> getParent();
			return guardL0 -> getSuccessor(0) == target || guardL0 -> getSuccessor(1) == target;
		}
		return false;
	}

	bool haveSameTripCount(Loop* L0, Loop* L1, ScalarEvolution &SE) {
		const SCEV* tripCountL0 = SE.getBackedgeTakenCount(L0);
		const SCEV* tripCountL1 = SE.getBackedgeTakenCount(L1);

		if (isa<SCEVCouldNotCompute>(tripCountL0) || isa<SCEVCouldNotCompute>(tripCountL1))
			return false;

		return tripCountL0 == tripCountL1;
	}

	bool areControlFlowEquivalent(Loop* L0, Loop* L1, DominatorTree &DT, PostDominatorTree &PDT) {
		BasicBlock* headerL0 = L0 -> getHeader();
		BasicBlock* headerL1 = L1 -> getHeader();

		return DT.dominates(headerL0, headerL1) && PDT.dominates(headerL1, headerL0);
	}

	bool hasNegativeDistanceDep(Loop* L0, Loop* L1, DependenceInfo &DI) {
		SmallVector<Instruction*, 8> loadsStoresL0, loadsStoresL1;

		for (BasicBlock* BB : L0 -> blocks())
			for (Instruction &I : *BB)
				if (isa<LoadInst>(I) || isa<StoreInst>(I))
					loadsStoresL0.push_back(&I);

		for (BasicBlock* BB : L1 -> blocks())
			for (Instruction &I : *BB)
				if (isa<LoadInst>(I) || isa<StoreInst>(I))
					loadsStoresL1.push_back(&I);

		for (Instruction* I0 : loadsStoresL0) {
			for (Instruction* I1 : loadsStoresL1) {
				auto depends = DI.depends(I0, I1, true);

				if (!depends)
					continue;
				if (depends -> getLevels() == 0)
					continue;
				if (depends -> isConfused())
					return true;

				unsigned level = 1;
				if (depends -> getLevels() >= level) {
					if (depends -> getDirection(level) & Dependence::DVEntry::GT)
						return true;
				}
			}
		}

		return false;
	}

	bool canFuse(Loop* L0, Loop* L1, DominatorTree &DT, ScalarEvolution &SE, PostDominatorTree &PDT, DependenceInfo &DI) {
		if(!L0->isLoopSimplifyForm() || !L1->isLoopSimplifyForm())
			return false;

		if (!isAdjacent(L0, L1))
			return false;

		if (!haveSameTripCount(L0, L1, SE))
			return false;

		if (!areControlFlowEquivalent(L0, L1, DT, PDT))
			return false;

		if (hasNegativeDistanceDep(L0, L1, DI))
			return false;

		return true;
	}

	void fuseLoops(Loop* L0, Loop* L1, ScalarEvolution &SE, LoopInfo &LI) {
		BasicBlock* headerL0 = L0 -> getHeader();
		BasicBlock* headerL1 = L1 -> getHeader();
		BasicBlock* latchL0  = L0 -> getLoopLatch();
		BasicBlock* latchL1  = L1 -> getLoopLatch();

		PHINode* IV0 = L0 -> getCanonicalInductionVariable();
		PHINode* IV1 = L1 -> getCanonicalInductionVariable();

		if (!IV0 || !IV1)
			return;

		Value* latchL1BackedgeValue = IV1 -> getIncomingValueForBlock(latchL1);

		IV1 -> replaceAllUsesWith(IV0);
		SE.forgetValue(IV1);
		IV1 -> eraseFromParent();

		Instruction* insertPoint = headerL0->getTerminator();
		for (auto it = headerL1->begin(); it != headerL1->end(); ) {
			Instruction &I = *it++;
			if (!isa<PHINode>(&I) && !isa<BranchInst>(&I)) {
				I.moveBefore(insertPoint);
			}
		}

		BasicBlock* firstBodyL1 = nullptr;
		BranchInst* branchHeaderL1 = cast<BranchInst>(headerL1->getTerminator());
		for (unsigned i = 0; i < branchHeaderL1->getNumSuccessors(); ++i) {
			BasicBlock* succ = branchHeaderL1->getSuccessor(i);
			if (L1->contains(succ)) {
				firstBodyL1 = succ;
				break;
			}
		}
		if (!firstBodyL1) return;

		BranchInst* branchLatchL0 = cast<BranchInst>(latchL0->getTerminator());
		for (unsigned i = 0; i < branchLatchL0->getNumSuccessors(); ++i) {
			if (branchLatchL0->getSuccessor(i) == headerL0) {
				branchLatchL0->setSuccessor(i, firstBodyL1);
			}
		}

		BranchInst* branchLatchL1 = cast<BranchInst>(latchL1->getTerminator());
		for (unsigned i = 0; i < branchLatchL1->getNumSuccessors(); ++i) {
			if (branchLatchL1->getSuccessor(i) == headerL1) {
				branchLatchL1->setSuccessor(i, headerL0);
			}
		}


		BranchInst* branchHeaderL0 = cast<BranchInst>(headerL0->getTerminator());
		BasicBlock* exitL1 = L1->getExitBlock();
		for (unsigned i = 0; i < branchHeaderL0->getNumSuccessors(); ++i) {
			if (!L0->contains(branchHeaderL0->getSuccessor(i))) {
				branchHeaderL0->setSuccessor(i, exitL1);
			}
		}

		for (PHINode &PN : headerL0->phis()) {
			int idx = PN.getBasicBlockIndex(latchL0);
			if (idx < 0)
				continue;
			if (&PN == IV0)
				PN.setIncomingValue(idx, latchL1BackedgeValue);
			PN.setIncomingBlock(idx, latchL1);
		}

	}

	struct LoopFusionPass : public PassInfoMixin<LoopFusionPass> {

		PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {

			LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
			DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
			ScalarEvolution &SE = AM.getResult<ScalarEvolutionAnalysis>(F);
			PostDominatorTree &PDT = AM.getResult<PostDominatorTreeAnalysis>(F);
			DependenceInfo &DI = AM.getResult<DependenceAnalysis>(F);

			bool Changed = false;
			bool MergedSomething = true;

			while (MergedSomething) {
				MergedSomething = false;
				SmallVector<Loop*, 8> Loops(LI.begin(), LI.end());

				for (size_t i = 0; i < Loops.size() && !MergedSomething; i++) {
					for (size_t j = i + 1; j < Loops.size() && !MergedSomething; j++) {
						Loop* L0 = Loops[i];
						Loop* L1 = Loops[j];

						if (canFuse(L0, L1, DT, SE, PDT, DI)) {
							fuseLoops(L0, L1, SE, LI);
							MergedSomething = true;
							Changed = true;
						}
						else if (canFuse(L1, L0, DT, SE, PDT, DI)) {
							fuseLoops(L1, L0, SE, LI);
							MergedSomething = true;
							Changed = true;
						}
					}
				}
			}
			return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
		}
	};
}

llvm::PassPluginLibraryInfo getLoopFusionPassPluginInfo() {
	return {LLVM_PLUGIN_API_VERSION, "LoopFusionPass", LLVM_VERSION_STRING,
		[](PassBuilder &PB) {
			PB.registerPipelineParsingCallback(
				[](StringRef Name, FunctionPassManager &FPM,
					ArrayRef<PassBuilder::PipelineElement>) {
						if (Name == "lf-pass") {
							FPM.addPass(LoopFusionPass());
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
	return getLoopFusionPassPluginInfo();
}
