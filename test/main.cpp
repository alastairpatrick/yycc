bool run_parser_tests();

const char* g_triple = "thumbv6m-none-eabi";
LLVMTargetRef g_target;
LLVMTargetMachineRef g_target_machine;
LLVMTargetDataRef g_target_data;

int main(int argc, const char *argv[]) {
    char *error = NULL;

    LLVMInitializeARMTarget();
    LLVMInitializeARMTargetMC();
    LLVMInitializeARMTargetInfo();
    //LLVMInitializeAllAsmPrinters();

    error = NULL;
    LLVMGetTargetFromTriple(g_triple, &g_target, &error);
    LLVMDisposeMessage(error);

    g_target_machine = LLVMCreateTargetMachine(g_target, g_triple, "generic", "", LLVMCodeGenLevelDefault, LLVMRelocDefault, LLVMCodeModelDefault);

    g_target_data = LLVMCreateTargetDataLayout(g_target_machine);

    auto success = true;

    success = success && run_parser_tests();

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
