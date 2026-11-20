#pragma once

#include <cstddef>
#include <ostream>
#include <string>

#include "lab2_files/DynamicArray.hpp"

// ========== Холст из символов для печати дерева сверху вниз ==========
// Прямоугольник rows x columns, в каждой клетке - один видимый символ.
// Клетка хранит std::string, а не char: в UTF-8 буква "Ж" занимает 2 байта,
// а символы рамки "┌", "─", "┴" - по 3 байта, но на экране каждый - одна позиция.

// Сколько позиций на экране займёт строка UTF-8: считаем символы, а не байты.
// Байты-продолжения UTF-8 имеют вид 10xxxxxx, их пропускаем.
inline std::size_t DisplayWidth(const std::string& text) {
    std::size_t width = 0;
    for (char byte : text) {
        if ((static_cast<unsigned char>(byte) & 0xC0) != 0x80)
            ++width;
    }
    return width;
}

class TextCanvas {
public:
    TextCanvas(std::size_t rows, std::size_t columns)
        : rows_(rows), columns_(columns), cells_(rows * columns, std::string(" ")) {}

    // Один символ (glyph - строка из одного символа UTF-8)
    void Put(std::size_t row, std::size_t column, const char* glyph) {
        cells_[row * columns_ + column] = glyph;
    }

    // Текст, начиная с позиции column: каждый символ UTF-8 - в свою клетку
    void PutText(std::size_t row, std::size_t column, const std::string& text) {
        for (std::size_t i = 0; i < text.size(); ++column) {
            std::size_t length = 1;
            while (i + length < text.size() && (static_cast<unsigned char>(text[i + length]) & 0xC0) == 0x80)
                ++length;
            cells_[row * columns_ + column] = text.substr(i, length);
            i += length;
        }
    }

    // Строки холста; пробелы в конце строки не печатаем
    void Print(std::ostream& out) const {
        for (std::size_t row = 0; row < rows_; ++row) {
            std::size_t end = columns_;
            while (end > 0 && cells_[row * columns_ + end - 1] == " ")
                --end;
            for (std::size_t column = 0; column < end; ++column)
                out << cells_[row * columns_ + column];
            out << "\n";
        }
    }

private:
    std::size_t rows_;
    std::size_t columns_;
    DynamicArray<std::string> cells_;
};
