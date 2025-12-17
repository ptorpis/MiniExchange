#include "foo.hpp"
#include <iostream>
#include <print>
#include <vector>

void foo() {
    std::vector<int> v{1, 2, 3};
    for (auto i : v) {
        std::cout << i << " ";
    }

    std::println();
}
