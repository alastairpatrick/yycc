#ifndef CODEGEN_H
#define CODEGEN_H

struct CodeGenContext {
	LLVMModuleRef mod;
	LLVMValueRef function;
	LLVMBuilderRef builder;
};

#endif
