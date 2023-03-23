#include "parser/Declaration.h"
#include "parser/Parser.h"
#include "parser/SymbolMap.h"
#include "preprocessor/Preprocessor2.h"
#include "TextStream.h"

struct DeclarationMarker {
    DeclarationMarker(const ASTNodeVector& declarations, const SymbolMap& symbols): declarations(declarations), symbols(symbols) {
    }

    const ASTNodeVector& declarations;
    const SymbolMap& symbols;
    unordered_set<const Declaration*> todo;
    unordered_set<const ASTNode*> marked;

    void mark(string_view filename) {
        for (auto node : declarations) {
            if (node->location.filename != filename) continue;

            auto declaration = dynamic_cast<Declaration*>(node);
            assert(declaration);

            todo.insert(declaration);
        }

        while (todo.size()) {
            auto it = todo.begin();
            auto declaration = *it;
            assert(declaration->text.length());

            PPTokenLexerSource preprocessor(declaration->text);
            for (;;) {
                TokenKind token = TokenKind(preprocessor.next_token());
                if (!token) break;
                if (token != TOK_IDENTIFIER) continue;

                Identifier id(preprocessor.text());
                lookup(false, id);
                lookup(true, id);
            }

            todo.erase(it);
            marked.insert(declaration);
        }
    }

    void lookup(bool tag, const Identifier& id) {
        auto declarator = symbols.lookup_declarator(tag, id);
        while (declarator) {
            if (!is_marked(declarator->declaration)) {
                todo.insert(declarator->declaration);
            }
            declarator = declarator->earlier;
        }
    }

    bool is_marked(const ASTNode* declaration) const {
        return marked.find(declaration) != marked.end();
    }
};

void sweep(ostream& stream, string_view input) {
    Parser parser(input, true);
    parser.parse_unit();

    DeclarationMarker marker(parser.declarations, parser.symbols);
    marker.mark("");

    Preprocessor2 preprocessor(input);
    auto token = TokenKind(preprocessor.next_token());

    TextStream text_stream(stream);

    for (auto declaration : parser.declarations) {
        if (!marker.is_marked(declaration)) continue;

        while (token && preprocessor.text().data() < declaration->text.data()) {
            token = TokenKind(preprocessor.next_token());
        }

        while (token && preprocessor.text().data() < (declaration->text.data() + declaration->text.length())) {
            auto location = preprocessor.location();
            text_stream.write(preprocessor.text(), location);

            token = TokenKind(preprocessor.next_token());
        }

        if (!token) break;
    }

    text_stream.stream << "\n";
}
