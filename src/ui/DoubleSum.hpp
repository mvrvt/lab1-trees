#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace ui {

namespace detail {

// Конечный double — целое число единиц denorm_min(). Храним положительные
// и отрицательные слагаемые отдельно, вычитаем и округляем один раз в конце.
class DoubleSum {
    using Word = std::uint64_t;
    using Limits = std::numeric_limits<double>;
    static_assert(Limits::is_iec559 && Limits::radix == 2 && Limits::digits == 53 &&
                  Limits::max_exponent == 1024 && Limits::min_exponent == -1021);

    static constexpr int kUnitExponent = Limits::min_exponent - Limits::digits;
    static constexpr int kValueBits = Limits::max_exponent - kUnitExponent;
    static constexpr int kWordBits = std::numeric_limits<Word>::digits;
    static constexpr int kCountBits = std::numeric_limits<std::size_t>::digits;
    static_assert(kCountBits <= kWordBits);
    static constexpr int kWords = (kValueBits + kCountBits + kWordBits - 1) / kWordBits;
    using Magnitude = Word[kWords];

public:
    void Add(double value) {
        if (!std::isfinite(value))
            throw std::invalid_argument("слагаемое должно быть конечным");
        if (value == 0)
            return;

        int exponent = 0;
        const double fraction = std::frexp(std::fabs(value), &exponent);
        auto significand = static_cast<Word>(std::ldexp(fraction, Limits::digits));
        int shift = exponent - Limits::digits - kUnitExponent;
        if (shift < 0) { // У субнормальных значений младшие биты значащей части нулевые.
            significand >>= -shift;
            shift = 0;
        }

        auto& magnitude = value < 0 ? negative_ : positive_;
        const int index = shift / kWordBits;
        const int offset = shift % kWordBits;
        AddWord(magnitude, index, significand << offset);
        if (offset != 0)
            AddWord(magnitude, index + 1, significand >> (kWordBits - offset));
    }

    double Result(std::size_t divisor = 1) const {
        if (divisor == 0)
            throw std::invalid_argument("делитель должен быть положительным");
        int first = kWords - 1;
        while (first >= 0 && positive_[first] == negative_[first])
            --first;
        if (first < 0)
            return 0;

        const bool is_negative = negative_[first] > positive_[first];
        const auto& larger = is_negative ? negative_ : positive_;
        const auto& smaller = is_negative ? positive_ : negative_;
        Magnitude quotient{};
        Word borrow = 0;
        for (int i = 0; i <= first; ++i) {
            quotient[i] = larger[i] - smaller[i] - borrow;
            borrow = larger[i] < smaller[i] || (borrow != 0 && larger[i] == smaller[i]);
        }

        const auto denominator = static_cast<Word>(divisor);
        const Word remainder = denominator == 1 ? 0 : Divide(quotient, denominator);
        int top = kWords * kWordBits - 1;
        while (top >= 0 && !Bit(quotient, top))
            --top;
        const int discarded = top < Limits::digits ? 0 : top - Limits::digits + 1;
        Word significand = 0;
        for (int bit = top; bit >= discarded; --bit)
            significand = (significand << 1) | Bit(quotient, bit);

        // Округление к ближайшему, при равенстве — к чётному.
        bool above_half = false;
        bool exactly_half = false;
        if (discarded == 0) {
            above_half = remainder > denominator - remainder;
            exactly_half = remainder == denominator - remainder;
        } else if (Bit(quotient, discarded - 1)) {
            above_half = remainder != 0;
            for (int bit = discarded - 2; bit >= 0 && !above_half; --bit)
                above_half = Bit(quotient, bit);
            exactly_half = !above_half;
        }
        if (above_half || (exactly_half && (significand & 1) != 0))
            ++significand;

        if (top >= kValueBits ||
            (top == kValueBits - 1 && significand == (Word{1} << Limits::digits)))
            throw std::overflow_error("результат не помещается в double");

        const double result = std::ldexp(static_cast<double>(significand), kUnitExponent + discarded);
        return is_negative ? -result : result;
    }

private:
    static void AddWord(Magnitude& magnitude, int index, Word word) {
        for (; word != 0; ++index) {
            const Word previous = magnitude[index];
            magnitude[index] += word;
            word = magnitude[index] < previous;
        }
    }

    static bool Bit(const Magnitude& magnitude, int bit) {
        return ((magnitude[bit / kWordBits] >> (bit % kWordBits)) & Word{1}) != 0;
    }

    // Двоичное деление на число элементов. Остаток нужен для округления.
    static Word Divide(Magnitude& magnitude, Word divisor) {
        Word remainder = 0;
        for (int bit = kWords * kWordBits - 1; bit >= 0; --bit) {
            auto& word = magnitude[bit / kWordBits];
            const Word mask = Word{1} << (bit % kWordBits);
            const bool carry = (remainder >> (kWordBits - 1)) != 0;
            remainder = (remainder << 1) | ((word & mask) != 0);
            word &= ~mask;
            if (carry || remainder >= divisor) {
                remainder -= divisor;
                word |= mask;
            }
        }
        return remainder;
    }

    Magnitude positive_{};
    Magnitude negative_{};
};

} // namespace detail

} // namespace ui
