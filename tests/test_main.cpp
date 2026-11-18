#include <iostream>

#include "TestFramework.hpp"

// Группы тестов, каждая - в своём файле
void RunBasicTests();
void RunAvlTests();

int main() {
    std::cout << "=== BinarySearchTree: basic operations ===\n";
    RunBasicTests();

    std::cout << "\n=== BinarySearchTree: AVL balancing ===\n";
    RunAvlTests();

    return test::PrintSummary();
}