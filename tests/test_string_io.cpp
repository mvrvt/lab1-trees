#include <cstddef>
#include <stdexcept>
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

// Маленькое дерево из примера в PLAN.md: 5 / 3 8
BinarySearchTree<int> MakeSmallTree() {
    BinarySearchTree<int> tree;
    tree.Insert(5);
    tree.Insert(3);
    tree.Insert(8);
    return tree;
}

template <class T>
bool SameElements(const BinarySearchTree<T>& a, const BinarySearchTree<T>& b) {
    DynamicArray<T> x = a.ToArray();
    DynamicArray<T> y = b.ToArray();
    if (x.GetCount() != y.GetCount())
        return false;
    for (std::size_t i = 0; i < x.GetCount(); ++i) {
        if (x[i] < y[i] || y[i] < x[i])
            return false;
    }
    return true;
}

// Позиция, на которой FromString бросил TreeParseError (или -1, если не бросил)
long ParseErrorPosition(const std::string& text) {
    try {
        BinarySearchTree<int>::FromString(text, Balancing::kNone);
    } catch (const TreeParseError& error) {
        return static_cast<long>(error.GetPosition());
    }
    return -1;
}

} // namespace

// ---------- Сохранение по формату ----------

TEST(StringIo, SaveDefaultFormat) {
    EXPECT_EQ(MakeSmallTree().ToFormattedString(), "{5}({3}()[])[{8}()[]]");
    EXPECT_EQ(MakeSampleTree().ToFormattedString("{К}(Л)[П]"), "{5}({3}({1}()[])[{4}()[]])[{8}({7}()[])[{9}()[]]]");
}

TEST(StringIo, SaveOtherFormats) {
    BinarySearchTree<int> tree = MakeSmallTree();
    EXPECT_EQ(tree.ToFormattedString("(Л){К}[П]"), "((){3}[]){5}[(){8}[]]"); // ЛКП
    EXPECT_EQ(tree.ToFormattedString("[П](Л){К}"), "[[](){8}]([](){3}){5}"); // ПЛК
    EXPECT_EQ(tree.ToFormattedString("<L>|N|(R)"), "<<>|3|()>|5|(<>|8|())"); // латиница, |..|
    EXPECT_EQ(tree.ToFormattedString("{К}(Л)[R]"), "{5}({3}()[])[{8}()[]]"); // смешанные буквы
}

TEST(StringIo, SaveEmptyAndSingle) {
    BinarySearchTree<int> empty;
    EXPECT_EQ(empty.ToFormattedString(), "");
    BinarySearchTree<int> single;
    single.Insert(42);
    EXPECT_EQ(single.ToFormattedString(), "{42}()[]");
    EXPECT_EQ(single.ToFormattedString("(Л)[П]{К}"), "()[]{42}");
}

TEST(StringIo, FormatErrors) {
    BinarySearchTree<int> tree = MakeSmallTree();
    EXPECT_THROW(tree.ToFormattedString(""), TreeFormatError);            // пусто
    EXPECT_THROW(tree.ToFormattedString("{К}(Л)"), TreeFormatError);      // нет П
    EXPECT_THROW(tree.ToFormattedString("{К}(К)[П]"), TreeFormatError);   // К дважды
    EXPECT_THROW(tree.ToFormattedString("{К(Л)[П]"), TreeFormatError);    // нет '}'
    EXPECT_THROW(tree.ToFormattedString("{К}(Л)[П]x"), TreeFormatError);  // лишнее в конце
    EXPECT_THROW(tree.ToFormattedString("{X}(Л)[П]"), TreeFormatError);   // не буква формата
    EXPECT_THROW(tree.ToFormattedString("aКa(Л)[П]"), TreeFormatError);   // буква вместо скобки
    EXPECT_THROW(tree.ToFormattedString("\\К\\(Л)[П]"), TreeFormatError); // '\' нельзя
    EXPECT_THROW(tree.ToFormattedString("{ К}(Л)[П]"), TreeFormatError);  // пробел
    // TreeFormatError - это std::invalid_argument
    EXPECT_THROW(tree.ToFormattedString("{К}"), std::invalid_argument);
}

TEST(StringIo, FormatToOrder) {
    EXPECT_EQ(FormatToOrder(ParseTreeFormat("{К}(Л)[П]")), TraversalOrder::kNLR);
    EXPECT_EQ(FormatToOrder(ParseTreeFormat("(Л){К}[П]")), TraversalOrder::kLNR);
    EXPECT_EQ(FormatToOrder(ParseTreeFormat("[R](L){N}")), TraversalOrder::kRLN);
}

// ---------- Чтение ----------

TEST(StringIo, ReadSimple) {
    BinarySearchTree<int> tree = BinarySearchTree<int>::FromString("{5}({3}()[])[{8}()[]]");
    EXPECT_EQ(tree.GetCount(), 3);
    EXPECT_TRUE(tree.Contains(3) && tree.Contains(5) && tree.Contains(8));
    EXPECT_TRUE(tree.IsValid());

    BinarySearchTree<int> empty = BinarySearchTree<int>::FromString("");
    EXPECT_TRUE(empty.IsEmpty());
}

// Без балансировки форма совпадает со строкой: сохранить -> прочитать -> сохранить
TEST(StringIo, RoundTripPlainInt) {
    BinarySearchTree<int> tree(Balancing::kNone);
    DynamicArray<int> values = MakeShuffled(500, 2024);
    for (int value : values)
        tree.Insert(value - 250); // есть и отрицательные
    std::string text = tree.ToFormattedString();
    BinarySearchTree<int> read = BinarySearchTree<int>::FromString(text, Balancing::kNone);
    EXPECT_EQ(read.ToFormattedString(), text);
    EXPECT_EQ(read.GetHeight(), tree.GetHeight());
    EXPECT_TRUE(read.IsValid());
}

// В режиме AVL элементы те же, дерево корректное AVL (форма может отличаться)
TEST(StringIo, ReadIntoAvl) {
    BinarySearchTree<int> plain(Balancing::kNone);
    DynamicArray<int> values = MakeSorted(100); // "палка"
    for (int value : values)
        plain.Insert(value);
    BinarySearchTree<int> avl = BinarySearchTree<int>::FromString(plain.ToFormattedString());
    EXPECT_EQ(avl.GetBalancing(), Balancing::kAvl);
    EXPECT_TRUE(avl.IsValid());
    EXPECT_TRUE(SameElements(avl, plain));
    EXPECT_TRUE(avl.GetHeight() < plain.GetHeight());
}

TEST(StringIo, RoundTripDouble) {
    BinarySearchTree<double> tree(Balancing::kNone);
    DynamicArray<int> values = MakeShuffled(300, 7);
    for (int value : values)
        tree.Insert(value * 0.1 - 3.7 + 1.0 / 3.0); // числа без короткой точной записи
    std::string text = tree.ToFormattedString();
    BinarySearchTree<double> read = BinarySearchTree<double>::FromString(text, Balancing::kNone);
    EXPECT_EQ(read.ToFormattedString(), text);
    EXPECT_TRUE(SameElements(read, tree)); // значения совпадают ТОЧНО, до последнего бита

    BinarySearchTree<double> simple;
    simple.Insert(0.1);
    simple.Insert(-2.5);
    EXPECT_EQ(simple.ToFormattedString(), "{0.1}({-2.5}()[])[]"); // без лишних цифр
}

TEST(StringIo, RoundTripString) {
    BinarySearchTree<std::string> tree(Balancing::kNone);
    const char* words[] = {"pear", "apple", "zebra", "two words", "кириллица", "", "(a)[b]{c"};
    for (const char* word : words)
        tree.Insert(word);
    std::string text = tree.ToFormattedString();
    BinarySearchTree<std::string> read = BinarySearchTree<std::string>::FromString(text, Balancing::kNone);
    EXPECT_EQ(read.ToFormattedString(), text);
    EXPECT_EQ(read.GetCount(), 7);
    EXPECT_TRUE(read.Contains("")); // пустая строка - тоже значение: "{}"
}

// '}' и '\' внутри значений экранируются
TEST(StringIo, Escaping) {
    BinarySearchTree<std::string> tree;
    tree.Insert("a}b");
    tree.Insert("c\\d");
    std::string text = tree.ToFormattedString();
    EXPECT_EQ(text, "{a\\}b}()[{c\\\\d}()[]]");
    BinarySearchTree<std::string> read = BinarySearchTree<std::string>::FromString(text);
    EXPECT_TRUE(read.Contains("a}b"));
    EXPECT_TRUE(read.Contains("c\\d"));
}

TEST(StringIo, ParseErrors) {
    using Tree = BinarySearchTree<int>;
    EXPECT_THROW(Tree::FromString("{5}({3}()[]"), TreeParseError);            // не закрыто
    EXPECT_THROW(Tree::FromString("{5}()[]x"), TreeParseError);               // мусор в конце
    EXPECT_THROW(Tree::FromString("{5}()[]{6}()[]"), TreeParseError);         // два корня
    EXPECT_THROW(Tree::FromString("{abc}()[]"), TreeParseError);              // не число
    EXPECT_THROW(Tree::FromString("{12abc}()[]"), TreeParseError);            // не целиком число
    EXPECT_THROW(Tree::FromString("{99999999999}()[]"), TreeParseError);      // не влезает в int
    EXPECT_THROW(Tree::FromString("5()[]"), TreeParseError);                  // нет скобок у значения
    EXPECT_THROW(Tree::FromString("{5}[]()"), TreeParseError);                // перепутаны поддеревья
    EXPECT_THROW(Tree::FromString("{5}({3}()[]{4}()[])[]"), TreeParseError);  // два узла в одном месте
    EXPECT_THROW(Tree::FromString(" {5}()[]"), TreeParseError);               // пробел в начале
    EXPECT_THROW(Tree::FromString("{5"), TreeParseError);                     // оборвано
    EXPECT_THROW(Tree::FromString("{5\\"), TreeParseError);                   // оборвано экранирование
    // TreeParseError - это std::invalid_argument
    EXPECT_THROW(Tree::FromString("{"), std::invalid_argument);
}

// Строка правильная по скобкам, но описывает не дерево поиска
TEST(StringIo, NotSearchTree) {
    using Tree = BinarySearchTree<int>;
    EXPECT_THROW(Tree::FromString("{5}({8}()[])[]"), TreeParseError);         // 8 слева от 5
    EXPECT_THROW(Tree::FromString("{5}()[{2}()[]]"), TreeParseError);         // 2 справа от 5
    EXPECT_THROW(Tree::FromString("{5}({5}()[])[]"), TreeParseError);         // повтор
    EXPECT_THROW(Tree::FromString("{5}({3}()[{7}()[]])[]"), TreeParseError);  // внук 7 > деда 5
}

// Позиция ошибки - номер байта, где начинается неверное место
TEST(StringIo, ParseErrorPosition) {
    EXPECT_EQ(ParseErrorPosition("{5}({8}()[])[]"), 5);   // значение 8
    EXPECT_EQ(ParseErrorPosition("{5}()[]x"), 7);          // лишний символ
    EXPECT_EQ(ParseErrorPosition("{5}({3}()[]"), 11);     // строка кончилась
    EXPECT_EQ(ParseErrorPosition("{5}({3}()[])[]"), -1);  // ошибки нет

    try {
        BinarySearchTree<int>::FromString("{5}({8}()[])[]");
        ADD_FAILURE() << "FromString must throw TreeParseError";
    } catch (const TreeParseError& error) {
        EXPECT_NE(std::string(error.what()).find("position 5"), std::string::npos);
    }
}

// Строка от "палки" - глубокая вложенность; разбор без рекурсии
TEST(StringIo, DeepString) {
    const int n = 5000;
    BinarySearchTree<int> stick(Balancing::kNone);
    DynamicArray<int> values = MakeSorted(n);
    for (int value : values)
        stick.Insert(value);
    std::string text = stick.ToFormattedString();
    BinarySearchTree<int> read = BinarySearchTree<int>::FromString(text, Balancing::kNone);
    EXPECT_EQ(read.GetHeight(), static_cast<std::size_t>(n));
    EXPECT_EQ(read.ToFormattedString(), text);
}
