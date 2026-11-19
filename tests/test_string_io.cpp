#include <cstddef>
#include <stdexcept>
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

// ---------- Сохранение по формату ----------

void TestSaveDefaultFormat() {
    CHECK(MakeSmallTree().ToFormattedString() == "{5}({3}()[])[{8}()[]]");
    CHECK(MakeSampleTree().ToFormattedString("{К}(Л)[П]") ==
          "{5}({3}({1}()[])[{4}()[]])[{8}({7}()[])[{9}()[]]]");
}

void TestSaveOtherFormats() {
    BinarySearchTree<int> tree = MakeSmallTree();
    CHECK(tree.ToFormattedString("(Л){К}[П]") == "((){3}[]){5}[(){8}[]]"); // ЛКП
    CHECK(tree.ToFormattedString("[П](Л){К}") == "[[](){8}]([](){3}){5}"); // ПЛК
    CHECK(tree.ToFormattedString("<L>|N|(R)") == "<<>|3|()>|5|(<>|8|())"); // латиница, |..|
    CHECK(tree.ToFormattedString("{К}(Л)[R]") == "{5}({3}()[])[{8}()[]]"); // смешанные буквы
}

void TestSaveEmptyAndSingle() {
    BinarySearchTree<int> empty;
    CHECK(empty.ToFormattedString() == "");
    BinarySearchTree<int> single;
    single.Insert(42);
    CHECK(single.ToFormattedString() == "{42}()[]");
    CHECK(single.ToFormattedString("(Л)[П]{К}") == "()[]{42}");
}

void TestFormatErrors() {
    BinarySearchTree<int> tree = MakeSmallTree();
    CHECK_THROWS(tree.ToFormattedString(""), TreeFormatError);            // пусто
    CHECK_THROWS(tree.ToFormattedString("{К}(Л)"), TreeFormatError);      // нет П
    CHECK_THROWS(tree.ToFormattedString("{К}(К)[П]"), TreeFormatError);   // К дважды
    CHECK_THROWS(tree.ToFormattedString("{К(Л)[П]"), TreeFormatError);    // нет '}'
    CHECK_THROWS(tree.ToFormattedString("{К}(Л)[П]x"), TreeFormatError);  // лишнее в конце
    CHECK_THROWS(tree.ToFormattedString("{X}(Л)[П]"), TreeFormatError);   // не буква формата
    CHECK_THROWS(tree.ToFormattedString("aКa(Л)[П]"), TreeFormatError);   // буква вместо скобки
    CHECK_THROWS(tree.ToFormattedString("\\К\\(Л)[П]"), TreeFormatError); // '\' нельзя
    CHECK_THROWS(tree.ToFormattedString("{ К}(Л)[П]"), TreeFormatError);  // пробел
    // TreeFormatError - это std::invalid_argument
    CHECK_THROWS(tree.ToFormattedString("{К}"), std::invalid_argument);
}

void TestFormatToOrder() {
    CHECK(FormatToOrder(ParseTreeFormat("{К}(Л)[П]")) == TraversalOrder::kNLR);
    CHECK(FormatToOrder(ParseTreeFormat("(Л){К}[П]")) == TraversalOrder::kLNR);
    CHECK(FormatToOrder(ParseTreeFormat("[R](L){N}")) == TraversalOrder::kRLN);
}

// ---------- Чтение ----------

void TestReadSimple() {
    BinarySearchTree<int> tree = BinarySearchTree<int>::FromString("{5}({3}()[])[{8}()[]]");
    CHECK(tree.GetCount() == 3);
    CHECK(tree.Contains(3) && tree.Contains(5) && tree.Contains(8));
    CHECK(tree.IsValid());

    BinarySearchTree<int> empty = BinarySearchTree<int>::FromString("");
    CHECK(empty.IsEmpty());
}

// Без балансировки форма совпадает со строкой: сохранить -> прочитать -> сохранить
void TestRoundTripPlainInt() {
    BinarySearchTree<int> tree(Balancing::kNone);
    DynamicArray<int> values = MakeShuffled(500, 2024);
    for (int value : values)
        tree.Insert(value - 250); // есть и отрицательные
    std::string text = tree.ToFormattedString();
    BinarySearchTree<int> read = BinarySearchTree<int>::FromString(text, Balancing::kNone);
    CHECK(read.ToFormattedString() == text);
    CHECK(read.GetHeight() == tree.GetHeight());
    CHECK(read.IsValid());
}

// В режиме AVL элементы те же, дерево корректное AVL (форма может отличаться)
void TestReadIntoAvl() {
    BinarySearchTree<int> plain(Balancing::kNone);
    DynamicArray<int> values = MakeSorted(100); // "палка"
    for (int value : values)
        plain.Insert(value);
    BinarySearchTree<int> avl = BinarySearchTree<int>::FromString(plain.ToFormattedString());
    CHECK(avl.GetBalancing() == Balancing::kAvl);
    CHECK(avl.IsValid());
    CHECK(SameElements(avl, plain));
    CHECK(avl.GetHeight() < plain.GetHeight());
}

void TestRoundTripDouble() {
    BinarySearchTree<double> tree(Balancing::kNone);
    DynamicArray<int> values = MakeShuffled(300, 7);
    for (int value : values)
        tree.Insert(value * 0.1 - 3.7 + 1.0 / 3.0); // числа без короткой точной записи
    std::string text = tree.ToFormattedString();
    BinarySearchTree<double> read = BinarySearchTree<double>::FromString(text, Balancing::kNone);
    CHECK(read.ToFormattedString() == text);
    CHECK(SameElements(read, tree)); // значения совпадают ТОЧНО, до последнего бита

    BinarySearchTree<double> simple;
    simple.Insert(0.1);
    simple.Insert(-2.5);
    CHECK(simple.ToFormattedString() == "{0.1}({-2.5}()[])[]"); // без лишних цифр
}

void TestRoundTripString() {
    BinarySearchTree<std::string> tree(Balancing::kNone);
    const char* words[] = {"pear", "apple", "zebra", "two words", "кириллица", "", "(a)[b]{c"};
    for (const char* word : words)
        tree.Insert(word);
    std::string text = tree.ToFormattedString();
    BinarySearchTree<std::string> read = BinarySearchTree<std::string>::FromString(text, Balancing::kNone);
    CHECK(read.ToFormattedString() == text);
    CHECK(read.GetCount() == 7);
    CHECK(read.Contains("")); // пустая строка - тоже значение: "{}"
}

// '}' и '\' внутри значений экранируются
void TestEscaping() {
    BinarySearchTree<std::string> tree;
    tree.Insert("a}b");
    tree.Insert("c\\d");
    std::string text = tree.ToFormattedString();
    CHECK(text == "{a\\}b}()[{c\\\\d}()[]]");
    BinarySearchTree<std::string> read = BinarySearchTree<std::string>::FromString(text);
    CHECK(read.Contains("a}b"));
    CHECK(read.Contains("c\\d"));
}

void TestParseErrors() {
    using Tree = BinarySearchTree<int>;
    CHECK_THROWS(Tree::FromString("{5}({3}()[]"), TreeParseError);            // не закрыто
    CHECK_THROWS(Tree::FromString("{5}()[]x"), TreeParseError);               // мусор в конце
    CHECK_THROWS(Tree::FromString("{5}()[]{6}()[]"), TreeParseError);         // два корня
    CHECK_THROWS(Tree::FromString("{abc}()[]"), TreeParseError);              // не число
    CHECK_THROWS(Tree::FromString("{12abc}()[]"), TreeParseError);            // не целиком число
    CHECK_THROWS(Tree::FromString("{99999999999}()[]"), TreeParseError);      // не влезает в int
    CHECK_THROWS(Tree::FromString("5()[]"), TreeParseError);                  // нет скобок у значения
    CHECK_THROWS(Tree::FromString("{5}[]()"), TreeParseError);                // перепутаны поддеревья
    CHECK_THROWS(Tree::FromString("{5}({3}()[]{4}()[])[]"), TreeParseError);  // два узла в одном месте
    CHECK_THROWS(Tree::FromString(" {5}()[]"), TreeParseError);               // пробел в начале
    CHECK_THROWS(Tree::FromString("{5"), TreeParseError);                     // оборвано
    CHECK_THROWS(Tree::FromString("{5\\"), TreeParseError);                   // оборвано экранирование
    // TreeParseError - это std::invalid_argument
    CHECK_THROWS(Tree::FromString("{"), std::invalid_argument);
}

// Строка правильная по скобкам, но описывает не дерево поиска
void TestNotSearchTree() {
    using Tree = BinarySearchTree<int>;
    CHECK_THROWS(Tree::FromString("{5}({8}()[])[]"), TreeParseError);         // 8 слева от 5
    CHECK_THROWS(Tree::FromString("{5}()[{2}()[]]"), TreeParseError);         // 2 справа от 5
    CHECK_THROWS(Tree::FromString("{5}({5}()[])[]"), TreeParseError);         // повтор
    CHECK_THROWS(Tree::FromString("{5}({3}()[{7}()[]])[]"), TreeParseError);  // внук 7 > деда 5
}

// Позиция ошибки - номер байта, где начинается неверное место
void TestParseErrorPosition() {
    CHECK(ParseErrorPosition("{5}({8}()[])[]") == 5);   // значение 8
    CHECK(ParseErrorPosition("{5}()[]x") == 7);          // лишний символ
    CHECK(ParseErrorPosition("{5}({3}()[]") == 11);     // строка кончилась
    CHECK(ParseErrorPosition("{5}({3}()[])[]") == -1);  // ошибки нет

    try {
        BinarySearchTree<int>::FromString("{5}({8}()[])[]");
        CHECK(false);
    } catch (const TreeParseError& error) {
        CHECK(std::string(error.what()).find("position 5") != std::string::npos);
    }
}

// Строка от "палки" - глубокая вложенность; разбор без рекурсии
void TestDeepString() {
    const int n = 5000;
    BinarySearchTree<int> stick(Balancing::kNone);
    DynamicArray<int> values = MakeSorted(n);
    for (int value : values)
        stick.Insert(value);
    std::string text = stick.ToFormattedString();
    BinarySearchTree<int> read = BinarySearchTree<int>::FromString(text, Balancing::kNone);
    CHECK(read.GetHeight() == static_cast<std::size_t>(n));
    CHECK(read.ToFormattedString() == text);
}

} // namespace

void RunStringIoTests() {
    test::RunTest("save default format", TestSaveDefaultFormat);
    test::RunTest("save other formats", TestSaveOtherFormats);
    test::RunTest("save empty and single", TestSaveEmptyAndSingle);
    test::RunTest("format errors", TestFormatErrors);
    test::RunTest("format to order", TestFormatToOrder);
    test::RunTest("read simple", TestReadSimple);
    test::RunTest("round trip plain int", TestRoundTripPlainInt);
    test::RunTest("read into avl", TestReadIntoAvl);
    test::RunTest("round trip double", TestRoundTripDouble);
    test::RunTest("round trip string", TestRoundTripString);
    test::RunTest("escaping", TestEscaping);
    test::RunTest("parse errors", TestParseErrors);
    test::RunTest("not a search tree", TestNotSearchTree);
    test::RunTest("parse error position", TestParseErrorPosition);
    test::RunTest("deep string", TestDeepString);
}
