#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instruction.h"

using namespace llvm;

namespace {

	struct AlgebraicIdentityPass : public PassInfoMixin<AlgebraicIdentityPass> {

		PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {

			outs()<<"Prima dell'ottimizzazione\n";
			for (auto &BB : F)
				for (auto &I : BB)
					outs()<<I<<"\n";
			outs()<<"-------------------------\n";

			std::vector<Instruction *> toErase;

			for (auto &BB : F) {
				for (auto &I : BB) {
					if (BinaryOperator *op = dyn_cast<BinaryOperator>(&I)){
						if (op->getOpcode() == Instruction::Add || op->getOpcode() == Instruction::Sub) {
							Value *op_sx = op->getOperand(0);
							Value *op_dx = op->getOperand(1);

							if (ConstantInt *c = dyn_cast<ConstantInt>(op_sx)) {
								if (c->getValue() == 0 && op->getOpcode() == Instruction::Add) {
									op->replaceAllUsesWith(op_dx);
									toErase.push_back(op);
								}
							}
							else if (ConstantInt *c = dyn_cast<ConstantInt>(op_dx)) {
								if (c->getValue() == 0) {
									op->replaceAllUsesWith(op_sx);
									toErase.push_back(op);
								}
							}
						}
						else if (op->getOpcode() == Instruction::Mul) {
							Value *op_sx = op->getOperand(0);
							Value *op_dx = op->getOperand(1);

							if (ConstantInt *c = dyn_cast<ConstantInt>(op_sx)) {
								if (c->getValue() == 1) {
									op->replaceAllUsesWith(op_dx);
									toErase.push_back(op);
								}
							}

							else if (ConstantInt *c = dyn_cast<ConstantInt>(op_dx)) {
								if (c->getValue() == 1) {
									op->replaceAllUsesWith(op_sx);
									toErase.push_back(op);
								}
							}
						}
					}
				}
			}

			for (Instruction *dead : toErase)
				dead->eraseFromParent();

			outs()<<"Dopo l'ottimizzazione\n";
			for (auto &BB : F)
				for (auto &I : BB)
					outs()<<I<<"\n";
			outs()<<"---------------------\n";

			return PreservedAnalyses::all();
		}

		static bool isRequired() { return true; }

	};
}

llvm::PassPluginLibraryInfo getAlgebraicIdentityPassPluginInfo() {
	return {LLVM_PLUGIN_API_VERSION, "AlgebraicIdentityPass", LLVM_VERSION_STRING,
		[](PassBuilder &PB) {
			PB.registerPipelineParsingCallback(
				[](StringRef Name, FunctionPassManager &FPM,
					ArrayRef<PassBuilder::PipelineElement>) {
						if (Name == "ai-pass") {
							FPM.addPass(AlgebraicIdentityPass());
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
	return getAlgebraicIdentityPassPluginInfo();
}
