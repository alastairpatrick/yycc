%option lexer=PPNumberLexer
%option prefix=PPNumberLexer
%option lex=next_token
%option token-type=TokenKind
%option freespace
%option case-insensitive
%option dotall
%option fast

%top {
#include "lex/Token.h"
%}

%class {
public:
    void buffer(string_view input) {
        AbstractBaseLexer::buffer((char*) input.data(), input.size() + 1);
    }

    string_view text() const {
        return string_view(matcher().begin(), size());
    }

    const char* rest() = delete;
}

D               [0-9]
H               [a-f0-9]
E               (e[+-]?{D}+)
P               (p[+-]?{D}+)
FS              [fl]
IS              (u|u?(l|ll)|(l|ll)u)

%%

0x {H}+ {IS}?               { return TOK_HEX_INT_LITERAL; }
0b [0-1]+ {IS}?             { return TOK_BIN_INT_LITERAL; }
0 [0-7]* {IS}?              { return TOK_OCT_INT_LITERAL; }
[1-9] {D}* {IS}?            { return TOK_DEC_INT_LITERAL; }

{D}+ {E} {FS}?              { return TOK_DEC_FLOAT_LITERAL; }
{D}* "." {D}+ {E}? {FS}?    { return TOK_DEC_FLOAT_LITERAL; }
{D}+ "." {D}* {E}? {FS}?    { return TOK_DEC_FLOAT_LITERAL; }
0x {H}+ {P} {FS}?           { return TOK_HEX_FLOAT_LITERAL; }
0x {H}* "." {H}+ {P} {FS}?  { return TOK_HEX_FLOAT_LITERAL; }
0x {H}+ "." {H}* {P} {FS}?  { return TOK_HEX_FLOAT_LITERAL;; }

.                           { return TOK_PP_NUMBER; }
