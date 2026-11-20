// Лабораторная работа №1: двоичное дерево поиска (AVL / без балансировки).
// Консольный интерфейс: выбор типа элементов и режима, затем меню операций.
// Ввод можно подать и из файла: ./build/lab1 < input.txt

#include <iostream>
#include <string>

#include "types/Person.hpp"
#include "ui/Demo.hpp"
#include "ui/Input.hpp"
#include "ui/TreeMenu.hpp"

namespace {

Balancing AskBalancing() {
    std::cout << "\nРежим дерева:\n"
                 "  1. AVL (сбалансированное, высота O(log n))\n"
                 "  2. Без балансировки (обычное дерево поиска)\n";
    return ui::AskInteger("> ", 1, 2) == 1 ? Balancing::kAvl : Balancing::kNone;
}

void RunMainMenu() {
    while (true) {
        std::cout << "\n=== Лабораторная работа №1: двоичное дерево поиска ===\n"
                     "Тип элементов:\n"
                     "  1. int\n"
                     "  2. double\n"
                     "  3. string\n"
                     "  4. Student (id Фамилия Имя год группа)\n"
                     "  5. Teacher (id Фамилия Имя год кафедра)\n"
                     "  6. Демонстрация без ввода\n"
                     "  0. Выход\n";
        switch (ui::AskChoice(6)) {
        case 0: return;
        case 1: ui::RunTreeMenu<int>(AskBalancing()); break;
        case 2: ui::RunTreeMenu<double>(AskBalancing()); break;
        case 3: ui::RunTreeMenu<std::string>(AskBalancing()); break;
        case 4: ui::RunTreeMenu<Student>(AskBalancing()); break;
        case 5: ui::RunTreeMenu<Teacher>(AskBalancing()); break;
        case 6: std::cout << "\n"; ui::RunDemo(); break;
        }
    }
}

} // namespace

int main() {
    try {
        RunMainMenu();
    } catch (const ui::InputClosed&) {
        std::cout << "\nВвод закончился.\n";
    }
    std::cout << "До свидания!\n";
    return 0;
}
