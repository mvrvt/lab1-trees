#include <cstddef>
#include <sstream> // std::ostringstream
#include <string>

#include "TestFramework.hpp"
#include "TestHelpers.hpp"
#include "tree/BinarySearchTree.hpp"

namespace {

using test::MakeShuffled;
using test::MakeSorted;

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

// Образец для поиска поддерева - читаем без балансировки, чтобы форма была ровно как в строке
BinarySearchTree<int> Pattern(const std::string& text) {
    return BinarySearchTree<int>::FromString(text, Balancing::kNone);
}

template <class T>
std::string Print(const T& value) {
    std::ostringstream out;
    out << value;
    return out.str();
}

// ---------- Извлечение поддерева ----------

void TestExtractInnerNode() {
    BinarySearchTree<int> tree = MakeSampleTree();
    BinarySearchTree<int> sub = tree.ExtractSubtree(3);
    CHECK(sub.ToFormattedString() == "{3}({1}()[])[{4}()[]]");
    CHECK(sub.GetCount() == 3);
    CHECK(sub.GetHeight() == 2);
    CHECK(sub.IsValid());
    CHECK(sub.GetBalancing() == Balancing::kAvl);
}

void TestExtractLeafAndRoot() {
    BinarySearchTree<int> tree = MakeSampleTree(Balancing::kNone);
    BinarySearchTree<int> leaf = tree.ExtractSubtree(9);
    CHECK(leaf.GetCount() == 1);
    CHECK(leaf.ToFormattedString() == "{9}()[]");
    CHECK(leaf.GetBalancing() == Balancing::kNone);

    BinarySearchTree<int> whole = tree.ExtractSubtree(5);
    CHECK(whole.ToFormattedString() == tree.ToFormattedString());
}

void TestExtractMissingThrows() {
    BinarySearchTree<int> tree = MakeSampleTree();
    CHECK_THROWS(tree.ExtractSubtree(6), ElementNotFound);
    BinarySearchTree<int> empty;
    CHECK_THROWS(empty.ExtractSubtree(1), ElementNotFound);
}

// Извлечённое поддерево - отдельная копия
void TestExtractIsIndependent() {
    BinarySearchTree<int> tree = MakeSampleTree();
    BinarySearchTree<int> sub = tree.ExtractSubtree(8);
    sub.Insert(100);
    sub.Remove(7);
    CHECK(tree.Contains(7));
    CHECK(!tree.Contains(100));
    CHECK(tree.GetCount() == 7);
    CHECK(tree.IsValid());
    CHECK(sub.IsValid());
}

// Поддерево большого AVL-дерева - корректное AVL-дерево (высоты и размеры скопированы)
void TestExtractFromLargeAvl() {
    BinarySearchTree<int> tree;
    DynamicArray<int> values = MakeShuffled(2000, 5);
    for (int value : values)
        tree.Insert(value);
    for (int value = 0; value < 2000; value += 97) {
        BinarySearchTree<int> sub = tree.ExtractSubtree(value);
        CHECK(sub.IsValid());
        CHECK(tree.ContainsSubtree(sub));
    }
}

// ---------- Поиск поддерева ----------

void TestContainsSubtreeYes() {
    BinarySearchTree<int> tree = MakeSampleTree();
    CHECK(tree.ContainsSubtree(Pattern("{3}({1}()[])[{4}()[]]")));
    CHECK(tree.ContainsSubtree(Pattern("{7}()[]")));                // лист
    CHECK(tree.ContainsSubtree(Pattern(tree.ToFormattedString()))); // всё дерево
}

void TestContainsSubtreeNo() {
    BinarySearchTree<int> tree = MakeSampleTree();
    CHECK(!tree.ContainsSubtree(Pattern("{3}({1}()[])[]")));       // тот же корень, нет листа 4
    CHECK(!tree.ContainsSubtree(Pattern("{4}({3}()[])[]")));       // те же значения, другая форма
    CHECK(!tree.ContainsSubtree(Pattern("{6}()[]")));              // значения нет
    CHECK(!tree.ContainsSubtree(Pattern("{8}({7}()[])[{10}()[]]"))); // другой лист
}

void TestContainsEmptyPattern() {
    BinarySearchTree<int> tree = MakeSampleTree();
    BinarySearchTree<int> empty;
    CHECK(tree.ContainsSubtree(empty));
    CHECK(empty.ContainsSubtree(empty));
    CHECK(!empty.ContainsSubtree(Pattern("{1}()[]")));
}

// Образец больше дерева
void TestPatternBiggerThanTree() {
    BinarySearchTree<int> small;
    small.Insert(5);
    CHECK(!small.ContainsSubtree(MakeSampleTree()));
}

// ---------- Слияние и операторы ----------

void TestMerge() {
    BinarySearchTree<int> a = MakeSampleTree();
    BinarySearchTree<int> b;
    const int values[] = {4, 5, 6, 10};
    for (int value : values)
        b.Insert(value);
    CHECK(a.Merge(b) == 2); // добавились только 6 и 10
    CHECK(a.GetCount() == 9);
    CHECK(a.IsValid());
    CHECK(a.Merge(a) == 0); // слияние с самим собой ничего не меняет
    CHECK(a.GetCount() == 9);
}

void TestPlusEqualsAndMinusEquals() {
    BinarySearchTree<int> tree;
    tree += 5;
    tree += 3;
    tree += 5; // повтор - ничего не меняет
    (tree += 8) += 1; // += возвращает ссылку на дерево - можно цепочкой
    CHECK(tree.GetCount() == 4);
    tree -= 3;
    tree -= 100; // нет такого - ничего не меняет
    CHECK(tree.GetCount() == 3);
    CHECK(!tree.Contains(3));
    CHECK(tree.IsValid());
}

void TestPlusIsUnion() {
    BinarySearchTree<int> a;
    BinarySearchTree<int> b(Balancing::kNone);
    a += 1;
    a += 2;
    b += 2;
    b += 3;
    BinarySearchTree<int> sum = a + b;
    CHECK(Print(sum) == "{1, 2, 3}");
    CHECK(sum.GetBalancing() == Balancing::kAvl); // как у левого операнда
    CHECK(a.GetCount() == 2);                     // операнды не меняются
    CHECK(b.GetCount() == 2);

    BinarySearchTree<int> c = a;
    c += b; // объединение "на месте"
    CHECK(c == sum);
}

// == сравнивает множества элементов, а не форму
void TestEqualityIgnoresShape() {
    BinarySearchTree<int> avl(Balancing::kAvl);
    BinarySearchTree<int> plain(Balancing::kNone);
    DynamicArray<int> values = MakeSorted(50);
    for (int value : values) {
        avl += value;
        plain += value;
    }
    CHECK(avl.GetHeight() != plain.GetHeight()); // формы разные
    CHECK(avl == plain);
    CHECK(!(avl != plain));

    plain -= 25;
    CHECK(avl != plain); // разное число элементов
    plain += 100;
    CHECK(avl != plain); // число то же, элементы разные

    BinarySearchTree<int> empty1;
    BinarySearchTree<int> empty2(Balancing::kNone);
    CHECK(empty1 == empty2);
}

void TestPrintOperator() {
    CHECK(Print(MakeSampleTree()) == "{1, 3, 4, 5, 7, 8, 9}");
    CHECK(Print(BinarySearchTree<int>()) == "{}");

    BinarySearchTree<double> doubles;
    doubles += 0.5;
    doubles += -1.25;
    CHECK(Print(doubles) == "{-1.25, 0.5}");

    BinarySearchTree<std::string> words;
    words += "b";
    words += "a";
    CHECK(Print(words) == "{a, b}");
}

} // namespace

void RunSubtreeOpsTests() {
    test::RunTest("extract inner node", TestExtractInnerNode);
    test::RunTest("extract leaf and root", TestExtractLeafAndRoot);
    test::RunTest("extract missing throws", TestExtractMissingThrows);
    test::RunTest("extract is independent", TestExtractIsIndependent);
    test::RunTest("extract from large avl", TestExtractFromLargeAvl);
    test::RunTest("contains subtree yes", TestContainsSubtreeYes);
    test::RunTest("contains subtree no", TestContainsSubtreeNo);
    test::RunTest("contains empty pattern", TestContainsEmptyPattern);
    test::RunTest("pattern bigger than tree", TestPatternBiggerThanTree);
    test::RunTest("merge", TestMerge);
    test::RunTest("operators += and -=", TestPlusEqualsAndMinusEquals);
    test::RunTest("operator + is union", TestPlusIsUnion);
    test::RunTest("equality ignores shape", TestEqualityIgnoresShape);
    test::RunTest("print operator", TestPrintOperator);
}
