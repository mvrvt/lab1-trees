#include <iostream>

#include "TestFramework.hpp"

// Группы тестов, каждая - в своём файле
void RunBasicTests();
void RunAvlTests();
void RunTraversalTests();
void RunThreadingTests();
void RunFunctionalTests();

int main() {
    std::cout << "=== BinarySearchTree: basic operations ===\n";
    RunBasicTests();

    std::cout << "\n=== BinarySearchTree: AVL balancing ===\n";
    RunAvlTests();

    std::cout << "\n=== BinarySearchTree: traversals, iterator, index ===\n";
    RunTraversalTests();

    std::cout << "\n=== BinarySearchTree: threading ===\n";
    RunThreadingTests();

    std::cout << "\n=== BinarySearchTree: map, where, reduce ===\n";
    RunFunctionalTests();

    return test::PrintSummary();
}
