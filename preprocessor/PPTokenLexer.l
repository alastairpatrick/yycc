%option lexer=PPTokenLexer
%option prefix=PPTokenLexer
%option lex=next_token
%option freespace
%option yylineno
%option fast

%{
#include "../Token.h"
%}

D               [0-9]
L               [a-zA-Z_]
N               ([0-9a-zA-Z_\.]|([EePp][+-]))
HW              [ \t\v\f\r]

%xstate PP_TOKENS
%xstate DIRECTIVE

%%

<*> {

"/*"            { if (!matcher().skip("*/")) return TOK_PP_UNTERMINATED_COMMENT; }
"//" [^\n]*     { }
{HW}+           { }
\n              { start(INITIAL); return '\n'; }

}

// This is the state at the start of each line. From here, switch to state
// DIRECTIVE or PP_TOKENS, depending on first non-white-space character.
<INITIAL> {
"#"             { start(DIRECTIVE); return '#'; }
.               { matcher().less(0); start(PP_TOKENS); }
}

// Tokenize regular text-line
<PP_TOKENS> {

{L} ({L} | {D})*            { return TOK_IDENTIFIER; }

"."? {D} {N}*               { return TOK_PP_NUMBER; }

L? \" (\\. | [^\\"\n])* \"  { return TOK_STRING_LITERAL; }
L?  ' (\\. | [^\\'\n])*  '  { return TOK_CHAR_LITERAL; }

"..."           { return TOK_ELLIPSIS; }
">>="           { return TOK_RIGHT_ASSIGN; }
"<<="           { return TOK_LEFT_ASSIGN; }
"+="            { return TOK_ADD_ASSIGN; }
"-="            { return TOK_SUB_ASSIGN; }
"*="            { return TOK_MUL_ASSIGN; }
"/="            { return TOK_DIV_ASSIGN; }
"%="            { return TOK_MOD_ASSIGN; }
"&="            { return TOK_AND_ASSIGN; }
"^="            { return TOK_XOR_ASSIGN; }
"|="            { return TOK_OR_ASSIGN; }
">>"            { return TOK_RIGHT_OP; }
"<<"            { return TOK_LEFT_OP; }
"++"            { return TOK_INC_OP; }
"--"            { return TOK_DEC_OP; }
"->"            { return TOK_PTR_OP; }
"&&"            { return TOK_AND_OP; }
"||"            { return TOK_OR_OP; }
"<="            { return TOK_LE_OP; }
">="            { return TOK_GE_OP; }
"=="            { return TOK_EQ_OP; }
"!="            { return TOK_NE_OP; }
"##" | "%:%:"   { return TOK_PP_CONCAT; }
";"             { return ';'; }
"{" | "<%"      { return '{'; }
"}" | "%>"      { return '}'; }
","             { return ','; }
":"             { return ':'; }
"="             { return '='; }
"("             { return '('; }
")"             { return ')'; }
"[" | "<:"      { return '['; }
"]" | ":>"      { return ']'; }
"."             { return '.'; }
"&"             { return '&'; }
"!"             { return '!'; }
"~"             { return '~'; }
"-"             { return '-'; }
"+"             { return '+'; }
"*"             { return '*'; }
"/"             { return '/'; }
"%"             { return '%'; }
"<"             { return '<'; }
">"             { return '>'; }
"^"             { return '^'; }
"|"             { return '|'; }
"?"             { return '?'; }
"#" | "%:"      { return '#'; }

}

// Determine the kind of directive and transition.
<DIRECTIVE> {

d(efine)?       { start(PP_TOKENS); return TOK_PP_DEFINE; }
error           { start(PP_TOKENS); return TOK_PP_ERROR; }
elif            { start(PP_TOKENS); return TOK_PP_ELIF; }
else            { start(PP_TOKENS); return TOK_PP_ELSE; }
endif           { start(PP_TOKENS); return TOK_PP_ENDIF; }
if              { start(PP_TOKENS); return TOK_PP_IF; }
ifdef           { start(PP_TOKENS); return TOK_PP_IFDEF; }
ifndef          { start(PP_TOKENS); return TOK_PP_IFNDEF; }
include         { start(PP_TOKENS); return TOK_PP_INCLUDE; }
line            { start(PP_TOKENS); return TOK_PP_LINE; }
pragma          { start(PP_TOKENS); return TOK_PP_PRAGMA; }
undef           { start(PP_TOKENS); return TOK_PP_UNDEF; }

}

<PP_TOKENS,DIRECTIVE> {
.               { return TOK_PP_UNRECOGNIZED; }
}

%%
