#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

	struct MultiInstructionOptimizationPass : public PassInfoMixin<MultiInstructionOptimizationPass> {

		PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {

			outs()<<"Prima dell'ottimizzazione\n";
			for (auto &BB : F)
				for (auto &I : BB)
					outs()<<I<<"\n";
			outs()<<"-------------------------\n";

			std::vector<Instruction *> toErase;

			for (auto &B : F){
				for (auto &I : B){
					if (BinaryOperator *op = dyn_cast<BinaryOperator>(&I)) {
						if (op->getOpcode() == Instruction::Add){
							Value *op_sx = op->getOperand(0);
							Value *op_dx = op->getOperand(1);

							if (ConstantInt *c = dyn_cast<ConstantInt>(op_dx)){
								for (auto iter = op->user_begin(); iter != op->user_end(); ++iter) {
									User *u = *iter;

									if (Instruction *inst2 = dyn_cast<Instruction>(u)){
										if (inst2->getOpcode() == Instruction::Sub){
											Value *op_sx2 = inst2->getOperand(0);
											Value *op_dx2 = inst2->getOperand(1);

											if (ConstantInt *c2 = dyn_cast<ConstantInt>(op_dx2)){
												if (c->getValue() == c2->getValue() && op_sx2 == op){
													inst2->replaceAllUsesWith(op_sx);
													toErase.push_back(inst2);
												}
											}
										}
									}
								}
							}
						}
						else if (op->getOpcode() == Instruction::Sub){
							Value *op_sx = op->getOperand(0);
							Value *op_dx = op->getOperand(1);

							if (ConstantInt *c = dyn_cast<ConstantInt>(op_dx)){

								for (auto iter = op->user_begin(); iter != op->user_end(); ++iter) {
									User *u = *iter;

									if (Instruction *inst2 = dyn_cast<Instruction>(u)){
										if (inst2->getOpcode() == Instruction::Add){
											Value *op_sx2 = inst2->getOperand(0);
											Value *op_dx2 = inst2->getOperand(1);

											if (ConstantInt *c2 = dyn_cast<ConstantInt>(op_dx2)){
												if (c->getValue() == c2->getValue() && op_sx2 == op){
													inst2->replaceAllUsesWith(op_sx);
													toErase.push_back(inst2);
												}
											}
										}
									}
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
			outs()<<"-------------------------\n";

			return PreservedAnalyses::all();
		}

		static bool isRequired() { return true; }

	};
}

llvm::PassPluginLibraryInfo getMultiInstructionOptimizationPassPluginInfo() {
	return {LLVM_PLUGIN_API_VERSION, "MultiInstructionOptimizationPass", LLVM_VERSION_STRING,
		[](PassBuilder &PB) {
			PB.registerPipelineParsingCallback(
				[](StringRef Name, FunctionPassManager &FPM,
					ArrayRef<PassBuilder::PipelineElement>) {
						if (Name == "mio-pass") {
							FPM.addPass(MultiInstructionOptimizationPass());
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
	return getMultiInstructionOptimizationPassPluginInfo();
}
