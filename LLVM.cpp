#include "LLVM.h"

const char* g_llvm_triple = "thumbv6m-none-eabi";
LLVMTargetRef g_llvm_target{};
LLVMTargetMachineRef g_llvm_target_machine{};

void initialize_llvm() {
    LLVMInitializeARMTarget();
    LLVMInitializeARMTargetMC();
    LLVMInitializeARMTargetInfo();
    LLVMInitializeARMAsmPrinter();
    
    char* error{};
    LLVMGetTargetFromTriple(g_llvm_triple, &g_llvm_target, &error);
    if (error) {
        cerr << error << "\n";
        LLVMDisposeMessage(error);
    }

    g_llvm_target_machine = LLVMCreateTargetMachine(g_llvm_target, g_llvm_triple, "generic", "", LLVMCodeGenLevelDefault, LLVMRelocDefault, LLVMCodeModelDefault);
}
