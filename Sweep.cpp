#include "parser/Declaration.h"
#include "parser/Parser.h"
#include "parser/SymbolMap.h"
#include "preprocessor/Preprocessor.h"
#include "TextStream.h"

struct DeclarationMarker {
    DeclarationMarker(string_view input, const ASTNodeVector& declarations, const SymbolMap& symbols)
        : input(input), declarations(declarations), symbols(symbols) {
    }

    string_view input;
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

            PPTokenLexer lexer;
            lexer.buffer(declaration->fragment.text(input));
            for (;;) {
                TokenKind token = TokenKind(lexer.next_token());
                if (!token) break;
                if (token != TOK_IDENTIFIER) continue;

                Identifier id(lexer.text());
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

void sweep(ostream& stream, const Input& input) {
    Preprocessor preprocessor1(true);
    preprocessor1.in(input);

    Parser parser(preprocessor1, true);
    parser.parse_unit();

    DeclarationMarker marker(preprocessor1.output(), parser.declarations, parser.symbols);
    marker.mark("");

    Preprocessor preprocessor2(false);
    preprocessor2.buffer(preprocessor1.output());
    auto token = preprocessor2.next_token();

    TextStream text_stream(stream);

    for (auto declaration : parser.declarations) {
        if (!marker.is_marked(declaration)) continue;

        while (token && preprocessor2.fragment().position < declaration->fragment.position) {
            token = preprocessor2.next_token();
        }

        while (token && preprocessor2.fragment().position < (declaration->fragment.position + declaration->fragment.length)) {
            auto location = preprocessor2.location();
            text_stream.write(preprocessor2.text(), location);

            token = TokenKind(preprocessor2.next_token());
        }

        if (!token) break;
    }

    stream << "\n";
}
