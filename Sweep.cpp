#include "FileCache.h"
#include "parse/Declaration.h"
#include "parse/IdentifierMap.h"
#include "parse/Parser.h"
#include "preprocess/Preprocessor.h"
#include "TextStream.h"

void output_declaration_directives_of_kind(TextStream& stream, const vector<Declarator*>& declarators, DeclaratorKind kind, string_view directive) {
    size_t count{};
    for (auto declarator: declarators) {
        if (declarator->delegate->kind() != kind) continue;
        ++count;
    }

    if (count == 0) return;

    stream.new_line();
    stream.write(directive);

    for (auto declarator: declarators) {
        if (declarator->delegate->kind() != kind) continue;
        stream.write(" ");
        stream.write(*declarator->identifier);
    }

    stream.write("\n");
}

void output_declaration_directives(TextStream& stream, Scope* scope) {
    vector<Declarator*> ordered_declarators;
    for (auto pair: scope->declarator_map) {
        Declarator* output_declarator{};
        auto kind = pair.second->delegate->kind();
        for (auto declarator = pair.second; declarator; declarator = declarator->next) {
            if (!output_declarator || output_declarator->delegate->kind() < declarator->delegate->kind()) {
                output_declarator = declarator;
            }
        }

        if (output_declarator) {
            ordered_declarators.push_back(output_declarator);
        }
    }

    sort(ordered_declarators.begin(), ordered_declarators.end(), [](Declarator* a, Declarator* b) {
        return *a->identifier < *b->identifier;
    });

    output_declaration_directives_of_kind(stream, ordered_declarators, DeclaratorKind::ENUM_CONSTANT, "#enum");
    output_declaration_directives_of_kind(stream, ordered_declarators, DeclaratorKind::FUNCTION, "#func");
    output_declaration_directives_of_kind(stream, ordered_declarators, DeclaratorKind::TYPE_DEF, "#type");
    output_declaration_directives_of_kind(stream, ordered_declarators, DeclaratorKind::VARIABLE, "#var");
}

void sweep(ostream& stream, const File& file) {
    Preprocessor preprocessor1(file.text, true);

    IdentifierMap identifiers;
    Parser parser(preprocessor1, identifiers);
    auto declarations = parser.parse();

    Preprocessor preprocessor2(preprocessor1.output(), false);
    
    TextStream text_stream(stream);

    output_declaration_directives(text_stream, identifiers.file_scope());

    auto& oi_scopes = parser.order_independent_scopes;
    auto oi_scope_it = oi_scopes.begin();

    for (auto token = preprocessor2.next_token(); token; token = preprocessor2.next_token()) {
        while (oi_scope_it != oi_scopes.end() && oi_scope_it->position == preprocessor2.position()) {
            output_declaration_directives(text_stream, oi_scope_it->scope);
            ++oi_scope_it;
        }

        if (token != '\n') {
            text_stream.locate(preprocessor2.location());
            text_stream.write(preprocessor2.text());
        }
    }

    stream << "\n";
}
