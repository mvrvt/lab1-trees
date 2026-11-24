#pragma once

#include <iostream>

#include "tree/BinarySearchTree.hpp"

// ========== Короткая демонстрация без ввода ==========
// Показывает главное за один экран: AVL против обычного дерева на отсортированных
// данных, шесть обходов, Get(i) / IndexOf, удаление корня.

namespace ui {

inline void PrintTreeInfo(const char* title, const BinarySearchTree<int>& tree) {
    std::cout << title << " (элементов: " << tree.GetCount() << ", высота: " << tree.GetHeight() << "):\n";
    tree.PrintStructure(std::cout);
    std::cout << "\n";
}

inline void RunDemo() {
    // 1. Одни и те же отсортированные данные - в обычное дерево и в AVL
    BinarySearchTree<int> plain(Balancing::kNone);
    BinarySearchTree<int> avl(Balancing::kAvl);
    for (int i = 1; i <= 7; ++i) {
        plain.Insert(i);
        avl.Insert(i);
    }
    std::cout << "=== Вставка 1..7 по возрастанию ===\n\n";
    PrintTreeInfo("Без балансировки", plain);
    PrintTreeInfo("AVL", avl);

    // 2. Шесть порядков обхода одного дерева
    BinarySearchTree<int> tree;
    const int values[] = {5, 3, 8, 1, 4, 7, 9};
    for (int value : values)
        tree.Insert(value);
    PrintTreeInfo("=== Пример дерева ===", tree);

    const TraversalOrder orders[] = {
        TraversalOrder::kNLR, TraversalOrder::kNRL, TraversalOrder::kLNR,
        TraversalOrder::kLRN, TraversalOrder::kRNL, TraversalOrder::kRLN,
    };
    for (TraversalOrder order : orders) {
        std::cout << ToString(order) << ":";
        tree.Traverse(order, [](const int& x) { std::cout << " " << x; });
        std::cout << "\n";
    }

    // 3. Цикл for, доступ по индексу, строка
    std::cout << "\nfor (x : tree):";
    for (const int& x : tree)
        std::cout << " " << x;
    std::cout << "\nGet(0) = " << tree.Get(0) << ", Get(3) = " << tree.Get(3)
              << ", IndexOf(8) = " << tree.IndexOf(8) << "\n";
    std::cout << "Строка {К}(Л)[П]: " << tree.ToFormattedString() << "\n";

    // 4. Удаление корня: на его место встаёт преемник (7)
    tree.Remove(5);
    std::cout << "\n";
    PrintTreeInfo("=== После Remove(5) ===", tree);
}

} // namespace ui
