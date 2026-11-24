#include <cstddef>
#include <limits>
#include <sstream>

#include <gtest/gtest.h>

#include "tree/BinarySearchTree.hpp"
#include "types/Complex.hpp"

TEST(ComplexType, OrderAndFiniteComponents) {
    EXPECT_LT(Complex(-1, 100), Complex(0, -100));
    EXPECT_LT(Complex(1, -2), Complex(1, 2));
    EXPECT_NE(Complex(0, 1), Complex(1, 0));
    EXPECT_EQ(Complex(-0.0, 0.0), Complex());
    EXPECT_THROW(Complex(0, std::numeric_limits<double>::infinity()), std::invalid_argument);
    EXPECT_THROW(Complex(std::numeric_limits<double>::quiet_NaN(), 0), std::invalid_argument);
}

TEST(ComplexType, StreamRoundTripAndFailedRead) {
    const Complex value(0x1.0000000000001p+0, -1.7e308);
    std::stringstream stream;
    stream << value;
    Complex loaded;
    ASSERT_TRUE(stream >> loaded);
    EXPECT_EQ(loaded, value);

    const char* malformed[] = {"1.5", "(3)", "(3 4)", "(3,)", "(3,4]", "(3,1e309)"};
    for (const char* text : malformed) {
        SCOPED_TRACE(text);
        std::istringstream bad(text);
        EXPECT_FALSE(bad >> loaded);
        EXPECT_EQ(loaded, value);
    }
}

TEST(ComplexType, BstAndAvlOperations) {
    using Tree = BinarySearchTree<Complex>;
    const Complex keys[] = {{1, 2}, {1, -2}, {-1, 2}, {0, 1}, {1, 0}};
    const Complex sorted[] = {{-1, 2}, {0, 1}, {1, -2}, {1, 0}, {1, 2}};
    for (const auto mode : {Balancing::kNone, Balancing::kAvl}) {
        Tree tree(mode);
        for (const auto& key : keys)
            ASSERT_TRUE(tree.Insert(key));
        EXPECT_FALSE(tree.Insert(Complex(1, 0)));
        ASSERT_EQ(tree.GetCount(), 5U);
        EXPECT_TRUE(tree.IsValid());
        EXPECT_EQ(tree.GetMin(), sorted[0]);
        EXPECT_EQ(tree.GetMax(), sorted[4]);

        std::size_t index = 0;
        for (const auto& key : tree) {
            ASSERT_LT(index, 5U);
            EXPECT_EQ(key, sorted[index]);
            EXPECT_EQ(tree.Get(index), key);
            EXPECT_EQ(tree.IndexOf(key), index);
            EXPECT_EQ(tree.Find(key), key);
            ++index;
        }
        EXPECT_EQ(index, 5U);

        for (const auto order : {TraversalOrder::kNLR, TraversalOrder::kNRL, TraversalOrder::kLNR,
                                 TraversalOrder::kLRN, TraversalOrder::kRNL, TraversalOrder::kRLN}) {
            const auto expected = tree.ToArray(order);
            tree.Thread(order);
            index = 0;
            tree.TraverseThreaded([&](const Complex& key) { EXPECT_EQ(key, expected.Get(index++)); });
            EXPECT_EQ(index, tree.GetCount());
        }
        ASSERT_TRUE(tree.Remove(Complex(1, 0)));
        EXPECT_FALSE(tree.IsThreaded());
        EXPECT_FALSE(tree.Contains(Complex(1, 0)));
        EXPECT_TRUE(tree.IsValid());
    }
}

TEST(ComplexType, SubtreeOperatorsAndFormattedRoundTrip) {
    using Tree = BinarySearchTree<Complex>;
    const Tree source = Tree::FromString("{(1,0)}({(0,1)}()[])[{(1,2)}()[]]", Balancing::kNone);
    Tree subtree = source.ExtractSubtree(Complex(1, 2));
    EXPECT_TRUE(source.ContainsSubtree(subtree));
    subtree += Complex(2, 3);
    EXPECT_FALSE(source.ContainsSubtree(subtree));
    EXPECT_FALSE(source.Contains(Complex(2, 3)));

    Tree merged = source + subtree;
    EXPECT_TRUE(merged.IsValid());
    EXPECT_EQ(merged.GetCount(), 4U);
    merged -= Complex(2, 3);
    EXPECT_EQ(merged, source);

    for (const char* format : {"{К}(Л)[П]", "(Л){К}[П]", "[П](Л)(К)", "|К||Л||П|"}) {
        SCOPED_TRACE(format);
        const std::string text = source.ToFormattedString(format);
        EXPECT_EQ(Tree::FromString(text, format, Balancing::kNone).ToFormattedString(format), text);
        EXPECT_EQ(Tree::FromString(text, format, Balancing::kAvl), source);
    }
    EXPECT_THROW(Tree::FromString("{(1,0)}({(1,1)}()[])[]"), TreeParseError);
    EXPECT_THROW(Tree::FromString("{(1,0)}()[{(1,0)}()[]]"), TreeParseError);
    EXPECT_THROW(Tree::FromString("{(1,abc)}()[]"), TreeParseError);
}
