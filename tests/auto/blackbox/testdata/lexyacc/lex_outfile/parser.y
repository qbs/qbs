%{
#include <types.h>
%}

%type <t> expr
%left OR
%left AND
%nonassoc NOT
%token <s> ID

%%

start: expr { root = $1; }
expr: expr AND expr { auto t = std::make_shared<Tree>(); t->val = "AND"; t->children = { $1, $3 }; $$ = t; }
    | expr OR expr { auto t = std::make_shared<Tree>(); t->val = "OR"; t->children = { $1, $3 }; $$ = t; }
    | NOT expr { auto t = std::make_shared<Tree>(); t->val = "NOT"; t->children = { $2 }; $$ = t; }
    | ID { auto t = std::make_shared<Tree>(); t->val = $1; $$ = t; }

%%

TreePtr root;

int main()
{
    yyparse();
    if (!root)
        return 1;
    root->print();
    std::cout << std::endl;
}
