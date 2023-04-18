#include "FileCache.h"
#include "lexer/Fragment.h"
#include "LLVM.h"
#include "Message.h"
#include "parser/Declaration.h"
#include "parser/Parser.h"
#include "parser/Statement.h"
#include "preprocessor/Preprocessor.h"
#include "TranslationUnitContext.h"
#include "visitor/Emitter.h"
#include "visitor/ResolvePass.h"

struct TranslationInput {
    explicit TranslationInput(IdentifierMap& identifiers, File&& f): file(f), preprocessor(file.text, false), parser(preprocessor, identifiers) {
    }
    void operator=(const TranslationInput&) = delete;

    File file;
    Preprocessor preprocessor;
    Parser parser;
};

bool emit(const ASTNodeVector& nodes) {
    Emitter emitter;
    emitter.outcome = EmitOutcome::IR;
    emitter.module = LLVMModuleCreateWithName("my_module");
    emitter.builder = LLVMCreateBuilder();

    for (auto node: nodes) {
        emitter.emit(node);
    }

    LLVMVerifyModule(emitter.module, LLVMAbortProcessAction, nullptr);

    char* error{};
    LLVMTargetMachineEmitToFile(g_llvm_target_machine, emitter.module, "generated.asm", LLVMAssemblyFile, &error);
    LLVMDisposeMessage(error);

    return true;
}

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

    resolve_pass(identifiers.scopes.front(), nodes);

    emit(nodes);

    return context.highest_severity == Severity::INFO ? EXIT_SUCCESS : EXIT_FAILURE;
}

