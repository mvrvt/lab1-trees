#include <iostream>

#include "TestFramework.hpp"

// Группы тестов, каждая - в своём файле
void RunBasicTests();
void RunAvlTests();
void RunTraversalTests();
void RunThreadingTests();

int main() {
    std::cout << "=== BinarySearchTree: basic operations ===\n";
    RunBasicTests();

    std::cout << "\n=== BinarySearchTree: AVL balancing ===\n";
    RunAvlTests();

    std::cout << "\n=== BinarySearchTree: traversals, iterator, index ===\n";
    RunTraversalTests();

    std::cout << "\n=== BinarySearchTree: threading ===\n";
    RunThreadingTests();

    return test::PrintSummary();
}
