#include "FileCache.h"
#include "parse/Declaration.h"
#include "parse/IdentifierMap.h"
#include "parse/Parser.h"
#include "preprocess/Preprocessor.h"
#include "TextStream.h"

struct DeclarationMarker {
    DeclarationMarker(string_view input, const ASTNodeVector& declarations, const IdentifierMap& identifiers)
        : input(input), declarations(declarations), identifiers(identifiers) {
    }

    string_view input;
    const ASTNodeVector& declarations;
    const IdentifierMap& identifiers;
    unordered_set<const Declaration*> todo;
    unordered_set<const Declaration*> marked;
    unordered_map<string_view, Declarator*> declarator_names;

    void mark() {
        for (auto node : declarations) {
            auto declaration = dynamic_cast<Declaration*>(node);
            assert(declaration);

            if (declaration->mark_root) {
                todo.insert(declaration);
            }
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
            auto name = *declarator->identifier.name;
            auto kind = declarator->delegate->kind();
            auto linkage = declarator->delegate->linkage();

            auto it = declarator_names.find(name);
            if (it == declarator_names.end()) {
                declarator_names[name] = declarator;
            } else {
                if (kind > it->second->delegate->kind()) {
                    it->second = declarator;
                }
            }

            if (!is_marked(declarator->declaration)) {
                todo.insert(declarator->declaration);
            }

            declarator = declarator->next;
        }
    }

    bool is_marked(const Declaration* declaration) const {
        return marked.find(declaration) != marked.end();
    }

    void output_declaration_directives(TextStream& stream) {
        vector<Declarator*> ordered_declarators;
        for (auto p: declarator_names) {
            ordered_declarators.push_back(p.second);
        }

        sort(ordered_declarators.begin(), ordered_declarators.end(), [](Declarator* a, Declarator* b) {
            return a->location < b->location;
        });

        for (auto declarator: ordered_declarators) {
            auto location = declarator->location;
            stream.locate(location);

            switch (declarator->delegate->kind()) {
              case DeclaratorKind::ENUM_CONSTANT:
                stream.write("#enum ");
                break;
              case DeclaratorKind::ENTITY:
                if (declarator->delegate->linkage() == Linkage::EXTERNAL) {
                    stream.write("#extern ");
                } else {
                    stream.write("#static ");
                }
                break;
              case DeclaratorKind::TYPE_DEF:
                stream.write("#type ");
                break;
            }


            stream.write(*declarator->identifier.name);
            stream.write("\n");
        }
    }
};

void sweep(ostream& stream, const File& file) {
    Preprocessor preprocessor1(file.text, true);

    IdentifierMap identifiers(true);
    Parser parser(preprocessor1, identifiers);
    auto declarations = parser.parse();

    DeclarationMarker marker(preprocessor1.output(), declarations, identifiers);
    marker.mark();

    Preprocessor preprocessor2(preprocessor1.output(), false);
    auto token = preprocessor2.next_token();

    TextStream text_stream(stream);

    marker.output_declaration_directives(text_stream);

    vector<const Declaration*> marked(marker.marked.begin(), marker.marked.end());
    sort(marked.begin(), marked.end(), [](const Declaration* a, const Declaration* b) {
        return a->fragment.position > b->fragment.position;
    });

    while (token && marked.size()) {
        auto declaration = marked.back();
        marked.pop_back();

        while (token && preprocessor2.fragment.position < declaration->fragment.position) {
            token = preprocessor2.next_token();
        }

        while (token && preprocessor2.fragment.position < (declaration->fragment.position + declaration->fragment.length)) {
            if (token != '\n') {
                text_stream.locate(preprocessor2.location());
                text_stream.write(preprocessor2.text());
            }

            token = TokenKind(preprocessor2.next_token());
        }
    }

    stream << "\n";
}
