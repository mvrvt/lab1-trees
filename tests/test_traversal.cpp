#include <cstddef>
#include <sstream> // std::ostringstream - печать в строку
#include <stdexcept>
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
BinarySearchTree<int> MakeSampleTree() {
    BinarySearchTree<int> tree;
    const int values[] = {5, 3, 8, 1, 4, 7, 9};
    for (int value : values)
        tree.Insert(value);
    return tree;
}

// Массив совпадает с ожидаемым поэлементно
bool ArrayEquals(const DynamicArray<int>& actual, const int* expected, std::size_t count) {
    if (actual.GetCount() != count)
        return false;
    for (std::size_t i = 0; i < count; ++i) {
        if (actual[i] != expected[i])
            return false;
    }
    return true;
}

} // namespace

// ---------- Обходы ----------

TEST(Traversal, SixTraversalOrders) {
    BinarySearchTree<int> tree = MakeSampleTree();
    const int nlr[] = {5, 3, 1, 4, 8, 7, 9}; // КЛП
    const int nrl[] = {5, 8, 9, 7, 3, 4, 1}; // КПЛ
    const int lnr[] = {1, 3, 4, 5, 7, 8, 9}; // ЛКП - по возрастанию
    const int lrn[] = {1, 4, 3, 7, 9, 8, 5}; // ЛПК - корень последним
    const int rnl[] = {9, 8, 7, 5, 4, 3, 1}; // ПКЛ - по убыванию
    const int rln[] = {9, 7, 8, 4, 1, 3, 5}; // ПЛК
    EXPECT_TRUE(ArrayEquals(tree.ToArray(TraversalOrder::kNLR), nlr, 7));
    EXPECT_TRUE(ArrayEquals(tree.ToArray(TraversalOrder::kNRL), nrl, 7));
    EXPECT_TRUE(ArrayEquals(tree.ToArray(TraversalOrder::kLNR), lnr, 7));
    EXPECT_TRUE(ArrayEquals(tree.ToArray(TraversalOrder::kLRN), lrn, 7));
    EXPECT_TRUE(ArrayEquals(tree.ToArray(TraversalOrder::kRNL), rnl, 7));
    EXPECT_TRUE(ArrayEquals(tree.ToArray(TraversalOrder::kRLN), rln, 7));
    EXPECT_TRUE(ArrayEquals(tree.ToArray(), lnr, 7)); // по умолчанию - ЛКП
}

TEST(Traversal, TraverseWithFunction) {
    BinarySearchTree<int> tree = MakeSampleTree();
    int sum = 0;
    int visited = 0;
    tree.Traverse(TraversalOrder::kLRN, [&sum, &visited](const int& value) {
        sum += value;
        ++visited;
    });
    EXPECT_EQ(sum, 37);
    EXPECT_EQ(visited, 7);
}

TEST(Traversal, TraverseEmptyTree) {
    BinarySearchTree<int> tree;
    int visited = 0;
    tree.Traverse(TraversalOrder::kNLR, [&visited](const int&) { ++visited; });
    EXPECT_EQ(visited, 0);
    EXPECT_EQ(tree.ToArray().GetCount(), 0);
}

TEST(Traversal, InOrderIsSorted) {
    // В обоих режимах симметричный обход дерева поиска даёт отсортированную последовательность
    const Balancing modes[] = {Balancing::kAvl, Balancing::kNone};
    for (Balancing mode : modes) {
        BinarySearchTree<int> tree(mode);
        for (int value : MakeShuffled(1000))
            tree.Insert(value);
        DynamicArray<int> sorted = tree.ToArray(TraversalOrder::kLNR);
        EXPECT_EQ(sorted.GetCount(), 1000);
        for (std::size_t i = 0; i < sorted.GetCount(); ++i)
            EXPECT_EQ(sorted[i], static_cast<int>(i));
    }
}

TEST(Traversal, DeepTreeTraversal) {
    // Обход "палки" (глубина = числу узлов) работает без рекурсии и даёт все узлы.
    // Больше узлов не берём: строить "палку" долго - каждая вставка проходит её целиком, O(n^2)
    const int n = 5000;
    BinarySearchTree<int> tree(Balancing::kNone);
    for (int i = 0; i < n; ++i)
        tree.Insert(i);
    DynamicArray<int> post_order = tree.ToArray(TraversalOrder::kLRN);
    EXPECT_EQ(post_order.GetCount(), static_cast<std::size_t>(n));
    EXPECT_EQ(post_order[0], n - 1); // в ЛПК первым идёт самый глубокий узел
    EXPECT_EQ(post_order[static_cast<std::size_t>(n - 1)], 0); // последним - корень
}

TEST(Traversal, ParseTraversalOrder) {
    EXPECT_EQ(ParseTraversalOrder("КЛП"), TraversalOrder::kNLR);
    EXPECT_EQ(ParseTraversalOrder("ЛКП"), TraversalOrder::kLNR);
    EXPECT_EQ(ParseTraversalOrder("ПЛК"), TraversalOrder::kRLN);
    EXPECT_EQ(ParseTraversalOrder("LRN"), TraversalOrder::kLRN);
    EXPECT_EQ(ParseTraversalOrder("RNL"), TraversalOrder::kRNL);
    EXPECT_EQ(ToString(TraversalOrder::kNRL), "КПЛ");

    const TraversalOrder all_orders[] = {
        TraversalOrder::kNLR, TraversalOrder::kNRL, TraversalOrder::kLNR,
        TraversalOrder::kLRN, TraversalOrder::kRNL, TraversalOrder::kRLN,
    };
    for (TraversalOrder order : all_orders)
        EXPECT_EQ(ParseTraversalOrder(ToString(order)), order);

    EXPECT_THROW(ParseTraversalOrder(""), std::invalid_argument);
    EXPECT_THROW(ParseTraversalOrder("ККП"), std::invalid_argument);
    EXPECT_THROW(ParseTraversalOrder("nlr"), std::invalid_argument);
    EXPECT_THROW(ParseTraversalOrder("КЛПК"), std::invalid_argument);
}

// ---------- Итератор (IEnumerable) ----------

TEST(Traversal, EnumeratorManual) {
    BinarySearchTree<int> tree = MakeSampleTree();
    IEnumerator<int>* it = tree.GetEnumerator();

    EXPECT_THROW(it->Current(), IndexOutOfRange); // до первого MoveNext элемента нет
    const int expected[] = {1, 3, 4, 5, 7, 8, 9};
    for (int value : expected) {
        EXPECT_TRUE(it->MoveNext());
        EXPECT_EQ(it->Current(), value);
    }
    EXPECT_FALSE(it->MoveNext()); // конец
    EXPECT_FALSE(it->MoveNext()); // и после конца обход не начинается заново
    EXPECT_THROW(it->Current(), IndexOutOfRange);

    it->Reset(); // с начала
    EXPECT_TRUE(it->MoveNext());
    EXPECT_EQ(it->Current(), 1);

    delete it; // GetEnumerator создаёт итератор в куче - удаляем сами
}

TEST(Traversal, EnumeratorEmptyTree) {
    BinarySearchTree<int> tree;
    IEnumerator<int>* it = tree.GetEnumerator();
    EXPECT_FALSE(it->MoveNext());
    delete it;
}

TEST(Traversal, RangeFor) {
    BinarySearchTree<int> tree;
    for (int value : MakeShuffled(500))
        tree.Insert(value);

    int expected = 0;
    for (const int& value : tree) { // итератор создаётся и удаляется сам
        EXPECT_EQ(value, expected);
        ++expected;
    }
    EXPECT_EQ(expected, 500);

    // Обход через базовый тип - так с деревом сможет работать любой код,
    // который умеет обходить IEnumerable
    const IEnumerable<int>& base = tree;
    int count = 0;
    for (const int& value : base) {
        (void)value;
        ++count;
    }
    EXPECT_EQ(count, 500);
}

// ---------- Доступ по индексу (ICollection) ----------

TEST(Traversal, GetByIndex) {
    BinarySearchTree<int> tree = MakeSampleTree();
    const int sorted[] = {1, 3, 4, 5, 7, 8, 9};
    for (std::size_t i = 0; i < 7; ++i)
        EXPECT_EQ(tree.Get(i), sorted[i]);
    EXPECT_THROW(tree.Get(7), IndexOutOfRange);

    BinarySearchTree<int> empty;
    EXPECT_THROW(empty.Get(0), IndexOutOfRange);
}

TEST(Traversal, GetAfterChanges) {
    // Размеры поддеревьев должны оставаться верными после вставок, удалений и поворотов
    BinarySearchTree<int> tree;
    for (int value : MakeShuffled(1000, 3))
        tree.Insert(value);
    for (int value = 0; value < 1000; value += 2)
        tree.Remove(value);
    EXPECT_TRUE(tree.IsValid()); // проверяет и size в каждом узле
    for (std::size_t i = 0; i < tree.GetCount(); ++i)
        EXPECT_EQ(tree.Get(i), static_cast<int>(2 * i + 1)); // остались нечётные
}

TEST(Traversal, IndexOf) {
    BinarySearchTree<int> tree = MakeSampleTree();
    EXPECT_EQ(tree.IndexOf(1), 0);
    EXPECT_EQ(tree.IndexOf(5), 3);
    EXPECT_EQ(tree.IndexOf(9), 6);
    EXPECT_EQ(tree.IndexOf(6), BinarySearchTree<int>::kNotFound);
    for (std::size_t i = 0; i < tree.GetCount(); ++i)
        EXPECT_EQ(tree.IndexOf(tree.Get(i)), i); // IndexOf и Get - обратные друг к другу
}

TEST(Traversal, AsICollection) {
    BinarySearchTree<int> tree = MakeSampleTree();
    const ICollection<int>& collection = tree; // работа через интерфейс
    EXPECT_EQ(collection.GetCount(), 7);
    EXPECT_FALSE(collection.IsEmpty());
    EXPECT_EQ(collection.Get(0), 1);
    EXPECT_EQ(collection.Get(6), 9);
}

// ---------- Печать ----------

TEST(Traversal, PrintStructure) {
    BinarySearchTree<int> tree = MakeSampleTree();
    std::ostringstream out;
    tree.PrintStructure(out);
    const std::string expected =
        "    ┌── 9\n"
        "┌── 8\n"
        "│   └── 7\n"
        "5\n"
        "│   ┌── 4\n"
        "└── 3\n"
        "    └── 1\n";
    EXPECT_EQ(out.str(), expected);

    BinarySearchTree<int> empty;
    std::ostringstream empty_out;
    empty.PrintStructure(empty_out);
    EXPECT_EQ(empty_out.str(), "(empty)\n");
}

// Печать "сверху вниз": узлы раскладываются по столбцам в порядке ЛКП
TEST(Traversal, PrintVertical) {
    BinarySearchTree<int> tree = MakeSampleTree();
    std::ostringstream out;
    EXPECT_TRUE(tree.PrintVertical(out, 100));
    const std::string expected =
        "      5\n"
        "  ┌───┴───┐\n"
        "  3       8\n"
        "┌─┴─┐   ┌─┴─┐\n"
        "1   4   7   9\n";
    EXPECT_EQ(out.str(), expected);

    BinarySearchTree<int> empty;
    std::ostringstream empty_out;
    EXPECT_TRUE(empty.PrintVertical(empty_out, 100));
    EXPECT_EQ(empty_out.str(), "(empty)\n");
}

// Один ребёнок: линия загибается только в его сторону
TEST(Traversal, PrintVerticalOneChild) {
    BinarySearchTree<int> right_only(Balancing::kNone);
    right_only.Insert(1);
    right_only.Insert(2);
    std::ostringstream right_out;
    right_only.PrintVertical(right_out, 100);
    EXPECT_EQ(right_out.str(), "1\n└─┐\n  2\n");

    BinarySearchTree<int> left_only(Balancing::kNone);
    left_only.Insert(2);
    left_only.Insert(1);
    std::ostringstream left_out;
    left_only.PrintVertical(left_out, 100);
    EXPECT_EQ(left_out.str(), "  2\n┌─┘\n1\n");
}

// Ширина считается в символах, а не в байтах: кириллица в UTF-8 - по 2 байта на букву
TEST(Traversal, PrintVerticalUtf8) {
    BinarySearchTree<std::string> tree;
    tree.Insert("кот");
    tree.Insert("арбуз");
    tree.Insert("слон");
    std::ostringstream out;
    EXPECT_TRUE(tree.PrintVertical(out, 100));
    const std::string expected =
        "      кот\n"
        "  ┌────┴───┐\n"
        "арбуз     слон\n";
    EXPECT_EQ(out.str(), expected);
}

// Не помещается в ширину - ничего не печатает и возвращает false
TEST(Traversal, PrintVerticalTooWide) {
    BinarySearchTree<int> tree = MakeSampleTree(); // ширина 13
    std::ostringstream narrow;
    EXPECT_FALSE(tree.PrintVertical(narrow, 12));
    EXPECT_EQ(narrow.str(), "");
    std::ostringstream exact;
    EXPECT_TRUE(tree.PrintVertical(exact, 13));

    // Большое дерево отбрасывается сразу по числу узлов: ширина не меньше 2n - 1
    BinarySearchTree<int> big;
    for (int value = 0; value < 100000; ++value)
        big.Insert(value);
    std::ostringstream big_out;
    EXPECT_FALSE(big.PrintVertical(big_out, 200));
    EXPECT_EQ(big_out.str(), "");
}

// "Палка" без балансировки: по две строки холста на уровень (узлы и линии), минус одна
TEST(Traversal, PrintVerticalDegenerate) {
    BinarySearchTree<int> tree(Balancing::kNone);
    const int n = 200;
    for (int value = 0; value < n; ++value)
        tree.Insert(value);
    std::ostringstream out;
    ASSERT_TRUE(tree.PrintVertical(out, 100000));
    std::string text = out.str();
    std::size_t lines = 0;
    for (char symbol : text)
        lines += symbol == '\n' ? 1 : 0;
    EXPECT_EQ(lines, static_cast<std::size_t>(2 * n - 1));
}
