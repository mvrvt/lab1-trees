cmake_minimum_required(VERSION 3.16)
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

# Вся библиотека - только заголовки (шаблоны), поэтому INTERFACE-цель:
# она ничего не компилирует, а лишь передаёт путь include/ тем, кто её подключает.
add_library(trees INTERFACE)
target_include_directories(trees INTERFACE ${CMAKE_SOURCE_DIR}/include)

set(WARNING_FLAGS -Wall -Wextra -Wpedantic)
set(SANITIZER_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer)

# ---------- Программа (пока демонстрация, потом - консольный интерфейс) ----------
add_executable(lab1 src/main.cpp)
target_link_libraries(lab1 PRIVATE trees)
target_compile_options(lab1 PRIVATE ${WARNING_FLAGS})
if(ENABLE_SANITIZERS)
    target_compile_options(lab1 PRIVATE ${SANITIZER_FLAGS})
    target_link_options(lab1 PRIVATE ${SANITIZER_FLAGS})
endif()

# ---------- Тесты ----------
add_executable(tests
    tests/test_main.cpp
    tests/test_basic.cpp
    tests/test_avl.cpp
    tests/test_traversal.cpp
    tests/test_threading.cpp
    tests/test_functional.cpp
    tests/test_string_io.cpp
)
target_link_libraries(tests PRIVATE trees)
target_compile_options(tests PRIVATE ${WARNING_FLAGS})
if(ENABLE_SANITIZERS)
    target_compile_options(tests PRIVATE ${SANITIZER_FLAGS})
    target_link_options(tests PRIVATE ${SANITIZER_FLAGS})
endif()
