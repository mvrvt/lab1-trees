#include <iostream>

#include "TestFramework.hpp"

// Группы тестов, каждая - в своём файле
void RunBasicTests();

int main() {
    std::cout << "=== BinarySearchTree: basic operations ===\n";
    RunBasicTests();

    return test::PrintSummary();
}
