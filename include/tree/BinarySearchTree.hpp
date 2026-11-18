#pragma once

#include <cstddef> // std::size_t
#include <memory>  // std::unique_ptr, std::make_unique
#include <utility> //std::move, std::swap

#include "lab2_files/DynamicArray.hpp"
#include "lab2_files/Exceptions.hpp"
#include "tree/TreeExceptions.hpp"

// Узел дерева.
// Владение: родитель ВЛАДЕЕТ детьми через std::unique_ptr
// Удалили узел -> unique_ptr сам удалил его детей.
template <typename T> //TODO: В чем разница между template <class T> и template <typename T>?
struct TreeNode {
    T value;
    std::unique_ptr<TreeNode<T>> left;
    std::unique_ptr<TreeNode<T>> right; 

    explicit TreeNode(const T& new_value) : value(new_value) {}
};
//TODO: почему мы не можем переопределить все операторы сравнения? 
// ========== BinarySearchTree<T> - бинарное дерево поиска ==========
// Требования к T: копирование и operator<.
// Других сравнений не нужно: a и b считаются равными, если !(a < b) && !(b < a).
// Одинаковые значения не хранятся: дерево ведёт себя как множество (set).
// Вставка, поиск и удаление - O(h), где h - высота дерева.
template <typename T>
class BinarySearchTree {
public:
    using Node = TreeNode<T>;

    BinarySearchTree() = default; 

    // Deep copy
    BinarySearchTree(const BinarySearchTree<T>& other) : root_(CopySubtree(other.root_.get())), size_(other.size_) {}
    //TODO: Как мы получаем значение триноды по root_.get()? Где мы писал метод get() для триноды?

    //TODO: Спросить у кого-нибудь будет ли код без не сильно нужных {} оцениваться как максимально чистый?

    BinarySearchTree(BinarySearchTree<T>&& other) noexcept : root_(std::move(other.root_)), size_(other.size_) {
        other.size_ = 0;
    }

    // Copy-and-swap: если копирование бросит исключение, *this не изменится
    BinarySearchTree<T>& operator=(const BinarySearchTree<T>& other) {
        if (this != &other) {
            BinarySearchTree<T> copy(other);
            Swap(copy);
        }
        return *this;
    }

    BinarySearchTree<T>& operator=(BinarySearchTree<T>&& other) noexcept {
        if (this != &other) {
            Clear();
            Swap(other);
        }
        return *this;
    }

    ~BinarySearchTree() { Clear(); }

    std::size_t GetCount() const { return size_; }
    bool IsEmpty() const { return size_ == 0; }

    // Высота: пустое дерево - 0, один узел - 1. Пока O(n), с AVL станет O(1)
    std::size_t GetHeight() const { return SubtreeHeight(root_.get()); }

    // ---------- Поиск ----------
    bool Contains(const T& value) const { return FindNode(value) != nullptr; }

    // Найденное значение, равное value. Бросает ElementNotFound, если его нет.
    // Пригодится в ЛР2: ищем по ключу - получаем всю пару (ключ, данные).
    const T& Find(const T& value) const {
        const Node* node = FindNode(value);
        if (node == nullptr)
            throw ElementNotFound("BinarySearchTree::Find: element not found");
        return node->value;
    }

    // Минимум - самый левый узел, максимум - самый правый. O(h).
    const T& GetMin() const {
        CheckNotEmpty();
        const Node* node = root_.get();
        while (node->left) 
            node = node->left.get();
        return node->value;
    }

    const T& GetMax() const {
        CheckNotEmpty();
        const Node* node = root_.get();
        while (node->right)
            node = node->right.get();
        return node->value;
    }

    // ---------- Изменение ----------

    // true - вставили, false - такое значение уже было (дерево не изменилось)
    bool Insert(const T& value) {
        bool inserted = InsertInto(root_, value);
        if (inserted)
            ++size_;
        return inserted;
    }

    // true - удалили, false - такого значения не было
    bool Remove(const T& value) {
        bool removed = RemoveFrom(root_, value);
        if (removed) 
            --size_;
        return removed;
    }

    // Удалить все узлы. 
    void Clear() {
        // Если просто написать root_.reset(), деструктор корня вызовет деструкторы
        // детей, те - своих детей и т.д. Это рекурсия глубиной h. У вырожденного
        // дерева ("палки") h = n, и на 10^5 узлов стек может переполниться.
        // Поэтому разбираем дерево вручную, со своим стеком в куче.
        DynamicArray<std::unique_ptr<Node>> stack;
        if (root_)
            stack.Append(std::move(root_));
        while (!stack.IsEmpty()) {
            std::unique_ptr<Node> node = std::move(stack.GetLast());
            stack.RemoveLast();
            if (node->left)
                stack.Append(std::move(node->left));
            if (node->right)
                stack.Append(std::move(node->right));
            // здесь node удаляется, но детей у него уже нет -> рекурсии нет
        }
        size_ = 0;
    }

    void Swap(BinarySearchTree<T>& other) noexcept {
        std::swap(root_, other.root_);
        std::swap(size_, other.size_);
    }

    // ---------- Проверка корректности (для тестов) ----------
    // true, если для каждого узла выполняется свойство дерева поиска
    // и число узлов совпадает с size_
    bool IsValid() const {
        std::size_t count = 0;
        return IsValidSubtree(root_.get(), nullptr, nullptr, count) && count == size_;
    }

private:
    void CheckNotEmpty() const {
        if (size_ == 0)
            throw EmptyCollection("BinarySearchTree: tree is empty");
    }

    // Поиск без рекурсии: на каждом шаге уходим влево или вправо - O(h).
    const Node* FindNode(const T& value) const {
        const Node* node = root_.get();
        while (node != nullptr) {
            if (value < node->value) 
                node = node->left.get();
            else if (node->value < value)
                node = node->right.get();
            else 
                return node; 
        }
        return nullptr; 
    }

    // node - это ССЫЛКА на поле родителя (root_, или left/right какого-то узла).
    // Поэтому, дойдя до пустого места, мы можем записать новый узел прямо туда.
    bool InsertInto(std::unique_ptr<Node>& node, const T& value) {
        if (!node) {
            node = std::make_unique<Node>(value);
            return true; //TODO: не понял этого
        }
        if (value < node->value)
            return InsertInto(node->left, value);
        if (node->value < value) 
            return InsertInto(node->right, value);
        return false; // равное значение уже существует.
    }

    bool RemoveFrom(std::unique_ptr<Node>& node, const T& value) {
        if (!node)
            return false; 
        if (value < node->value)
            return RemoveFrom(node->left, value);
        if (node->value < value) 
            return RemoveFrom(node->right, value);

        // Нашли удаляемый узел. 3 случая: 
        if (!node->left) {
            // 1-2) левого ребёнка нет: на место узла встаёт правый ребёнок (или nullptr).
            // Старый узел удалится сам: unique_ptr освобождает то, чем владел раньше.
            node = std::move(node->right);
        } else if (!node->right) {
            // 2) есть только левый ребёнок: он встаёт на место узла
            node = std::move(node->left);
        } else {
            // 3) два ребёнка: на место узла ставим его ПРЕЕМНИКА -
            // минимальный узел правого поддерева. Он больше всего слева
            // и меньше всего остального справа, поэтому порядок не нарушится.
            std::unique_ptr<Node> successor = DetachMin(node->right); 
            successor->left = std::move(node->left);
            successor->right = std::move(node->right);
            node = std::move(successor); // старый узел (уже без детей) удаляется
        }
        return true;
    }

    // Отцепить минимальный узел поддерева и вернуть его (дерево остаётся корректным)
    std::unique_ptr<Node> DetachMin(std::unique_ptr<Node>& node) {
        if (node->left)
            return DetachMin(node->left);
        // У минимального узла нет левого ребёнка, а правый встаёт на его место
        std::unique_ptr<Node> min_node = std::move(node);
        node = std::move(min_node->right);
        return min_node;
    }

    // Рекурсивная глубокая копия дерева
    static std::unique_ptr<Node> CopySubtree(const Node* node) {
        if (node == nullptr)
            return nullptr;
        auto copy = std::make_unique<Node>(node->value);
        copy->left = CopySubtree(node->left.get());
        copy->right = CopySubtree(node->right.get());
        return copy;
    }

    static std::size_t SubtreeHeight(const Node* node) {
        if (node == nullptr)
            return 0;
        std::size_t left_height = SubtreeHeight(node->left.get());
        std::size_t right_height = SubtreeHeight(node->right.get());
        return 1 + (left_height > right_height ? left_height : right_height);
    }

    // min и max - границы, в которых должны лежать значения поддерева
    // (nullptr - границы нет). Проверять только "левый ребёнок < узел" мало:
    // внук может нарушить порядок относительно деда.
    static bool IsValidSubtree(const Node* node, const T* min, const T* max, std::size_t& count) {
        if (node == nullptr)
            return true; 
        if (min != nullptr && !(*min < node->value))
            return false; 
        if (max != nullptr && !(node->value < *max))
            return false;
        ++count;
        return IsValidSubtree(node->left.get(), min, &node->value, count)
            && IsValidSubtree(node->right.get(), &node->value, max, count);
    }

    std::unique_ptr<Node> root_; // по умолчанию = nullptr
    std::size_t size_ = 0;
};
