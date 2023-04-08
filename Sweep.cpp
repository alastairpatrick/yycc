#include "FileCache.h"
#include "parser/Declaration.h"
#include "parser/IdentifierMap.h"
#include "parser/Parser.h"
#include "preprocessor/Preprocessor.h"
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
    map<string_view, pair<DeclaratorKind, Linkage>> declarator_names;

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
                declarator_names[name] = make_pair(kind, linkage);
            } else {
                if (kind > it->second.first) {
                    it->second = make_pair(kind, linkage);
                }
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

    void output_declaration_directives(ostream& stream, const char* directive, DeclaratorKind kind, Linkage linkage) {
        const int max_col = 120;
        int col = max_col;
        auto need_newline = false;
        for (auto name : declarator_names) {
            if (name.second.first != kind || name.second.second != linkage) continue;

            if (col + name.first.length() > max_col) {
                if (need_newline) stream << '\n';
                need_newline = true;
                col = 0;
                stream << directive;
            }
            stream << ' ' << name.first;
        }
        if (need_newline) stream << '\n';
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

    marker.output_declaration_directives(stream, "#static type", DeclaratorKind::TYPE_DEF, Linkage::NONE);
    marker.output_declaration_directives(stream, "#static enum", DeclaratorKind::ENUM_CONSTANT, Linkage::NONE);
    marker.output_declaration_directives(stream, "#static variable", DeclaratorKind::VARIABLE, Linkage::INTERNAL);
    marker.output_declaration_directives(stream, "#extern variable", DeclaratorKind::VARIABLE, Linkage::EXTERNAL);
    marker.output_declaration_directives(stream, "#static function", DeclaratorKind::FUNCTION, Linkage::INTERNAL);
    marker.output_declaration_directives(stream, "#extern function", DeclaratorKind::FUNCTION, Linkage::EXTERNAL);

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
