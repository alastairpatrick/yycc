#include "Module.h"
#include "LLVM.h"
#include "TranslationUnitContext.h"

#pragma warning(disable: 4624)

#include "llvm/IR/InstrTypes.h"

// todo: contribute to LLVM
LLVMValueRef LLVMGetArgOperand2(LLVMValueRef Instr, unsigned i) {
  return llvm::wrap(llvm::unwrap<llvm::CallBase>(Instr)->getArgOperand(i));
}

struct SubstitutionPass {
    Module& module;

    SubstitutionPass(Module& module): module(module) {
    }

    void go() {
        auto context = TranslationUnitContext::it;

        auto builder = LLVMCreateBuilder();

        auto destructor_type = LLVMFunctionType(context->llvm_void_type, &context->llvm_pointer_type, 1, false);

        for (auto function = LLVMGetFirstFunction(module.llvm_module); function; function = LLVMGetNextFunction(function)) {
            for (auto block = LLVMGetFirstBasicBlock(function); block; block = LLVMGetNextBasicBlock(block)) {
                for (auto instruction = LLVMGetFirstInstruction(block); instruction;) {
                    auto next_instruction = LLVMGetNextInstruction(instruction);

                    if (LLVMIsACallInst(instruction)) {
                        auto function = LLVMGetCalledValue(instruction);
                        auto it = module.destructor_placeholder_functions.find(function);
                        if (it != module.destructor_placeholder_functions.end()) {
                            auto num_args = LLVMGetNumArgOperands(instruction);
                            assert(num_args == 4);

                            auto receiver = LLVMGetArgOperand2(instruction, 0);
                            auto destructor_function = LLVMGetArgOperand2(instruction, 1);
                            auto actual_state = LLVMGetArgOperand2(instruction, 2);
                            auto default_state = LLVMGetArgOperand2(instruction, 3);

                            if (actual_state != default_state) {
                                assert(LLVMIsAConstant(default_state));
                                LLVMPositionBuilderBefore(builder, instruction);
                                LLVMBuildCall2(builder, destructor_type, destructor_function, &receiver, 1, "");
                            }

                            LLVMInstructionEraseFromParent(instruction);
                        }
                    }

                    instruction = next_instruction;
                }
            }
        }

        LLVMDisposeBuilder(builder);

        for (auto function: module.destructor_placeholder_functions) {
            LLVMDeleteFunction(function);
        }

        module.destructor_placeholders.clear();
        module.destructor_placeholder_functions.clear();
    }
};


void Module::substitution_pass() {
    SubstitutionPass pass(*this);
    pass.go();
}
