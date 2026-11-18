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
    int height = 1; // высота поддерева с корнем в этом узле (лист - 1).

    explicit TreeNode(const T& new_value) : value(new_value) {}
};

enum class Balancing {
    kNone, // обычное дерево поиска: на отсортированных данных вырождается в "палку"
    kAvl, // AVL-дерево: после каждой вставки/удаления выравнивается поворотами
};

//TODO: почему мы не можем переопределить все операторы сравнения? 
// ========== BinarySearchTree<T> - бинарное дерево поиска ==========
// Требования к T: копирование и operator<.
// Других сравнений не нужно: a и b считаются равными, если !(a < b) && !(b < a).
// Одинаковые значения не хранятся: дерево ведёт себя как множество (set).
// Вставка, поиск, удаление - O(h), где h - высота дерева:
//   Balancing::kAvl  -> h <= 1.44 * log2(n), то есть O(log n) всегда;
//   Balancing::kNone -> h от log2(n) до n (в худшем случае O(n)).
template <typename T>
class BinarySearchTree {
public:
    using Node = TreeNode<T>;

    explicit BinarySearchTree(Balancing balancing = Balancing::kAvl) : balancing_(balancing) {}


    // Deep copy
    BinarySearchTree(const BinarySearchTree<T>& other) 
        : root_(CopySubtree(other.root_.get())), size_(other.size_), balancing_(other.balancing_) {}
    //TODO: Как мы получаем значение триноды по root_.get()? Где мы писал метод get() для триноды?

    //TODO: Спросить у кого-нибудь будет ли код без не сильно нужных {} оцениваться как максимально чистый?

    BinarySearchTree(BinarySearchTree<T>&& other) noexcept 
        : root_(std::move(other.root_)), size_(other.size_), balancing_(other.balancing_) {
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

    // ---------- Информация ----------
    std::size_t GetCount() const { return size_; }
    bool IsEmpty() const { return size_ == 0; }
    Balancing GetBalancing() const { return balancing_; }


    // Высота: пустое дерево - 0, один узел - 1. Пока O(n), с AVL станет O(1)
    std::size_t GetHeight() const { return static_cast<std::size_t>(Height(root_.get())); }

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
        std::swap(balancing_, other.balancing_);
    }

    // ---------- Проверка корректности (для тестов) ----------
    // true, если для каждого узла выполняется свойство дерева поиска
    // и число узлов совпадает с size_
    bool IsValid() const {
        std::size_t count = 0;
        return CheckSubtree(root_.get(), nullptr, nullptr, count) >= 0 && count == size_;
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
        bool inserted = false; 
        if (value < node->value)
            inserted = InsertInto(node->left, value);
        else if (node->value < value) 
            inserted = InsertInto(node->right, value);
        // иначе равное значение уже есть: inserted == false

        // Возвращаясь из рекурсии, проходим путь снизу вверх
        // и чиним высоты и баланс всех узлов на этом пути
        if (inserted)
            Rebalance(node);
        return inserted;
    }

    bool RemoveFrom(std::unique_ptr<Node>& node, const T& value) {
        if (!node)
            return false; 
        bool removed = true;
        if (value < node->value)
            removed = RemoveFrom(node->left, value);
        else if (node->value < value) 
            removed = RemoveFrom(node->right, value);
        else if (!node->left) {
            // Нашли удаляемый узел. Левого ребёнка нет (лист или только правый ребёнок):
            // на место узла встаёт правый ребёнок (или nullptr).
            // Старый узел удалится сам: unique_ptr освобождает то, чем владел раньше.
            node = std::move(node->right);
        } else if (!node->right) {
            // Есть только левый ребёнок: он встаёт на место узла
            node = std::move(node->left);
        } else {
            // Два ребёнка: на место узла ставим его ПРЕЕМНИКА -
            // минимальный узел правого поддерева. Он больше всего слева
            // и меньше всего остального справа, поэтому порядок не нарушится.
            std::unique_ptr<Node> successor = DetachMin(node->right);
            successor->left = std::move(node->left);
            successor->right = std::move(node->right);
            node = std::move(successor); // старый узел (уже без детей) удаляется
        }

        if (removed && node)
            Rebalance(node);
        return removed;
    }

    // Отцепить минимальный узел поддерева и вернуть его (дерево остаётся корректным)
    std::unique_ptr<Node> DetachMin(std::unique_ptr<Node>& node) {
        if (node->left) {
            std::unique_ptr<Node> min_node = DetachMin(node->left);
            Rebalance(node); // слева стало на один узел меньше
            return min_node; 
        }
        // У минимального узла нет левого ребёнка, а правый встаёт на его место
        std::unique_ptr<Node> min_node = std::move(node);
        node = std::move(min_node->right);
        return min_node;
    }

    // ---------- AVL: высоты, баланс, повороты ----------

    // Высота поддерева; у пустого поддерева (nullptr) - 0
    static int Height(const Node* node) { return node ? node->height : 0; }

    // Пересчитать высоту узла по детям (их высоты должны быть уже правильными)
    static void UpdateHeight(Node* node) {
        int left_height = Height(node->left.get());
        int right_height = Height(node->right.get());
        node->height = 1 + (left_height > right_height ? left_height : right_height);
    }

    // Коэффициент баланса: > 0 - левое поддерево выше, < 0 - правое выше.
    static int BalanceFactor(const Node* node) {
        return Height(node->left.get()) - Height(node->right.get());
    }

    /* Правый поворот вокруг node:

              node              L
             /    \           /   \
            L      C   ->    A    node
           / \                    /  \
          A   B                  B    C

       Порядок A < L < B < node < C не меняется - меняется только форма. */
    static void RotateRight(std::unique_ptr<Node>& node) {
        std::unique_ptr<Node> new_root = std::move(node->left); // L
        node->left = std::move(new_root->right);                // В переходит к node
        UpdateHeight(node.get());                               // node теперь ниже L
        new_root->right = std::move(node);                      // node становится правым ребёнком L
        UpdateHeight(new_root.get());
        node = std::move(new_root);                             // L встаёт на место node у родителя
    } //TODO: Сложности с пониманием того как происходит правый поворот.

    /* Левый поворот вокруг node - зеркальное отражение правого:

            node                  R
           /    \               /   \
          A      R      ->    node   C
                / \           /  \
               B   C         A    B
    */
    static void RotateLeft(std::unique_ptr<Node>& node) {
        std::unique_ptr<Node> new_root = std::move(node->right); // R
        node->right = std::move(new_root->left);                 // B переходит к node
        UpdateHeight(node.get());
        new_root->left = std::move(node);
        UpdateHeight(new_root.get());
        node = std::move(new_root);
    }

    // Вызывается для каждого узла на пути от изменённого места к корню.
    // Высоты детей node к этому моменту уже правильные.
    void Rebalance(std::unique_ptr<Node>& node) {
        UpdateHeight(node.get());
        if (balancing_ == Balancing::kNone)
            return; // Обычное дерево: только поддерживаем высоты
        
        int balance = BalanceFactor(node.get());
        if (balance > 1) {
            // Левое поддерево выше на 2
            if (BalanceFactor(node->left.get()) < 0)
                RotateLeft(node->left); // случай "лево-право": сначала превращаем в "лево-лево"
            RotateRight(node);          // случай "лево-лево"
        } else if (balance < -1) {
            // Правое поддерево выше на 2 - всё зеркально
            if (BalanceFactor(node->right.get()) > 0)
                RotateRight(node->right); // случай "право-лево"
            RotateLeft(node);             // случай "право-право"
        }
    }

    // ---------- Вспомогательное ----------

    // Рекурсивная глубокая копия дерева (вместе с высотами)
    static std::unique_ptr<Node> CopySubtree(const Node* node) {
        if (node == nullptr)
            return nullptr;
        auto copy = std::make_unique<Node>(node->value);
        copy->height = node->height;
        copy->left = CopySubtree(node->left.get());
        copy->right = CopySubtree(node->right.get());
        return copy;
    }

    // Проверяет поддерево и возвращает его настоящую высоту, или -1, если что-то нарушено.
    // min и max - границы, в которых должны лежать значения поддерева (nullptr - границы нет).
    // Проверять только "левый ребёнок < узел" мало: внук может нарушить порядок относительно деда.
    int CheckSubtree(const Node* node, const T* min, const T* max, std::size_t& count) const {
        if (node == nullptr)
            return 0;
        if (min != nullptr && !(*min < node->value))
            return -1;
        if (max != nullptr && !(node->value < *max))
            return -1;
        ++count;

        int left_height = CheckSubtree(node->left.get(), min, &node->value, count);
        int right_height = CheckSubtree(node->right.get(), &node->value, max, count);
        if (left_height < 0 || right_height < 0)
            return -1;

        int real_height = 1 + (left_height > right_height ? left_height : right_height);
        if (node->height != real_height)
            return -1;
        int difference = left_height - right_height;
        if (balancing_ == Balancing::kAvl && (difference > 1 || difference < -1))
            return -1;
        return real_height;
    }

    std::unique_ptr<Node> root_; // по умолчанию = nullptr
    std::size_t size_ = 0;
    Balancing balancing_ = Balancing::kAvl;
};
