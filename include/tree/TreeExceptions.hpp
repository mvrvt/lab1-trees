#pragma once

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
