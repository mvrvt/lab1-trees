cmake_minimum_required(VERSION 3.25)
project(Lab1Trees LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Без явного типа сборки собираем Debug (отладочная информация, без оптимизаций)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Санитайзеры ловят выход за границы памяти, use-after-free и неопределённое поведение.
# Для замеров скорости их нужно выключать: cmake -DENABLE_SANITIZERS=OFF ...
option(ENABLE_SANITIZERS "Build with AddressSanitizer and UBSan" ON)

# Санитайзеры включаем для ВСЕГО проекта (и для GoogleTest тоже): если часть кода
# собрана с ASan, а часть без, бывают ложные срабатывания на std::string/std::vector.
if(ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# Вся библиотека - только заголовки (шаблоны), поэтому INTERFACE-цель:
# она ничего не компилирует, а лишь передаёт путь include/ тем, кто её подключает.
add_library(trees INTERFACE)
target_include_directories(trees INTERFACE ${CMAKE_SOURCE_DIR}/include)

set(WARNING_FLAGS -Wall -Wextra -Wpedantic)

# ---------- Программа (пока демонстрация, потом - консольный интерфейс) ----------
add_executable(lab1 src/main.cpp)
target_link_libraries(lab1 PRIVATE trees)
target_compile_options(lab1 PRIVATE ${WARNING_FLAGS})

# ---------- GoogleTest ----------
# Скачивается при первом запуске cmake в build/_deps (нужен интернет один раз),
# в систему ничего не ставится. Версия и контрольная сумма зафиксированы.
# SYSTEM - заголовки gtest считаются системными: наши строгие предупреждения
# не срабатывают на чужом коде.
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
    URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM
)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ---------- Тесты ----------
add_executable(tests
    tests/test_basic.cpp
    tests/test_avl.cpp
    tests/test_traversal.cpp
    tests/test_threading.cpp
    tests/test_functional.cpp
    tests/test_string_io.cpp
    tests/test_subtree_ops.cpp
    tests/test_person.cpp
)
# gtest_main даёт готовую функцию main, которая запускает все TEST(...)
target_link_libraries(tests PRIVATE trees GTest::gtest_main)
target_compile_options(tests PRIVATE ${WARNING_FLAGS})

# Регистрация тестов в CTest: можно запускать и "./tests", и "ctest"
enable_testing()
include(GoogleTest)
gtest_discover_tests(tests DISCOVERY_MODE PRE_TEST)
