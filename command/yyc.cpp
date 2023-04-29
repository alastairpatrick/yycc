#include "FileCache.h"
#include "lex/Fragment.h"
#include "LLVM.h"
#include "Message.h"
#include "parse/Declaration.h"
#include "parse/Parser.h"
#include "parse/Statement.h"
#include "preprocess/Preprocessor.h"
#include "TranslationUnitContext.h"
#include "visit/Emitter.h"
#include "visit/ResolvePass.h"

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

    IdentifierMap identifiers(false);
    list<TranslationInput> inputs;
    for (auto i = 1; i < argc; ++i) {
        auto in_file = FileCache::it->read(argv[i]);

        if (!in_file.exists) {
            message(Severity::ERROR, Location{1, 1, intern_string(argv[i])}) << "could not open input file\n";
        } else {
            inputs.emplace_back(identifiers, move(in_file));
        }
    }

    ASTNodeVector nodes;
    for (auto& input : inputs) {
        ASTNodeVector parsed = input.parser.parse();
        nodes.insert(nodes.end(), parsed.begin(), parsed.end());
    }

    resolve_pass(nodes);

    EmitOptions options;
    auto module = emit_pass(nodes, options);
    
    char* error{};
    LLVMTargetMachineEmitToFile(g_llvm_target_machine, module, "generated.asm", LLVMAssemblyFile, &error);
    LLVMDisposeMessage(error);

    return context.highest_severity == Severity::INFO ? EXIT_SUCCESS : EXIT_FAILURE;
}

