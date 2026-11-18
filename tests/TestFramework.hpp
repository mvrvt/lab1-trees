#pragma once

#include <exception> // std::exception
#include <iostream>

// ========== Минимальный тестовый фреймворк ==========
// CHECK(условие)                     - условие должно быть истинным
// CHECK_THROWS(выражение, Исключение) - выражение должно бросить это исключение
// RunTest("имя", Функция)            - запустить один тест и напечатать результат
//
// При провале CHECK печатает файл, строку и само условие, но тест продолжается -
// так за один запуск видно сразу все ошибки.

namespace test {

inline int g_failed_checks = 0; // провалено проверок в текущем тесте
inline int g_passed_tests = 0;
inline int g_failed_tests = 0;

inline void ReportFailure(const char* file, int line, const char* text) {
    ++g_failed_checks;
    std::cout << "    FAILED: " << text << "  (" << file << ":" << line << ")\n";
}

template <class Func>
void RunTest(const char* name, Func test_function) {
    g_failed_checks = 0;
    try {
        test_function();
    } catch (const std::exception& error) {
        ReportFailure(__FILE__, __LINE__, error.what());
    } catch (...) {
        ReportFailure(__FILE__, __LINE__, "unknown exception");
    }
    if (g_failed_checks == 0) {
        ++g_passed_tests;
        std::cout << "  [OK]   " << name << "\n";
    } else {
        ++g_failed_tests;
        std::cout << "  [FAIL] " << name << "\n";
    }
}

// Итог: 0 - всё прошло, 1 - есть провалы (код возврата для main)
inline int PrintSummary() {
    std::cout << "\nPassed: " << g_passed_tests << ", failed: " << g_failed_tests << "\n";
    return g_failed_tests == 0 ? 0 : 1;
}

} // namespace test

#define CHECK(condition)                                         \
    do {                                                         \
        if (!(condition))                                        \
            test::ReportFailure(__FILE__, __LINE__, #condition); \
    } while (false)

#define CHECK_THROWS(expression, ExceptionType)                                       \
    do {                                                                              \
        bool thrown_ = false;                                                         \
        try {                                                                         \
            (void)(expression);                                                       \
        } catch (const ExceptionType&) {                                              \
            thrown_ = true;                                                           \
        } catch (...) {                                                               \
        }                                                                             \
        if (!thrown_)                                                                 \
            test::ReportFailure(__FILE__, __LINE__, #expression " must throw " #ExceptionType); \
    } while (false)
