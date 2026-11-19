#include <cstddef>
#include <utility> // std::move

#include "TestFramework.hpp"
#include "TestHelpers.hpp"
#include "tree/BinarySearchTree.hpp"

namespace {

using test::MakeShuffled;
using test::MakeSorted;

const TraversalOrder kAllOrders[] = {
    TraversalOrder::kNLR, TraversalOrder::kNRL, TraversalOrder::kLNR,
    TraversalOrder::kLRN, TraversalOrder::kRNL, TraversalOrder::kRLN,
};

// Дерево для тестов (корень 5):
//             5
//          3     8
//        1   4 7   9
BinarySearchTree<int> MakeSampleTree() {
    BinarySearchTree<int> tree;
    const int values[] = {5, 3, 8, 1, 4, 7, 9};
    for (int value : values)
        tree.Insert(value);
    return tree;
}

// Значения в порядке прошивки
DynamicArray<int> ThreadedToArray(const BinarySearchTree<int>& tree) {
    DynamicArray<int> result;
    tree.TraverseThreaded([&result](const int& value) { result.Append(value); });
    return result;
}

bool ArraysEqual(const DynamicArray<int>& a, const DynamicArray<int>& b) {
    if (a.GetCount() != b.GetCount())
        return false;
    for (std::size_t i = 0; i < a.GetCount(); ++i) {
        if (a[i] != b[i])
            return false;
    }
    return true;
}

// ---------- Тесты ----------

// Для каждого из 6 порядков обход по прошивке совпадает с обычным обходом
void TestAllOrdersMatchTraverse() {
    BinarySearchTree<int> tree = MakeSampleTree();
    for (TraversalOrder order : kAllOrders) {
        tree.Thread(order);
        CHECK(tree.IsThreaded());
        CHECK(tree.GetThreadOrder() == order);
        CHECK(ArraysEqual(ThreadedToArray(tree), tree.ToArray(order)));
    }
}

// То же на большом случайном AVL-дереве
void TestAllOrdersOnRandomTree() {
    BinarySearchTree<int> tree;
    DynamicArray<int> values = MakeShuffled(2000, 777);
    for (int value : values)
        tree.Insert(value);
    for (TraversalOrder order : kAllOrders) {
        tree.Thread(order);
        CHECK(ArraysEqual(ThreadedToArray(tree), tree.ToArray(order)));
    }
}

void TestEmptyAndSingleNode() {
    BinarySearchTree<int> empty;
    empty.Thread(TraversalOrder::kLNR);
    CHECK(empty.IsThreaded());
    CHECK(ThreadedToArray(empty).GetCount() == 0);

    BinarySearchTree<int> single;
    single.Insert(42);
    single.Thread(TraversalOrder::kLRN);
    DynamicArray<int> result = ThreadedToArray(single);
    CHECK(result.GetCount() == 1);
    CHECK(result[0] == 42);
}

// Без прошивки обход по ней - ошибка
void TestNotThreadedThrows() {
    BinarySearchTree<int> tree = MakeSampleTree();
    CHECK(!tree.IsThreaded());
    CHECK_THROWS(tree.TraverseThreaded([](const int&) {}), TreeNotThreaded);
    CHECK_THROWS(tree.TraverseThreadedFrom(5, [](const int&) {}), TreeNotThreaded);
    CHECK_THROWS(tree.GetThreadOrder(), TreeNotThreaded);
}

// Каждое изменение дерева сбрасывает прошивку
void TestChangesResetThreading() {
    BinarySearchTree<int> tree = MakeSampleTree();

    tree.Thread(TraversalOrder::kLNR);
    tree.Insert(6);
    CHECK(!tree.IsThreaded());
    CHECK_THROWS(tree.TraverseThreaded([](const int&) {}), TreeNotThreaded);

    tree.Thread(TraversalOrder::kLNR);
    tree.Remove(3);
    CHECK(!tree.IsThreaded());
    CHECK_THROWS(tree.TraverseThreaded([](const int&) {}), TreeNotThreaded);

    tree.Thread(TraversalOrder::kLNR);
    tree.Clear();
    CHECK(!tree.IsThreaded());

    // После перепрошивки всё снова работает
    tree.Insert(2);
    tree.Insert(1);
    tree.Thread(TraversalOrder::kLNR);
    CHECK(ArraysEqual(ThreadedToArray(tree), tree.ToArray()));
}

// Если дерево на самом деле не изменилось, прошивка остаётся верной
void TestNoChangeKeepsThreading() {
    BinarySearchTree<int> tree = MakeSampleTree();
    tree.Thread(TraversalOrder::kNLR);
    CHECK(!tree.Insert(5));  // дубликат
    CHECK(!tree.Remove(100)); // такого нет
    CHECK(tree.IsThreaded());
    CHECK(ArraysEqual(ThreadedToArray(tree), tree.ToArray(TraversalOrder::kNLR)));
}

void TestRethreadInOtherOrder() {
    BinarySearchTree<int> tree = MakeSampleTree();
    tree.Thread(TraversalOrder::kLNR);
    tree.Thread(TraversalOrder::kRLN);
    CHECK(tree.GetThreadOrder() == TraversalOrder::kRLN);
    CHECK(ArraysEqual(ThreadedToArray(tree), tree.ToArray(TraversalOrder::kRLN)));
}

// Обход с середины цепочки
void TestTraverseFromElement() {
    BinarySearchTree<int> tree = MakeSampleTree();
    tree.Thread(TraversalOrder::kLNR);
    DynamicArray<int> result;
    tree.TraverseThreadedFrom(4, [&result](const int& value) { result.Append(value); });
    const int expected[] = {4, 5, 7, 8, 9};
    CHECK(result.GetCount() == 5);
    for (std::size_t i = 0; i < result.GetCount() && i < 5; ++i)
        CHECK(result[i] == expected[i]);

    CHECK_THROWS(tree.TraverseThreadedFrom(6, [](const int&) {}), ElementNotFound);
}

// Копия прошитого дерева прошита в том же порядке, но своими узлами
void TestCopyOfThreadedTree() {
    BinarySearchTree<int> original = MakeSampleTree();
    original.Thread(TraversalOrder::kLRN);

    BinarySearchTree<int> copy(original);
    CHECK(copy.IsThreaded());
    CHECK(copy.GetThreadOrder() == TraversalOrder::kLRN);
    CHECK(ArraysEqual(ThreadedToArray(copy), original.ToArray(TraversalOrder::kLRN)));

    // Изменение копии не трогает оригинал: его прошивка остаётся рабочей
    copy.Remove(5);
    copy.Clear();
    CHECK(original.IsThreaded());
    CHECK(ArraysEqual(ThreadedToArray(original), original.ToArray(TraversalOrder::kLRN)));

    // Присваивание - тоже копия
    BinarySearchTree<int> assigned;
    assigned = original;
    CHECK(assigned.IsThreaded());
    CHECK(ArraysEqual(ThreadedToArray(assigned), original.ToArray(TraversalOrder::kLRN)));
}

// При перемещении прошивка переезжает вместе с узлами
void TestMoveOfThreadedTree() {
    BinarySearchTree<int> source = MakeSampleTree();
    source.Thread(TraversalOrder::kNRL);
    DynamicArray<int> expected = source.ToArray(TraversalOrder::kNRL);

    BinarySearchTree<int> moved(std::move(source));
    CHECK(moved.IsThreaded());
    CHECK(ArraysEqual(ThreadedToArray(moved), expected));

    BinarySearchTree<int> target;
    target = std::move(moved);
    CHECK(target.IsThreaded());
    CHECK(ArraysEqual(ThreadedToArray(target), expected));
}

// Вырожденное дерево ("палка"): прошивка и обход по ней без рекурсии
void TestThreadDeepTree() {
    const int n = 5000;
    BinarySearchTree<int> tree(Balancing::kNone);
    DynamicArray<int> values = MakeSorted(n);
    for (int value : values)
        tree.Insert(value);
    CHECK(tree.GetHeight() == static_cast<std::size_t>(n));

    tree.Thread(TraversalOrder::kLRN);
    std::size_t count = 0;
    int last = -1;
    tree.TraverseThreaded([&count, &last](const int& value) {
        ++count;
        last = value;
    });
    CHECK(count == static_cast<std::size_t>(n));
    CHECK(last == 0); // ЛПК: корень (0) - последним
}

} // namespace

void RunThreadingTests() {
    test::RunTest("all orders match traverse", TestAllOrdersMatchTraverse);
    test::RunTest("all orders on random tree", TestAllOrdersOnRandomTree);
    test::RunTest("threading empty and single node", TestEmptyAndSingleNode);
    test::RunTest("not threaded throws", TestNotThreadedThrows);
    test::RunTest("changes reset threading", TestChangesResetThreading);
    test::RunTest("no change keeps threading", TestNoChangeKeepsThreading);
    test::RunTest("rethread in other order", TestRethreadInOtherOrder);
    test::RunTest("traverse from element", TestTraverseFromElement);
    test::RunTest("copy of threaded tree", TestCopyOfThreadedTree);
    test::RunTest("move of threaded tree", TestMoveOfThreadedTree);
    test::RunTest("thread deep tree", TestThreadDeepTree);
}
