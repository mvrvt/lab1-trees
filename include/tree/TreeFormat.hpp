#pragma once

#include <cctype>  // std::ispunct
#include <cstddef> // std::size_t
#include <string>

#include "tree/Traversal.hpp"
#include "tree/TreeExceptions.hpp"

// ========== Строка формата для сохранения дерева ==========
// Формат - три группы "открывающая скобка, буква, закрывающая скобка", например "{К}(Л)[П]".
//   Порядок букв = порядок обхода: "{К}(Л)[П]" - КЛП, "(Л){К}[П]" - ЛКП.
//   Скобки вокруг буквы = чем обрамлять: К - значение узла, Л / П - левое / правое поддерево.
// Буквы: К, Л, П (кириллица) или N, L, R (латиница), можно смешивать.
// Скобки: любые однобайтовые знаки препинания ASCII, кроме '\' (он нужен для экранирования).
//
// Кириллическая буква в UTF-8 занимает 2 байта ("К" = 0xD0 0x9A), поэтому буквы
// сравниваются как подстроки, а не как отдельные char.

// Формат, который умеет читать FromString (и формат сохранения по умолчанию)
inline const std::string kDefaultTreeFormat = "{К}(Л)[П]";

// Одна группа формата
struct FormatPart {
    TraversalStep step; // К, Л или П
    char open;          // открывающая скобка
    char close;         // закрывающая скобка
};

// Разобранный формат: три группы в порядке обхода
struct TreeFormat {
    FormatPart parts[3];
};

namespace tree_format_detail {

inline bool IsBracket(char symbol) {
    unsigned char code = static_cast<unsigned char>(symbol);
    return code < 128 && std::ispunct(code) != 0 && symbol != '\\';
}

// Если с позиции position начинается буква формата - записать её в step и длину в length
inline bool MatchLetter(const std::string& text, std::size_t position, TraversalStep& step, std::size_t& length) {
    struct Letter {
        const char* text;
        TraversalStep step;
    };
    const Letter letters[] = {
        {"К", TraversalStep::kNode}, {"Л", TraversalStep::kLeft}, {"П", TraversalStep::kRight},
        {"N", TraversalStep::kNode}, {"L", TraversalStep::kLeft}, {"R", TraversalStep::kRight},
    };
    for (const Letter& letter : letters) {
        std::string candidate(letter.text);
        if (text.compare(position, candidate.size(), candidate) == 0) {
            step = letter.step;
            length = candidate.size();
            return true;
        }
    }
    return false;
}

inline TreeFormatError MakeError(const std::string& format, const std::string& what) {
    return TreeFormatError("invalid tree format '" + format + "': " + what);
}

} // namespace tree_format_detail

// Разобрать строку формата. Бросает TreeFormatError, если:
// групп не три, буква повторяется, нет скобки, скобка - не знак препинания, есть лишние символы.
inline TreeFormat ParseTreeFormat(const std::string& text) {
    using namespace tree_format_detail;
    TreeFormat format{};
    bool used[3] = {false, false, false}; // какие буквы уже встретились (по номеру TraversalStep)
    std::size_t position = 0;

    for (FormatPart& part : format.parts) {
        if (position >= text.size())
            throw MakeError(text, "expected 3 groups like {К}");
        if (!IsBracket(text[position]))
            throw MakeError(text, "expected an opening bracket at position " + std::to_string(position));
        part.open = text[position++];

        std::size_t length = 0;
        if (!MatchLetter(text, position, part.step, length))
            throw MakeError(text, "expected К, Л, П (or N, L, R) at position " + std::to_string(position));
        std::size_t index = static_cast<std::size_t>(part.step);
        if (used[index])
            throw MakeError(text, "letter repeats at position " + std::to_string(position));
        used[index] = true;
        position += length;

        if (position >= text.size() || !IsBracket(text[position]))
            throw MakeError(text, "expected a closing bracket at position " + std::to_string(position));
        part.close = text[position++];
    }
    if (position != text.size())
        throw MakeError(text, "extra characters at position " + std::to_string(position));
    return format;
}

// Какому обходу соответствует формат: "(Л){К}[П]" -> ЛКП
inline TraversalOrder FormatToOrder(const TreeFormat& format) {
    const TraversalOrder all_orders[] = {
        TraversalOrder::kNLR, TraversalOrder::kNRL, TraversalOrder::kLNR,
        TraversalOrder::kLRN, TraversalOrder::kRNL, TraversalOrder::kRLN,
    };
    for (TraversalOrder order : all_orders) {
        TraversalSteps steps = GetTraversalSteps(order);
        if (steps.steps[0] == format.parts[0].step && steps.steps[1] == format.parts[1].step &&
            steps.steps[2] == format.parts[2].step)
            return order;
    }
    throw TreeFormatError("FormatToOrder: impossible format"); // ParseTreeFormat такого не пропустит
}
