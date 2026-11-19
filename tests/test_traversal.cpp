#include <cstddef>
#include <sstream> // std::ostringstream - печать в строку
#include <stdexcept>
#include <string>

#include "TestFramework.hpp"
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

// ---------- Обходы ----------

void TestSixTraversalOrders() {
    BinarySearchTree<int> tree = MakeSampleTree();
    const int nlr[] = {5, 3, 1, 4, 8, 7, 9}; // КЛП
    const int nrl[] = {5, 8, 9, 7, 3, 4, 1}; // КПЛ
    const int lnr[] = {1, 3, 4, 5, 7, 8, 9}; // ЛКП - по возрастанию
    const int lrn[] = {1, 4, 3, 7, 9, 8, 5}; // ЛПК - корень последним
    const int rnl[] = {9, 8, 7, 5, 4, 3, 1}; // ПКЛ - по убыванию
    const int rln[] = {9, 7, 8, 4, 1, 3, 5}; // ПЛК
    CHECK(ArrayEquals(tree.ToArray(TraversalOrder::kNLR), nlr, 7));
    CHECK(ArrayEquals(tree.ToArray(TraversalOrder::kNRL), nrl, 7));
    CHECK(ArrayEquals(tree.ToArray(TraversalOrder::kLNR), lnr, 7));
    CHECK(ArrayEquals(tree.ToArray(TraversalOrder::kLRN), lrn, 7));
    CHECK(ArrayEquals(tree.ToArray(TraversalOrder::kRNL), rnl, 7));
    CHECK(ArrayEquals(tree.ToArray(TraversalOrder::kRLN), rln, 7));
    CHECK(ArrayEquals(tree.ToArray(), lnr, 7)); // по умолчанию - ЛКП
}

void TestTraverseWithFunction() {
    BinarySearchTree<int> tree = MakeSampleTree();
    int sum = 0;
    int visited = 0;
    tree.Traverse(TraversalOrder::kLRN, [&sum, &visited](const int& value) {
        sum += value;
        ++visited;
    });
    CHECK(sum == 37);
    CHECK(visited == 7);
}

void TestTraverseEmptyTree() {
    BinarySearchTree<int> tree;
    int visited = 0;
    tree.Traverse(TraversalOrder::kNLR, [&visited](const int&) { ++visited; });
    CHECK(visited == 0);
    CHECK(tree.ToArray().GetCount() == 0);
}

void TestInOrderIsSorted() {
    // В обоих режимах симметричный обход дерева поиска даёт отсортированную последовательность
    const Balancing modes[] = {Balancing::kAvl, Balancing::kNone};
    for (Balancing mode : modes) {
        BinarySearchTree<int> tree(mode);
        for (int value : MakeShuffled(1000))
            tree.Insert(value);
        DynamicArray<int> sorted = tree.ToArray(TraversalOrder::kLNR);
        CHECK(sorted.GetCount() == 1000);
        for (std::size_t i = 0; i < sorted.GetCount(); ++i)
            CHECK(sorted[i] == static_cast<int>(i));
    }
}

void TestDeepTreeTraversal() {
    // Обход "палки" (глубина = числу узлов) работает без рекурсии и даёт все узлы.
    // Больше узлов не берём: строить "палку" долго - каждая вставка проходит её целиком, O(n^2)
    const int n = 5000;
    BinarySearchTree<int> tree(Balancing::kNone);
    for (int i = 0; i < n; ++i)
        tree.Insert(i);
    DynamicArray<int> post_order = tree.ToArray(TraversalOrder::kLRN);
    CHECK(post_order.GetCount() == static_cast<std::size_t>(n));
    CHECK(post_order[0] == n - 1); // в ЛПК первым идёт самый глубокий узел
    CHECK(post_order[static_cast<std::size_t>(n - 1)] == 0); // последним - корень
}

void TestParseTraversalOrder() {
    CHECK(ParseTraversalOrder("КЛП") == TraversalOrder::kNLR);
    CHECK(ParseTraversalOrder("ЛКП") == TraversalOrder::kLNR);
    CHECK(ParseTraversalOrder("ПЛК") == TraversalOrder::kRLN);
    CHECK(ParseTraversalOrder("LRN") == TraversalOrder::kLRN);
    CHECK(ParseTraversalOrder("RNL") == TraversalOrder::kRNL);
    CHECK(ToString(TraversalOrder::kNRL) == "КПЛ");

    const TraversalOrder all_orders[] = {
        TraversalOrder::kNLR, TraversalOrder::kNRL, TraversalOrder::kLNR,
        TraversalOrder::kLRN, TraversalOrder::kRNL, TraversalOrder::kRLN,
    };
    for (TraversalOrder order : all_orders)
        CHECK(ParseTraversalOrder(ToString(order)) == order);

    CHECK_THROWS(ParseTraversalOrder(""), std::invalid_argument);
    CHECK_THROWS(ParseTraversalOrder("ККП"), std::invalid_argument);
    CHECK_THROWS(ParseTraversalOrder("nlr"), std::invalid_argument);
    CHECK_THROWS(ParseTraversalOrder("КЛПК"), std::invalid_argument);
}

// ---------- Итератор (IEnumerable) ----------

void TestEnumeratorManual() {
    BinarySearchTree<int> tree = MakeSampleTree();
    IEnumerator<int>* it = tree.GetEnumerator();

    CHECK_THROWS(it->Current(), IndexOutOfRange); // до первого MoveNext элемента нет
    const int expected[] = {1, 3, 4, 5, 7, 8, 9};
    for (int value : expected) {
        CHECK(it->MoveNext());
        CHECK(it->Current() == value);
    }
    CHECK(!it->MoveNext()); // конец
    CHECK(!it->MoveNext()); // и после конца обход не начинается заново
    CHECK_THROWS(it->Current(), IndexOutOfRange);

    it->Reset(); // с начала
    CHECK(it->MoveNext());
    CHECK(it->Current() == 1);

    delete it; // GetEnumerator создаёт итератор в куче - удаляем сами
}

void TestEnumeratorEmptyTree() {
    BinarySearchTree<int> tree;
    IEnumerator<int>* it = tree.GetEnumerator();
    CHECK(!it->MoveNext());
    delete it;
}

void TestRangeFor() {
    BinarySearchTree<int> tree;
    for (int value : MakeShuffled(500))
        tree.Insert(value);

    int expected = 0;
    for (const int& value : tree) { // итератор создаётся и удаляется сам
        CHECK(value == expected);
        ++expected;
    }
    CHECK(expected == 500);

    // Обход через базовый тип - так с деревом сможет работать любой код,
    // который умеет обходить IEnumerable
    const IEnumerable<int>& base = tree;
    int count = 0;
    for (const int& value : base) {
        (void)value;
        ++count;
    }
    CHECK(count == 500);
}

// ---------- Доступ по индексу (ICollection) ----------

void TestGetByIndex() {
    BinarySearchTree<int> tree = MakeSampleTree();
    const int sorted[] = {1, 3, 4, 5, 7, 8, 9};
    for (std::size_t i = 0; i < 7; ++i)
        CHECK(tree.Get(i) == sorted[i]);
    CHECK_THROWS(tree.Get(7), IndexOutOfRange);

    BinarySearchTree<int> empty;
    CHECK_THROWS(empty.Get(0), IndexOutOfRange);
}

void TestGetAfterChanges() {
    // Размеры поддеревьев должны оставаться верными после вставок, удалений и поворотов
    BinarySearchTree<int> tree;
    for (int value : MakeShuffled(1000, 3))
        tree.Insert(value);
    for (int value = 0; value < 1000; value += 2)
        tree.Remove(value);
    CHECK(tree.IsValid()); // проверяет и size в каждом узле
    for (std::size_t i = 0; i < tree.GetCount(); ++i)
        CHECK(tree.Get(i) == static_cast<int>(2 * i + 1)); // остались нечётные
}

void TestIndexOf() {
    BinarySearchTree<int> tree = MakeSampleTree();
    CHECK(tree.IndexOf(1) == 0);
    CHECK(tree.IndexOf(5) == 3);
    CHECK(tree.IndexOf(9) == 6);
    CHECK(tree.IndexOf(6) == BinarySearchTree<int>::kNotFound);
    for (std::size_t i = 0; i < tree.GetCount(); ++i)
        CHECK(tree.IndexOf(tree.Get(i)) == i); // IndexOf и Get - обратные друг к другу
}

void TestAsICollection() {
    BinarySearchTree<int> tree = MakeSampleTree();
    const ICollection<int>& collection = tree; // работа через интерфейс
    CHECK(collection.GetCount() == 7);
    CHECK(!collection.IsEmpty());
    CHECK(collection.Get(0) == 1);
    CHECK(collection.Get(6) == 9);
}

// ---------- Печать ----------

void TestPrintStructure() {
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
    CHECK(out.str() == expected);

    BinarySearchTree<int> empty;
    std::ostringstream empty_out;
    empty.PrintStructure(empty_out);
    CHECK(empty_out.str() == "(empty)\n");
}

} // namespace

void RunTraversalTests() {
    test::RunTest("six traversal orders", TestSixTraversalOrders);
    test::RunTest("traverse with function", TestTraverseWithFunction);
    test::RunTest("traverse empty tree", TestTraverseEmptyTree);
    test::RunTest("in-order is sorted", TestInOrderIsSorted);
    test::RunTest("deep tree traversal", TestDeepTreeTraversal);
    test::RunTest("parse traversal order", TestParseTraversalOrder);
    test::RunTest("enumerator manual", TestEnumeratorManual);
    test::RunTest("enumerator empty tree", TestEnumeratorEmptyTree);
    test::RunTest("range for", TestRangeFor);
    test::RunTest("get by index", TestGetByIndex);
    test::RunTest("get after changes", TestGetAfterChanges);
    test::RunTest("index of", TestIndexOf);
    test::RunTest("as ICollection", TestAsICollection);
    test::RunTest("print structure", TestPrintStructure);
}
