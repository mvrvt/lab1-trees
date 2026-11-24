#pragma once

#include <istream>
#include <ostream>
#include <string>
#include <utility> // std::move

// ========== Пользовательские типы: человек, студент, преподаватель ==========
// Нужны, чтобы показать, что дерево - настоящий шаблон: ему хватает operator<
// (порядок), а для строк - operator<< и operator>> (печать и чтение).
//
// Порядок - ТОЛЬКО по id (номер студбилета / табельный номер). Поэтому для дерева
// две записи с одним id - "один и тот же элемент", даже если имена разные:
// эквивалентность !(a < b) && !(b < a) смотрит лишь на id.
// Для полного сравнения записей есть operator== - он сравнивает все поля.
//
// Текстовый вид (через пробел):  id Фамилия Имя год_рождения группа|кафедра
//   1024 Иванов Иван 2005 Б23-504
// Каждое поле - одно слово: пробел разделяет поля. Кириллица работает, потому что
// operator>> для std::string читает байты до пробела, а UTF-8 пробелов не содержит.

struct Person {
    static constexpr int kMinBirthYear = 1900;
    static constexpr int kMaxBirthYear = 2100;

    int id = 0;
    std::string last_name;
    std::string first_name;
    int birth_year = 2000;

    Person() = default;

    // explicit - чтобы число случайно не превращалось в Person: tree.Find(5) не
    // скомпилируется, нужно явно Person(5). Остальные поля для поиска не нужны.
    explicit Person(int new_id, std::string new_last_name = {}, std::string new_first_name = {},
                    int new_birth_year = 2000)
        : id(new_id),
          last_name(std::move(new_last_name)),
          first_name(std::move(new_first_name)),
          birth_year(new_birth_year) {}
};

struct Student : Person {
    std::string group;

    Student() = default;
    explicit Student(int new_id, std::string new_last_name = {}, std::string new_first_name = {},
                     int new_birth_year = 2000, std::string new_group = {})
        : Person(new_id, std::move(new_last_name), std::move(new_first_name), new_birth_year),
          group(std::move(new_group)) {}
};

struct Teacher : Person {
    std::string department; 

    Teacher() = default;
    explicit Teacher(int new_id, std::string new_last_name = {}, std::string new_first_name = {},
                     int new_birth_year = 2000, std::string new_department = {})
        : Person(new_id, std::move(new_last_name), std::move(new_first_name), new_birth_year),
          department(std::move(new_department)) {}
};

// ---------- Порядок (для дерева) ----------
inline bool operator<(const Person& a, const Person& b) {
    return a.id < b.id;
}

// ---------- Полное сравнение записей ----------
inline bool operator==(const Person& a, const Person& b) {
    return a.id == b.id && a.last_name == b.last_name && a.first_name == b.first_name &&
           a.birth_year == b.birth_year;
}
inline bool operator==(const Student& a, const Student& b) {
    return static_cast<const Person&>(a) == static_cast<const Person&>(b) && a.group == b.group;
}
inline bool operator==(const Teacher& a, const Teacher& b) {
    return static_cast<const Person&>(a) == static_cast<const Person&>(b) &&
           a.department == b.department;
}
inline bool operator!=(const Person& a, const Person& b) { return !(a == b); }
inline bool operator!=(const Student& a, const Student& b) { return !(a == b); }
inline bool operator!=(const Teacher& a, const Teacher& b) { return !(a == b); }

// ---------- Печать ----------
inline std::ostream& operator<<(std::ostream& out, const Person& person) {
    return out << person.id << ' ' << person.last_name << ' ' << person.first_name << ' '
               << person.birth_year;
}
inline std::ostream& operator<<(std::ostream& out, const Student& student) {
    return out << static_cast<const Person&>(student) << ' ' << student.group;
}
inline std::ostream& operator<<(std::ostream& out, const Teacher& teacher) {
    return out << static_cast<const Person&>(teacher) << ' ' << teacher.department;
}

// ---------- Чтение ----------
// При любой ошибке (не хватает полей, id не число, id < 0, год вне диапазона)
// поток переходит в состояние fail, а объект не меняется - так принято для operator>>.
inline std::istream& operator>>(std::istream& in, Person& person) {
    Person read;
    if (!(in >> read.id >> read.last_name >> read.first_name >> read.birth_year))
        return in;
    if (read.id < 0 || read.birth_year < Person::kMinBirthYear ||
        read.birth_year > Person::kMaxBirthYear) {
        in.setstate(std::ios::failbit);
        return in;
    }
    person = std::move(read);
    return in;
}
inline std::istream& operator>>(std::istream& in, Student& student) {
    Student read;
    if (in >> static_cast<Person&>(read) >> read.group)
        student = std::move(read);
    return in;
}
inline std::istream& operator>>(std::istream& in, Teacher& teacher) {
    Teacher read;
    if (in >> static_cast<Person&>(read) >> read.department)
        teacher = std::move(read);
    return in;
}
