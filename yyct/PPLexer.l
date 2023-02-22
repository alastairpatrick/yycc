%option yylineno
%option lexer=Phase3Lexer

%top {
#include "../Token.h"
%}

%class{
    TokenKind skip_comment();
public:
    size_t comment_lines{};
}

D               [0-9]
L               [a-zA-Z_]
H               [a-fA-F0-9]
E               ([Ee][+-]?{D}+)
P               ([Pp][+-]?{D}+)
FS              (f|F|l|L)
IS              ((u|U)|(u|U)?(l|L|ll|LL)|(l|L|ll|LL)(u|U))
N               ([0-9a-zA-Z_\.]|([EePp][+-]))
HW              [ \t\v\f\r]

STRING          \"(\\.|[^\\"\n])*\"
    
%xstate PP_TOKENS
%xstate DIRECTIVE

%%


<INITIAL,PP_TOKENS,DIRECTIVE> {

"/*"            { auto token = skip_comment(); return token; }
"//"[^\n]*      { comment_lines = 0; return TOK_PP_COMMENT; }
{HW}+           { }

}

// This is the state at the start of the file and the start of each line. From here, switch to state
// D1 if there is a directive or T otherwise.
<INITIAL> {

\n              { return '\n'; }
"#"             { start(DIRECTIVE); return TOK_PP_CONTROL; }
.               { matcher().wunput(matcher().wchr()); start(PP_TOKENS); }

}

// Tokenize regular text-line
<PP_TOKENS> {

\n              { start(INITIAL); return '\n'; }

{L}({L}|{D})*               { return TOK_IDENTIFIER; }

0[xX]{H}+{IS}?              { return TOK_HEX_INT_LITERAL; }
0[bB][0-1]+{IS}?            { return TOK_BIN_INT_LITERAL; }
0[0-7]*{IS}?                { return TOK_OCT_INT_LITERAL; }
[1-9]{D}*{IS}?              { return TOK_DEC_INT_LITERAL; }

{D}+{E}{FS}?                { return TOK_FLOAT_LITERAL; }
{D}*"."{D}+{E}?{FS}?        { return TOK_FLOAT_LITERAL; }
{D}+"."{D}*{E}?{FS}?        { return TOK_FLOAT_LITERAL; }
0[xX]{H}+{P}{FS}?           { return TOK_FLOAT_LITERAL; }
0[xX]{H}*"."{H}+{P}?{FS}?   { return TOK_FLOAT_LITERAL; }
0[xX]{H}+"."{H}*{P}?{FS}?   { return TOK_FLOAT_LITERAL; }

// This matches any pp-number not recognized as a valid number literal.
"."?{D}{N}*                 { return TOK_PP_NUMBER; }

L?\"(\\.|[^\\"\n])*\"       { return TOK_STRING_LITERAL; }
L?'(\\.|[^\\'\n])*'         { return TOK_CHAR_LITERAL; }

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
";"             { return ';'; }
("{"|"<%")      { return '{'; }
("}"|"%>")      { return '}'; }
","             { return ','; }
":"             { return ':'; }
"="             { return '='; }
"("             { return '('; }
")"             { return ')'; }
("["|"<:")      { return '['; }
("]"|":>")      { return ']'; }
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

.               { return TOK_PP_UNRECOGNIZED; }

}

// Determine the kind of directive and transition.
<DIRECTIVE> {

"define"        { start(PP_TOKENS); return TOK_PP_DEFINE; }
"error"         { start(PP_TOKENS); return TOK_PP_ERROR; }
"elif"          { start(PP_TOKENS); return TOK_PP_ELIF; }
"else"          { start(PP_TOKENS); return TOK_PP_ELSE; }
"endif"         { start(PP_TOKENS); return TOK_PP_ENDIF; }
"if"            { start(PP_TOKENS); return TOK_PP_IF; }
"ifdef"         { start(PP_TOKENS); return TOK_PP_IFDEF; }
"ifndef"        { start(PP_TOKENS); return TOK_PP_IFNDEF; }
"include"       { start(PP_TOKENS); return TOK_PP_INCLUDE; }
"line"          { start(PP_TOKENS); return TOK_PP_LINE; }
"pragma"        { start(PP_TOKENS); return TOK_PP_PRAGMA; }
"undef"         { start(PP_TOKENS); return TOK_PP_UNDEF; }
\n              { start(PP_TOKENS); return '\n'; }

.               { start(PP_TOKENS); return TOK_PP_NONDIRECTIVE; }

}

%%

TokenKind Phase3Lexer::skip_comment() {
    auto start_line = lineno();
    if (!matcher().skip("*/")) return TOK_PP_UNTERMINATED_COMMENT;
    comment_lines = lineno_end() - start_line;
    return TOK_PP_COMMENT;
}
