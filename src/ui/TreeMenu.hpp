#pragma once

#include <chrono>
#include <cstddef>
#include <exception>
#include <iostream>
#include <random>
#include <string>

#include "tree/BinarySearchTree.hpp"
#include "ui/ElementTraits.hpp"
#include "ui/Input.hpp"

// ========== Меню работы с деревом ==========
// Всё, что зависит от типа (разбор ввода, функции для Map/Where/Reduce), берётся
// из ElementTraits<T>. Каждая операция выполняется внутри try/catch: исключение
// дерева (нет элемента, неверная строка, переполнение) печатается сообщением,
// а программа продолжает работать.

namespace ui {

constexpr std::size_t kStructurePrintLimit = 64;   // печать структуры после изменений
constexpr std::size_t kStructureForceLimit = 2000; // печать структуры по запросу
constexpr std::size_t kValuesPrintLimit = 100;     // сколько значений обхода показывать
constexpr std::size_t kTextPrintLimit = 4000;      // сколько байт строки-дерева показывать
constexpr std::size_t kMaxAutoFill = 10'000'000;
// Построение BST из монотонного ввода занимает O(n^2).
// Ограничиваем такую автогенерацию, чтобы меню оставалось отзывчивым.
constexpr std::size_t kMaxDegenerateFill = 5000;

inline const char* BalancingName(Balancing balancing) {
    return balancing == Balancing::kAvl ? "AVL" : "без балансировки";
}

template <class T>
class TreeMenu {
public:
    explicit TreeMenu(Balancing balancing) : tree_(balancing), rng_(std::random_device{}()) {}

    void Run() {
        while (true) {
            std::cout << "\n";
            PrintStatus();
            PrintMenu();
            int choice = AskChoice(static_cast<int>(kItemCount));
            if (choice == 0)
                return;
            std::cout << "\n";
            try {
                (this->*Items()[choice - 1].action)();
            } catch (const std::exception& error) {
                std::cout << "Ошибка: " << error.what() << "\n";
            }
        }
    }

private:
    using Traits = ElementTraits<T>;
    using Tree = BinarySearchTree<T>;

    struct Item {
        const char* section; // заголовок группы; nullptr - та же группа
        const char* title;
        void (TreeMenu::*action)();
    };

    static constexpr std::size_t kItemCount = 22;

    static const Item* Items() {
        static const Item items[kItemCount] = {
            {"Заполнение", "Вставить элемент", &TreeMenu::InsertOne},
            {nullptr, "Вставить N элементов (случайные / по возрастанию / по убыванию)", &TreeMenu::FillAuto},
            {nullptr, "Прочитать дерево из строки по формату (заменит текущее)", &TreeMenu::ReadFromString},
            {nullptr, "Удалить элемент", &TreeMenu::RemoveOne},
            {nullptr, "Очистить дерево", &TreeMenu::ClearTree},
            {"Поиск", "Найти элемент", &TreeMenu::FindOne},
            {nullptr, "Минимум и максимум", &TreeMenu::ShowMinMax},
            {nullptr, "Элемент по номеру в порядке возрастания: Get(i)", &TreeMenu::GetByIndex},
            {nullptr, "Номер элемента: IndexOf(x)", &TreeMenu::IndexOf},
            {"Обход и печать", "Показать структуру дерева", &TreeMenu::ShowStructure},
            {nullptr, "Обход в заданном порядке (КЛП, ЛКП, ПКЛ, ... или NLR, LNR, ...)", &TreeMenu::TraverseInOrder},
            {nullptr, "Прошить дерево по выбранному обходу", &TreeMenu::ThreadTree},
            {nullptr, "Обход по прошивке (весь или с заданного элемента)", &TreeMenu::TraverseThreaded},
            {"Map, Where, Reduce", "Map - применить функцию к каждому элементу", &TreeMenu::DoMap},
            {nullptr, "Where - оставить элементы по условию", &TreeMenu::DoWhere},
            {nullptr, "Reduce - свернуть дерево в одно значение", &TreeMenu::DoReduce},
            {"Строки и поддеревья", "Сохранить в строку по формату (например {К}(Л)[П], [П]{К}(Л))", &TreeMenu::SaveToString},
            {nullptr, "Извлечь поддерево с корнем в элементе", &TreeMenu::Extract},
            {nullptr, "Найти поддерево (образец вводится строкой)", &TreeMenu::FindSubtree},
            {"Два дерева", "Слить с другим деревом (+=)", &TreeMenu::MergeWith},
            {nullptr, "Сравнить с другим деревом (==)", &TreeMenu::CompareWith},
            {"Проверка", "Проверить корректность дерева (IsValid)", &TreeMenu::CheckValid},
        };
        return items;
    }

    // ---------- Печать ----------

    void PrintStatus() const {
        std::cout << "[" << Traits::Name() << " | " << BalancingName(tree_.GetBalancing())
                  << " | элементов: " << tree_.GetCount() << " | высота: " << tree_.GetHeight()
                  << " | прошивка: " << (tree_.IsThreaded() ? ToString(tree_.GetThreadOrder()) : "нет")
                  << "]\n";
    }

    static void PrintMenu() {
        const Item* items = Items();
        for (std::size_t i = 0; i < kItemCount; ++i) {
            if (items[i].section)
                std::cout << " " << items[i].section << "\n";
            std::cout << (i + 1 < 10 ? "   " : "  ") << (i + 1) << ". " << items[i].title << "\n";
        }
        std::cout << "   0. Назад (выбор типа)\n";
    }

    // После изменений печатаем небольшие деревья, для больших показываем сводку.
    static void PrintTree(const Tree& tree) {
        if (tree.GetCount() <= kStructurePrintLimit) {
            tree.PrintStructure(std::cout);
        } else {
            std::cout << "(дерево большое: " << tree.GetCount() << " элементов, высота " << tree.GetHeight()
                      << "; структура - пункт 10)\n";
        }
    }

    // Печатает значения через запятую, но не больше kValuesPrintLimit штук;
    // остальные только считает. Передаётся в Traverse / TraverseThreaded как функция.
    class LimitedPrinter {
    public:
        void operator()(const T& value) {
            if (printed_ < kValuesPrintLimit)
                std::cout << (printed_ == 0 ? "" : ", ") << value;
            ++printed_;
        }
        void Finish() const {
            if (printed_ > kValuesPrintLimit)
                std::cout << ", ... (ещё " << printed_ - kValuesPrintLimit << ")";
            std::cout << (printed_ == 0 ? "(пусто)\n" : "\n");
        }

    private:
        std::size_t printed_ = 0;
    };

    static void PrintValues(const Tree& tree, TraversalOrder order) {
        LimitedPrinter printer;
        // Traverse принимает функцию по значению - передаём лямбду со ссылкой на printer,
        // иначе считала бы копия, а Finish увидел бы 0
        tree.Traverse(order, [&printer](const T& value) { printer(value); });
        printer.Finish();
    }

    // Результат Map / Where / ExtractSubtree: показать и предложить сделать текущим
    void OfferResult(Tree& result) {
        std::cout << "Результат (элементов: " << result.GetCount() << "):\n";
        PrintTree(result);
        std::cout << "По возрастанию: ";
        PrintValues(result, TraversalOrder::kLNR);
        if (AskYesNo("Сделать результат текущим деревом?")) {
            tree_.Swap(result);
            std::cout << "Готово.\n";
        }
    }

    // ---------- Ввод ----------

    static T AskValue() {
        return AskParsed<T>(std::string("Элемент (") + Traits::ValueHint() + "): ", &Traits::Parse);
    }

    static T AskKey(const std::string& what = "Элемент") {
        return AskParsed<T>(what + " (" + Traits::KeyHint() + "): ", &Traits::ParseKey);
    }

    // Пустая строка - значение по умолчанию
    static TraversalOrder AskOrder(TraversalOrder default_order = TraversalOrder::kLNR) {
        while (true) {
            std::string line = AskLine("Порядок обхода (КЛП КПЛ ЛКП ЛПК ПКЛ ПЛК или NLR ...; Enter = " +
                                       ToString(default_order) + "): ");
            if (line.empty())
                return default_order;
            try {
                return ParseTraversalOrder(line);
            } catch (const std::invalid_argument&) {
                std::cout << "  Нет такого порядка. Нужны три разные буквы из К, Л, П (или N, L, R).\n";
            }
        }
    }

    static std::string AskTreeFormat() {
        for (;;) {
            std::string format = AskLine("Формат из групп К, Л, П (Enter = " + kDefaultTreeFormat + "): ");
            if (format.empty())
                return kDefaultTreeFormat;
            try {
                ParseTreeFormat(format);
                return format;
            } catch (const TreeFormatError& error) {
                std::cout << "  Ошибка формата: " << error.what() << "\n";
            }
        }
    }

    static Tree AskTree(const std::string& what, Balancing balancing) {
        const std::string format = AskTreeFormat();
        if (format == kDefaultTreeFormat)
            std::cout << "Пример: " << Traits::ExampleTree() << "\n";
        while (true) {
            std::string line = AskLine(what + ": ");
            try {
                return Tree::FromString(line, format, balancing);
            } catch (const TreeParseError& error) {
                std::cout << "  Ошибка в строке: " << error.what() << "\n";
            }
        }
    }

    template <class Entry>
    static const Entry& AskEntry(const char* title, EntryList<Entry> list) {
        std::cout << title << ":\n";
        for (std::size_t i = 0; i < list.count; ++i)
            std::cout << "  " << (i + 1) << ". " << list.data[i].name << "\n";
        long long choice = AskInteger("> ", 1, static_cast<long long>(list.count));
        return list.data[choice - 1];
    }

    // ---------- Заполнение ----------

    void InsertOne() {
        T value = AskValue();
        std::cout << (tree_.Insert(value) ? "Добавлен.\n" : "Такой элемент уже есть - дерево не изменилось.\n");
        PrintTree(tree_);
    }

    void FillAuto() {
        std::cout << "Как заполнять:\n  1. Случайные значения\n  2. По возрастанию\n  3. По убыванию\n";
        int mode = static_cast<int>(AskInteger("> ", 1, 3));
        std::size_t max_count = kMaxAutoFill;
        if (mode != 1 && tree_.GetBalancing() == Balancing::kNone) {
            // Худший случай: новая "палка" растёт от самого глубокого листа,
            // поэтому итоговая высота может стать height + N
            std::size_t height = tree_.GetHeight();
            max_count = height < kMaxDegenerateFill ? kMaxDegenerateFill - height : 0;
            std::cout << "Без балансировки отсортированный вход превращает дерево в \"палку\":\n"
                         "построение требует квадратичного времени. Высота после автозаполнения\n"
                         "не должна превысить "
                      << kMaxDegenerateFill << ".\n";
            if (max_count == 0) {
                std::cout << "Дерево уже слишком высокое. Очистите его или заполняйте случайными значениями.\n";
                return;
            }
        }
        std::size_t n = AskCount("Сколько элементов (до " + std::to_string(max_count) + "): ", max_count);

        auto start = std::chrono::steady_clock::now();
        std::size_t added = 0;
        if (mode == 1) {
            // Повторы не вставляются, поэтому пробуем, пока не добавится n новых.
            // Диапазон случайных значений в ~10 раз больше n, так что попыток немного;
            // ограничение попыток - защита от вечного цикла.
            for (std::size_t attempts = 0; added < n && attempts < 20 * n + 100; ++attempts) {
                if (tree_.Insert(Traits::Random(rng_, n)))
                    ++added;
            }
        } else {
            for (std::size_t i = 0; i < n; ++i) {
                if (tree_.Insert(Traits::Sequential(mode == 2 ? i : n - 1 - i)))
                    ++added;
            }
        }
        double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        std::cout << "Добавлено " << added << " из " << n << " за " << ms << " мс"
                  << (added < n ? " (остальные уже были в дереве)" : "") << ".\n";
        PrintTree(tree_);
    }

    void ReadFromString() {
        Tree read = AskTree("Строка (Enter - пустое дерево)", tree_.GetBalancing());
        tree_.Swap(read);
        std::cout << "Прочитано элементов: " << tree_.GetCount() << ".\n";
        if (tree_.GetBalancing() == Balancing::kAvl)
            std::cout << "(Режим AVL: элементы вставлены заново с балансировкой, форма может отличаться от строки.)\n";
        PrintTree(tree_);
    }

    void RemoveOne() {
        T key = AskKey();
        std::cout << (tree_.Remove(key) ? "Удалён.\n" : "Такого элемента нет - дерево не изменилось.\n");
        PrintTree(tree_);
    }

    void ClearTree() {
        tree_.Clear();
        std::cout << "Дерево очищено.\n";
    }

    // ---------- Поиск ----------

    void FindOne() {
        T key = AskKey();
        if (tree_.Contains(key))
            std::cout << "Найден: " << tree_.Find(key) << "\n";
        else
            std::cout << "Не найден.\n";
    }

    void ShowMinMax() {
        if (tree_.IsEmpty()) {
            std::cout << "Дерево пустое.\n";
            return;
        }
        std::cout << "Минимум:  " << tree_.GetMin() << "\nМаксимум: " << tree_.GetMax() << "\n";
    }

    void GetByIndex() {
        if (tree_.IsEmpty()) {
            std::cout << "Дерево пустое.\n";
            return;
        }
        std::size_t index = AskCount("Номер i (от 0 до " + std::to_string(tree_.GetCount() - 1) + "): ",
                                     tree_.GetCount() - 1);
        std::cout << "Get(" << index << ") = " << tree_.Get(index) << "\n";
    }

    void IndexOf() {
        T key = AskKey();
        std::size_t index = tree_.IndexOf(key);
        if (index == Tree::kNotFound)
            std::cout << "Элемента нет в дереве.\n";
        else
            std::cout << "IndexOf = " << index << " (столько элементов меньше него)\n";
    }

    // ---------- Обход и печать ----------

    void ShowStructure() {
        const std::size_t count = tree_.GetCount();
        if (count > kStructureForceLimit) {
            std::cout << "Дерево слишком большое для печати (" << count << " элементов, предел "
                      << kStructureForceLimit << ").\n";
            return;
        }
        if (count > kStructurePrintLimit &&
            !AskYesNo("В дереве " + std::to_string(count) + " элементов. Печатать структуру?"))
            return;
        tree_.PrintStructure(std::cout);
        std::cout << "Элементы (operator<<): ";
        if (count <= kValuesPrintLimit)
            std::cout << tree_ << "\n";
        else
            PrintValues(tree_, TraversalOrder::kLNR);
    }

    void TraverseInOrder() {
        TraversalOrder order = AskOrder();
        std::cout << ToString(order) << ": ";
        PrintValues(tree_, order);
    }

    void ThreadTree() {
        TraversalOrder order = AskOrder();
        tree_.Thread(order);
        std::cout << "Дерево прошито в порядке " << ToString(order)
                  << ". Прошивка сбросится при любом изменении дерева.\n";
    }

    void TraverseThreaded() {
        if (!tree_.IsThreaded()) {
            std::cout << "Дерево не прошито (или изменилось после прошивки). Сначала пункт 12.\n";
            return;
        }
        LimitedPrinter printer;
        auto print = [&printer](const T& value) { printer(value); };
        std::cout << "Прошивка " << ToString(tree_.GetThreadOrder()) << ".\n";
        if (AskYesNo("Начать с заданного элемента?")) {
            T key = AskKey("С какого элемента");
            tree_.TraverseThreadedFrom(key, print);
        } else {
            tree_.TraverseThreaded(print);
        }
        printer.Finish();
    }

    // ---------- Map, Where, Reduce ----------

    void DoMap() {
        const MapEntry<T>& entry = AskEntry("Функция", Traits::Maps());
        Tree result = tree_.Map(entry.func);
        std::cout << "Map(" << entry.name << "): элементов было " << tree_.GetCount() << ", стало "
                  << result.GetCount() << ".\n";
        OfferResult(result);
    }

    void DoWhere() {
        EntryList<WhereEntry<T>> list = Traits::Wheres();
        std::cout << "Условие:\n";
        for (std::size_t i = 0; i < list.count; ++i)
            std::cout << "  " << (i + 1) << ". " << list.data[i].name << "\n";
        // Последний пункт общий для всех типов: нужен только operator<
        std::cout << "  " << (list.count + 1) << ". в диапазоне от a до b включительно\n";
        long long choice = AskInteger("> ", 1, static_cast<long long>(list.count) + 1);

        Tree result(tree_.GetBalancing());
        if (static_cast<std::size_t>(choice) <= list.count) {
            result = tree_.Where(list.data[choice - 1].predicate);
        } else {
            T low = AskKey("a");
            T high = AskKey("b");
            if (high < low)
                throw std::invalid_argument("нижняя граница больше верхней");
            // a <= x <= b, записанное только через <:  !(x < a) && !(b < x)
            result = tree_.Where([&low, &high](const T& x) { return !(x < low) && !(high < x); });
        }
        OfferResult(result);
    }

    void DoReduce() {
        const ReduceEntry<T>& entry = AskEntry("Свёртка", Traits::Reduces());
        TraversalOrder order = AskOrder();
        std::cout << "Reduce(" << entry.name << ", " << ToString(order) << ") = " << entry.run(tree_, order) << "\n";
    }

    // ---------- Строки и поддеревья ----------

    void SaveToString() {
        const std::string text = tree_.ToFormattedString(AskTreeFormat());
        if (text.size() <= kTextPrintLimit ||
            AskYesNo("Строка длинная. Показать её целиком?")) {
            std::cout << text << '\n';
        }
    }

    void Extract() {
        T key = AskKey("Корень поддерева");
        Tree result = tree_.ExtractSubtree(key);
        OfferResult(result);
    }

    void FindSubtree() {
        std::cout << "Образец читается без балансировки: его форма - ровно как в строке.\n";
        Tree pattern = AskTree("Образец", Balancing::kNone);
        std::cout << (tree_.ContainsSubtree(pattern) ? "Да, такое поддерево есть.\n" : "Нет, такого поддерева нет.\n");
    }

    // ---------- Два дерева ----------

    void MergeWith() {
        Tree other = AskTree("Второе дерево", tree_.GetBalancing());
        std::size_t added = tree_.Merge(other);
        std::cout << "Добавлено новых элементов: " << added << ".\n";
        PrintTree(tree_);
    }

    void CompareWith() {
        Tree other = AskTree("Второе дерево", Balancing::kNone);
        bool equal = tree_ == other;
        std::cout << (equal ? "Равны" : "Не равны")
                  << " (== сравнивает множества элементов, форма не важна).\n";
        if (equal) {
            const bool same_shape = tree_.ContainsSubtree(other);
            std::cout << (same_shape ? "Форма тоже совпадает.\n" : "Форма при этом разная.\n");
        }
    }

    // ---------- Проверка ----------

    void CheckValid() {
        if (!tree_.IsValid()) {
            std::cout << "ДЕРЕВО ПОВРЕЖДЕНО: нарушен порядок, размеры или баланс.\n";
            return;
        }
        std::cout << "Дерево корректно: порядок дерева поиска, высоты и размеры поддеревьев"
                  << (tree_.GetBalancing() == Balancing::kAvl ? ", баланс AVL" : "") << " в порядке.\n";
    }

    Tree tree_;
    std::mt19937 rng_;
};

template <class T>
void RunTreeMenu(Balancing balancing) {
    TreeMenu<T> menu(balancing);
    menu.Run();
}

} // namespace ui
