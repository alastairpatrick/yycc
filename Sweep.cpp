#include "FileCache.h"
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
    unordered_set<const Declaration*> marked;
    set<string_view> type_names;

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
        auto declarator = symbols.lookup_declarator(id);
        while (declarator) {
            if (auto type_def = dynamic_cast<const TypeDef*>(declarator)) {
                type_names.insert(*type_def->identifier.name);
            }
            if (!is_marked(declarator->declaration)) {
                todo.insert(declarator->declaration);
            }
            declarator = declarator->earlier;
        }
    }

    bool is_marked(const Declaration* declaration) const {
        return marked.find(declaration) != marked.end();
    }
};

void sweep(ostream& stream, const File& file) {
    Preprocessor preprocessor1(true);
    preprocessor1.buffer(file.text);

    Parser parser(preprocessor1, true);
    parser.parse_unit();

    DeclarationMarker marker(preprocessor1.output(), parser.declarations, parser.symbols);
    marker.mark();

    Preprocessor preprocessor2(false);
    preprocessor2.buffer(preprocessor1.output());
    auto token = preprocessor2.next_token();

    TextStream text_stream(stream);

    const int max_col = 120;
    int col = max_col;
    auto need_newline = false;
    for (auto type_name : marker.type_names) {
        if (col + type_name.length() > max_col) {
            if (need_newline) stream << '\n';
            need_newline = true;
            col = 0;
            stream << "#type";
        }
        stream << ' ' << type_name;
    }
    if (need_newline) stream << '\n';

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
            text_stream.locate(preprocessor2.location());
            text_stream.write(preprocessor2.text());

            token = TokenKind(preprocessor2.next_token());
        }
    }

    stream << "\n";
}
