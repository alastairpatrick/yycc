#ifndef LLVM_H
#define LLVM_H

extern const char* g_llvm_triple;
extern LLVMTargetRef g_llvm_target;
extern LLVMTargetMachineRef g_llvm_target_machine;

void initialize_llvm();

#endif
