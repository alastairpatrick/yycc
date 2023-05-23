%option lexer=IdentifierLexer
%option prefix=IdentifierLexer
%option lex=next_token
%option token-type=TokenKind
%option freespace
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

%%

auto          { return TOK_AUTO; }
_Bool         { return TOK_BOOL; }
break         { return TOK_BREAK; }
case          { return TOK_CASE; }
char          { return TOK_CHAR; }
_Complex      { return TOK_COMPLEX; }
const         { return TOK_CONST; }
continue      { return TOK_CONTINUE; }
default       { return TOK_DEFAULT; }
do            { return TOK_DO; }
double        { return TOK_DOUBLE; }
else          { return TOK_ELSE; }
enum          { return TOK_ENUM; }
extern        { return TOK_EXTERN; }
float         { return TOK_FLOAT; }
for           { return TOK_FOR; }
goto          { return TOK_GOTO; }
if            { return TOK_IF; }
_Imaginary    { return TOK_IMAGINARY; }
inline        { return TOK_INLINE; }
int           { return TOK_INT; }
long          { return TOK_LONG; }
register      { return TOK_REGISTER; }
restrict      { return TOK_RESTRICT; }
return        { return TOK_RETURN; }
short         { return TOK_SHORT; }
signed        { return TOK_SIGNED; }
sizeof        { return TOK_SIZEOF; }
static        { return TOK_STATIC; }
struct        { return TOK_STRUCT; }
switch        { return TOK_SWITCH; }
typedef       { return TOK_TYPEDEF; }
typeof        { return TOK_TYPEOF; }
typeof_unqual { return TOK_TYPEOF_UNQUAL; }
union         { return TOK_UNION; }
unsigned      { return TOK_UNSIGNED; }
void          { return TOK_VOID; }
volatile      { return TOK_VOLATILE; }
while         { return TOK_WHILE; }

std::bool           { return TOK_BOOL; }
std::catch          { return TOK_CATCH; }
std::false          { return TOK_FALSE; }
std::throw          { return TOK_THROW; }
std::transitory     { return TOK_TRANSITORY; }
std::true           { return TOK_TRUE; }
std::try            { return TOK_TRY; }

// PPTokenLexer already matched the input as an identifier preprocessing token, so this rule just needs to return as soon
// as it is evident that the input is not a keyword.
.             { return TOK_IDENTIFIER; }
