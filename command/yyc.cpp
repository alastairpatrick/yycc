#include "FileCache.h"
#include "LLVM.h"
#include "Message.h"
#include "parse/Declaration.h"
#include "parse/Parser.h"
#include "parse/Statement.h"
#include "pass/Module.h"
#include "preprocess/Preprocessor.h"
#include "TranslationUnitContext.h"

struct TranslationInput {
    explicit TranslationInput(IdentifierMap& identifiers, File&& f): file(f), preprocessor(file.text, false), parser(preprocessor, identifiers) {
    }
    void operator=(const TranslationInput&) = delete;

    File file;
    Preprocessor preprocessor;
    Parser parser;
};

int main(int argc, const char* argv[]) {
    initialize_llvm();
    FileCache file_cache(true);

    TranslationUnitContext context(cerr);

    IdentifierMap identifiers;
    list<TranslationInput> inputs;
    for (auto i = 1; i < argc; ++i) {
        auto in_file = FileCache::it->read(argv[i]);

        if (!in_file.exists) {
            message(Severity::ERROR, Location{1, 1, intern_string(argv[i])}) << "could not open input file\n";
        } else {
            inputs.emplace_back(identifiers, move(in_file));
        }
    }

    vector<Declaration*> declarations;
    for (auto& input : inputs) {
        vector<Declaration*> parsed = input.parser.parse();
        declarations.insert(declarations.end(), parsed.begin(), parsed.end());
    }

    Module module;
    module.resolve_pass(declarations, *identifiers.file_scope());
    module.entity_pass();
    module.emit_pass({});
    module.analysis_pass();
    module.post_analysis_pass();

    auto pass_builder_options = LLVMCreatePassBuilderOptions();

    LLVMRunPasses(module.llvm_module, "default<O0>", g_llvm_target_machine, pass_builder_options);

    LLVMDisposePassBuilderOptions(pass_builder_options);

    char* error{};
    LLVMTargetMachineEmitToFile(g_llvm_target_machine, module.llvm_module, "generated.asm", LLVMAssemblyFile, &error);
    LLVMDisposeMessage(error);

    return context.highest_severity == Severity::INFO ? EXIT_SUCCESS : EXIT_FAILURE;
}

