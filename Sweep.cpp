#include "FileCache.h"
#include "parse/Declaration.h"
#include "parse/IdentifierMap.h"
#include "parse/Parser.h"
#include "preprocess/Preprocessor.h"
#include "TextStream.h"

struct DeclarationMarker {
    DeclarationMarker(const IdentifierMap& identifiers): identifiers(identifiers) {
    }

    const IdentifierMap& identifiers;
    unordered_set<const Declaration*> todo;
    unordered_set<const Declaration*> marked;
    unordered_map<string_view, Declarator*> declarator_names;

    void mark(string_view input, const ASTNodeVector& initial_todo) {
        for (auto node : initial_todo) {
            auto declaration = dynamic_cast<Declaration*>(node);
            assert(declaration);

            todo.insert(declaration);
        }

        while (todo.size()) {
            auto it = todo.begin();
            auto declaration = *it;
            assert(declaration->fragment.length);

            PPTokenLexer lexer;
            lexer.buffer(declaration->fragment.text(input));
            for (;;) {
                TokenKind token = TokenKind(lexer.next_token());
                if (!token) break;
                if (token != TOK_IDENTIFIER) continue;
                lookup(Identifier(lexer.text()));
            }

            todo.erase(it);
            marked.insert(declaration);
        }
    }

    void lookup(const Identifier& id) {
        auto declarator = identifiers.lookup_declarator(id);
        while (declarator) {
            if (!is_marked(declarator->declaration)) {
                todo.insert(declarator->declaration);
            }

            declarator = declarator->next;
        }
    }

    bool is_marked(const Declaration* declaration) const {
        return marked.find(declaration) != marked.end();
    }
};


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
        stream.write(*declarator->identifier.name);
    }

    stream.write("\n");
}

void output_declaration_directives(TextStream& stream, Scope* scope, DeclarationMarker* marker) {
    vector<Declarator*> ordered_declarators;
    for (auto p: scope->declarators) {
        Declarator* output_declarator{};
        auto kind = p.second->delegate->kind();
        for (auto declarator = p.second; declarator; declarator = declarator->next) {
            if (marker && !marker->is_marked(declarator->declaration)) continue;
                
            if (!output_declarator || output_declarator->delegate->kind() < declarator->delegate->kind()) {
                output_declarator = declarator;
            }
        }

        if (output_declarator) {
            ordered_declarators.push_back(output_declarator);
        }
    }

    sort(ordered_declarators.begin(), ordered_declarators.end(), [](Declarator* a, Declarator* b) {
        return *a->identifier.name < *b->identifier.name;
    });

    output_declaration_directives_of_kind(stream, ordered_declarators, DeclaratorKind::ENUM_CONSTANT, "#enum");
    output_declaration_directives_of_kind(stream, ordered_declarators, DeclaratorKind::FUNCTION, "#func");
    output_declaration_directives_of_kind(stream, ordered_declarators, DeclaratorKind::TYPE_DEF, "#type");
    output_declaration_directives_of_kind(stream, ordered_declarators, DeclaratorKind::VARIABLE, "#var");
}

void sweep(ostream& stream, const File& file) {
    Preprocessor preprocessor1(file.text, true);

    IdentifierMap identifiers(true);
    Parser parser(preprocessor1, identifiers);
    auto marked_declarations = parser.parse();

    DeclarationMarker marker(identifiers);
    marker.mark(preprocessor1.output(), marked_declarations);

    Preprocessor preprocessor2(preprocessor1.output(), false);
    auto token = preprocessor2.next_token();

    TextStream text_stream(stream);

    output_declaration_directives(text_stream, &identifiers.scopes.front(), &marker);

    vector<const Declaration*> marked(marker.marked.begin(), marker.marked.end());
    sort(marked.begin(), marked.end(), [](const Declaration* a, const Declaration* b) {
        return a->fragment.position > b->fragment.position;
    });

    auto& oi_scopes = parser.order_independent_scopes;
    auto oi_scope_it = oi_scopes.begin();

    while (token && marked.size()) {
        auto declaration = marked.back();
        marked.pop_back();

        while (token && preprocessor2.fragment.position < declaration->fragment.position) {
            token = preprocessor2.next_token();
        }
        
        while (token && oi_scope_it != oi_scopes.end() && oi_scope_it->position < preprocessor2.fragment.position) {
            ++oi_scope_it;
        }

        while (token && preprocessor2.fragment.position < (declaration->fragment.position + declaration->fragment.length)) {
            if (oi_scope_it != oi_scopes.end() && preprocessor2.fragment.position == oi_scope_it->position) {
                output_declaration_directives(text_stream, oi_scope_it->scope, nullptr);
                ++oi_scope_it;
            }

            if (token != '\n') {
                text_stream.locate(preprocessor2.location());
                text_stream.write(preprocessor2.text());
            }

            token = TokenKind(preprocessor2.next_token());
        }
    }

    stream << "\n";
}
