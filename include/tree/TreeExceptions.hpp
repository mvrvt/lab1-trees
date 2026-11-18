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
