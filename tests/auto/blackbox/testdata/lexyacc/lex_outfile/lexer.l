%option outfile="quark.cpp"
%{
#include <types.h>
void yyerror(const char *e) { std::cerr << e; }
extern "C" int yywrap() { return 1; }
extern YYSTYPE yylval;
%}

ID [a-z]+
AND "&&"
OR "||"
NOT "!"

%%
[[:space:]]+
{ID} yylval.s = yytext; return 1;
{AND} return 2;
{OR} return 3;
{NOT} return 4;

%%
