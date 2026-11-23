#include <cstddef>
#include <limits>
#include <string>

#include <gtest/gtest.h>

#include "TestHelpers.hpp"
#include "tree/BinarySearchTree.hpp"

namespace {

using test::LiveCounter;
using test::MakeShuffled;

// ---------- Вспомогательные типы ----------

// Запись "ключ -> данные", сравнивается только по ключу.
// Так в ЛР2 будет устроен словарь: Find по ключу возвращает всю запись.
struct Record {
    int         key;
    std::string data;

    bool operator<(const Record& other) const { return key < other.key; }
};

// Дерево для большинства тестов (корень 5):
//
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

} // namespace

// ---------- Тесты ----------

TEST(Basic, EmptyTree) {
    BinarySearchTree<int> tree;
    EXPECT_TRUE(tree.IsEmpty());
    EXPECT_EQ(tree.GetCount(), 0);
    EXPECT_EQ(tree.GetHeight(), 0);
    EXPECT_FALSE(tree.Contains(42));
    EXPECT_FALSE(tree.Remove(42));
    EXPECT_TRUE(tree.IsValid());
    EXPECT_THROW(tree.GetMin(), EmptyCollection);
    EXPECT_THROW(tree.GetMax(), EmptyCollection);
    EXPECT_THROW(tree.Find(42), ElementNotFound);
}

TEST(Basic, InsertAndContains) {
    BinarySearchTree<int> tree = MakeSampleTree();
    EXPECT_EQ(tree.GetCount(), 7);
    EXPECT_EQ(tree.GetHeight(), 3);
    EXPECT_TRUE(tree.IsValid());

    const int present[] = {1, 3, 4, 5, 7, 8, 9};
    for (int value : present)
        EXPECT_TRUE(tree.Contains(value));
    const int absent[] = {0, 2, 6, 10, -5};
    for (int value : absent)
        EXPECT_FALSE(tree.Contains(value));

    EXPECT_EQ(tree.GetMin(), 1);
    EXPECT_EQ(tree.GetMax(), 9);
}

TEST(Basic, InsertDuplicate) {
    BinarySearchTree<int> tree = MakeSampleTree();
    EXPECT_FALSE(tree.Insert(5)); // корень
    EXPECT_FALSE(tree.Insert(1)); // лист
    EXPECT_EQ(tree.GetCount(), 7);
    EXPECT_TRUE(tree.IsValid());
}

TEST(Basic, FindReturnsStoredValue) {
    BinarySearchTree<Record> tree;
    tree.Insert({2, "two"});
    tree.Insert({1, "one"});
    tree.Insert({3, "three"});
    // Ищем по ключу, данные в "образце" не важны
    EXPECT_EQ(tree.Find({1, ""}).data, "one");
    EXPECT_EQ(tree.Find({3, ""}).data, "three");
    EXPECT_THROW(tree.Find({4, ""}), ElementNotFound);
    // Повторный ключ не вставляется, старые данные остаются
    EXPECT_FALSE(tree.Insert({2, "другое"}));
    EXPECT_EQ(tree.Find({2, ""}).data, "two");
}

TEST(Basic, RemoveLeaf) {
    BinarySearchTree<int> tree = MakeSampleTree();
    EXPECT_TRUE(tree.Remove(1));
    EXPECT_FALSE(tree.Contains(1));
    EXPECT_EQ(tree.GetCount(), 6);
    EXPECT_EQ(tree.GetMin(), 3);
    EXPECT_TRUE(tree.IsValid());
}

TEST(Basic, RemoveNodeWithOneChild) {
    BinarySearchTree<int> tree = MakeSampleTree();
    tree.Remove(1); // теперь у 3 только правый ребёнок 4
    EXPECT_TRUE(tree.Remove(3));
    EXPECT_FALSE(tree.Contains(3));
    EXPECT_TRUE(tree.Contains(4));
    EXPECT_EQ(tree.GetCount(), 5);
    EXPECT_TRUE(tree.IsValid());
}

TEST(Basic, RemoveNodeWithTwoChildren) {
    BinarySearchTree<int> tree = MakeSampleTree();
    EXPECT_TRUE(tree.Remove(8)); // дети 7 и 9, преемник - 9
    EXPECT_FALSE(tree.Contains(8));
    EXPECT_TRUE(tree.Contains(7));
    EXPECT_TRUE(tree.Contains(9));
    EXPECT_EQ(tree.GetCount(), 6);
    EXPECT_TRUE(tree.IsValid());
}

TEST(Basic, RemoveRoot) {
    BinarySearchTree<int> tree = MakeSampleTree();
    EXPECT_TRUE(tree.Remove(5)); // преемник корня - 7
    EXPECT_FALSE(tree.Contains(5));
    EXPECT_EQ(tree.GetCount(), 6);
    EXPECT_TRUE(tree.IsValid());

    BinarySearchTree<int> single;
    single.Insert(1);
    EXPECT_TRUE(single.Remove(1));
    EXPECT_TRUE(single.IsEmpty());
    EXPECT_EQ(single.GetHeight(), 0);
}

TEST(Basic, RemoveMissing) {
    BinarySearchTree<int> tree = MakeSampleTree();
    EXPECT_FALSE(tree.Remove(6));
    EXPECT_EQ(tree.GetCount(), 7);
    EXPECT_TRUE(tree.IsValid());
}

TEST(Basic, ManyInsertsAndRemoves) {
    const int n = 2000;
    DynamicArray<int> values = MakeShuffled(n);
    BinarySearchTree<int> tree;
    for (int value : values)
        EXPECT_TRUE(tree.Insert(value));
    EXPECT_EQ(tree.GetCount(), static_cast<std::size_t>(n));
    EXPECT_TRUE(tree.IsValid());
    EXPECT_EQ(tree.GetMin(), 0);
    EXPECT_EQ(tree.GetMax(), n - 1);

    // Удаляем чётные (в перемешанном порядке) и проверяем, что осталось ровно нужное
    for (int value : values) {
        if (value % 2 == 0) {
            EXPECT_TRUE(tree.Remove(value));
        }
    }
    EXPECT_EQ(tree.GetCount(), static_cast<std::size_t>(n / 2));
    EXPECT_TRUE(tree.IsValid());
    for (int i = 0; i < n; ++i)
        EXPECT_EQ(tree.Contains(i), (i % 2 != 0));
}

TEST(Basic, DegenerateTree) {
    // Без балансировки отсортированный вход даёт "палку": высота равна числу элементов.
    // Как это исправляет AVL - см. test_avl.cpp.
    const int n = 5000;
    BinarySearchTree<int> tree(Balancing::kNone);
    for (int i = 0; i < n; ++i)
        tree.Insert(i);
    EXPECT_EQ(tree.GetHeight(), static_cast<std::size_t>(n));
    EXPECT_TRUE(tree.IsValid());
    tree.Clear(); // не должно быть переполнения стека
    EXPECT_TRUE(tree.IsEmpty());
}

TEST(Basic, Copy) {
    BinarySearchTree<int> original = MakeSampleTree();
    BinarySearchTree<int> copy(original);
    EXPECT_EQ(copy.GetCount(), original.GetCount());
    EXPECT_EQ(copy.GetHeight(), original.GetHeight()); // та же форма
    EXPECT_TRUE(copy.IsValid());

    copy.Remove(5);     // копия независима от оригинала
    copy.Insert(100);
    EXPECT_TRUE(original.Contains(5));
    EXPECT_FALSE(original.Contains(100));
    EXPECT_EQ(original.GetCount(), 7);
}

TEST(Basic, Assignment) {
    BinarySearchTree<int> tree = MakeSampleTree();
    BinarySearchTree<int> other;
    other.Insert(42);
    other = tree;
    EXPECT_EQ(other.GetCount(), 7);
    EXPECT_FALSE(other.Contains(42));

    BinarySearchTree<int>& same = other;
    other = same; // самоприсваивание не должно ломать дерево
    EXPECT_EQ(other.GetCount(), 7);
    EXPECT_TRUE(other.IsValid());
}

TEST(Basic, Move) {
    BinarySearchTree<int> source = MakeSampleTree();
    BinarySearchTree<int> moved(std::move(source));
    EXPECT_EQ(moved.GetCount(), 7);
    EXPECT_TRUE(source.IsEmpty()); // NOLINT: проверяем состояние после перемещения
    EXPECT_TRUE(source.IsValid());

    BinarySearchTree<int> target;
    target.Insert(1);
    target = std::move(moved);
    EXPECT_EQ(target.GetCount(), 7);
    EXPECT_TRUE(moved.IsEmpty());
}

TEST(Basic, Strings) {
    BinarySearchTree<std::string> tree;
    tree.Insert("pear");
    tree.Insert("apple");
    tree.Insert("plum");
    tree.Insert("banana");
    EXPECT_EQ(tree.GetCount(), 4);
    EXPECT_EQ(tree.GetMin(), "apple");
    EXPECT_EQ(tree.GetMax(), "plum");
    EXPECT_TRUE(tree.Remove("pear"));
    EXPECT_TRUE(tree.IsValid());
}

TEST(Basic, NoLeaks) {
    LiveCounter::alive = 0;
    {
        BinarySearchTree<LiveCounter> tree;
        DynamicArray<int> values = MakeShuffled(500);
        for (int value : values)
            tree.Insert(LiveCounter(value));
        EXPECT_EQ(LiveCounter::alive, 500);

        tree.Insert(LiveCounter(7)); // дубликат: временный объект умер, новый не сохранён
        EXPECT_EQ(LiveCounter::alive, 500);

        for (int i = 0; i < 100; ++i)
            tree.Remove(LiveCounter(i));
        EXPECT_EQ(LiveCounter::alive, 400);

        BinarySearchTree<LiveCounter> copy(tree);
        EXPECT_EQ(LiveCounter::alive, 800);
        copy.Clear();
        EXPECT_EQ(LiveCounter::alive, 400);
    } // здесь вызывается деструктор tree
    EXPECT_EQ(LiveCounter::alive, 0);
}

TEST(Basic, RejectNonFiniteNumbers) {
    BinarySearchTree<double> tree;
    tree.Insert(1e308);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();

    EXPECT_THROW(tree.Insert(infinity), std::invalid_argument);
    EXPECT_THROW(tree.Find(nan), std::invalid_argument);
    EXPECT_THROW(tree.Contains(nan), std::invalid_argument);
    EXPECT_THROW(tree.Remove(nan), std::invalid_argument);
    EXPECT_THROW(tree.IndexOf(nan), std::invalid_argument);
    EXPECT_THROW(tree.Map([](double value) { return value * 2.0; }), std::invalid_argument);
    EXPECT_EQ(tree.GetCount(), 1U);
    EXPECT_EQ(tree.Get(0), 1e308);
}
