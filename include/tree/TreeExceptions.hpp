#pragma once

#include <cstddef> // std::size_t
#include <stdexcept>
#include <string>

// Исключения дерева. Общие исключения (IndexOutOfRange, EmptyCollection)
// берём из lab2_files/Exceptions.hpp, здесь только то, чего там нет.

// Искомого эл-та нет в дереве
class ElementNotFound : public std::runtime_error {
public:
    explicit ElementNotFound(const char* message) : std::runtime_error(message) {}
    explicit ElementNotFound(const std::string& message) : std::runtime_error(message) {}
};

// Обход по прошивке вызван, когда дерево не прошито (или прошивка устарела
// после изменения дерева). Это ошибка логики вызывающего кода, поэтому logic_error.
class TreeNotThreaded : public std::logic_error {
public:
    explicit TreeNotThreaded(const char* message) : std::logic_error(message) {}
    explicit TreeNotThreaded(const std::string& message) : std::logic_error(message) {}
};

// Неверная строка формата сохранения, например "{К}(К)[П]" или "{К}(Л)".
class TreeFormatError : public std::invalid_argument {
public:
    explicit TreeFormatError(const std::string& message) : std::invalid_argument(message) {}
};

// Строку с деревом не удалось прочитать. Хранит позицию (номер байта от 0),
// на которой разбор остановился, и дописывает её в текст сообщения.
class TreeParseError : public std::invalid_argument {
public:
    TreeParseError(const std::string& message, std::size_t position)
        : std::invalid_argument(message + " (position " + std::to_string(position) + ")"),
          position_(position) {}

    std::size_t GetPosition() const { return position_; }

private:
    std::size_t position_;
};
