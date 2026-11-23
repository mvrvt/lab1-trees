#pragma once

#include <algorithm> // std::min
#include <cmath>     // std::round, std::fabs
#include <cstddef>
#include <limits>
#include <random>    // std::mt19937, распределения
#include <stdexcept>
#include <string>

#include "tree/BinarySearchTree.hpp"
#include "tree/ValueIo.hpp"
#include "types/Person.hpp"

// ========== Всё, что меню должно знать о типе элементов ==========
// Меню (TreeMenu.hpp) написано один раз для любого T. Особенности конкретного типа
// собраны здесь, в специализациях ElementTraits<T>:
//   Name, ValueHint, KeyHint, ExampleTree - подсказки для пользователя;
//   Parse / ParseKey   - разбор введённой строки (ключ для Student - это только id);
//   Random / Sequential - значения для автозаполнения;
//   Maps / Wheres / Reduces - готовые функции для Map, Where, Reduce
//     (вводить код с клавиатуры нельзя, поэтому выбор из списка).
//
// Функции хранятся как обычные указатели на функции. Лямбда без захвата
// неявно превращается в такой указатель, поэтому списки можно писать лямбдами.

namespace ui {

template <class T>
struct MapEntry {
    const char* name;
    T (*func)(const T&);
};

template <class T>
struct WhereEntry {
    const char* name;
    bool (*predicate)(const T&);
};

// Reduce возвращает значения разных типов (сумма - long long, сцепка - строка),
// поэтому функция сразу считает свёртку и возвращает результат текстом.
template <class T>
struct ReduceEntry {
    const char* name;
    std::string (*run)(const BinarySearchTree<T>& tree, TraversalOrder order);
};

// Указатель на массив + длина (стандартные контейнеры в лабораторной запрещены)
template <class Entry>
struct EntryList {
    const Entry* data;
    std::size_t count;
};

template <class Entry, std::size_t N>
EntryList<Entry> MakeList(const Entry (&entries)[N]) {
    return {entries, N};
}

// ---------- Арифметика с проверкой переполнения ----------
// Переполнение знаковых чисел в C++ - неопределённое поведение (UBSan его ловит).
// Поэтому считаем через встроенные функции GCC/Clang, которые сообщают о переполнении,
// и бросаем исключение - меню напечатает понятную ошибку.
inline long long CheckedAdd(long long a, long long b) {
    long long result = 0;
    if (__builtin_add_overflow(a, b, &result))
        throw std::overflow_error("переполнение long long при сложении");
    return result;
}

inline long long CheckedMul(long long a, long long b) {
    long long result = 0;
    if (__builtin_mul_overflow(a, b, &result))
        throw std::overflow_error("переполнение long long при умножении");
    return result;
}

inline int ToIntChecked(long long value) {
    if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
        throw std::overflow_error("результат " + std::to_string(value) + " не помещается в int");
    return static_cast<int>(value);
}

// Обёртка над ParseValue с понятным сообщением
template <class T>
T ParseOrExplain(const std::string& text, const char* expected) {
    try {
        return ParseValue<T>(text);
    } catch (const std::invalid_argument&) {
        throw std::invalid_argument(std::string("ожидалось ") + expected);
    }
}

// Количество элементов - работает для любого типа
template <class T>
std::string ReduceCount(const BinarySearchTree<T>& tree, TraversalOrder order) {
    std::size_t count = tree.Reduce([](const T&, std::size_t acc) { return acc + 1; },
                                    std::size_t{0}, order);
    return std::to_string(count);
}

// Сцепить все элементы в одну строку в порядке обхода - тоже для любого типа.
// Хорошо видно, что порядок обхода влияет на результат Reduce.
template <class T>
std::string ReduceJoin(const BinarySearchTree<T>& tree, TraversalOrder order) {
    return "\"" +
           tree.Reduce(
               [](const T& value, std::string acc) {
                   return acc.empty() ? ToText(value) : acc + "; " + ToText(value);
               },
               std::string(), order) +
           "\"";
}

template <class T>
struct ElementTraits; // общий шаблон не определён: для нового типа нужна специализация

// =============================== int ===============================
template <>
struct ElementTraits<int> {
    static const char* Name() { return "int"; }
    static const char* ValueHint() { return "целое число"; }
    static const char* KeyHint() { return ValueHint(); }
    static const char* ExampleTree() { return "{5}({3}({1}()[])[{4}()[]])[{8}()[{9}()[]]]"; }

    static int Parse(const std::string& text) { return ParseOrExplain<int>(text, ValueHint()); }
    static int ParseKey(const std::string& text) { return Parse(text); }

    // Диапазон в 10 раз больше n - повторов мало, и n разных чисел точно найдутся
    static int Random(std::mt19937& rng, std::size_t n) {
        long long upper = std::min<long long>(10LL * static_cast<long long>(n) + 100,
                                              std::numeric_limits<int>::max());
        std::uniform_int_distribution<long long> dist(1, upper);
        return static_cast<int>(dist(rng));
    }
    static int Sequential(std::size_t i) { return ToIntChecked(static_cast<long long>(i) + 1); }

    static EntryList<MapEntry<int>> Maps() {
        static const MapEntry<int> entries[] = {
            {"x * 2", [](const int& x) { return ToIntChecked(2LL * x); }},
            {"-x (порядок элементов разворачивается)", [](const int& x) { return ToIntChecked(-static_cast<long long>(x)); }},
            {"x % 10 (одинаковые результаты сольются)", [](const int& x) { return x % 10; }},
            {"x / 2 (целочисленно)", [](const int& x) { return x / 2; }},
            {"x + 1", [](const int& x) { return ToIntChecked(static_cast<long long>(x) + 1); }},
        };
        return MakeList(entries);
    }
    static EntryList<WhereEntry<int>> Wheres() {
        static const WhereEntry<int> entries[] = {
            {"чётные", [](const int& x) { return x % 2 == 0; }},
            {"нечётные", [](const int& x) { return x % 2 != 0; }},
            {"положительные", [](const int& x) { return x > 0; }},
            {"делятся на 3", [](const int& x) { return x % 3 == 0; }},
        };
        return MakeList(entries);
    }
    static EntryList<ReduceEntry<int>> Reduces() {
        static const ReduceEntry<int> entries[] = {
            {"сумма (long long)",
             [](const BinarySearchTree<int>& tree, TraversalOrder order) {
                 return std::to_string(tree.Reduce(
                     [](const int& x, long long acc) { return CheckedAdd(acc, x); }, 0LL, order));
             }},
            {"максимум",
             [](const BinarySearchTree<int>& tree, TraversalOrder order) {
                 return std::to_string(tree.Reduce(
                     [](const int& x, int acc) { return x > acc ? x : acc; },
                     std::numeric_limits<int>::min(), order));
             }},
            {"формула из ТЗ: f(x, acc) = 2x + 3acc, начальное значение 4",
             [](const BinarySearchTree<int>& tree, TraversalOrder order) {
                 return std::to_string(tree.Reduce(
                     [](const int& x, long long acc) { return CheckedAdd(2LL * x, CheckedMul(3, acc)); },
                     4LL, order));
             }},
            {"количество элементов", ReduceCount<int>},
            {"сцепить в строку", ReduceJoin<int>},
        };
        return MakeList(entries);
    }
};

// =============================== double ===============================
template <>
struct ElementTraits<double> {
    static const char* Name() { return "double"; }
    static const char* ValueHint() { return "вещественное число, например 3.14 или -2e5"; }
    static const char* KeyHint() { return ValueHint(); }
    static const char* ExampleTree() { return "{2.5}({-1}()[])[{10.75}()[]]"; }

    static double Parse(const std::string& text) { return ParseOrExplain<double>(text, ValueHint()); }
    static double ParseKey(const std::string& text) { return Parse(text); }

    // Два знака после запятой, диапазон растёт вместе с n
    static double Random(std::mt19937& rng, std::size_t n) {
        std::uniform_int_distribution<long long> dist(-500LL * static_cast<long long>(n) - 1000,
                                                      500LL * static_cast<long long>(n) + 1000);
        return static_cast<double>(dist(rng)) / 100.0;
    }
    static double Sequential(std::size_t i) { return (static_cast<double>(i) + 1.0) / 2.0; }

    static EntryList<MapEntry<double>> Maps() {
        static const MapEntry<double> entries[] = {
            {"x * 2", [](const double& x) { return x * 2.0; }},
            {"-x (порядок элементов разворачивается)", [](const double& x) { return -x; }},
            {"|x| (модуль)", [](const double& x) { return std::fabs(x); }},
            {"округлить до целого (одинаковые сольются)", [](const double& x) { return std::round(x); }},
        };
        return MakeList(entries);
    }
    static EntryList<WhereEntry<double>> Wheres() {
        static const WhereEntry<double> entries[] = {
            {"положительные", [](const double& x) { return x > 0.0; }},
            {"отрицательные", [](const double& x) { return x < 0.0; }},
            {"целые", [](const double& x) { return x == std::round(x); }},
        };
        return MakeList(entries);
    }
    static EntryList<ReduceEntry<double>> Reduces() {
        static const ReduceEntry<double> entries[] = {
            {"сумма",
             [](const BinarySearchTree<double>& tree, TraversalOrder order) {
                 return ToText(tree.Reduce([](const double& x, double acc) { return acc + x; }, 0.0, order));
             }},
            {"среднее",
             [](const BinarySearchTree<double>& tree, TraversalOrder order) -> std::string {
                 if (tree.IsEmpty())
                     return "нет элементов";
                 double sum = tree.Reduce([](const double& x, double acc) { return acc + x; }, 0.0, order);
                 return ToText(sum / static_cast<double>(tree.GetCount()));
             }},
            {"количество элементов", ReduceCount<double>},
            {"сцепить в строку", ReduceJoin<double>},
        };
        return MakeList(entries);
    }
};

// =============================== std::string ===============================
// Длина первого символа UTF-8 по его первому байту (кириллица - 2 байта)
inline std::size_t Utf8CharLength(unsigned char lead) {
    if (lead < 0x80) return 1;
    if ((lead >> 5) == 0x6) return 2;
    if ((lead >> 4) == 0xE) return 3;
    if ((lead >> 3) == 0x1E) return 4;
    return 1; // неверный байт - берём как есть
}

template <>
struct ElementTraits<std::string> {
    static const char* Name() { return "string"; }
    static const char* ValueHint() { return "строка (вся введённая строка целиком)"; }
    static const char* KeyHint() { return ValueHint(); }
    static const char* ExampleTree() { return "{кот}({арбуз}()[])[{слон}()[{яблоко}()[]]]"; }

    static std::string Parse(const std::string& text) { return text; }
    static std::string ParseKey(const std::string& text) { return text; }

    static std::string Random(std::mt19937& rng, std::size_t) {
        std::uniform_int_distribution<int> length(3, 8);
        std::uniform_int_distribution<int> letter('a', 'z');
        std::string word(static_cast<std::size_t>(length(rng)), ' ');
        for (char& symbol : word)
            symbol = static_cast<char>(letter(rng));
        return word;
    }
    // "s00000001", "s00000002", ... - строки идут по возрастанию, как и числа
    static std::string Sequential(std::size_t i) {
        std::string digits = std::to_string(i + 1);
        return "s" + std::string(digits.size() < 8 ? 8 - digits.size() : 0, '0') + digits;
    }

    static EntryList<MapEntry<std::string>> Maps() {
        static const MapEntry<std::string> entries[] = {
            {"в верхний регистр (латиница)",
             [](const std::string& s) {
                 std::string result = s;
                 for (char& symbol : result)
                     if (symbol >= 'a' && symbol <= 'z')
                         symbol = static_cast<char>(symbol - 'a' + 'A');
                 return result;
             }},
            {"дописать \"!\" в конец", [](const std::string& s) { return s + "!"; }},
            {"первая буква (одинаковые сольются)",
             [](const std::string& s) {
                 if (s.empty())
                     return s;
                 return s.substr(0, Utf8CharLength(static_cast<unsigned char>(s[0])));
             }},
        };
        return MakeList(entries);
    }
    static EntryList<WhereEntry<std::string>> Wheres() {
        static const WhereEntry<std::string> entries[] = {
            {"длина не больше 4 байт", [](const std::string& s) { return s.size() <= 4; }},
            {"содержат букву 'a' (латинскую)", [](const std::string& s) { return s.find('a') != std::string::npos; }},
        };
        return MakeList(entries);
    }
    static EntryList<ReduceEntry<std::string>> Reduces() {
        static const ReduceEntry<std::string> entries[] = {
            {"суммарная длина в байтах",
             [](const BinarySearchTree<std::string>& tree, TraversalOrder order) {
                 return std::to_string(tree.Reduce(
                     [](const std::string& s, std::size_t acc) { return acc + s.size(); }, std::size_t{0}, order));
             }},
            {"самая длинная строка",
             [](const BinarySearchTree<std::string>& tree, TraversalOrder order) {
                 return "\"" + tree.Reduce(
                     [](const std::string& s, std::string acc) { return s.size() > acc.size() ? s : acc; },
                     std::string(), order) + "\"";
             }},
            {"количество элементов", ReduceCount<std::string>},
            {"сцепить в строку", ReduceJoin<std::string>},
        };
        return MakeList(entries);
    }
};

// =============================== Student и Teacher ===============================
// Общая часть для обоих типов: P - Student или Teacher
template <class P>
struct PersonTraitsBase {
    static const char* KeyHint() { return "id (целое число >= 0)"; }

    static P Parse(const std::string& text) {
        return ParseOrExplain<P>(text, ElementTraits<P>::ValueHint());
    }
    // Для поиска и удаления достаточно id: дерево сравнивает только его
    static P ParseKey(const std::string& text) {
        return P(ToIntChecked(ParseOrExplain<long long>(text, KeyHint())));
    }

    static P Make(int id, std::mt19937& rng) {
        // Все фамилии кончаются на -ов / -ев: женская форма - та же фамилия + "а"
        static const char* last_names[] = {"Иванов", "Петров", "Сидоров", "Кузнецов", "Смирнов",
                                           "Попов", "Волков", "Орлов", "Лебедев", "Козлов"};
        static const char* male_names[] = {"Иван", "Пётр", "Олег", "Дмитрий", "Сергей"};
        static const char* female_names[] = {"Анна", "Мария", "Елена", "Ольга", "Алина"};
        std::uniform_int_distribution<int> pick_last(0, 9);
        std::uniform_int_distribution<int> pick_first(0, 4);
        std::uniform_int_distribution<int> extra(0, 3);
        std::string last_name = last_names[pick_last(rng)];
        bool female = std::uniform_int_distribution<int>(0, 1)(rng) == 1;
        std::string first_name = female ? female_names[pick_first(rng)] : male_names[pick_first(rng)];
        if (female)
            last_name += "а";
        return P(id, last_name, first_name, ElementTraits<P>::kTypicalBirthYear + extra(rng),
                 ElementTraits<P>::Extras()[extra(rng)]);
    }
    static P Random(std::mt19937& rng, std::size_t n) {
        std::uniform_int_distribution<long long> id(1, 10LL * static_cast<long long>(n) + 100);
        return Make(ToIntChecked(id(rng)), rng);
    }
    static P Sequential(std::size_t i) {
        static std::mt19937 rng(2024); // имена случайные, id - по порядку
        return Make(ToIntChecked(static_cast<long long>(i) + 1), rng);
    }

    static EntryList<MapEntry<P>> Maps() {
        static const MapEntry<P> entries[] = {
            {"id + 1000 (перенумеровать)",
             [](const P& p) { P r = p; r.id = ToIntChecked(static_cast<long long>(p.id) + 1000); return r; }},
            {"id % 100 (записи с одинаковым результатом сольются)",
             [](const P& p) { P r = p; r.id = p.id % 100; return r; }},
            {"год рождения + 1 (порядок не меняется: он только по id)",
             [](const P& p) { P r = p; ++r.birth_year; return r; }},
        };
        return MakeList(entries);
    }
    static EntryList<WhereEntry<P>> Wheres() {
        static const WhereEntry<P> entries[] = {
            {"чётный id", [](const P& p) { return p.id % 2 == 0; }},
            {"родились не раньше типичного года + 2",
             [](const P& p) { return p.birth_year >= ElementTraits<P>::kTypicalBirthYear + 2; }},
        };
        return MakeList(entries);
    }
    static EntryList<ReduceEntry<P>> Reduces() {
        static const ReduceEntry<P> entries[] = {
            {"самый ранний год рождения",
             [](const BinarySearchTree<P>& tree, TraversalOrder order) {
                 return std::to_string(tree.Reduce(
                     [](const P& p, int acc) { return p.birth_year < acc ? p.birth_year : acc; },
                     Person::kMaxBirthYear, order));
             }},
            {"фамилии через запятую",
             [](const BinarySearchTree<P>& tree, TraversalOrder order) {
                 return tree.Reduce(
                     [](const P& p, std::string acc) { return acc.empty() ? p.last_name : acc + ", " + p.last_name; },
                     std::string(), order);
             }},
            {"количество записей", ReduceCount<P>},
        };
        return MakeList(entries);
    }
};

template <>
struct ElementTraits<Student> : PersonTraitsBase<Student> {
    static constexpr int kTypicalBirthYear = 2003;
    static const char* Name() { return "Student"; }
    static const char* ValueHint() { return "id Фамилия Имя год группа, например: 1024 Иванов Иван 2005 Б23-504"; }
    static const char* ExampleTree() {
        return "{20 Петров Пётр 2005 Б23-504}({10 Иванов Иван 2004 Б23-504}()[])[{30 Орлова Анна 2006 Б24-101}()[]]";
    }
    static const char* const* Extras() {
        static const char* groups[] = {"Б23-504", "Б23-505", "Б24-101", "Б22-303"};
        return groups;
    }
};

template <>
struct ElementTraits<Teacher> : PersonTraitsBase<Teacher> {
    static constexpr int kTypicalBirthYear = 1970;
    static const char* Name() { return "Teacher"; }
    static const char* ValueHint() { return "id Фамилия Имя год кафедра, например: 17 Смирнов Алексей 1975 Кафедра-22"; }
    static const char* ExampleTree() {
        return "{17 Смирнов Алексей 1975 Кафедра-22}({3 Волкова Елена 1980 Кафедра-12}()[])[]";
    }
    static const char* const* Extras() {
        static const char* departments[] = {"Кафедра-12", "Кафедра-22", "Кафедра-36", "Кафедра-42"};
        return departments;
    }
};

} // namespace ui
