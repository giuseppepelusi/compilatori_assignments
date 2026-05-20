#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IRBuilder.h"

using namespace llvm;

namespace {

	bool isLoopInvariant(Instruction *I, Loop *L) {
		for (Use &U : I->operands()) {
			if (Instruction *OpDef = dyn_cast<Instruction>(U))
				if (L->contains(OpDef->getParent()))
					return false;
		}
		return true;
	}

	struct LoopICMPass : public PassInfoMixin<LoopICMPass> {

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

			for (Loop *L : LI.getLoopsInPreorder()) {
				if (!L->isLoopSimplifyForm())
					continue;

				BasicBlock *Preheader = L->getLoopPreheader();
				if (!Preheader) continue;

				SmallVector<BasicBlock *, 4> ExitBlocks;
				L->getExitBlocks(ExitBlocks);

				SmallVector<Instruction *, 16> ToHoist;

				for (BasicBlock *BB : L->blocks()) {
					for (Instruction &I : *BB) {
						if (I.isTerminator() || isa<PHINode>(I))
							continue;

						if (!isLoopInvariant(&I, L))
							continue;

						bool dominatesAllExits = true;
						for (BasicBlock *Exit : ExitBlocks) {
							if (!DT.dominates(BB, Exit)) {
								dominatesAllExits = false;
								break;
							}
						}

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
					}
				}

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
