#pragma once

#include <iostream>
#include <stdexcept>
#include <string>

#include "tree/ValueIo.hpp"

// ========== Безопасный ввод с клавиатуры ==========
// Всё читается ЦЕЛЫМИ СТРОКАМИ (std::getline), а уже потом разбирается.
// Так неверный ввод ("abc" вместо числа) не ломает std::cin: строка просто
// разбирается заново, а вопрос повторяется. С `std::cin >> x` пришлось бы
// сбрасывать флаги ошибки и выкидывать остаток строки вручную.

namespace ui {

// Ввод закончился (Ctrl+D в терминале или конец файла при `./lab1 < input.txt`).
// Специально НЕ наследник std::exception: меню ловит std::exception, чтобы печатать
// ошибки дерева, и не должно случайно "проглотить" конец ввода.
struct InputClosed {};

// Напечатать подсказку и прочитать строку целиком. Конец ввода -> InputClosed.
inline std::string AskLine(const std::string& prompt) {
    std::cout << prompt;
    std::cout.flush();
    std::string line;
    if (!std::getline(std::cin, line))
        throw InputClosed{};
    // Windows-файлы заканчивают строку "\r\n": '\r' не нужен
    if (!line.empty() && line.back() == '\r')
        line.pop_back();
    return line;
}

// Непустая строка (повторяет вопрос, пока не введут хоть что-то)
inline std::string AskNonEmptyLine(const std::string& prompt) {
    while (true) {
        std::string line = AskLine(prompt);
        if (line.find_first_not_of(" \t") != std::string::npos)
            return line;
        std::cout << "  Пустой ввод, попробуйте ещё раз.\n";
    }
}

// Целое число в диапазоне [min, max]. Читаем как long long: если читать сразу
// std::size_t, то "-1" молча превратилось бы в огромное положительное число.
inline long long AskInteger(const std::string& prompt, long long min, long long max) {
    while (true) {
        std::string line = AskLine(prompt);
        try {
            long long value = ParseValue<long long>(line);
            if (value >= min && value <= max)
                return value;
            std::cout << "  Нужно число от " << min << " до " << max << ".\n";
        } catch (const std::invalid_argument&) {
            std::cout << "  Это не целое число, попробуйте ещё раз.\n";
        }
    }
}

inline std::size_t AskCount(const std::string& prompt, std::size_t max) {
    return static_cast<std::size_t>(AskInteger(prompt, 0, static_cast<long long>(max)));
}

// Пункт меню 0..max
inline int AskChoice(int max) {
    return static_cast<int>(AskInteger("> ", 0, max));
}

inline bool AskYesNo(const std::string& prompt) {
    while (true) {
        std::string line = AskLine(prompt + " (y/n): ");
        if (line == "y" || line == "Y" || line == "д" || line == "Д" || line == "да")
            return true;
        if (line == "n" || line == "N" || line == "н" || line == "Н" || line == "нет")
            return false;
        std::cout << "  Ответьте y или n.\n";
    }
}

// Значение произвольного типа через функцию разбора parse (бросает std::invalid_argument
// при ошибке). Повторяет вопрос, пока значение не прочитается.
template <class T, class ParseFunc>
T AskParsed(const std::string& prompt, ParseFunc parse) {
    while (true) {
        std::string line = AskNonEmptyLine(prompt);
        try {
            return parse(line);
        } catch (const std::invalid_argument& error) {
            std::cout << "  Неверное значение: " << error.what() << "\n";
        }
    }
}

} // namespace ui
