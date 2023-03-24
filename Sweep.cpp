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
            assert(declaration->fragment.length);

            PPTokenLexerSource lexer;
            lexer.set_input(declaration->fragment);
            for (;;) {
                TokenKind token = TokenKind(lexer.next_token());
                if (!token) break;
                if (token != TOK_IDENTIFIER) continue;

                Identifier id(lexer.fragment().text());
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

void sweep(ostream& stream) {
    Parser parser(true);
    parser.parse_unit();

    DeclarationMarker marker(parser.declarations, parser.symbols);
    marker.mark("");

    Preprocessor2 preprocessor;
    preprocessor.set_input(Fragment::context());
    auto token = TokenKind(preprocessor.next_token());

    TextStream text_stream(stream);

    for (auto declaration : parser.declarations) {
        if (!marker.is_marked(declaration)) continue;

        while (token && preprocessor.fragment().position < declaration->fragment.position) {
            token = TokenKind(preprocessor.next_token());
        }

        while (token && preprocessor.fragment().position < (declaration->fragment.position + declaration->fragment.length)) {
            auto location = preprocessor.location();
            text_stream.write(preprocessor.fragment().text(), location);

            token = TokenKind(preprocessor.next_token());
        }

        if (!token) break;
    }

    text_stream.stream << "\n";
}
