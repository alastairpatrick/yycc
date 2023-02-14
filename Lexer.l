%option yylineno

%top {
#include "CompileContext.h"
#include "Identifier.h"
#include "Location.h"

enum class DirectiveKind {
    NONE,
    ERROR,
    LINE,
    PRAGMA,
};
}

%class {
    void skip_comment();
    void handle_directive();
    void parse_line_directive(const char* text);

    unordered_set<string> filenames;
    const char* current_filename = "";

    ostream& message(Severity severity);
    ostream& message(Severity severity, const Location&);

public:

    Location location() const {
        return Location { lineno(), columno() + 1, current_filename };
    }

    size_t byte_offset() const {
        return matcher().first();
    }

    Identifier identifier() const;

    DirectiveKind directive_kind{};
    string directive;
    Location directive_location;
};

%{
#include "Token.h"
%}

D               [0-9]
L               [a-zA-Z_]
H               [a-fA-F0-9]
E               ([Ee][+-]?{D}+)
P               ([Pp][+-]?{D}+)
FS              (f|F|l|L)
IS              ((u|U)|(u|U)?(l|L|ll|LL)|(l|L|ll|LL)(u|U))
HW              [ \t\v\f\r]

STRING          \"(\\.|[^\\"\n])*\"

%xstate C
%xstate D1
%xstate D2

%%


<INITIAL,C,D1> {

"/*"            { skip_comment(); }
"//"[^\n]*      { }
{HW}+           { }

}

// This is the state at the start of the file and the start of each line. From here, switch to state
// D1 if there is a directive or C otherwise.
<INITIAL> {

\n              { }
"#"             { directive_location = location(); start(D1); }
.               { matcher().unput(text()[0]); start(C); }

}

// Tokenize regular C code until end of line.
<C> {

\n              { start(INITIAL); }

"auto"          { return TOK_AUTO; }
"_Bool"         { return TOK_BOOL; }
"break"         { return TOK_BREAK; }
"case"          { return TOK_CASE; }
"char"          { return TOK_CHAR; }
"_Complex"      { return TOK_COMPLEX; }
"const"         { return TOK_CONST; }
"continue"      { return TOK_CONTINUE; }
"default"       { return TOK_DEFAULT; }
"do"            { return TOK_DO; }
"double"        { return TOK_DOUBLE; }
"else"          { return TOK_ELSE; }
"enum"          { return TOK_ENUM; }
"extern"        { return TOK_EXTERN; }
"float"         { return TOK_FLOAT; }
"for"           { return TOK_FOR; }
"goto"          { return TOK_GOTO; }
"if"            { return TOK_IF; }
"_Imaginary"    { return TOK_IMAGINARY; }
"inline"        { return TOK_INLINE; }
"int"           { return TOK_INT; }
"long"          { return TOK_LONG; }
"register"      { return TOK_REGISTER; }
"restrict"      { return TOK_RESTRICT; }
"return"        { return TOK_RETURN; }
"short"         { return TOK_SHORT; }
"signed"        { return TOK_SIGNED; }
"sizeof"        { return TOK_SIZEOF; }
"static"        { return TOK_STATIC; }
"struct"        { return TOK_STRUCT; }
"switch"        { return TOK_SWITCH; }
"typedef"       { return TOK_TYPEDEF; }
"union"         { return TOK_UNION; }
"unsigned"      { return TOK_UNSIGNED; }
"void"          { return TOK_VOID; }
"volatile"      { return TOK_VOLATILE; }
"while"         { return TOK_WHILE; }

{L}({L}|{D})*               { return TOK_IDENTIFIER; }

0[xX]{H}+{IS}?              { return TOK_HEX_INT_LITERAL; }
0[bB][0-1]+{IS}?            { return TOK_BIN_INT_LITERAL; }
0[0-7]*{IS}?                { return TOK_OCT_INT_LITERAL; }
[1-9]{D}*{IS}?              { return TOK_DEC_INT_LITERAL; }
L?'(\\.|[^\\'\n])+'         { return TOK_CHAR_LITERAL; }

{D}+{E}{FS}?                { return TOK_FLOAT_LITERAL; }
{D}*"."{D}+{E}?{FS}?        { return TOK_FLOAT_LITERAL; }
{D}+"."{D}*{E}?{FS}?        { return TOK_FLOAT_LITERAL; }
0[xX]{H}+{P}{FS}?           { return TOK_FLOAT_LITERAL; }
0[xX]{H}*"."{H}+{P}?{FS}?   { return TOK_FLOAT_LITERAL; }
0[xX]{H}+"."{H}*{P}?{FS}?   { return TOK_FLOAT_LITERAL;; }

L?\"(\\.|[^\\"\n])*\"       { return TOK_STRING_LITERAL; }

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

.               { message(Severity::ERROR) << "unexpected character '" << text() << "'\n"; }

}

// Determine the kind of directive and transition.
<D1> {

"error"{HW}*    { directive_kind = DirectiveKind::ERROR; start(D2); }
"line"{HW}*     { directive_kind = DirectiveKind::LINE; start(D2); }
"pragma"{HW}*   { directive_kind = DirectiveKind::PRAGMA; start(D2); }
.               { message(Severity::ERROR, directive_location) << "unexpected directive\n"; matcher().skip('\n'); start(INITIAL); }

}

// Tokenize directive until end of line.
<D2> {

"/*"            { skip_comment(); directive += " "; }
"//"[^\n]*      { directive += " "; }

{L}({L}|{D})*   { directive += str(); }
{STRING}        { directive += str(); }
{D}+            { directive += str(); }

\n              { handle_directive(); start(INITIAL); }

.               { directive += str(); }

}

%%

ostream& Lexer::message(Severity severity) {
    return ::message(severity, location());
}

ostream& Lexer::message(Severity severity, const Location& location) {
    return ::message(severity, location);
}

void Lexer::skip_comment() {
    if (!matcher().skip("*/")) {
        message(Severity::ERROR) << "unterminated comment\n";
    }
}

void Lexer::handle_directive() {
    if (directive_kind == DirectiveKind::LINE) {
        parse_line_directive(directive.c_str());
    } else if (directive_kind == DirectiveKind::PRAGMA) {
    } else if (directive_kind == DirectiveKind::ERROR) {
        message(Severity::ERROR, directive_location) << directive << '\n';
    } else {
        message(Severity::ERROR, directive_location) << "unexpected directive\n";
    }

    directive_kind = DirectiveKind::NONE;
    directive.clear();
}

string parse_string(const char* text, size_t capacity_hint, const Location& location);

void Lexer::parse_line_directive(const char* text) {
    const char* p = text;
    while (isspace(*p)) ++p;

    lineno(strtoll(p, (char**) &p, 10) - 1);

    while (isspace(*p)) ++p;

    if (*p == '"') {
        auto filename = parse_string(p, 0, location());
        current_filename = filenames.insert(filename).first->c_str();
    }
}

Identifier Lexer::identifier() const
{
    Identifier result;
    result.name = intern(str());
    result.byte_offset = byte_offset();
    return result;
}
