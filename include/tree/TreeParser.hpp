#pragma once

#include <cstddef>   // std::size_t
#include <stdexcept> // std::invalid_argument
#include <string>
#include <utility>   // std::move

#include "lab2_files/DynamicArray.hpp"
#include "tree/TreeExceptions.hpp"
#include "tree/ValueIo.hpp"

// ========== Чтение дерева из строки фиксированного формата "{К}(Л)[П]" ==========
// Грамматика (пустая строка = пустое поддерево):
//     Дерево := ""  |  "{" значение "}" "(" Дерево ")" "[" Дерево "]"
// Пример: дерево 5 / 3 8  ->  "{5}({3}()[])[{8}()[]]".
// Внутри {} заканчивает значение только '}', поэтому значения-строки могут содержать
// ( ) [ ] {. Сам '}' и '\' внутри значения пишутся как "\}" и "\\".
//
// Результат - значения в порядке КЛП (прямой обход). Если вставить их в этом порядке
// в пустое дерево без балансировки, получится РОВНО та же форма: каждое значение
// спускается по уже вставленным предкам туда же, где оно стоит в строке.
//
// Заодно проверяется, что строка описывает именно дерево ПОИСКА: у каждого узла есть
// допустимый интервал (lower, upper) - как в IsValid. Значение левого поддерева должно
// быть меньше узла, правого - больше, и всё вместе - в интервале предков.
// "{5}({8}()[])[]" - ошибка: 8 в левом поддереве 5.
//
// Разбор БЕЗ рекурсии (свой стек): строка от "палки" из 10^5 узлов имеет
// глубину вложенности 10^5, и рекурсивный спуск переполнил бы стек вызовов.

template <class T>
class FixedFormatParser {
public:
    explicit FixedFormatParser(const std::string& text) : text_(text) {}

    // Бросает TreeParseError (с позицией), если строка неверна
    DynamicArray<T> Parse() {
        if (text_.empty())
            return values_; // пустое дерево
        OpenNode(kNoBound, kNoBound); // корень

        while (!stack_.IsEmpty()) {
            Frame& top = stack_.GetLast();
            // Сейчас мы сразу после '(' (slot 0 - левое поддерево) или '[' (slot 1 - правое).
            // Если там '{' и поддерево ещё не читали - это узел-ребёнок.
            if (!top.slot_used && Peek() == '{') {
                top.slot_used = true; // ДО OpenNode: тот делает Append, и ссылка top станет висячей
                std::size_t lower = (top.slot == 0) ? top.lower : top.node;
                std::size_t upper = (top.slot == 0) ? top.node : top.upper;
                OpenNode(lower, upper);
                continue;
            }
            // Поддерево в этом месте закончилось (или было пустым)
            if (top.slot == 0) {
                Expect(')');
                Expect('[');
                top.slot = 1;
                top.slot_used = false;
            } else {
                Expect(']');
                stack_.RemoveLast(); // узел прочитан целиком
            }
        }
        if (position_ != text_.size())
            Fail("unexpected characters after the end of the tree", position_);
        return std::move(values_);
    }

private:
    static constexpr std::size_t kNoBound = static_cast<std::size_t>(-1);

    // Узел, который сейчас читается. Границы и сам узел - индексы в values_
    // (не указатели: при росте массива указатели на его элементы стали бы висячими).
    struct Frame {
        std::size_t node;
        std::size_t lower;   // kNoBound - границы нет
        std::size_t upper;
        int slot;            // 0 - читаем левое поддерево, 1 - правое
        bool slot_used;      // в текущем поддереве уже был узел
    };

    char Peek() const { return position_ < text_.size() ? text_[position_] : '\0'; }

    [[noreturn]] void Fail(const std::string& message, std::size_t position) const {
        throw TreeParseError(message, position);
    }

    void Expect(char symbol) {
        if (Peek() == symbol && position_ < text_.size()) {
            ++position_;
            return;
        }
        std::string found;
        if (position_ >= text_.size())
            found = "the string ended";
        else if (static_cast<unsigned char>(text_[position_]) >= 128)
            found = "a non-ASCII character";
        else
            found = std::string("'") + text_[position_] + "'";
        Fail(std::string("expected '") + symbol + "' but found " + found, position_);
    }

    // Текст значения после '{' до неэкранированной '}' (её тоже съедает)
    std::string ReadValueText() {
        std::string result;
        while (position_ < text_.size()) {
            char symbol = text_[position_];
            if (symbol == '}') {
                ++position_;
                return result;
            }
            if (symbol == '\\') {
                if (position_ + 1 >= text_.size())
                    Fail("unfinished escape sequence", position_);
                result += text_[position_ + 1];
                position_ += 2;
            } else {
                result += symbol;
                ++position_;
            }
        }
        Fail("expected '}' but the string ended", position_);
    }

    T ReadValue(const std::string& raw, std::size_t position) const {
        try {
            return ParseValue<T>(raw);
        } catch (const std::invalid_argument& error) {
            Fail(error.what(), position);
        }
    }

    // Прочитать "{значение}(" и положить узел в стек
    void OpenNode(std::size_t lower, std::size_t upper) {
        Expect('{');
        std::size_t value_position = position_;
        std::string raw = ReadValueText();
        T value = ReadValue(raw, value_position);

        // Строго больше нижней и строго меньше верхней границы (заодно ловит повторы)
        if (lower != kNoBound && !(values_[lower] < value))
            Fail("value '" + raw + "' breaks the search-tree order: it must be greater than '" +
                     ToText(values_[lower]) + "'", value_position);
        if (upper != kNoBound && !(value < values_[upper]))
            Fail("value '" + raw + "' breaks the search-tree order: it must be less than '" +
                     ToText(values_[upper]) + "'", value_position);

        values_.Append(value);
        Expect('(');
        stack_.Append(Frame{values_.GetCount() - 1, lower, upper, 0, false});
    }

    const std::string& text_;
    std::size_t position_ = 0;
    DynamicArray<T> values_;    // прочитанные значения в порядке КЛП
    DynamicArray<Frame> stack_; // узлы, которые ещё не дочитаны
};

// Значения дерева из строки "{К}(Л)[П]" в порядке КЛП. Бросает TreeParseError.
template <class T>
DynamicArray<T> ParseTreeString(const std::string& text) {
    return FixedFormatParser<T>(text).Parse();
}
