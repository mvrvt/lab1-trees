#pragma once

#include <cstddef>

#include "lab2_files/DynamicArray.hpp"

// Общие помощники для всех файлов с тестами

namespace test {

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

// Числа 0..n-1 по возрастанию
inline DynamicArray<int> MakeSorted(int n) {
    DynamicArray<int> values;
    values.Reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
        values.Append(i);
    return values;
}

// Детерминированная "случайная" перестановка 0..n-1: при одинаковом seed
// получается одна и та же, поэтому тесты повторяемы.
// Алгоритм перемешивания Фишера-Йетса + линейный конгруэнтный генератор.
inline DynamicArray<int> MakeShuffled(int n, unsigned int seed = 12345) {
    DynamicArray<int> values = MakeSorted(n);
    for (int i = n - 1; i > 0; --i) {
        seed = seed * 1103515245u + 12345u;
        std::size_t j = seed % static_cast<unsigned int>(i + 1);
        std::size_t k = static_cast<std::size_t>(i);
        int temp = values[k];
        values[k] = values[j];
        values[j] = temp;
    }
    return values;
}

} // namespace test
