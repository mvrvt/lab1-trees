#include <cstddef>
#include <sstream> // std::ostringstream
#include <string>

#include <gtest/gtest.h>

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

} // namespace

// ---------- Извлечение поддерева ----------

TEST(SubtreeOps, ExtractInnerNode) {
    BinarySearchTree<int> tree = MakeSampleTree();
    BinarySearchTree<int> sub = tree.ExtractSubtree(3);
    EXPECT_EQ(sub.ToFormattedString(), "{3}({1}()[])[{4}()[]]");
    EXPECT_EQ(sub.GetCount(), 3);
    EXPECT_EQ(sub.GetHeight(), 2);
    EXPECT_TRUE(sub.IsValid());
    EXPECT_EQ(sub.GetBalancing(), Balancing::kAvl);
}

TEST(SubtreeOps, ExtractLeafAndRoot) {
    BinarySearchTree<int> tree = MakeSampleTree(Balancing::kNone);
    BinarySearchTree<int> leaf = tree.ExtractSubtree(9);
    EXPECT_EQ(leaf.GetCount(), 1);
    EXPECT_EQ(leaf.ToFormattedString(), "{9}()[]");
    EXPECT_EQ(leaf.GetBalancing(), Balancing::kNone);

    BinarySearchTree<int> whole = tree.ExtractSubtree(5);
    EXPECT_EQ(whole.ToFormattedString(), tree.ToFormattedString());
}

TEST(SubtreeOps, ExtractMissingThrows) {
    BinarySearchTree<int> tree = MakeSampleTree();
    EXPECT_THROW(tree.ExtractSubtree(6), ElementNotFound);
    BinarySearchTree<int> empty;
    EXPECT_THROW(empty.ExtractSubtree(1), ElementNotFound);
}

// Извлечённое поддерево - отдельная копия
TEST(SubtreeOps, ExtractIsIndependent) {
    BinarySearchTree<int> tree = MakeSampleTree();
    BinarySearchTree<int> sub = tree.ExtractSubtree(8);
    sub.Insert(100);
    sub.Remove(7);
    EXPECT_TRUE(tree.Contains(7));
    EXPECT_FALSE(tree.Contains(100));
    EXPECT_EQ(tree.GetCount(), 7);
    EXPECT_TRUE(tree.IsValid());
    EXPECT_TRUE(sub.IsValid());
}

// Поддерево большого AVL-дерева - корректное AVL-дерево (высоты и размеры скопированы)
TEST(SubtreeOps, ExtractFromLargeAvl) {
    BinarySearchTree<int> tree;
    DynamicArray<int> values = MakeShuffled(2000, 5);
    for (int value : values)
        tree.Insert(value);
    for (int value = 0; value < 2000; value += 97) {
        BinarySearchTree<int> sub = tree.ExtractSubtree(value);
        EXPECT_TRUE(sub.IsValid());
        EXPECT_TRUE(tree.ContainsSubtree(sub));
    }
}

// ---------- Поиск поддерева ----------

TEST(SubtreeOps, ContainsSubtreeYes) {
    BinarySearchTree<int> tree = MakeSampleTree();
    EXPECT_TRUE(tree.ContainsSubtree(Pattern("{3}({1}()[])[{4}()[]]")));
    EXPECT_TRUE(tree.ContainsSubtree(Pattern("{7}()[]")));                // лист
    EXPECT_TRUE(tree.ContainsSubtree(Pattern(tree.ToFormattedString()))); // всё дерево
}

TEST(SubtreeOps, ContainsSubtreeNo) {
    BinarySearchTree<int> tree = MakeSampleTree();
    EXPECT_FALSE(tree.ContainsSubtree(Pattern("{3}({1}()[])[]")));       // тот же корень, нет листа 4
    EXPECT_FALSE(tree.ContainsSubtree(Pattern("{4}({3}()[])[]")));       // те же значения, другая форма
    EXPECT_FALSE(tree.ContainsSubtree(Pattern("{6}()[]")));              // значения нет
    EXPECT_FALSE(tree.ContainsSubtree(Pattern("{8}({7}()[])[{10}()[]]"))); // другой лист
}

TEST(SubtreeOps, ContainsEmptyPattern) {
    BinarySearchTree<int> tree = MakeSampleTree();
    BinarySearchTree<int> empty;
    EXPECT_TRUE(tree.ContainsSubtree(empty));
    EXPECT_TRUE(empty.ContainsSubtree(empty));
    EXPECT_FALSE(empty.ContainsSubtree(Pattern("{1}()[]")));
}

// Образец больше дерева
TEST(SubtreeOps, PatternBiggerThanTree) {
    BinarySearchTree<int> small;
    small.Insert(5);
    EXPECT_FALSE(small.ContainsSubtree(MakeSampleTree()));
}

// ---------- Слияние и операторы ----------

TEST(SubtreeOps, Merge) {
    BinarySearchTree<int> a = MakeSampleTree();
    BinarySearchTree<int> b;
    const int values[] = {4, 5, 6, 10};
    for (int value : values)
        b.Insert(value);
    EXPECT_EQ(a.Merge(b), 2); // добавились только 6 и 10
    EXPECT_EQ(a.GetCount(), 9);
    EXPECT_TRUE(a.IsValid());
    EXPECT_EQ(a.Merge(a), 0); // слияние с самим собой ничего не меняет
    EXPECT_EQ(a.GetCount(), 9);
}

TEST(SubtreeOps, PlusEqualsAndMinusEquals) {
    BinarySearchTree<int> tree;
    tree += 5;
    tree += 3;
    tree += 5; // повтор - ничего не меняет
    (tree += 8) += 1; // += возвращает ссылку на дерево - можно цепочкой
    EXPECT_EQ(tree.GetCount(), 4);
    tree -= 3;
    tree -= 100; // нет такого - ничего не меняет
    EXPECT_EQ(tree.GetCount(), 3);
    EXPECT_FALSE(tree.Contains(3));
    EXPECT_TRUE(tree.IsValid());
}

TEST(SubtreeOps, PlusIsUnion) {
    BinarySearchTree<int> a;
    BinarySearchTree<int> b(Balancing::kNone);
    a += 1;
    a += 2;
    b += 2;
    b += 3;
    BinarySearchTree<int> sum = a + b;
    EXPECT_EQ(Print(sum), "{1, 2, 3}");
    EXPECT_EQ(sum.GetBalancing(), Balancing::kAvl); // как у левого операнда
    EXPECT_EQ(a.GetCount(), 2);                     // операнды не меняются
    EXPECT_EQ(b.GetCount(), 2);

    BinarySearchTree<int> c = a;
    c += b; // объединение "на месте"
    EXPECT_EQ(c, sum);
}

// == сравнивает множества элементов, а не форму
TEST(SubtreeOps, EqualityIgnoresShape) {
    BinarySearchTree<int> avl(Balancing::kAvl);
    BinarySearchTree<int> plain(Balancing::kNone);
    DynamicArray<int> values = MakeSorted(50);
    for (int value : values) {
        avl += value;
        plain += value;
    }
    EXPECT_NE(avl.GetHeight(), plain.GetHeight()); // формы разные
    EXPECT_EQ(avl, plain);
    EXPECT_FALSE(avl != plain);

    plain -= 25;
    EXPECT_NE(avl, plain); // разное число элементов
    plain += 100;
    EXPECT_NE(avl, plain); // число то же, элементы разные

    BinarySearchTree<int> empty1;
    BinarySearchTree<int> empty2(Balancing::kNone);
    EXPECT_EQ(empty1, empty2);
}

TEST(SubtreeOps, PrintOperator) {
    EXPECT_EQ(Print(MakeSampleTree()), "{1, 3, 4, 5, 7, 8, 9}");
    EXPECT_EQ(Print(BinarySearchTree<int>()), "{}");

    BinarySearchTree<double> doubles;
    doubles += 0.5;
    doubles += -1.25;
    EXPECT_EQ(Print(doubles), "{-1.25, 0.5}");

    BinarySearchTree<std::string> words;
    words += "b";
    words += "a";
    EXPECT_EQ(Print(words), "{a, b}");
}
