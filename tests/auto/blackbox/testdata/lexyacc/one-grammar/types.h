#include <iostream>
#include <memory>
#include <string>
#include <vector>

struct Tree;
using TreePtr = std::shared_ptr<Tree>;
struct Tree {
    std::string val;
    std::vector<TreePtr> children;
    void print() const {
        std::cout << val << ' ';
        for (const TreePtr &t : children)
            t->print();
    }
};
struct YaccType { TreePtr t; std::string s; };
#define YYSTYPE YaccType
extern TreePtr root;

int yylex();
void yyerror(const char *);
