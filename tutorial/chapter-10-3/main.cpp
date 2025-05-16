//![0]
// main.cpp

import std;

int main()
{
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    std::ranges::for_each(numbers, [](int n) { std::cout << n << ' '; });
    std::cout << std::endl;
    return 0;
}
//![0]
