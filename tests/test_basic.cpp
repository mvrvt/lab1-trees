#include <cstddef>
#include <string>

#include "TestFramework.hpp"
#include "tree/BinarySearchTree.hpp"

namespace {

// ---------- Вспомогательные типы ----------

// Считает, сколько объектов сейчас "живо". Если после удаления дерева
// alive != 0 - где-то утечка (или, наоборот, двойное удаление).
// На Mac AddressSanitizer не ищет утечки, а этот счётчик работает везде.
struct LiveCounter {
    static inline int alive = 0;
    int value;

    LiveCounter(int new_value) : value(new_value) { ++alive; }
    LiveCounter(const LiveCounter& other) : value(other.value) { ++alive; }
    LiveCounter& operator=(const LiveCounter& other) = default;
    ~LiveCounter() { --alive; }

    bool operator<(const LiveCounter& other) const { return value < other.value; }
};

// Запись "ключ -> данные", сравнивается только по ключу.
// Так в ЛР2 будет устроен словарь: Find по ключу возвращает всю запись.
struct Record {
    int         key;
    std::string data;

    bool operator<(const Record& other) const { return key < other.key; }
};

// Детерминированная "случайная" перестановка 0..n-1 (одинаковая при каждом запуске)
DynamicArray<int> MakeShuffled(int n) {
    DynamicArray<int> values;
    for (int i = 0; i < n; ++i)
        values.Append(i);
    unsigned int seed = 12345;
    for (int i = n - 1; i > 0; --i) {
        seed = seed * 1103515245u + 12345u; // линейный конгруэнтный генератор
        int j = static_cast<int>(seed % static_cast<unsigned int>(i + 1));
        int temp = values[static_cast<std::size_t>(i)];
        values[static_cast<std::size_t>(i)] = values[static_cast<std::size_t>(j)];
        values[static_cast<std::size_t>(j)] = temp;
    }
    return values;
}

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

// ---------- Тесты ----------

void TestEmptyTree() {
    BinarySearchTree<int> tree;
    CHECK(tree.IsEmpty());
    CHECK(tree.GetCount() == 0);
    CHECK(tree.GetHeight() == 0);
    CHECK(!tree.Contains(42));
    CHECK(!tree.Remove(42));
    CHECK(tree.IsValid());
    CHECK_THROWS(tree.GetMin(), EmptyCollection);
    CHECK_THROWS(tree.GetMax(), EmptyCollection);
    CHECK_THROWS(tree.Find(42), ElementNotFound);
}

void TestInsertAndContains() {
    BinarySearchTree<int> tree = MakeSampleTree();
    CHECK(tree.GetCount() == 7);
    CHECK(tree.GetHeight() == 3);
    CHECK(tree.IsValid());

    const int present[] = {1, 3, 4, 5, 7, 8, 9};
    for (int value : present)
        CHECK(tree.Contains(value));
    const int absent[] = {0, 2, 6, 10, -5};
    for (int value : absent)
        CHECK(!tree.Contains(value));

    CHECK(tree.GetMin() == 1);
    CHECK(tree.GetMax() == 9);
}

void TestInsertDuplicate() {
    BinarySearchTree<int> tree = MakeSampleTree();
    CHECK(!tree.Insert(5)); // корень
    CHECK(!tree.Insert(1)); // лист
    CHECK(tree.GetCount() == 7);
    CHECK(tree.IsValid());
}

void TestFindReturnsStoredValue() {
    BinarySearchTree<Record> tree;
    tree.Insert({2, "two"});
    tree.Insert({1, "one"});
    tree.Insert({3, "three"});
    // Ищем по ключу, данные в "образце" не важны
    CHECK(tree.Find({1, ""}).data == "one");
    CHECK(tree.Find({3, ""}).data == "three");
    CHECK_THROWS(tree.Find({4, ""}), ElementNotFound);
    // Повторный ключ не вставляется, старые данные остаются
    CHECK(!tree.Insert({2, "другое"}));
    CHECK(tree.Find({2, ""}).data == "two");
}

void TestRemoveLeaf() {
    BinarySearchTree<int> tree = MakeSampleTree();
    CHECK(tree.Remove(1));
    CHECK(!tree.Contains(1));
    CHECK(tree.GetCount() == 6);
    CHECK(tree.GetMin() == 3);
    CHECK(tree.IsValid());
}

void TestRemoveNodeWithOneChild() {
    BinarySearchTree<int> tree = MakeSampleTree();
    tree.Remove(1); // теперь у 3 только правый ребёнок 4
    CHECK(tree.Remove(3));
    CHECK(!tree.Contains(3));
    CHECK(tree.Contains(4));
    CHECK(tree.GetCount() == 5);
    CHECK(tree.IsValid());
}

void TestRemoveNodeWithTwoChildren() {
    BinarySearchTree<int> tree = MakeSampleTree();
    CHECK(tree.Remove(8)); // дети 7 и 9, преемник - 9
    CHECK(!tree.Contains(8));
    CHECK(tree.Contains(7));
    CHECK(tree.Contains(9));
    CHECK(tree.GetCount() == 6);
    CHECK(tree.IsValid());
}

void TestRemoveRoot() {
    BinarySearchTree<int> tree = MakeSampleTree();
    CHECK(tree.Remove(5)); // преемник корня - 7
    CHECK(!tree.Contains(5));
    CHECK(tree.GetCount() == 6);
    CHECK(tree.IsValid());

    BinarySearchTree<int> single;
    single.Insert(1);
    CHECK(single.Remove(1));
    CHECK(single.IsEmpty());
    CHECK(single.GetHeight() == 0);
}

void TestRemoveMissing() {
    BinarySearchTree<int> tree = MakeSampleTree();
    CHECK(!tree.Remove(6));
    CHECK(tree.GetCount() == 7);
    CHECK(tree.IsValid());
}

void TestManyInsertsAndRemoves() {
    const int n = 2000;
    DynamicArray<int> values = MakeShuffled(n);
    BinarySearchTree<int> tree;
    for (int value : values)
        CHECK(tree.Insert(value));
    CHECK(tree.GetCount() == static_cast<std::size_t>(n));
    CHECK(tree.IsValid());
    CHECK(tree.GetMin() == 0);
    CHECK(tree.GetMax() == n - 1);

    // Удаляем чётные (в перемешанном порядке) и проверяем, что осталось ровно нужное
    for (int value : values) {
        if (value % 2 == 0)
            CHECK(tree.Remove(value));
    }
    CHECK(tree.GetCount() == static_cast<std::size_t>(n / 2));
    CHECK(tree.IsValid());
    for (int i = 0; i < n; ++i)
        CHECK(tree.Contains(i) == (i % 2 != 0));
}

void TestDegenerateTree() {
    // Отсортированный вход -> "палка": высота равна числу элементов.
    // Именно этот случай исправит балансировка (следующий шаг).
    const int n = 5000;
    BinarySearchTree<int> tree;
    for (int i = 0; i < n; ++i)
        tree.Insert(i);
    CHECK(tree.GetHeight() == static_cast<std::size_t>(n));
    CHECK(tree.IsValid());
    tree.Clear(); // не должно быть переполнения стека
    CHECK(tree.IsEmpty());
}

void TestCopy() {
    BinarySearchTree<int> original = MakeSampleTree();
    BinarySearchTree<int> copy(original);
    CHECK(copy.GetCount() == original.GetCount());
    CHECK(copy.GetHeight() == original.GetHeight()); // та же форма
    CHECK(copy.IsValid());

    copy.Remove(5);     // копия независима от оригинала
    copy.Insert(100);
    CHECK(original.Contains(5));
    CHECK(!original.Contains(100));
    CHECK(original.GetCount() == 7);
}

void TestAssignment() {
    BinarySearchTree<int> tree = MakeSampleTree();
    BinarySearchTree<int> other;
    other.Insert(42);
    other = tree;
    CHECK(other.GetCount() == 7);
    CHECK(!other.Contains(42));

    BinarySearchTree<int>& same = other;
    other = same; // самоприсваивание не должно ломать дерево
    CHECK(other.GetCount() == 7);
    CHECK(other.IsValid());
}

void TestMove() {
    BinarySearchTree<int> source = MakeSampleTree();
    BinarySearchTree<int> moved(std::move(source));
    CHECK(moved.GetCount() == 7);
    CHECK(source.IsEmpty()); // NOLINT: проверяем состояние после перемещения
    CHECK(source.IsValid());

    BinarySearchTree<int> target;
    target.Insert(1);
    target = std::move(moved);
    CHECK(target.GetCount() == 7);
    CHECK(moved.IsEmpty());
}

void TestStrings() {
    BinarySearchTree<std::string> tree;
    tree.Insert("pear");
    tree.Insert("apple");
    tree.Insert("plum");
    tree.Insert("banana");
    CHECK(tree.GetCount() == 4);
    CHECK(tree.GetMin() == "apple");
    CHECK(tree.GetMax() == "plum");
    CHECK(tree.Remove("pear"));
    CHECK(tree.IsValid());
}

void TestNoLeaks() {
    LiveCounter::alive = 0;
    {
        BinarySearchTree<LiveCounter> tree;
        DynamicArray<int> values = MakeShuffled(500);
        for (int value : values)
            tree.Insert(LiveCounter(value));
        CHECK(LiveCounter::alive == 500);

        tree.Insert(LiveCounter(7)); // дубликат: временный объект умер, новый не сохранён
        CHECK(LiveCounter::alive == 500);

        for (int i = 0; i < 100; ++i)
            tree.Remove(LiveCounter(i));
        CHECK(LiveCounter::alive == 400);

        BinarySearchTree<LiveCounter> copy(tree);
        CHECK(LiveCounter::alive == 800);
        copy.Clear();
        CHECK(LiveCounter::alive == 400);
    } // здесь вызывается деструктор tree
    CHECK(LiveCounter::alive == 0);
}

} // namespace

void RunBasicTests() {
    test::RunTest("empty tree", TestEmptyTree);
    test::RunTest("insert and contains", TestInsertAndContains);
    test::RunTest("insert duplicate", TestInsertDuplicate);
    test::RunTest("find returns stored value", TestFindReturnsStoredValue);
    test::RunTest("remove leaf", TestRemoveLeaf);
    test::RunTest("remove node with one child", TestRemoveNodeWithOneChild);
    test::RunTest("remove node with two children", TestRemoveNodeWithTwoChildren);
    test::RunTest("remove root", TestRemoveRoot);
    test::RunTest("remove missing", TestRemoveMissing);
    test::RunTest("many inserts and removes", TestManyInsertsAndRemoves);
    test::RunTest("degenerate tree", TestDegenerateTree);
    test::RunTest("copy", TestCopy);
    test::RunTest("assignment", TestAssignment);
    test::RunTest("move", TestMove);
    test::RunTest("strings", TestStrings);
    test::RunTest("no leaks", TestNoLeaks);
}
