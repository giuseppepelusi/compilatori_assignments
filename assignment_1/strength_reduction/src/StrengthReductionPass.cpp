#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

	struct StrengthReductionPass : public PassInfoMixin<StrengthReductionPass> {

		PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {

			outs()<<"Prima dell'ottimizzazione\n";
			for (auto &BB : F)
				for (auto &I : BB)
					outs()<<I<<"\n";
			outs()<<"-------------------------\n";

			auto isPow2 = [](int64_t n) -> bool {
			    return n > 0 && (n & (n - 1)) == 0;
			};
			auto log2Floor = [](int64_t n) -> unsigned {
			    unsigned k = 0;
			    while (n > 1) { n >>= 1; ++k; }
			    return k;
			};

			std::vector<Instruction *> toErase;

			for (auto &BB : F) {
			    for (auto &I : BB) {
					if (BinaryOperator *op = dyn_cast<BinaryOperator>(&I)){
				        if (op->getOpcode() == Instruction::Mul) {
				            Value *op_sx = op->getOperand(0);
				            Value *op_dx = op->getOperand(1);

				            if (ConstantInt *c = dyn_cast<ConstantInt>(op_sx)) {
				                int64_t val = c->getSExtValue();
				                if (val > 0 && isPow2(val)) {
				                    unsigned k = log2Floor(val);
				                    Instruction *new_inst = BinaryOperator::Create(
				                        Instruction::Shl, op_dx, ConstantInt::get(c->getType(), k));
				                    new_inst->insertAfter(op);
				                    op->replaceAllUsesWith(new_inst);
									toErase.push_back(op);
				                }
								else if (val > 0 && isPow2(val + 1)) {
				                    unsigned k = log2Floor(val + 1);
				                    Instruction *tmp = BinaryOperator::Create(
				                        Instruction::Shl, op_dx, ConstantInt::get(c->getType(), k));
				                    tmp->insertAfter(op);
				                    Instruction *new_inst = BinaryOperator::Create(
				                        Instruction::Sub, tmp, op_dx);
				                    new_inst->insertAfter(tmp);
				                    op->replaceAllUsesWith(new_inst);
									toErase.push_back(op);
				                }
								else if (val > 0 && isPow2(val - 1)) {
				                    unsigned k = log2Floor(val - 1);
				                    Instruction *tmp = BinaryOperator::Create(
				                        Instruction::Shl, op_dx, ConstantInt::get(c->getType(), k));
				                    tmp->insertAfter(op);
				                    Instruction *new_inst = BinaryOperator::Create(
				                        Instruction::Add, tmp, op_dx);
				                    new_inst->insertAfter(tmp);
				                    op->replaceAllUsesWith(new_inst);
									toErase.push_back(op);
				                }
				            }
							else if (ConstantInt *c = dyn_cast<ConstantInt>(op_dx)) {
				                int64_t val = c->getSExtValue();

				                if (val > 0 && isPow2(val)) {
				                    unsigned k = log2Floor(val);
				                    Instruction *new_inst = BinaryOperator::Create(
				                        Instruction::Shl, op_sx, ConstantInt::get(c->getType(), k));
				                    new_inst->insertAfter(op);
				                    op->replaceAllUsesWith(new_inst);
									toErase.push_back(op);
				                }
								else if (val > 0 && isPow2(val + 1)) {
				                    unsigned k = log2Floor(val + 1);
				                    Instruction *tmp = BinaryOperator::Create(
				                        Instruction::Shl, op_sx, ConstantInt::get(c->getType(), k));
				                    tmp->insertAfter(op);
				                    Instruction *new_inst = BinaryOperator::Create(
				                        Instruction::Sub, tmp, op_sx);
				                    new_inst->insertAfter(tmp);
				                    op->replaceAllUsesWith(new_inst);
									toErase.push_back(op);
				                }
								else if (val > 0 && isPow2(val - 1)) {
				                    unsigned k = log2Floor(val - 1);
				                    Instruction *tmp = BinaryOperator::Create(
				                        Instruction::Shl, op_sx, ConstantInt::get(c->getType(), k));
				                    tmp->insertAfter(op);
				                    Instruction *new_inst = BinaryOperator::Create(
				                        Instruction::Add, tmp, op_sx);
				                    new_inst->insertAfter(tmp);
				                    op->replaceAllUsesWith(new_inst);
									toErase.push_back(op);
				                }
				            }
				        }
						else if (op->getOpcode() == Instruction::SDiv) {
				            Value *op_sx = op->getOperand(0);
				            Value *op_dx = op->getOperand(1);

				            if (ConstantInt *c = dyn_cast<ConstantInt>(op_dx)) {
				                int64_t val = c->getSExtValue();
				                if (val > 0 && isPow2(val)) {
				                    unsigned k = log2Floor(val);
				                    Instruction *new_inst = BinaryOperator::Create(
				                        Instruction::AShr, op_sx, ConstantInt::get(c->getType(), k));
				                    new_inst->insertAfter(&I);
				                    op->replaceAllUsesWith(new_inst);
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

llvm::PassPluginLibraryInfo getStrengthReductionPassPluginInfo() {
	return {LLVM_PLUGIN_API_VERSION, "StrengthReductionPass", LLVM_VERSION_STRING,
		[](PassBuilder &PB) {
			PB.registerPipelineParsingCallback(
				[](StringRef Name, FunctionPassManager &FPM,
					ArrayRef<PassBuilder::PipelineElement>) {
						if (Name == "sr-pass") {
							FPM.addPass(StrengthReductionPass());
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
	return getStrengthReductionPassPluginInfo();
}
