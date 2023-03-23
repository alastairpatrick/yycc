#include "parser/Declaration.h"
#include "parser/Parser.h"
#include "parser/SymbolMap.h"
#include "preprocessor/Preprocessor.h"

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

    Preprocessor preprocessor(input);
    auto token = TokenKind(preprocessor.next_token());
    auto line = 1;
    auto col = 1;
    string_view filename;

    for (auto declaration : parser.declarations) {
        if (!marker.is_marked(declaration)) continue;

        while (token && preprocessor.text().data() < declaration->text.data()) {
            token = TokenKind(preprocessor.next_token());
        }

        while (token && preprocessor.text().data() < (declaration->text.data() + declaration->text.length())) {
            auto location = preprocessor.location();

            if (location.filename != filename) {
                stream << "\n#line " << location.line << " \"" << location.filename << "\"\n";
                line = location.line;
                col = 1;
                filename = location.filename;
            } else if (location.line - line < 5) {
                while (location.line > line) {
                    stream << '\n';
                    ++line;
                    col = 1;
                }
            } else if (location.line != line) {
                stream << "\n#line " << location.line << "\n";
                line = location.line;
                col = 1;
            }

            while (col < location.column) {
                stream << ' ';
                ++col;
            }

            stream << preprocessor.text();
            col += preprocessor.text().size();
        
            token = TokenKind(preprocessor.next_token());
        }

        if (!token) break;
    }

    stream << '\n';
}
