// Пока это демонстрация того, что уже умеет дерево.
// Позже на этом месте будет полноценный консольный интерфейс.

#include <iostream>

#include "tree/BinarySearchTree.hpp"

namespace {

void PrintArray(const DynamicArray<int>& values) {
    for (std::size_t i = 0; i < values.GetCount(); ++i)
        std::cout << (i == 0 ? "" : " ") << values[i];
    std::cout << "\n";
}

void PrintTreeInfo(const char* title, const BinarySearchTree<int>& tree) {
    std::cout << title << " (count = " << tree.GetCount() << ", height = " << tree.GetHeight() << "):\n";
    tree.PrintStructure(std::cout);
    std::cout << "\n";
}

} // namespace

int main() {
    // 1. Одни и те же отсортированные данные - в обычное дерево и в AVL
    BinarySearchTree<int> plain(Balancing::kNone);
    BinarySearchTree<int> avl(Balancing::kAvl);
    for (int i = 1; i <= 7; ++i) {
        plain.Insert(i);
        avl.Insert(i);
    }
    std::cout << "=== Insert 1..7 in sorted order ===\n\n";
    PrintTreeInfo("Without balancing", plain);
    PrintTreeInfo("AVL", avl);

    // 2. Шесть порядков обхода одного дерева
    BinarySearchTree<int> tree;
    const int values[] = {5, 3, 8, 1, 4, 7, 9};
    for (int value : values)
        tree.Insert(value);
    PrintTreeInfo("=== Sample tree ===", tree);

    const TraversalOrder orders[] = {
        TraversalOrder::kNLR, TraversalOrder::kNRL, TraversalOrder::kLNR,
        TraversalOrder::kLRN, TraversalOrder::kRNL, TraversalOrder::kRLN,
    };
    for (TraversalOrder order : orders) {
        std::cout << ToString(order) << ": ";
        PrintArray(tree.ToArray(order));
    }

    // 3. Цикл for и доступ по индексу
    std::cout << "\nfor (x : tree):";
    for (const int& x : tree)
        std::cout << " " << x;
    std::cout << "\nGet(0) = " << tree.Get(0) << ", Get(3) = " << tree.Get(3)
              << ", IndexOf(8) = " << tree.IndexOf(8) << "\n";

    // 4. Удаление корня: на его место встаёт преемник (7)
    tree.Remove(5);
    std::cout << "\n";
    PrintTreeInfo("=== After Remove(5) ===", tree);
    return 0;
}
