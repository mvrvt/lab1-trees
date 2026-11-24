#pragma once

#include <cmath>
#include <istream>
#include <ostream>
#include <stdexcept>

#include "tree/ValueIo.hpp"

class Complex {
public:
    Complex() = default;

    Complex(double real, double imaginary)
        : real_(real == 0 ? 0 : real), imaginary_(imaginary == 0 ? 0 : imaginary) {
        if (!AreFinite(real, imaginary))
            throw std::invalid_argument("complex components must be finite");
    }

    double Real() const { return real_; }
    double Imaginary() const { return imaginary_; }

    // Лексикографический порядок ключей для BST и AVL.
    friend bool operator<(const Complex& a, const Complex& b) {
        if (a.real_ != b.real_)
            return a.real_ < b.real_;
        return a.imaginary_ < b.imaginary_;
    }

    friend bool operator==(const Complex& a, const Complex& b) {
        return a.real_ == b.real_ && a.imaginary_ == b.imaginary_;
    }

    friend bool operator!=(const Complex& a, const Complex& b) {
        return !(a == b);
    }

    friend std::ostream& operator<<(std::ostream& out, const Complex& value) {
        return out << '(' << ToText(value.real_) << ',' << ToText(value.imaginary_) << ')';
    }

    friend std::istream& operator>>(std::istream& in, Complex& value) {
        char delimiter;
        double real;
        double imaginary;
        if (!(in >> delimiter) || delimiter != '(' ||
            !(in >> real >> delimiter) || delimiter != ',' ||
            !(in >> imaginary >> delimiter) || delimiter != ')' ||
            !AreFinite(real, imaginary)) {
            in.setstate(std::ios::failbit);
            return in;
        }
        value = Complex(real, imaginary);
        return in;
    }

private:
    static bool AreFinite(double real, double imaginary) {
        return std::isfinite(real) && std::isfinite(imaginary);
    }

    double real_ = 0;
    double imaginary_ = 0;
};
