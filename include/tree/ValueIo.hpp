#pragma once

#include <cmath>       // std::isfinite
#include <limits>      // std::numeric_limits
#include <iomanip>     // std::setprecision
#include <sstream>     // std::ostringstream, std::istringstream
#include <stdexcept>   // std::invalid_argument
#include <string>
#include <type_traits> // std::is_same_v, std::is_floating_point_v

template <class T>
void RequireFiniteValue(const T& value) {
    if constexpr (std::is_floating_point_v<T>) {
        if (!std::isfinite(value))
            throw std::invalid_argument("number must be finite");
    }
}

// ========== Перевод значения в текст и обратно ==========
// Дерево не знает, как печатать и читать, например, Student: это знает сам тип
// (через operator<< и operator>>). Здесь только общий код и два особых случая:
//   std::string - берётся целиком, как есть (operator>> прочитал бы лишь одно слово);
//   double      - печатается так, чтобы при чтении получилось РОВНО то же число.
// if constexpr (C++17) выбирает ветку при компиляции: для int ветка со std::string
// даже не компилируется.

template <class T>
std::string ToText(const T& value) {
    if constexpr (std::is_same_v<T, std::string>) {
        return value;
    } else if constexpr (std::is_floating_point_v<T>) {
        RequireFiniteValue(value);
        // Сначала пробуем короткую запись. Если она округляет значение,
        // выводим столько цифр, сколько требуется конкретному типу.
        std::ostringstream short_out;
        short_out << std::setprecision(std::numeric_limits<T>::digits10) << value;
        std::istringstream in(short_out.str());
        T read_back{};
        if ((in >> read_back) && read_back == value)
            return short_out.str();
        std::ostringstream exact_out;
        exact_out << std::setprecision(std::numeric_limits<T>::max_digits10) << value;
        return exact_out.str();
    } else {
        std::ostringstream out;
        out << value;
        return out.str();
    }
}

// Обратное к ToText. Бросает std::invalid_argument, если текст не является значением
// типа T целиком: "12abc" для int - ошибка, а не 12.
template <class T>
T ParseValue(const std::string& text) {
    if constexpr (std::is_same_v<T, std::string>) {
        return text;
    } else {
        std::istringstream in(text);
        T value{};
        in >> value;
        if (in.fail())
            throw std::invalid_argument("cannot read a value from '" + text + "'");
        in >> std::ws; // пробелы в конце разрешены
        if (!in.eof())
            throw std::invalid_argument("extra characters after the value in '" + text + "'");
        RequireFiniteValue(value);
        return value;
    }
}

// Экранирование: перед special и перед самим '\' ставится '\'.
// Нужно, чтобы значение-строка могла содержать закрывающую скобку формата: "a}b" -> "a\}b".
inline std::string EscapeText(const std::string& text, char special) {
    std::string result;
    result.reserve(text.size());
    for (char symbol : text) {
        if (symbol == special || symbol == '\\')
            result += '\\';
        result += symbol;
    }
    return result;
}
