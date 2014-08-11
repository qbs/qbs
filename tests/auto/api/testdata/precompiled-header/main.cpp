#include "myobject.h"
#include <algorithm>
#include <iostream>
#include <list>

using namespace std;

int main()
{
    MyObject obj;
    list<int> lst;
    lst.push_back(1);
    lst.push_back(2);
    lst.push_back(3);
    lst.push_back(4);
    lst.push_back(5);
    lst.push_back(6);
    lst.push_back(7);
    lst.push_back(8);
    lst.push_back(9);
    reverse(lst.begin(), lst.end());
    for (list<int>::iterator it=lst.begin(); it != lst.end(); ++it)
        cout << *it << ", ";
    cout << endl;
    return 0;
}

