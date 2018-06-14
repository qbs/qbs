%option noyywrap

%{
#include <g1.tab.h>
#ifdef _MSC_BUILD
#pragma message("whatever")
#else
#pragma whatever
#endif
void g1error(const char *e) { }
%}

%%

%%
