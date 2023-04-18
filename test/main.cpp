#include "LLVM.h"

bool run_parser_tests();

int main(int argc, const char *argv[]) {
    char *error = NULL;

    initialize_llvm();

    auto success = true;

    success = success && run_parser_tests();

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
