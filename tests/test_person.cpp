#include <sstream> // std::ostringstream
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "tree/BinarySearchTree.hpp"
#include "types/Person.hpp"

namespace {

using StudentTree = BinarySearchTree<Student>;
using TeacherTree = BinarySearchTree<Teacher>;

// Студенты вставляются НЕ по порядку id, а фамилии идут не по алфавиту -
// так видно, что дерево упорядочено именно по id.
StudentTree MakeStudents(Balancing balancing = Balancing::kAvl) {
    StudentTree tree(balancing);
    tree.Insert(Student(1024, "Иванов", "Иван", 2005, "Б23-504"));
    tree.Insert(Student(512, "Петрова", "Анна", 2006, "Б24-101"));
    tree.Insert(Student(2048, "Абрамов", "Олег", 2004, "Б22-303"));
    tree.Insert(Student(256, "Сидоров", "Пётр", 2005, "Б23-504"));
    tree.Insert(Student(4096, "Кузнецова", "Мария", 2003, "Б21-202"));
    return tree;
}

TeacherTree MakeTeachers() {
    TeacherTree tree;
    tree.Insert(Teacher(17, "Смирнов", "Алексей", 1975, "Кафедра-22"));
    tree.Insert(Teacher(3, "Волкова", "Елена", 1980, "Кафедра-12"));
    tree.Insert(Teacher(42, "Орлов", "Дмитрий", 1968, "Кафедра-22"));
    return tree;
}

template <class T>
std::string Print(const T& value) {
    std::ostringstream out;
    out << value;
    return out.str();
}

} // namespace

// ---------- Сам тип ----------

TEST(Person, OrderIsById) {
    Student a(1, "Яковлев", "Ян", 2005, "Б");
    Student b(2, "Аксёнов", "Антон", 2005, "А");
    EXPECT_TRUE(a < b); // фамилии не важны
    EXPECT_FALSE(b < a);

    // Один id, разные данные: для дерева эквивалентны, но как записи - разные
    Student same_id(1, "Другой", "Человек", 1999, "В");
    EXPECT_FALSE(a < same_id);
    EXPECT_FALSE(same_id < a);
    EXPECT_NE(a, same_id);
    EXPECT_EQ(a, Student(1, "Яковлев", "Ян", 2005, "Б"));
}

TEST(Person, PrintOneRecord) {
    EXPECT_EQ(Print(Student(1024, "Иванов", "Иван", 2005, "Б23-504")),
              "1024 Иванов Иван 2005 Б23-504");
    EXPECT_EQ(Print(Teacher(17, "Смирнов", "Алексей", 1975, "Кафедра-22")),
              "17 Смирнов Алексей 1975 Кафедра-22");
}

TEST(Person, ReadOneRecord) {
    Student student = ParseValue<Student>("1024 Иванов Иван 2005 Б23-504");
    EXPECT_EQ(student, Student(1024, "Иванов", "Иван", 2005, "Б23-504"));

    Teacher teacher = ParseValue<Teacher>("  17   Смирнов Алексей 1975 Кафедра-22 ");
    EXPECT_EQ(teacher, Teacher(17, "Смирнов", "Алексей", 1975, "Кафедра-22"));

    // ToText и ParseValue - взаимно обратные
    EXPECT_EQ(ParseValue<Student>(ToText(student)), student);
}

TEST(Person, ReadErrors) {
    EXPECT_THROW(ParseValue<Student>(""), std::invalid_argument);
    EXPECT_THROW(ParseValue<Student>("1024 Иванов Иван 2005"), std::invalid_argument);         // нет группы
    EXPECT_THROW(ParseValue<Student>("abc Иванов Иван 2005 Б23"), std::invalid_argument);      // id не число
    EXPECT_THROW(ParseValue<Student>("1024 Иванов Иван год Б23"), std::invalid_argument);      // год не число
    EXPECT_THROW(ParseValue<Student>("-5 Иванов Иван 2005 Б23"), std::invalid_argument);       // id < 0
    EXPECT_THROW(ParseValue<Student>("1024 Иванов Иван 1800 Б23"), std::invalid_argument);     // год вне диапазона
    EXPECT_THROW(ParseValue<Student>("1024 Иванов Иван 2005 Б23 лишнее"), std::invalid_argument);
}

// При ошибке чтения объект не меняется
TEST(Person, FailedReadKeepsObject) {
    Student student(7, "Старый", "Студент", 2001, "Г");
    std::istringstream in("8 Новый Студент 1500 Д");
    in >> student;
    EXPECT_TRUE(in.fail());
    EXPECT_EQ(student, Student(7, "Старый", "Студент", 2001, "Г"));
}

// ---------- Дерево студентов ----------

TEST(Person, TreeIsSortedById) {
    StudentTree tree = MakeStudents();
    EXPECT_EQ(tree.GetCount(), 5);
    EXPECT_TRUE(tree.IsValid());

    DynamicArray<Student> sorted = tree.ToArray();
    const int expected_ids[] = {256, 512, 1024, 2048, 4096};
    ASSERT_EQ(sorted.GetSize(), 5);
    for (std::size_t i = 0; i < 5; ++i)
        EXPECT_EQ(sorted[i].id, expected_ids[i]);
}

// Find ищет только по id, а возвращает полную запись из дерева
TEST(Person, FindByIdReturnsFullRecord) {
    StudentTree tree = MakeStudents();
    const Student& found = tree.Find(Student(2048));
    EXPECT_EQ(found, Student(2048, "Абрамов", "Олег", 2004, "Б22-303"));

    EXPECT_TRUE(tree.Contains(Student(256)));
    EXPECT_FALSE(tree.Contains(Student(300)));
    EXPECT_THROW(tree.Find(Student(300)), ElementNotFound);
}

// Повторный id - это повтор элемента: запись не добавляется и не заменяется
TEST(Person, SameIdIsDuplicate) {
    StudentTree tree = MakeStudents();
    EXPECT_FALSE(tree.Insert(Student(1024, "Самозванец", "Некто", 2000, "X")));
    EXPECT_EQ(tree.GetCount(), 5);
    EXPECT_EQ(tree.Find(Student(1024)).last_name, "Иванов");
}

TEST(Person, RemoveById) {
    StudentTree tree = MakeStudents();
    EXPECT_TRUE(tree.Remove(Student(1024)));
    EXPECT_FALSE(tree.Remove(Student(1024)));
    EXPECT_EQ(tree.GetCount(), 4);
    EXPECT_FALSE(tree.Contains(Student(1024)));
    EXPECT_TRUE(tree.IsValid());
}

TEST(Person, PrintTree) {
    TeacherTree tree = MakeTeachers();
    EXPECT_EQ(Print(tree), "{3 Волкова Елена 1980 Кафедра-12, 17 Смирнов Алексей 1975 Кафедра-22, "
                           "42 Орлов Дмитрий 1968 Кафедра-22}");
}

// ---------- Map / Where / Reduce на записях ----------

TEST(Person, WhereByGroup) {
    StudentTree tree = MakeStudents();
    StudentTree group = tree.Where([](const Student& s) { return s.group == "Б23-504"; });
    EXPECT_EQ(Print(group), "{256 Сидоров Пётр 2005 Б23-504, 1024 Иванов Иван 2005 Б23-504}");
    EXPECT_TRUE(group.IsValid());
}

// Map в дерево другого типа: студенты -> множество годов рождения (повторы сливаются)
TEST(Person, MapToBirthYears) {
    StudentTree tree = MakeStudents();
    BinarySearchTree<int> years = tree.Map<int>([](const Student& s) { return s.birth_year; });
    EXPECT_EQ(Print(years), "{2003, 2004, 2005, 2006}");
}

TEST(Person, ReduceCountAndOldest) {
    TeacherTree tree = MakeTeachers();
    int in_22 = tree.Reduce(
        [](const Teacher& t, int count) { return t.department == "Кафедра-22" ? count + 1 : count; },
        0);
    EXPECT_EQ(in_22, 2);

    int oldest_year = tree.Reduce(
        [](const Teacher& t, int year) { return t.birth_year < year ? t.birth_year : year; },
        Person::kMaxBirthYear);
    EXPECT_EQ(oldest_year, 1968);
}

// ---------- Сохранение в строку и чтение ----------

TEST(Person, SaveToString) {
    TeacherTree tree = MakeTeachers(); // AVL: корень 17, слева 3, справа 42
    EXPECT_EQ(tree.ToFormattedString(),
              "{17 Смирнов Алексей 1975 Кафедра-22}"
              "({3 Волкова Елена 1980 Кафедра-12}()[])"
              "[{42 Орлов Дмитрий 1968 Кафедра-22}()[]]");
}

// Записи после чтения совпадают полностью (все поля), форма дерева - тоже
TEST(Person, StringRoundTrip) {
    StudentTree original = MakeStudents(Balancing::kNone);
    std::string text = original.ToFormattedString();
    StudentTree restored = StudentTree::FromString(text, Balancing::kNone);

    EXPECT_EQ(restored.ToFormattedString(), text);
    DynamicArray<Student> a = original.ToArray();
    DynamicArray<Student> b = restored.ToArray();
    ASSERT_EQ(a.GetSize(), b.GetSize());
    for (std::size_t i = 0; i < a.GetSize(); ++i)
        EXPECT_EQ(a[i], b[i]);
}

TEST(Person, StringReadErrors) {
    EXPECT_THROW(StudentTree::FromString("{1024 Иванов}()[]"), TreeParseError);                // мало полей
    EXPECT_THROW(StudentTree::FromString("{5 А Б 2000 Г}({7 В Г 2000 Д}()[])[]"), TreeParseError); // 7 слева от 5
}
