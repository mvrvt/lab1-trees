#include <iostream>
#include <limits>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "ui/DoubleSum.hpp"
#include "ui/TreeMenu.hpp"

namespace {

class ConsoleSession {
public:
    explicit ConsoleSession(const std::string& input)
        : input_(input), original_input_(std::cin.rdbuf(input_.rdbuf())),
          original_output_(std::cout.rdbuf(output_.rdbuf())) {}

    ConsoleSession(const ConsoleSession&) = delete;
    ConsoleSession& operator=(const ConsoleSession&) = delete;

    ~ConsoleSession() {
        std::cin.rdbuf(original_input_);
        std::cout.rdbuf(original_output_);
    }

    std::string GetOutput() const { return output_.str(); }

private:
    std::istringstream input_;
    std::ostringstream output_;
    std::streambuf* original_input_;
    std::streambuf* original_output_;
};

} // namespace

TEST(Ui, EmptyAndWhitespaceStringKeys) {
    const std::string keys[] = {"", "   "};
    for (const auto& key : keys) {
        SCOPED_TRACE("key length: " + std::to_string(key.size()));
        ConsoleSession console("3\n\n{" + key + "}()[]\n6\n" + key + "\n4\n" + key +
                               "\n6\n" + key + "\n1\n" + key + "\n6\n" + key + "\n0\n");
        ui::RunTreeMenu<std::string>(Balancing::kAvl);
        const std::string output = console.GetOutput();
        EXPECT_NE(output.find("Прочитано элементов: 1"), std::string::npos);
        const std::string found = "Найден: " + key + '\n';
        const auto first_found = output.find(found);
        ASSERT_NE(first_found, std::string::npos);
        const auto removed = output.find("Удалён.", first_found);
        ASSERT_NE(removed, std::string::npos);
        const auto missing = output.find("Не найден.", removed);
        ASSERT_NE(missing, std::string::npos);
        const auto inserted = output.find("Добавлен.", missing);
        ASSERT_NE(inserted, std::string::npos);
        EXPECT_NE(output.find(found, inserted), std::string::npos);
    }
}

TEST(Ui, DoubleMeanWithLargeFiniteValues) {
    struct Case {
        const char* tree;
        double expected;
    };
    const Case cases[] = {
        {"{1.6e308}()[{1.7e308}()[]]", 0x1.d5ef83691d7d4p+1023},
        {"{-1.6e308}({-1.7e308}()[])[]", -0x1.d5ef83691d7d4p+1023},
        {"{1.7e308}({-1.7e308}()[])[]", 0.0},
        {"{0}()[]", 0.0},
        {"{3}({-1e308}()[])[{1e308}()[]]", 1.0},
    };

    const std::string prefix = "Reduce(среднее, ЛКП) = ";
    for (const auto& example : cases) {
        SCOPED_TRACE(example.tree);
        ConsoleSession console(std::string("3\n\n") + example.tree + "\n16\n2\n\n0\n");
        ui::RunTreeMenu<double>(Balancing::kAvl);
        const std::string output = console.GetOutput();
        const auto start = output.find(prefix);
        ASSERT_NE(start, std::string::npos);
        const auto value = start + prefix.size();
        const auto end = output.find('\n', value);
        ASSERT_NE(end, std::string::npos);
        EXPECT_EQ(ParseValue<double>(output.substr(value, end - value)), example.expected);
    }
}

TEST(Ui, DoubleSumWithLargeFiniteResult) {
    ConsoleSession console("3\n\n{-1.6e308}({-1.7e308}()[])[{1.7e308}()[]]\n16\n1\n\n0\n");
    ui::RunTreeMenu<double>(Balancing::kAvl);
    const std::string output = console.GetOutput();
    const std::string prefix = "Reduce(сумма, ЛКП) = ";
    const auto start = output.find(prefix);
    ASSERT_NE(start, std::string::npos);
    const auto value = start + prefix.size();
    const auto end = output.find('\n', value);
    ASSERT_NE(end, std::string::npos);
    EXPECT_EQ(ParseValue<double>(output.substr(value, end - value)), -1.6e308);
}

TEST(Ui, DoubleSumReportsUnrepresentableResult) {
    ConsoleSession console("3\n\n{1.6e308}()[{1.7e308}()[]]\n16\n1\n\n0\n");
    ui::RunTreeMenu<double>(Balancing::kAvl);
    EXPECT_NE(console.GetOutput().find("результат не помещается в double"), std::string::npos);
}

TEST(Ui, DoubleAccumulatorRoundsSubnormalMean) {
    const double smallest = std::numeric_limits<double>::denorm_min();
    ui::detail::DoubleSum sum;
    sum.Add(-std::numeric_limits<double>::max());
    sum.Add(smallest);
    sum.Add(std::numeric_limits<double>::max());
    EXPECT_EQ(sum.Result(), smallest);
    EXPECT_EQ(sum.Result(2), 0.0); // Половина denorm_min округляется к чётному нулю.

    sum.Add(smallest * 2);
    EXPECT_EQ(sum.Result(2), smallest * 2); // 1,5 единицы округляются к двум.
}

TEST(Ui, ComplexMenuSupportsInputTraversalAndFunctionalOperations) {
    ConsoleSession console(
        "3\n\n{(1,0)}({(0,1)}()[])[{(1,2)}()[]]\n"
        "1\n(2,x)\n(2,1)\n6\n(1,2)\n8\n1\n"
        "12\nЛКП\n13\ny\n(1,0)\n14\n3\nn\n15\n2\nn\n16\n1\nЛКП\n"
        "17\n(Л){К}[П]\n4\n(2,1)\n6\n(2,1)\n22\n0\n");
    ui::RunTreeMenu<Complex>(Balancing::kAvl);
    const std::string output = console.GetOutput();
    for (const char* expected : {
             "┌── (1,2)\n(1,0)\n└── (0,1)\n", "Прочитано элементов: 3", "Неверное значение", "Добавлен.",
             "Найден: (1,2)", "Get(1) = (1,0)", "Прошивка ЛКП.",
             "(1,0), (1,2), (2,1)", "По возрастанию: (-2,1), (-1,0), (-1,2), (0,1)",
             "По возрастанию: (0,1), (1,2), (2,1)", "Reduce(сумма, ЛКП) = (4,4)",
             "((){(0,1)}[]){(1,0)}[(){(1,2)}[(){(2,1)}[]]]", "Удалён.", "Не найден.",
             "Дерево корректно"}) {
        EXPECT_NE(output.find(expected), std::string::npos) << expected;
    }
}

TEST(Ui, ComplexSumPreservesSmallRealAndImaginaryComponents) {
    ConsoleSession console(
        "3\n\n{(3,5)}({(-1e308,-1e308)}()[])[{(1e308,1e308)}()[]]\n16\n1\nЛКП\n0\n");
    ui::RunTreeMenu<Complex>(Balancing::kNone);
    EXPECT_NE(console.GetOutput().find("Reduce(сумма, ЛКП) = (3,5)"), std::string::npos);
}

TEST(Ui, StructurePrintingLimits) {
    ConsoleSession console("2\n2\n64\n1\n65\n10\nn\n10\ny\n5\n2\n2\n2001\n10\n0\n");
    ui::RunTreeMenu<int>(Balancing::kAvl);
    const std::string output = console.GetOutput();

    const auto summary = output.find("дерево большое: 65 элементов");
    ASSERT_NE(summary, std::string::npos);
    const auto first_prompt = output.find("Печатать структуру? (y/n): ", summary);
    ASSERT_NE(first_prompt, std::string::npos);
    const auto second_prompt = output.find("Печатать структуру? (y/n): ", first_prompt + 1);
    ASSERT_NE(second_prompt, std::string::npos);
    EXPECT_EQ(output.find("┌──", first_prompt), output.find("┌──", second_prompt));
    const auto structure = output.find("┌──", second_prompt);
    ASSERT_NE(structure, std::string::npos);
    const auto values = output.find("Элементы (operator<<): ", structure);
    ASSERT_NE(values, std::string::npos);
    EXPECT_NE(output.find("Дерево слишком большое для печати (2001 элементов, предел 2000)", values),
              std::string::npos);
    EXPECT_EQ(output.find("Элементы (operator<<): ", values + 1), std::string::npos);
}
