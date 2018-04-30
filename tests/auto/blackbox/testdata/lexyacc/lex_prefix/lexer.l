%option prefix="bla"
%{
#include <types.h>
void blaerror(const char *e) { std::cerr << e; }
extern "C" int blawrap() { return 1; }
extern BLASTYPE blalval;
%}

ID [a-z]+
AND "&&"
OR "||"
NOT "!"

%%
[[:space:]]+
{ID} blalval.s = blatext; return 1;
{AND} return 2;
{OR} return 3;
{NOT} return 4;

%%
