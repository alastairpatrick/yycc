#include "Preprocessor.h"

#include "FileCache.h"
#include "lex/StringLiteral.h"
#include "Message.h"

Preprocessor::Preprocessor(bool preparse): preparse(preparse), text_stream(string_stream) {
    reset_namespace();
}

Preprocessor::Preprocessor(string_view input, bool preparse): Preprocessor(preparse) {
    lexer.buffer(input);
    reset_namespace();
}

void Preprocessor::in(const Input& input) {
    lexer.in(input);
}

TokenKind Preprocessor::next_token() {
    for (;;) {
        next_pp_token();

        if (pending_line) {
            lexer.matcher().lineno(pending_line);
            lexer.matcher().columno(pending_column);
            pending_line = 0;
        }

        switch (token) {
            default: {
              commit_token();
              return token;
          } case TOK_IDENTIFIER: {
              id_lexer.buffer(*identifier.usage_at_file_scope);
              token = id_lexer.next_token();
              if (id_lexer.size() != identifier.usage_at_file_scope->size()) {
                  token = TOK_IDENTIFIER;
              }
              commit_token();
              return token;
          } case TOK_PP_NUMBER: {
              num_lexer.buffer(text());
              token = num_lexer.next_token();
              if (num_lexer.size() != lexer.size()) {
                  token = TOK_PP_NUMBER;
              }
              commit_token();
              return token;
          } case '\n': {
              continue;
          } case '#': {
              next_pp_token();
              if (!handle_directive()) return token;
              continue;
          } case TOK_PP_UNRECOGNIZED: {
              message(Severity::ERROR, location()) << "unexpected character '" << lexer.text() << "'\n";
              pause_messages();
              continue;
          } case TOK_PP_UNTERMINATED_COMMENT: {
              message(Severity::ERROR, location()) << "unterminated comment\n";
              continue;
          }
        }
    }
}

TokenKind Preprocessor::next_pp_token() {
    for (;;) {
        token = lexer.next_token();
        if (token != TOK_EOF || include_stack.empty()) break;

        lexer.pop_matcher();

        auto& context = include_stack.back();

        lexer.lineno(context.location.line);
        lexer.set_filename(context.location.filename);

        current_namespace_prefix = context.current_namespace_prefix;
        namespace_handles = move(context.namespace_handles);

        include_stack.pop_back();
    }

    if (token == TOK_IDENTIFIER) {
        identifier = Identifier();

        auto text = lexer.text();
        identifier.text = intern_string(text);

        if (text[0] == ':') {
            identifier.qualified = intern_string(text.substr(2));
        } else {
            identifier.qualified = intern_string(*current_namespace_prefix, text);
        }

        identifier.usage_at_file_scope = evaluate_identifier(identifier.text);
    }

    return token;
}

bool Preprocessor::handle_directive() {
    switch (token) {
      case TOK_PP_INCLUDE:
        handle_include_directive();
        return true;
      case TOK_PP_LINE:
        handle_line_directive();
        break;
      case TOK_PP_NAMESPACE:
        handle_namespace_directive();
        break;
      case TOK_PP_USING:
        handle_using_directive();
        break;
    }

    if (!preparse) {
      switch (token) {
        case TOK_PP_ERROR:
          handle_error_directive();
          break;
        case TOK_PP_PRAGMA:
          handle_pragma_directive(); 
          break;
        case TOK_PP_ENUM: 
        case TOK_PP_FUNC:
        case TOK_PP_TYPE:
        case TOK_PP_VAR:
          return false;
      }
    }

    require_eol();
    return true;
}

void Preprocessor::skip_to_eol() {
    while (token && token != '\n') {
        next_pp_token();
    }
}

void Preprocessor::require_eol() {
    if (token != '\n') {
        unexpected_directive_token();
    }

    skip_to_eol();
}

void Preprocessor::unexpected_directive_token() {
    if (token == '\n') {
        message(Severity::ERROR, location()) << "unexpected new-line token in directive\n";
    } else {
        message(Severity::ERROR, location()) << "unexpected '" << text() << "' token in directive\n";
    }
}

void Preprocessor::commit_token() {
    if (preparse) {
        text_stream.locate(lexer.location());
        position = string_stream.pcount();
        text_stream.write(text());
    } else {
        position = lexer.position();
    }
}

string_view Preprocessor::output() {
    return string_view(string_stream.str(), string_stream.pcount());
}

static bool parse_line_column(size_t& value, string_view text) {
    auto result = from_chars(text.data(), text.data() + text.size(), value, 10);
    return result.ec == errc{} && value != 0 && result.ptr == text.data() + text.size();
}

void Preprocessor::handle_line_directive() {
    next_pp_token();

    if (token != TOK_PP_NUMBER) return;

    size_t line;
    size_t column = 1;

    if (!parse_line_column(line, lexer.text())) return;
    next_pp_token();

    if (token == TOK_PP_NUMBER) {
        if (!parse_line_column(column, lexer.text())) return;
        next_pp_token();
    }

    InternedString filename{};
    if (token == TOK_STRING_LITERAL) {
        filename = intern_string(unescape_string(lexer.text(), false, lexer.location()).chars);
        next_pp_token();
    }

    if (token != '\n') return;

    pending_line = line;
    pending_column = column - 1;
    if (filename) {
        lexer.set_filename(filename);
    }
}

void Preprocessor::handle_error_directive() {
    auto& stream = message(Severity::ERROR, location(), false);
    next_pp_token();
    auto begin = lexer.text().data();
    auto end = begin;
    while (token && token != '\n') {
        auto text = lexer.text();
        end = text.data() + text.length();
        next_pp_token();
    }

    stream << string_view(begin, end - begin) << '\n';
}

void Preprocessor::handle_include_directive() {
    next_pp_token();

    if (token != TOK_STRING_LITERAL) {
        unexpected_directive_token();
        skip_to_eol();
        return;
    }

    auto header_name = lexer.text();

    auto file = FileCache::it->search(header_name);
    if (!file) {
        message(Severity::ERROR, location()) << "cannot read file " << header_name << '\n';
        skip_to_eol();
        return;
    }

    next_pp_token();
    if (token != '\n') {
        unexpected_directive_token();
        skip_to_eol();
    }

    IncludeContext context;
    context.location = lexer.location();
    context.current_namespace_prefix = current_namespace_prefix;
    context.namespace_handles = namespace_handles;

    include_stack.push_back(context);

    auto matcher = lexer.new_matcher();
    matcher->buffer((char*) file->text.c_str(), file->text.length() + 1);

    lexer.push_matcher(matcher);
    lexer.lineno(1);
    lexer.set_filename(intern_string(file->path.string()));
}

void Preprocessor::handle_pragma_directive() {
    skip_to_eol();
}

void Preprocessor::reset_namespace() {
    current_namespace_prefix = empty_interned_string;
    namespace_handles.clear();

    add_keyword("auto");
    add_keyword("_Bool");
    add_keyword("break");
    add_keyword("case");
    add_keyword("char");
    add_keyword("_Complex");
    add_keyword("const");
    add_keyword("continue");
    add_keyword("default");
    add_keyword("do");
    add_keyword("double");
    add_keyword("else");
    add_keyword("enum");
    add_keyword("extern");
    add_keyword("float");
    add_keyword("for");
    add_keyword("goto");
    add_keyword("if");
    add_keyword("_Imaginary");
    add_keyword("inline");
    add_keyword("int");
    add_keyword("long");
    add_keyword("register");
    add_keyword("restrict");
    add_keyword("return");
    add_keyword("short");
    add_keyword("signed");
    add_keyword("sizeof");
    add_keyword("static");
    add_keyword("struct");
    add_keyword("switch");
    add_keyword("typedef");
    add_keyword("typeof");
    add_keyword("typeof_unqual");
    add_keyword("union");
    add_keyword("unsigned");
    add_keyword("void");
    add_keyword("volatile");
    add_keyword("while");
}

void Preprocessor::add_keyword(string_view id) {
    auto interned = intern_string(id);
    namespace_handles[interned] = interned;
}

void Preprocessor::handle_namespace_directive() {
    reset_namespace();

    next_pp_token();
    if (token != TOK_IDENTIFIER) return;

    auto text = lexer.text();

    if (text[0] == ':') {
        text.remove_prefix(2);
    }

    current_namespace_prefix = intern_string(text, "::");

    size_t handle_begin = 0;
    for (;;) {
        auto handle_end = text.find(':', handle_begin);

        auto handle_name = text.substr(handle_begin, handle_end);
        auto substitution = text.substr(0, handle_end);

        namespace_handles[intern_string(handle_name)] = intern_string(substitution);

        if (handle_end == text.npos) break;
        handle_begin = handle_end + 2;
    }

    next_pp_token();
}

void Preprocessor::handle_using_directive() {
    next_pp_token();
    if (token == '\n') {
        message(Severity::ERROR, location()) << "expected identifier\n";
    }

    if (token != TOK_IDENTIFIER) return;

    auto text = lexer.text();
    next_pp_token();

    string_view handle_name;
    string_view substitution;
    if (token == '=') {
        next_pp_token();
        if (token != TOK_IDENTIFIER) return;

        handle_name = text;
        substitution = lexer.text();
        next_pp_token();
    } else {
        substitution = text;
        handle_name = text;
        auto last_colon_idx = text.rfind(':');
        if (last_colon_idx != text.npos) {
            handle_name = text.substr(last_colon_idx + 1);
        }
    }

    namespace_handles[intern_string(handle_name)] = evaluate_identifier(intern_string(substitution));
}

InternedString Preprocessor::evaluate_identifier(InternedString text) const {
    if ((*text)[0] == ':') return intern_string((*text).substr(2));

    auto it = namespace_handles.find(text);
    if (it != namespace_handles.end()) return it->second;

    return intern_string(*current_namespace_prefix, *text);
}
