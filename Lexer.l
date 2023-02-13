%option yylineno

%top {
#include "Location.h"
#include "SymbolMap.h"
#include "Type.h"

enum class DirectiveKind {
    NONE,
    LINE,
    PRAGMA,
};
}

%option ctorarg="SymbolMap& symbols"
%option ctorinit="symbols(symbols)"

%class {
    void skip_comment();
    int parse_int_lit(const char* text, int radix);
    int parse_float_lit(const char* text);
    int parse_char_lit(const char* text);
    int parse_string_lit(const char* text, size_t capacity_hint);
    int check_identifier(std::string&& identifier);
    void handle_directive();
    void parse_line_directive(const char* text);

    unordered_set<string> filenames;
    const char* current_filename = "";

    ostream& message();

    SymbolMap& symbols;

public:

    Location location() const {
        return Location { lineno(), columno() + 1, current_filename };
    }

    IntegerSignedness int_signedness;
    IntegerSize int_size;
    unsigned long long int_lit;

    FloatingPointSize float_size;
    long double float_lit;

    bool string_wide;
    string string_lit;  // UTF-8 with C string literal escape sequences expanded

    const string* identifier;  // UTF-8

    DirectiveKind directive_kind{};
    string directive;
};

%{
#include <stdlib.h>
#include <stdio.h>

#include "CompileContext.h"
#include "Decl.h"
#include "Token.h"

static void comment(void);

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


<*> {

"/*"            { skip_comment(); }
"//"[^\n]*      { }
{HW}+           { }

}

// This is the state at the start of the file and the start of each line. From here, switch to state
// D1 if there is a directive or C otherwise.
<INITIAL> {

\n                  { }
"#"                 { start(D1); }
.                   { matcher().unput(text()[0]); start(C); }

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

{L}({L}|{D})*               { return check_identifier(std::move(str())); }

0[xX]{H}+{IS}?              { return parse_int_lit(text() + 2, 16); }
0[bB][0-1]+{IS}?            { return parse_int_lit(text() + 2, 2); }
0[0-7]*{IS}?                { return parse_int_lit(text(), 8); }
[1-9]{D}*{IS}?              { return parse_int_lit(text(), 10); }
L?'(\\.|[^\\'\n])+'         { return parse_char_lit(text()); }

{D}+{E}{FS}?                { return parse_float_lit(text()); }
{D}*"."{D}+{E}?{FS}?        { return parse_float_lit(text()); }
{D}+"."{D}*{E}?{FS}?        { return parse_float_lit(text()); }
0[xX]{H}+{P}{FS}?           { return parse_float_lit(text()); }
0[xX]{H}*"."{H}+{P}?{FS}?   { return parse_float_lit(text()); }
0[xX]{H}+"."{H}*{P}?{FS}?   { return parse_float_lit(text());; }

L?\"(\\.|[^\\"\n])*\"       { return parse_string_lit(text(), size()); }

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

.               { message() << "error unexpected character '" << text() << "'\n"; }

}

// Determine the kind of directive and transition.
<D1> {

"line"          { directive_kind = DirectiveKind::LINE; start(D2); }
"pragma"        { directive_kind = DirectiveKind::PRAGMA; start(D2); }
.               { message() << "error unexpected directive\n"; matcher().skip('\n'); start(INITIAL); }

}

// Tokenize directive until end of line.
<D2> {

{L}({L}|{D})*   { directive += str() + " "; }
{STRING}        { directive += str() + " "; }
{D}+            { directive += str() + " "; }

\n              { handle_directive(); start(INITIAL); }

.               { directive += str() + " "; }

}

%%

ostream& Lexer::message() {
    return ::message(location());
}

void Lexer::skip_comment() {
    if (!matcher().skip("*/")) {
        message() << "error unterminated comment\n";
    }
}

int Lexer::parse_int_lit(const char* text, int radix) {
    char* p;
    auto value = strtoull(text, &p, radix);
    if (errno == ERANGE) {
        // TODO: warn about truncation of int literal
    }

    auto signedness = IntegerSignedness::SIGNED;
    int longs = 0;
    for (; *p; ++p) {
        char c = toupper(*p);
        if (c == 'U') signedness = IntegerSignedness::UNSIGNED;
        else if (c == 'L') ++longs;
        else assert(false);
    }

    assert(longs <= 2);
    auto size = IntegerSize::INT;
    if (longs >= 2) size = IntegerSize::LONG_LONG;
    else if (longs == 1) size = IntegerSize::LONG;

    // TODO: should be checking against target max value.
    unsigned long long max_value;
    if (size == IntegerSize::INT) {
        max_value = UINT_MAX;
    } else if (size == IntegerSize::LONG) {
        max_value = ULONG_MAX;
    } else {
        max_value = ULLONG_MAX;
    }
    
    if (value > max_value) {
        // TODO: warn about truncation of int literal
        value = max_value;
    }

    int_lit = value;
    int_signedness = signedness;
    int_size = size;

    return TOK_INT_LITERAL;
}

int Lexer::parse_float_lit(const char* text) {
    char* p;
    double value = strtod(text, &p);
    if (errno == ERANGE) {
    }

    auto size = FloatingPointSize::DOUBLE;
    for (; *p; ++p) {
        char c = toupper(*p);
        if (c == 'F') size = FloatingPointSize::FLOAT;
        else if (c == 'L') size = FloatingPointSize::LONG_DOUBLE;
        else assert(false);
    }

    float_lit = value;
    float_size = size;

    return TOK_FLOAT_LITERAL;
}


static wchar_t parse_char_code(const char** p, int num_digits, int radix) {
    char digits[9];
    assert(num_digits <= 8);

    for (int i = 0; i < num_digits; ++i) {
        if (**p == 0) {
            cerr << "Truncated string";
            return 0;
        }
        digits[i] = **p;
        (*p)++;
    }
    digits[num_digits] = 0;

    char* end = nullptr;
    wchar_t result = wchar_t(strtol(digits, &end, radix));
    if (end - digits != num_digits) {
        cerr << "Invalid character code";
        return 0;
    }

    return result;
}

static uint32_t unescape_char(const char** p) {
    uint32_t c = **p;
    (*p)++;

    if (c == '\\') {
        c = **p;
        (*p)++;

        switch (c) {
        case 'a':
            c = '\a';
            break;
        case 'b':
            c = '\b';
            break;
        case 'f':
            c = '\f';
            break;
        case 'n':
            c = '\n';
            break;
        case 'r':
            c = '\r';
            break;
        case 't':
            c = '\t';
            break;
        case 'v':
            c = '\v';
            break;
        case 'u':
            c = parse_char_code(p, 4, 16);
            break;
        case 'U':
            c = parse_char_code(p, 8, 16);
            break;
        case 'x':
            c = parse_char_code(p, 2, 16);
            break;
        case '\'':
        case '"':
        case '?':
        case '\\':
            break;
        default:
            if (c >= 0 && c <= '9') {
                (*p)--;
                c = parse_char_code(p, 3, 8);
                break;
            }
            cerr << "Unrecognized escape sequence\n";
        }

        return c;
    } else if ((c & 0x80) == 0) {
        return c;
    } else {
        char b1 = **p;
        (*p)++;
        if ((b1 & 0xC0) != 0x80) return 0;
        if ((c & 0xE0) == 0xC0) return ((c & 0b11111) << 6) | (b1 & 0b111111);

        char b2 = **p;
        (*p)++;
        if ((b2 & 0xC0) != 0x80) return 0;
        if ((c & 0xF0) == 0xE0) return ((c & 0b1111) << 12) | ((b1 & 0b111111) << 6) | (b2 & 0b111111);

        char b3 = **p;
        (*p)++;
        if ((b3 & 0xC0) != 0x80) return 0;
        return ((c & 0b111) << 18) | ((b1 & 0b111111) << 12) | ((b2 & 0b111111) << 6) | (b3 & 0b111111);;
    }

    return 0;
}

int Lexer::parse_char_lit(const char* text) {
    const char* p = text;
    bool is_wide = false;
    if (*p == 'L') {
        is_wide = true;
        ++p;
    }

    assert(*p == '\'');
    ++p;

    if (*p == '\'') {
        // TODO empty character literal
    }

    auto c = unescape_char(&p);

    if (*p != '\'') {
        // TODO multi-characer character literal
    }

    int_lit = c;
    string_wide = is_wide;

    return TOK_CHAR_LITERAL;
}

int Lexer::parse_string_lit(const char* text, size_t capacity_hint) {
    std::string value;
    value.reserve(capacity_hint);

    const char* p = text;
    bool is_wide = false;
    if (*p == 'L') {
        is_wide = true;
        ++p;
    }

    assert(*p == '"');
    ++p;

    while (*p != '"') {
        assert(*p);

        auto c = unescape_char(&p);
        if (c < 0x80) {
            value += c;
        } else if (c < 0x800) {
            value += 0xC0 | (c >> 6);
            value += 0x80 | (c & 0x3F);
        } else if (c < 0x10000) {
            value += 0xE0 | (c >> 12);
            value += 0x80 | ((c >> 6) & 0x3F);
            value += 0x80 | (c & 0x3F);
        } else {
            value += 0xF0 | (c >> 18);
            value += 0x80 | ((c >> 12) & 0x3F);
            value += 0x80 | ((c >> 6) & 0x3F);
            value += 0x80 | (c & 0x3F);
        }
    }

    string_lit = std::move(value);
    string_wide = is_wide;

    return TOK_STRING_LITERAL;
}

void Lexer::handle_directive() {
    if (directive_kind == DirectiveKind::LINE) {
        parse_line_directive(directive.c_str());
    } else if (directive_kind == DirectiveKind::PRAGMA) {
    } else {
        message() << "error unexpected directive\n";
    }

    directive.clear();
}

void Lexer::parse_line_directive(const char* text) {
    const char* p = text;
    while (isspace(*p)) ++p;

    lineno(strtoll(p, (char**) &p, 10) - 1);

    while (isspace(*p)) ++p;

    if (*p == '"') {
        parse_string_lit(p, 0);
        current_filename = filenames.insert(string_lit).first->c_str();
    }
}

int Lexer::check_identifier(std::string&& id)
{
    identifier = intern(std::move(id));
    auto decl = symbols.lookup_decl(TypeNameKind::ORDINARY, identifier);
    return (decl && dynamic_cast<TypeDef*>(decl)) ? TOK_TYPEDEF_IDENTIFIER : TOK_IDENTIFIER;
}
