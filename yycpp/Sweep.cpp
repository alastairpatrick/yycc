#include "ast/Declaration.h"
#include "ast/Parser.h"
#include "ast/SymbolMap.h"
#include "PPTokenLexer.yy.h"

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

            PPTokenLexer lexer(Input(declaration->text.data(), declaration->text.length()));
            for (;;) {
                TokenKind token = TokenKind(lexer.next_token());
                if (!token) break;
                if (token != TOK_IDENTIFIER) continue;

                Identifier id(string_view(lexer.matcher().begin(), lexer.size()));
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

    bool is_marked(const ASTNode* declaration) {
        return marked.find(declaration) != marked.end();
    }
};

void sweep(ostream& stream, const string& translation_unit) {
    Parser parser(Input(translation_unit), true);
    parser.parse_unit();

    DeclarationMarker marker(parser.declarations, parser.symbols);
    marker.mark("");

    for (auto declaration: parser.declarations) {
        if (!marker.is_marked(declaration)) continue;
        
        stream << declaration->text;
    }
}
