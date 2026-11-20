#include <cstddef>
#include <utility> // std::move

#include <gtest/gtest.h>

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

} // namespace

// ---------- Тесты ----------

// Для каждого из 6 порядков обход по прошивке совпадает с обычным обходом
TEST(Threading, AllOrdersMatchTraverse) {
    BinarySearchTree<int> tree = MakeSampleTree();
    for (TraversalOrder order : kAllOrders) {
        tree.Thread(order);
        EXPECT_TRUE(tree.IsThreaded());
        EXPECT_EQ(tree.GetThreadOrder(), order);
        EXPECT_TRUE(ArraysEqual(ThreadedToArray(tree), tree.ToArray(order)));
    }
}

// То же на большом случайном AVL-дереве
TEST(Threading, AllOrdersOnRandomTree) {
    BinarySearchTree<int> tree;
    DynamicArray<int> values = MakeShuffled(2000, 777);
    for (int value : values)
        tree.Insert(value);
    for (TraversalOrder order : kAllOrders) {
        tree.Thread(order);
        EXPECT_TRUE(ArraysEqual(ThreadedToArray(tree), tree.ToArray(order)));
    }
}

TEST(Threading, EmptyAndSingleNode) {
    BinarySearchTree<int> empty;
    empty.Thread(TraversalOrder::kLNR);
    EXPECT_TRUE(empty.IsThreaded());
    EXPECT_EQ(ThreadedToArray(empty).GetCount(), 0);

    BinarySearchTree<int> single;
    single.Insert(42);
    single.Thread(TraversalOrder::kLRN);
    DynamicArray<int> result = ThreadedToArray(single);
    EXPECT_EQ(result.GetCount(), 1);
    EXPECT_EQ(result[0], 42);
}

// Без прошивки обход по ней - ошибка
TEST(Threading, NotThreadedThrows) {
    BinarySearchTree<int> tree = MakeSampleTree();
    EXPECT_FALSE(tree.IsThreaded());
    EXPECT_THROW(tree.TraverseThreaded([](const int&) {}), TreeNotThreaded);
    EXPECT_THROW(tree.TraverseThreadedFrom(5, [](const int&) {}), TreeNotThreaded);
    EXPECT_THROW(tree.GetThreadOrder(), TreeNotThreaded);
}

// Каждое изменение дерева сбрасывает прошивку
TEST(Threading, ChangesResetThreading) {
    BinarySearchTree<int> tree = MakeSampleTree();

    tree.Thread(TraversalOrder::kLNR);
    tree.Insert(6);
    EXPECT_FALSE(tree.IsThreaded());
    EXPECT_THROW(tree.TraverseThreaded([](const int&) {}), TreeNotThreaded);

    tree.Thread(TraversalOrder::kLNR);
    tree.Remove(3);
    EXPECT_FALSE(tree.IsThreaded());
    EXPECT_THROW(tree.TraverseThreaded([](const int&) {}), TreeNotThreaded);

    tree.Thread(TraversalOrder::kLNR);
    tree.Clear();
    EXPECT_FALSE(tree.IsThreaded());

    // После перепрошивки всё снова работает
    tree.Insert(2);
    tree.Insert(1);
    tree.Thread(TraversalOrder::kLNR);
    EXPECT_TRUE(ArraysEqual(ThreadedToArray(tree), tree.ToArray()));
}

// Если дерево на самом деле не изменилось, прошивка остаётся верной
TEST(Threading, NoChangeKeepsThreading) {
    BinarySearchTree<int> tree = MakeSampleTree();
    tree.Thread(TraversalOrder::kNLR);
    EXPECT_FALSE(tree.Insert(5));  // дубликат
    EXPECT_FALSE(tree.Remove(100)); // такого нет
    EXPECT_TRUE(tree.IsThreaded());
    EXPECT_TRUE(ArraysEqual(ThreadedToArray(tree), tree.ToArray(TraversalOrder::kNLR)));
}

TEST(Threading, RethreadInOtherOrder) {
    BinarySearchTree<int> tree = MakeSampleTree();
    tree.Thread(TraversalOrder::kLNR);
    tree.Thread(TraversalOrder::kRLN);
    EXPECT_EQ(tree.GetThreadOrder(), TraversalOrder::kRLN);
    EXPECT_TRUE(ArraysEqual(ThreadedToArray(tree), tree.ToArray(TraversalOrder::kRLN)));
}

// Обход с середины цепочки
TEST(Threading, TraverseFromElement) {
    BinarySearchTree<int> tree = MakeSampleTree();
    tree.Thread(TraversalOrder::kLNR);
    DynamicArray<int> result;
    tree.TraverseThreadedFrom(4, [&result](const int& value) { result.Append(value); });
    const int expected[] = {4, 5, 7, 8, 9};
    EXPECT_EQ(result.GetCount(), 5);
    for (std::size_t i = 0; i < result.GetCount() && i < 5; ++i)
        EXPECT_EQ(result[i], expected[i]);

    EXPECT_THROW(tree.TraverseThreadedFrom(6, [](const int&) {}), ElementNotFound);
}

// Копия прошитого дерева прошита в том же порядке, но своими узлами
TEST(Threading, CopyOfThreadedTree) {
    BinarySearchTree<int> original = MakeSampleTree();
    original.Thread(TraversalOrder::kLRN);

    BinarySearchTree<int> copy(original);
    EXPECT_TRUE(copy.IsThreaded());
    EXPECT_EQ(copy.GetThreadOrder(), TraversalOrder::kLRN);
    EXPECT_TRUE(ArraysEqual(ThreadedToArray(copy), original.ToArray(TraversalOrder::kLRN)));

    // Изменение копии не трогает оригинал: его прошивка остаётся рабочей
    copy.Remove(5);
    copy.Clear();
    EXPECT_TRUE(original.IsThreaded());
    EXPECT_TRUE(ArraysEqual(ThreadedToArray(original), original.ToArray(TraversalOrder::kLRN)));

    // Присваивание - тоже копия
    BinarySearchTree<int> assigned;
    assigned = original;
    EXPECT_TRUE(assigned.IsThreaded());
    EXPECT_TRUE(ArraysEqual(ThreadedToArray(assigned), original.ToArray(TraversalOrder::kLRN)));
}

// При перемещении прошивка переезжает вместе с узлами
TEST(Threading, MoveOfThreadedTree) {
    BinarySearchTree<int> source = MakeSampleTree();
    source.Thread(TraversalOrder::kNRL);
    DynamicArray<int> expected = source.ToArray(TraversalOrder::kNRL);

    BinarySearchTree<int> moved(std::move(source));
    EXPECT_TRUE(moved.IsThreaded());
    EXPECT_TRUE(ArraysEqual(ThreadedToArray(moved), expected));

    BinarySearchTree<int> target;
    target = std::move(moved);
    EXPECT_TRUE(target.IsThreaded());
    EXPECT_TRUE(ArraysEqual(ThreadedToArray(target), expected));
}

// Вырожденное дерево ("палка"): прошивка и обход по ней без рекурсии
TEST(Threading, ThreadDeepTree) {
    const int n = 5000;
    BinarySearchTree<int> tree(Balancing::kNone);
    DynamicArray<int> values = MakeSorted(n);
    for (int value : values)
        tree.Insert(value);
    EXPECT_EQ(tree.GetHeight(), static_cast<std::size_t>(n));

    tree.Thread(TraversalOrder::kLRN);
    std::size_t count = 0;
    int last = -1;
    tree.TraverseThreaded([&count, &last](const int& value) {
        ++count;
        last = value;
    });
    EXPECT_EQ(count, static_cast<std::size_t>(n));
    EXPECT_EQ(last, 0); // ЛПК: корень (0) - последним
}
