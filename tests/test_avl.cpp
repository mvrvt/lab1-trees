#include <cmath> // std::log2
#include <cstddef>

#include "TestFramework.hpp"
#include "TestHelpers.hpp"
#include "tree/BinarySearchTree.hpp"

namespace {

using test::MakeShuffled;
using test::MakeSorted;

// Теоретическая граница высоты AVL-дерева: h < 1.4405 * log2(n + 2) - 0.3277
bool HeightWithinAvlBound(const BinarySearchTree<int>& tree) {
    double n = static_cast<double>(tree.GetCount());
    double bound = 1.4405 * std::log2(n + 2.0) - 0.3277;
    return static_cast<double>(tree.GetHeight()) < bound;
}

BinarySearchTree<int> BuildTree(const DynamicArray<int>& values, Balancing balancing) {
    BinarySearchTree<int> tree(balancing);
    for (int value : values)
        tree.Insert(value);
    return tree;
}

// ---------- Тесты ----------

// Четыре случая дисбаланса на трёх элементах. Без балансировки каждый из этих
// порядков вставки даёт "палку" высоты 3, с AVL - ровное дерево высоты 2.
void TestFourRotationCases() {
    const int orders[4][3] = {
        {3, 2, 1}, // лево-лево   -> один правый поворот
        {1, 2, 3}, // право-право -> один левый поворот
        {3, 1, 2}, // лево-право  -> левый поворот у ребёнка, затем правый
        {1, 3, 2}, // право-лево  -> правый поворот у ребёнка, затем левый
    };
    for (const auto& order : orders) {
        BinarySearchTree<int> avl(Balancing::kAvl);
        BinarySearchTree<int> plain(Balancing::kNone);
        for (int value : order) {
            avl.Insert(value);
            plain.Insert(value);
        }
        CHECK(avl.GetHeight() == 2);
        CHECK(plain.GetHeight() == 3);
        CHECK(avl.IsValid());
        CHECK(plain.IsValid());
        for (int value = 1; value <= 3; ++value)
            CHECK(avl.Contains(value));
    }
}

// Отсортированный вход - худший случай для обычного дерева
void TestSortedInput() {
    const int n = 1023; // 2^10 - 1: AVL на отсортированных данных строит идеальное дерево
    DynamicArray<int> ascending = MakeSorted(n);
    DynamicArray<int> descending;
    for (int i = n - 1; i >= 0; --i)
        descending.Append(i);

    BinarySearchTree<int> avl_up = BuildTree(ascending, Balancing::kAvl);
    BinarySearchTree<int> avl_down = BuildTree(descending, Balancing::kAvl);
    BinarySearchTree<int> plain_up = BuildTree(ascending, Balancing::kNone);

    CHECK(avl_up.GetHeight() == 10);
    CHECK(avl_down.GetHeight() == 10);
    CHECK(plain_up.GetHeight() == static_cast<std::size_t>(n));
    CHECK(avl_up.IsValid());
    CHECK(avl_down.IsValid());
    CHECK(plain_up.IsValid());
}

void TestHeightBoundOnRandomInput() {
    const int sizes[] = {1, 2, 10, 100, 1000, 10000};
    for (int n : sizes) {
        BinarySearchTree<int> tree = BuildTree(MakeShuffled(n), Balancing::kAvl);
        CHECK(tree.GetCount() == static_cast<std::size_t>(n));
        CHECK(HeightWithinAvlBound(tree));
        CHECK(tree.IsValid());
    }
}

// Самая строгая проверка: дерево корректно после КАЖДОЙ операции
void TestValidAfterEveryOperation() {
    const int n = 500;
    BinarySearchTree<int> tree;
    for (int value : MakeShuffled(n, 1)) {
        tree.Insert(value);
        CHECK(tree.IsValid());
    }
    for (int value : MakeShuffled(n, 2)) {
        tree.Remove(value);
        CHECK(tree.IsValid());
        if (!tree.IsEmpty())
            CHECK(HeightWithinAvlBound(tree));
    }
    CHECK(tree.IsEmpty());
}

// Удаление тоже может разбалансировать дерево
void TestRemoveRebalances() {
    const int n = 1023;
    BinarySearchTree<int> tree = BuildTree(MakeSorted(n), Balancing::kAvl);
    for (int value = 0; value < n / 2; ++value) // убираем всю левую половину
        CHECK(tree.Remove(value));
    CHECK(tree.GetCount() == static_cast<std::size_t>(n - n / 2));
    CHECK(tree.IsValid()); // включает проверку баланса каждого узла
    CHECK(HeightWithinAvlBound(tree));
    CHECK(tree.GetMin() == n / 2);
}

// В обычном дереве высоты в узлах тоже должны быть правильными
void TestPlainTreeKeepsHeights() {
    BinarySearchTree<int> tree(Balancing::kNone);
    for (int value : MakeShuffled(300, 7)) {
        tree.Insert(value);
        CHECK(tree.IsValid());
    }
    for (int value : MakeShuffled(300, 8)) {
        if (value % 3 == 0) {
            tree.Remove(value);
            CHECK(tree.IsValid());
        }
    }
    CHECK(tree.GetCount() == 200);
}

// Режим балансировки копируется и переносится вместе с деревом
void TestBalancingModeIsKept() {
    BinarySearchTree<int> plain(Balancing::kNone);
    BinarySearchTree<int> copy(plain);
    CHECK(copy.GetBalancing() == Balancing::kNone);
    for (int i = 0; i < 100; ++i)
        copy.Insert(i);
    CHECK(copy.GetHeight() == 100); // копия тоже не балансируется

    BinarySearchTree<int> avl; // по умолчанию - AVL
    CHECK(avl.GetBalancing() == Balancing::kAvl);
    avl = copy; // присваивание забирает и режим
    CHECK(avl.GetBalancing() == Balancing::kNone);
    CHECK(avl.GetHeight() == 100);

    BinarySearchTree<int> moved(std::move(avl));
    CHECK(moved.GetBalancing() == Balancing::kNone);
}

} // namespace

void RunAvlTests() {
    test::RunTest("four rotation cases", TestFourRotationCases);
    test::RunTest("sorted input", TestSortedInput);
    test::RunTest("height bound on random input", TestHeightBoundOnRandomInput);
    test::RunTest("valid after every operation", TestValidAfterEveryOperation);
    test::RunTest("remove rebalances", TestRemoveRebalances);
    test::RunTest("plain tree keeps heights", TestPlainTreeKeepsHeights);
    test::RunTest("balancing mode is kept", TestBalancingModeIsKept);
}
