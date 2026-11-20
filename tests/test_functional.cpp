#include <cstddef>
#include <string>

#include <gtest/gtest.h>

#include "TestHelpers.hpp"
#include "tree/BinarySearchTree.hpp"

namespace {

using test::MakeShuffled;

// Дерево для тестов (корень 5):
//             5
//          3     8
//        1   4 7   9
BinarySearchTree<int> MakeSampleTree(Balancing balancing = Balancing::kAvl) {
    BinarySearchTree<int> tree(balancing);
    const int values[] = {5, 3, 8, 1, 4, 7, 9};
    for (int value : values)
        tree.Insert(value);
    return tree;
}

template <class T>
bool ArrayEquals(const DynamicArray<T>& actual, const T* expected, std::size_t count) {
    if (actual.GetCount() != count)
        return false;
    for (std::size_t i = 0; i < count; ++i) {
        if (actual[i] != expected[i])
            return false;
    }
    return true;
}

template <class T>
bool ArraysEqual(const DynamicArray<T>& a, const DynamicArray<T>& b) {
    if (a.GetCount() != b.GetCount())
        return false;
    for (std::size_t i = 0; i < a.GetCount(); ++i) {
        if (a[i] != b[i])
            return false;
    }
    return true;
}

// Обычные функции: ТЗ говорит про указатели на функции, шаблон принимает и их
int Triple(const int& x) { return 3 * x; }
bool IsOdd(const int& x) { return x % 2 != 0; }

// Функция из примера в ТЗ: f(x1, x2) = 2*x1 + 3*x2
int TzExample(const int& x1, const int& x2) { return 2 * x1 + 3 * x2; }

} // namespace

// ---------- Map ----------

TEST(Functional, MapDouble) {
    BinarySearchTree<int> tree = MakeSampleTree();
    BinarySearchTree<int> doubled = tree.Map([](const int& x) { return x * 2; });
    const int expected[] = {2, 6, 8, 10, 14, 16, 18};
    EXPECT_TRUE(ArrayEquals(doubled.ToArray(), expected, 7));
    EXPECT_TRUE(doubled.IsValid());
}

// f(x) = -x переворачивает порядок; результат всё равно корректное дерево поиска
TEST(Functional, MapNegateReversesOrder) {
    BinarySearchTree<int> tree = MakeSampleTree();
    BinarySearchTree<int> negated = tree.Map([](const int& x) { return -x; });
    const int expected[] = {-9, -8, -7, -5, -4, -3, -1};
    EXPECT_TRUE(ArrayEquals(negated.ToArray(), expected, 7));
    EXPECT_TRUE(negated.IsValid());
}

// f(x) = x / 10 склеивает значения: в множестве одинаковые хранятся один раз
TEST(Functional, MapMergesEqualResults) {
    BinarySearchTree<int> tree;
    const int values[] = {1, 5, 12, 18, 25, 31, 39};
    for (int value : values)
        tree.Insert(value);
    BinarySearchTree<int> tens = tree.Map([](const int& x) { return x / 10; });
    const int expected[] = {0, 1, 2, 3};
    EXPECT_EQ(tens.GetCount(), 4);
    EXPECT_TRUE(ArrayEquals(tens.ToArray(), expected, 4));
    EXPECT_TRUE(tens.IsValid());
}

TEST(Functional, MapWithFunctionPointer) {
    BinarySearchTree<int> tree = MakeSampleTree();
    BinarySearchTree<int> tripled = tree.Map(Triple);
    const int expected[] = {3, 9, 12, 15, 21, 24, 27};
    EXPECT_TRUE(ArrayEquals(tripled.ToArray(), expected, 7));
}

// Map в дерево другого типа: int -> std::string (порядок строк - словарный)
TEST(Functional, MapToOtherType) {
    BinarySearchTree<int> tree;
    const int values[] = {2, 10, 1};
    for (int value : values)
        tree.Insert(value);
    BinarySearchTree<std::string> strings =
        tree.Map<std::string>([](const int& x) { return std::to_string(x); });
    const std::string expected[] = {"1", "10", "2"}; // "10" < "2" по словарю
    EXPECT_TRUE(ArrayEquals(strings.ToArray(), expected, 3));
}

// Возрастающая f в режиме kNone сохраняет форму (вставка в порядке КЛП)
TEST(Functional, MapIncreasingKeepsShape) {
    BinarySearchTree<int> tree = MakeSampleTree(Balancing::kNone);
    BinarySearchTree<int> shifted = tree.Map([](const int& x) { return x + 100; });
    const int expected_nlr[] = {105, 103, 101, 104, 108, 107, 109};
    EXPECT_TRUE(ArrayEquals(shifted.ToArray(TraversalOrder::kNLR), expected_nlr, 7));
    EXPECT_EQ(shifted.GetHeight(), tree.GetHeight());
}

// Исходное дерево не меняется, режим балансировки переходит в результат
TEST(Functional, MapKeepsSourceAndMode) {
    BinarySearchTree<int> tree = MakeSampleTree(Balancing::kNone);
    DynamicArray<int> before = tree.ToArray(TraversalOrder::kNLR);
    BinarySearchTree<int> result = tree.Map([](const int& x) { return x * x; });
    EXPECT_TRUE(ArraysEqual(tree.ToArray(TraversalOrder::kNLR), before));
    EXPECT_EQ(result.GetBalancing(), Balancing::kNone);
    EXPECT_EQ(MakeSampleTree().Map(Triple).GetBalancing(), Balancing::kAvl);

    BinarySearchTree<int> empty;
    EXPECT_TRUE(empty.Map(Triple).IsEmpty());
}

// Большое дерево: Map(-x) даёт те же элементы с обратным знаком
TEST(Functional, MapLargeTree) {
    BinarySearchTree<int> tree;
    DynamicArray<int> values = MakeShuffled(3000, 99);
    for (int value : values)
        tree.Insert(value);
    BinarySearchTree<int> negated = tree.Map([](const int& x) { return -x; });
    EXPECT_TRUE(negated.IsValid());
    EXPECT_EQ(negated.GetCount(), 3000);
    EXPECT_EQ(negated.GetMin(), -2999);
    EXPECT_EQ(negated.GetMax(), 0);
}

// ---------- Where ----------

TEST(Functional, WhereEven) {
    BinarySearchTree<int> tree = MakeSampleTree();
    BinarySearchTree<int> even = tree.Where([](const int& x) { return x % 2 == 0; });
    const int expected[] = {4, 8};
    EXPECT_TRUE(ArrayEquals(even.ToArray(), expected, 2));
    EXPECT_TRUE(even.IsValid());

    BinarySearchTree<int> odd = tree.Where(IsOdd);
    const int expected_odd[] = {1, 3, 5, 7, 9};
    EXPECT_TRUE(ArrayEquals(odd.ToArray(), expected_odd, 5));
}

TEST(Functional, WhereNothingAndEverything) {
    BinarySearchTree<int> tree = MakeSampleTree(Balancing::kNone);
    BinarySearchTree<int> nothing = tree.Where([](const int&) { return false; });
    EXPECT_TRUE(nothing.IsEmpty());

    // "Всегда true" для kNone повторяет и элементы, и форму
    BinarySearchTree<int> everything = tree.Where([](const int&) { return true; });
    EXPECT_TRUE(ArraysEqual(everything.ToArray(TraversalOrder::kNLR), tree.ToArray(TraversalOrder::kNLR)));
    EXPECT_EQ(everything.GetBalancing(), Balancing::kNone);
}

TEST(Functional, WhereKeepsSource) {
    BinarySearchTree<int> tree = MakeSampleTree();
    BinarySearchTree<int> big = tree.Where([](const int& x) { return x > 4; });
    EXPECT_EQ(big.GetCount(), 4);
    EXPECT_EQ(tree.GetCount(), 7);
    EXPECT_TRUE(tree.Contains(1));
}

// ---------- Reduce ----------

// Пример из ТЗ: reduce(f, [1, 2, 3], 4), f(x1, x2) = 2*x1 + 3*x2.
//   f(1, 4)  = 2*1 + 3*4  = 14
//   f(2, 14) = 2*2 + 3*14 = 46
//   f(3, 46) = 2*3 + 3*46 = 144
// В тексте ТЗ ответ 132: там при раскрытии скобок потеряно слагаемое 2*2
// (написано 3*42 вместо 3*46). По самой формуле правильно 144.
TEST(Functional, ReduceExampleFromTask) {
    BinarySearchTree<int> tree;
    tree.Insert(2);
    tree.Insert(1);
    tree.Insert(3);
    EXPECT_EQ(tree.Reduce(TzExample, 4), 144);
}

TEST(Functional, ReduceSumAndMax) {
    BinarySearchTree<int> tree = MakeSampleTree();
    int sum = tree.Reduce([](const int& x, int accumulated) { return accumulated + x; }, 0);
    EXPECT_EQ(sum, 37);

    int maximum = tree.Reduce([](const int& x, int best) { return x > best ? x : best; }, tree.GetMin());
    EXPECT_EQ(maximum, 9);
}

// T2 выводится из initial: сумма int, которая не влезает в int, считается в long long
TEST(Functional, ReduceToLongLong) {
    BinarySearchTree<int> tree;
    const int big = 2000000000;
    tree.Insert(big);
    tree.Insert(big - 1);
    tree.Insert(big - 2);
    long long sum = tree.Reduce([](const int& x, long long accumulated) { return accumulated + x; }, 0LL);
    EXPECT_EQ(sum, 3LL * big - 3);
}

// Конкатенация показывает порядок, в котором Reduce берёт элементы
TEST(Functional, ReduceOrder) {
    BinarySearchTree<std::string> tree;
    tree.Insert("b");
    tree.Insert("a");
    tree.Insert("c");
    auto append = [](const std::string& x, std::string accumulated) { return accumulated + x; };
    EXPECT_EQ(tree.Reduce(append, std::string()), "abc");                        // ЛКП
    EXPECT_EQ(tree.Reduce(append, std::string(), TraversalOrder::kRNL), "cba");  // ПКЛ
    EXPECT_EQ(tree.Reduce(append, std::string(), TraversalOrder::kNLR), "bac");  // КЛП
}

TEST(Functional, ReduceEmptyTree) {
    BinarySearchTree<int> empty;
    EXPECT_EQ(empty.Reduce(TzExample, 4), 4);
}
