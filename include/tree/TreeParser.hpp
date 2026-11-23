#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>

#include "lab2_files/DynamicArray.hpp"
#include "tree/TreeFormat.hpp"
#include "tree/ValueIo.hpp"

inline constexpr std::size_t kNoParsedNode = static_cast<std::size_t>(-1);

template <class T>
struct ParsedTreeNode {
    T value{};
    std::size_t position = 0;
    std::size_t left = kNoParsedNode;
    std::size_t right = kNoParsedNode;
    std::size_t min = kNoParsedNode;
    std::size_t max = kNoParsedNode;
};

template <class T>
class FormattedTreeParser {
public:
    FormattedTreeParser(const std::string& text, const TreeFormat& format)
        : text_(text), format_(format) {}

    DynamicArray<ParsedTreeNode<T>> Parse() {
        if (!text_.empty())
            StartNode();

        while (!stack_.IsEmpty()) {
            Frame& frame = stack_.GetLast();
            if (frame.next_part == 3) {
                CheckNode(frame.node);
                stack_.RemoveLast();
                if (!stack_.IsEmpty())
                    Expect(format_.parts[stack_.GetLast().next_part - 1].close);
                continue;
            }

            const FormatPart& part = format_.parts[frame.next_part++];
            Expect(part.open);
            auto& node = nodes_[frame.node];
            if (part.step == TraversalStep::kNode) {
                ReadValue(node, part.close);
            } else if (Peek() == part.close) {
                ++position_;
            } else {
                if (NeedsChildMarker(format_, part))
                    Expect('\\');
                auto& child = part.step == TraversalStep::kLeft ? node.left : node.right;
                child = nodes_.GetCount();
                StartNode();
            }
        }

        if (position_ != text_.size())
            Fail("unexpected characters after the end of the tree", position_);
        return std::move(nodes_);
    }

private:
    struct Frame {
        std::size_t node;
        std::size_t next_part = 0;
    };

    void StartNode() {
        stack_.Append(Frame{nodes_.GetCount()});
        nodes_.Append(ParsedTreeNode<T>{});
    }

    char Peek() const { return position_ < text_.size() ? text_[position_] : '\0'; }

    [[noreturn]] static void Fail(const std::string& message, std::size_t position) {
        throw TreeParseError(message, position);
    }

    void Expect(char symbol) {
        if (Peek() != symbol)
            Fail(std::string("expected '") + symbol + "'", position_);
        ++position_;
    }

    std::string ReadValueText(char closer) {
        std::string result;
        while (position_ < text_.size()) {
            const char symbol = text_[position_++];
            if (symbol == closer)
                return result;
            if (symbol == '\\') {
                if (position_ == text_.size())
                    Fail("unfinished escape sequence", position_ - 1);
                result += text_[position_++];
            } else {
                result += symbol;
            }
        }
        Fail(std::string("expected '") + closer + "'", position_);
    }

    void ReadValue(ParsedTreeNode<T>& node, char closer) {
        node.position = position_;
        const std::string raw = ReadValueText(closer);
        try {
            node.value = ParseValue<T>(raw);
        } catch (const std::invalid_argument& error) {
            Fail(error.what(), node.position);
        }
    }

    // Дети уже прочитаны. Достаточно сравнить крайние ключи их поддеревьев.
    void CheckNode(std::size_t index) {
        auto& node = nodes_[index];
        node.min = index;
        node.max = index;
        if (node.left != kNoParsedNode) {
            const auto& left = nodes_[node.left];
            const auto& largest = nodes_[left.max];
            if (!(largest.value < node.value))
                Fail("value in the left subtree must be less than its root", largest.position);
            node.min = left.min;
        }
        if (node.right != kNoParsedNode) {
            const auto& right = nodes_[node.right];
            const auto& smallest = nodes_[right.min];
            if (!(node.value < smallest.value))
                Fail("value in the right subtree must be greater than its root", smallest.position);
            node.max = right.max;
        }
    }

    const std::string& text_;
    const TreeFormat& format_;
    std::size_t position_ = 0;
    DynamicArray<ParsedTreeNode<T>> nodes_;
    DynamicArray<Frame> stack_;
};
