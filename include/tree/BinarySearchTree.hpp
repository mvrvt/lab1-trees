#pragma once

#include <cstddef> // std::size_t
#include <memory>  // std::unique_ptr, std::make_unique
#include <utility> //std::move, std::swap
#include <ostream> // std::ostream //TODO: Разве не надо это сделать в отдельном файле
#include <string>

#include "lab2_files/DynamicArray.hpp"
#include "lab2_files/Exceptions.hpp"
#include "lab2_files/ICollection.hpp"
#include "lab2_files/IEnumerable.hpp"
#include "tree/TreeExceptions.hpp"
#include "tree/TreeFormat.hpp"
#include "tree/TreeParser.hpp"
#include "tree/Traversal.hpp"
#include "tree/ValueIo.hpp"

// Узел дерева.
// Владение: родитель ВЛАДЕЕТ детьми через std::unique_ptr
// Удалили узел -> unique_ptr сам удалил его детей.
template <typename T> //TODO: В чем разница между template <class T> и template <typename T>?
struct TreeNode {
    T value;
    std::unique_ptr<TreeNode<T>> left;
    std::unique_ptr<TreeNode<T>> right; 
    int height = 1; // высота поддерева с корнем в этом узле (лист - 1).
    std::size_t size = 1; // число узлов в этом поддереве (лист - 1).
    // Прошивка: следующий узел в порядке выбранного обхода (nullptr - последний).
    // НЕ владеющий указатель: узлами владеют unique_ptr left/right, next только "смотрит".
    // Имеет смысл, только пока дерево прошито (см. BinarySearchTree::Thread).
    TreeNode<T>* next = nullptr;

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
class BinarySearchTree : public ICollection<T>, public IEnumerable<T> {
public:
    using Node = TreeNode<T>;
    // Результат IndexOf, если эл-нт не найден (как в Sequence)
    static constexpr std::size_t kNotFound = static_cast<std::size_t>(-1);

    // ---------- Итератор для IEnumerable: симметричный обход (ЛКП), по возрастанию ----------
    // Хранит стек узлов, до которых ещё нужно дойти. Весь обход - O(n), один шаг - O(1)
    // в среднем, память - O(h). Если дерево изменить во время обхода, итератор
    // становится недействительным (как у любых коллекций).
    class TreeEnumerator : public IEnumerator<T> {
    public:
        explicit TreeEnumerator(const Node* root) : root_(root) {}

        bool MoveNext() override {
            if (!started_) {
                started_ = true; 
                PushLeftPath(root_); // самый маленький эл-нт - в конце левого пути
            } else if (current_ != nullptr) {
                // Следующий после current_ - самый левый узел его правого поддерева,
                // а если правого поддерева нет - ближайший предок, лежащий в стеке
                PushLeftPath(current_->right.get());
            }
            if (stack_.IsEmpty()) {
                current_ = nullptr;
                return false;
            }
            current_ = stack_.GetLast();
            stack_.RemoveLast();
            return true;
        }

        const T& Current() const override {
            if (current_ == nullptr) 
                throw IndexOutOfRange("TreeEnumerator: no current element");
            return current_->value; 
        }

        void Reset() override {
            started_ = false;
            current_ = nullptr;
            stack_.Clear();
        }

    private:
        // Положить в стек узел и всех его левых потомков
        void PushLeftPath(const Node* node) {
            while (node != nullptr) {
                stack_.Append(node);
                node = node->left.get();
            }
        }

        const Node* root_;
        const Node* current_ = nullptr;
        DynamicArray<const Node*> stack_;
        bool started_ = false; 
    };

    explicit BinarySearchTree(Balancing balancing = Balancing::kAvl) : balancing_(balancing) {}


    // Deep copy
    // Указатели next копировать нельзя: они смотрели бы в узлы ДРУГОГО дерева.
    // Поэтому, если оригинал был прошит, копия прошивается заново в том же порядке.
    BinarySearchTree(const BinarySearchTree<T>& other)
        : root_(CopySubtree(other.root_.get())), size_(other.size_), balancing_(other.balancing_) {
        if (other.is_threaded_)
            Thread(other.thread_order_);
    }
    //TODO: Как мы получаем значение триноды по root_.get()? Где мы писал метод get() для триноды?

    //TODO: Спросить у кого-нибудь будет ли код без не сильно нужных {} оцениваться как максимально чистый?

    // При перемещении узлы остаются на своих местах в памяти (переезжает только root_),
    // поэтому прошивка остаётся верной и переходит вместе с узлами.
    BinarySearchTree(BinarySearchTree<T>&& other) noexcept
        : root_(std::move(other.root_)), size_(other.size_), balancing_(other.balancing_),
          thread_head_(other.thread_head_), thread_order_(other.thread_order_),
          is_threaded_(other.is_threaded_) {
        other.size_ = 0;
        other.ResetThreading();
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

    ~BinarySearchTree() override { Clear(); }

    // ---------- Информация (IsEmpty берётся из ICollection) ----------
    std::size_t GetCount() const override { return size_; }
    Balancing GetBalancing() const { return balancing_; }

    // Высота: пустое дерево - 0, один узел - 1. O(1): высота хранится в корне
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

    // ---------- Доступ по индексу (ICollection) ----------

    // index-й по возрастанию элемент (Get(0) - минимум). O(h).
    // Бросает IndexOutOfRange, если index >= GetCount().
    const T& Get(std::size_t index) const override {
        if (index >= size_)
            throw IndexOutOfRange("BinarySearchTree::Get: index out of range");
        const Node* node = root_.get();
        while (true) {
            std::size_t left_size = Size(node->left.get());
            if (index < left_size) {
                node = node->left.get(); // искомый - среди меньших
            } else if (index == left_size) {
                return node->value;
            } else {
                index -= left_size + 1; // пропускаем левое поддерево и сам узел
                node = node->right.get();
            }
        }
    }

    // Сколько элементов меньше value (то есть индекс value по возрастанию),
    // или kNotFound, если value нет в дереве. O(h)
    std::size_t IndexOf(const T& value) const {
        std::size_t index = 0;
        const Node* node = root_.get();
        while (node != nullptr) {
            if (value < node->value) {
                node = node->left.get();
            } else if (node->value < value) {
                index += Size(node->left.get()) + 1;
                node = node->right.get();
            } else {
                return index + Size(node->left.get());
            }
        }
        return kNotFound;
    }

    // ---------- Обходы ----------

    // Вызвать visit(значение) для каждого узла в заданном порядке.
    // visit - функция или лямбда вида void visit(const T& value). O(n).
    template <typename Func> //TODO: В твоем коде было написано template <class Func>, имеет ли значение писать по-твоему или можно записать с typename вместо class? 
    void Traverse(TraversalOrder order, Func visit) const {
        const Node* root = root_.get();
        TraverseNodes(root, order, [&visit](const Node* node) { visit(node->value); });
    }

    // Все значения в заданном порядке (по умолчанию ЛКП - по возрастанию). O(n).
    DynamicArray<T> ToArray(TraversalOrder order = TraversalOrder::kLNR) const {
        DynamicArray<T> result; 
        result.Reserve(size_);
        Traverse(order, [&result](const T& value) { result.Append(value); });
        return result; 
    }

    // Итератор для IEnumerable (его удаляет вызывающий). Цикл for (const T& x : tree)
    // делает это сам - см. EnumeratorIterator в IEnumerable.hpp
    IEnumerator<T>* GetEnumerator() const override { return new TreeEnumerator(root_.get()); }

    // ---------- Прошивка ----------
    // Узлы связываются в цепочку через next в порядке обхода order (любой из 6).
    // После этого обход идёт по цепочке: без стека и без рекурсии, каждый шаг строго O(1),
    // и начать его можно с любого узла (основа поиска по диапазону в ЛР-2).
    // Любое изменение дерева (Insert, Remove, Clear) прошивку сбрасывает:
    // повороты AVL меняют форму, а с ней порядки КЛП, ЛПК и др. Нужно вызвать Thread снова.

    // Прошить дерево в заданном порядке. O(n) время, O(h) память (на время прошивки).
    void Thread(TraversalOrder order) {
        Node* previous = nullptr;
        thread_head_ = nullptr;
        TraverseNodes(root_.get(), order, [this, &previous](Node* node) {
            node->next = nullptr;
            if (previous == nullptr)
                thread_head_ = node; // первый узел цепочки
            else
                previous->next = node;
            previous = node;
        });
        thread_order_ = order;
        is_threaded_ = true;
    }

    bool IsThreaded() const { return is_threaded_; }

    // Порядок, в котором дерево прошито. Бросает TreeNotThreaded, если не прошито.
    TraversalOrder GetThreadOrder() const {
        CheckThreaded();
        return thread_order_;
    }

    // Обход по прошивке: visit(значение) для каждого узла в порядке прошивки.
    // O(n) время, O(1) дополнительной памяти. Бросает TreeNotThreaded, если не прошито.
    template <typename Func>
    void TraverseThreaded(Func visit) const {
        CheckThreaded();
        for (const Node* node = thread_head_; node != nullptr; node = node->next)
            visit(node->value);
    }

    // То же, но начиная с узла, равного start (сам start тоже посещается).
    // O(h) на поиск узла + O(1) на каждый следующий.
    // Бросает TreeNotThreaded, если не прошито, и ElementNotFound, если start нет в дереве.
    template <typename Func>
    void TraverseThreadedFrom(const T& start, Func visit) const {
        CheckThreaded();
        const Node* node = FindNode(start);
        if (node == nullptr)
            throw ElementNotFound("BinarySearchTree::TraverseThreadedFrom: element not found");
        for (; node != nullptr; node = node->next)
            visit(node->value);
    }

    // ---------- map, where, reduce ----------
    // func / predicate - функция, лямбда или указатель на функцию (шаблон принимает всё).
    // Исходное дерево не меняется. Режим балансировки результата - как у исходного.

    // Новое дерево из значений func(x) для каждого x. O(n log n) для AVL.
    // U - тип элементов результата, по умолчанию T: tree.Map(f).
    // Можно получить дерево другого типа: tree.Map<std::string>(f).
    // - Новое дерево строится вставками, а не копированием формы: func может
    //   нарушить порядок (f(x) = -x разворачивает его).
    // - Элементов может стать меньше: если f склеивает значения (f(x) = x / 10),
    //   одинаковые результаты в множестве хранятся один раз.
    // - Вставка в порядке КЛП (родители раньше детей): для возрастающей f и режима kNone
    //   форма дерева сохраняется, а вставка по возрастанию выродила бы его в "палку".
    template <typename U = T, typename Func>
    BinarySearchTree<U> Map(Func func) const {
        BinarySearchTree<U> result(balancing_);
        Traverse(TraversalOrder::kNLR, [&result, &func](const T& value) { result.Insert(func(value)); });
        return result;
    }

    // Новое дерево из тех x, для которых predicate(x) == true. O(n log n) для AVL.
    // Порядок вставки КЛП: Where(всегда true) для kNone повторяет форму исходного дерева.
    template <typename Func>
    BinarySearchTree<T> Where(Func predicate) const {
        BinarySearchTree<T> result(balancing_);
        Traverse(TraversalOrder::kNLR, [&result, &predicate](const T& value) {
            if (predicate(value))
                result.Insert(value);
        });
        return result;
    }

    // Свёртка, как в ТЗ: reduce(f, [a1, ..., an], c) = f(an, f(a(n-1), ... f(a2, f(a1, c))...)).
    // То есть func(элемент, накопленное) -> новое накопленное, начиная с initial.
    // Элементы берутся в порядке order (по умолчанию ЛКП - по возрастанию). O(n).
    // T2 - тип результата, выводится из initial: сумма int в long long - Reduce(f, 0LL).
    // Для пустого дерева возвращает initial.
    template <typename T2, typename Func>
    T2 Reduce(Func func, T2 initial, TraversalOrder order = TraversalOrder::kLNR) const {
        T2 accumulator = std::move(initial);
        Traverse(order, [&accumulator, &func](const T& value) { accumulator = func(value, accumulator); });
        return accumulator;
    }

    // ---------- Сохранение в строку и чтение из строки ----------

    // Сохранить дерево в строку по формату (см. TreeFormat.hpp), например:
    //   дерево 5 / 3 8, формат "{К}(Л)[П]"  ->  "{5}({3}()[])[{8}()[]]"
    //   то же дерево, формат "(Л){К}[П]"    ->  "((){3}[]){5}[(){8}[]]"
    // Пустое поддерево - пустые скобки, пустое дерево - пустая строка.
    // Закрывающая скобка К и '\' внутри значений экранируются через '\'.
    // Бросает TreeFormatError, если формат неверный. O(n), без рекурсии.
    std::string ToFormattedString(const std::string& format = kDefaultTreeFormat) const {
        const TreeFormat parsed = ParseTreeFormat(format);
        std::string result;

        // Как в TraverseNodes: для узла помним, сколько групп формата уже выведено
        struct Frame {
            const Node* node;
            std::size_t done_parts;
        };
        DynamicArray<Frame> stack;
        if (root_)
            stack.Append(Frame{root_.get(), 0});
        while (!stack.IsEmpty()) {
            Frame& top = stack.GetLast();
            if (top.done_parts == 3) {
                stack.RemoveLast();
                // Узел выведен целиком - значит, закончилось поддерево родителя:
                // закрываем ту группу родителя, внутри которой мы были
                if (!stack.IsEmpty())
                    result += parsed.parts[stack.GetLast().done_parts - 1].close;
                continue;
            }
            const Node* node = top.node; // копируем до Append (см. TraverseNodes)
            const FormatPart part = parsed.parts[top.done_parts];
            ++top.done_parts;

            result += part.open;
            if (part.step == TraversalStep::kNode) {
                result += EscapeText(ToText(node->value), part.close);
                result += part.close;
                continue;
            }
            const Node* child = (part.step == TraversalStep::kLeft) ? node->left.get() : node->right.get();
            if (child != nullptr)
                stack.Append(Frame{child, 0}); // скобку закроем, когда ребёнок закончится
            else
                result += part.close;          // пустое поддерево: "()"
        }
        return result;
    }

    // Прочитать дерево из строки фиксированного формата "{К}(Л)[П]" (см. TreeParser.hpp).
    // Значения вставляются в порядке КЛП, поэтому при Balancing::kNone форма дерева
    // совпадает со строкой. В режиме kAvl дерево само перебалансируется: элементы те же,
    // а форма может отличаться. Бросает TreeParseError (с позицией ошибки), если строка
    // неверна или описывает не дерево поиска. O(n log n) для AVL.
    static BinarySearchTree<T> FromString(const std::string& text, Balancing balancing = Balancing::kAvl) {
        DynamicArray<T> values = ParseTreeString<T>(text);
        BinarySearchTree<T> tree(balancing);
        for (const T& value : values)
            tree.Insert(value);
        return tree;
    }

    // ---------- Поддеревья ----------

    // Копия поддерева с корнем в узле, равном value. Исходное дерево не меняется.
    // Режим балансировки - как у исходного. Поддерево AVL-дерева - тоже корректное
    // AVL-дерево (каждый его узел сбалансирован), поэтому форму можно копировать как есть.
    // Бросает ElementNotFound, если value нет. O(h + m), m - размер поддерева.
    BinarySearchTree<T> ExtractSubtree(const T& value) const {
        const Node* node = FindNode(value);
        if (node == nullptr)
            throw ElementNotFound("BinarySearchTree::ExtractSubtree: element not found");
        BinarySearchTree<T> result(balancing_);
        result.root_ = CopySubtree(node);
        result.size_ = node->size;
        return result;
    }

    // Есть ли в дереве узел, поддерево которого полностью (по форме и значениям,
    // вместе с листьями) совпадает с pattern. Пустой pattern содержится в любом дереве.
    // В дереве поиска значения не повторяются, поэтому кандидат в корень вхождения
    // ровно один: находим его спуском за O(h) и сравниваем поддеревья за O(m).
    // Итого O(h + m), а не O(n * m), как в произвольном бинарном дереве.
    bool ContainsSubtree(const BinarySearchTree<T>& pattern) const {
        if (!pattern.root_)
            return true;
        const Node* candidate = FindNode(pattern.root_->value);
        if (candidate == nullptr || candidate->size != pattern.size_)
            return false; // размеры в узлах есть - разное число узлов видно сразу
        return SameSubtrees(candidate, pattern.root_.get());
    }

    // ---------- Слияние ----------

    // Добавить в дерево все элементы other (объединение множеств).
    // Возвращает, сколько элементов действительно добавилось (повторы не считаются).
    // Порядок вставки КЛП - по той же причине, что в Map. O(m log(n + m)) для AVL.
    std::size_t Merge(const BinarySearchTree<T>& other) {
        if (&other == this)
            return 0; // a.Merge(a): всё и так уже есть
        std::size_t added = 0;
        other.Traverse(TraversalOrder::kNLR, [this, &added](const T& value) {
            if (Insert(value))
                ++added;
        });
        return added;
    }

    // ---------- Операторы ----------
    // Составные присваивания - члены класса: они меняют левый операнд (*this).
    // +, ==, !=, << - свободные функции (объявлены после класса).

    BinarySearchTree<T>& operator+=(const T& value) {
        Insert(value);
        return *this;
    }

    BinarySearchTree<T>& operator-=(const T& value) {
        Remove(value);
        return *this;
    }

    BinarySearchTree<T>& operator+=(const BinarySearchTree<T>& other) {
        Merge(other);
        return *this;
    }

    // ---------- Изменение ----------

    // true - вставили, false - такое значение уже было (дерево не изменилось)
    bool Insert(const T& value) {
        bool inserted = InsertInto(root_, value);
        if (inserted) {
            ++size_;
            ResetThreading(); // дерево изменилось - прошивка устарела
        }
        return inserted;
    }

    // true - удалили, false - такого значения не было
    bool Remove(const T& value) {
        bool removed = RemoveFrom(root_, value);
        if (removed) {
            --size_;
            ResetThreading(); // иначе в цепочке остался бы next на удалённый узел
        }
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
        ResetThreading();
    }

    void Swap(BinarySearchTree<T>& other) noexcept {
        std::swap(root_, other.root_);
        std::swap(size_, other.size_);
        std::swap(balancing_, other.balancing_);
        std::swap(thread_head_, other.thread_head_);
        std::swap(thread_order_, other.thread_order_);
        std::swap(is_threaded_, other.is_threaded_);
    }

    // ---------- Печать структуры ---------- //TODO: Может сразу сделаем так, чтобы дерево выводилось правильно? То есть корень посередине сверху, левое поддерево слева, а правое - справа
    // Дерево "лёжа на боку": корень слева, правое поддерево сверху, левое снизу.
    //     ┌── 9
    // ┌── 8
    // │   └── 7
    // 5
    // │   ┌── 4
    // └── 3
    //     └── 1
    void PrintStructure(std::ostream& out) const {
        if (!root_) {
            out << "(empty)\n";
            return;
        }
        if (root_->right)
            PrintBranch(out, root_->right.get(), "", false);
        out << root_->value << "\n";
        if (root_->left)
            PrintBranch(out, root_->left.get(), "", true);
    }

    // ---------- Проверка корректности (для тестов) ----------
    // true, если для каждого узла выполняется свойство дерева поиска
    // и число узлов совпадает с size_
    bool IsValid() const {
        int height = 0;
        std::size_t count = 0;
        return CheckSubtree(root_.get(), nullptr, nullptr, height, count) && count == size_;
    }

private:
    void CheckNotEmpty() const {
        if (size_ == 0)
            throw EmptyCollection("BinarySearchTree: tree is empty");
    }

    void CheckThreaded() const {
        if (!is_threaded_)
            throw TreeNotThreaded("BinarySearchTree: tree is not threaded (call Thread first)");
    }

    // Старые next в узлах не стираем (это O(n)): пока is_threaded_ == false, их никто не читает,
    // а Thread перезаписывает next у каждого узла.
    void ResetThreading() noexcept {
        thread_head_ = nullptr;
        is_threaded_ = false;
    }

    // Эквивалентность через operator<: ни a < b, ни b < a
    static bool Equivalent(const T& a, const T& b) { return !(a < b) && !(b < a); }

    // Совпадают ли два поддерева по форме и значениям. Без рекурсии: свой стек пар узлов.
    static bool SameSubtrees(const Node* first, const Node* second) {
        struct NodePair {
            const Node* first;
            const Node* second;
        };
        DynamicArray<NodePair> stack;
        stack.Append(NodePair{first, second});
        while (!stack.IsEmpty()) {
            NodePair pair = stack.GetLast(); // копия: дальше будет Append
            stack.RemoveLast();
            if (pair.first == nullptr || pair.second == nullptr) {
                if (pair.first != pair.second)
                    return false; // с одной стороны узел есть, с другой - нет
                continue;
            }
            if (pair.first->size != pair.second->size || !Equivalent(pair.first->value, pair.second->value))
                return false;
            stack.Append(NodePair{pair.first->left.get(), pair.second->left.get()});
            stack.Append(NodePair{pair.first->right.get(), pair.second->right.get()});
        }
        return true;
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

    // Обход в любом из 6 порядков БЕЗ рекурсии, со своим стеком.
    // Для каждого узла в стеке помним, сколько из трёх его действий (К, Л, П) уже сделано.
    // visit вызывается с указателем на узел.
    // NodePtr - тип указателя: const Node* (для обходов только чтения)
    // или Node* (для Thread, которая записывает в узлы next).
    template <typename NodePtr, typename Func>
    static void TraverseNodes(NodePtr root, TraversalOrder order, Func visit) {
        struct Frame {
            NodePtr node = nullptr;
            int done_steps = 0;
        };
        const TraversalSteps order_steps = GetTraversalSteps(order);

        DynamicArray<Frame> stack;
        if (root != nullptr)
            stack.Append(Frame{root, 0});
        while (!stack.IsEmpty()) {
            Frame& top = stack.GetLast();
            if (top.done_steps == 3) {
                stack.RemoveLast(); // все три действия в этом узле сделаны
                continue;
            }
            // Копируем всё нужное ДО Append: при росте массива ссылка top
            // станет висячей (старый буфер удаляется)
            NodePtr node = top.node;
            TraversalStep step = order_steps.steps[top.done_steps];
            ++top.done_steps;

            if (step == TraversalStep::kNode)
                visit(node);
            else if (step == TraversalStep::kLeft && node->left)
                stack.Append(Frame{node->left.get(), 0});
            else if (step == TraversalStep::kRight && node->right)
                stack.Append(Frame{node->right.get(), 0});
        }
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

    static std::size_t Size(const Node* node) { return node ? node->size : 0; }

    // Пересчитать высоту и размер узла по детям (у детей они должны быть уже правильными)
    static void UpdateNode(Node* node) {
        int left_height = Height(node->left.get());
        int right_height = Height(node->right.get());
        node->height = 1 + (left_height > right_height ? left_height : right_height);
        node->size = 1 + Size(node->left.get()) + Size(node->right.get());
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
        UpdateNode(node.get());                               // node теперь ниже L
        new_root->right = std::move(node);                      // node становится правым ребёнком L
        UpdateNode(new_root.get());
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
        UpdateNode(node.get());
        new_root->left = std::move(node);
        UpdateNode(new_root.get());
        node = std::move(new_root);
    }

    // Вызывается для каждого узла на пути от изменённого места к корню.
    // Высоты детей node к этому моменту уже правильные.
    void Rebalance(std::unique_ptr<Node>& node) {
        UpdateNode(node.get());
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
        copy->size = node->size; 
        copy->left = CopySubtree(node->left.get());
        copy->right = CopySubtree(node->right.get());
        return copy;
    }

    // Одна ветка для PrintStructure.
    // prefix - то, что печатается слева от узла (вертикальные линии предков);
    // is_left - узел левый ребёнок (рисуется снизу, "└──") или правый (сверху, "┌──").
    static void PrintBranch(std::ostream& out, const Node* node, const std::string& prefix, bool is_left) {
        if (node->right)
            PrintBranch(out, node->right.get(), prefix + (is_left ? "│   " : "    "), false);
        out << prefix << (is_left ? "└── " : "┌── ") << node->value << "\n";
        if (node->left)
            PrintBranch(out, node->left.get(), prefix + (is_left ? "    " : "│   "), true);
    }

        // Проверяет поддерево; если всё верно, записывает в height и count его настоящие
    // высоту и число узлов и возвращает true.
    // min и max - границы, в которых должны лежать значения поддерева (nullptr - границы нет).
    // Проверять только "левый ребёнок < узел" мало: внук может нарушить порядок относительно деда.
    bool CheckSubtree(const Node* node, const T* min, const T* max, int& height, std::size_t& count) const {
        height = 0;
        count = 0;
        if (node == nullptr)
            return true;
        if (min != nullptr && !(*min < node->value))
            return false;
        if (max != nullptr && !(node->value < *max))
            return false;

        int left_height = 0;
        int right_height = 0;
        std::size_t left_count = 0;
        std::size_t right_count = 0;
        if (!CheckSubtree(node->left.get(), min, &node->value, left_height, left_count))
            return false;
        if (!CheckSubtree(node->right.get(), &node->value, max, right_height, right_count))
            return false;

        height = 1 + (left_height > right_height ? left_height : right_height);
        count = 1 + left_count + right_count;
        if (node->height != height || node->size != count)
            return false;
        int difference = left_height - right_height;
        if (balancing_ == Balancing::kAvl && (difference > 1 || difference < -1))
            return false;
        return true;
    }

    std::unique_ptr<Node> root_; // по умолчанию = nullptr
    std::size_t size_ = 0;
    Balancing balancing_ = Balancing::kAvl;

    // Прошивка
    Node* thread_head_ = nullptr;                         // первый узел цепочки next
    TraversalOrder thread_order_ = TraversalOrder::kLNR;  // в каком порядке прошито
    bool is_threaded_ = false;                            // цепочка соответствует дереву
};

// ========== Свободные операторы ==========

// Объединение: новое дерево из элементов a и b (режим балансировки - как у a).
// Выражено через +=, как принято: логика объединения написана один раз (Merge).
template <typename T>
BinarySearchTree<T> operator+(const BinarySearchTree<T>& a, const BinarySearchTree<T>& b) {
    BinarySearchTree<T> result(a);
    result += b;
    return result;
}

// Равенство МНОЖЕСТВ элементов, форма не важна (как у std::set): AVL-дерево и
// обычное дерево из одних и тех же значений равны. Сравнить форму можно через
// ContainsSubtree или ToFormattedString. O(n): два итератора идут по возрастанию параллельно.
template <typename T>
bool operator==(const BinarySearchTree<T>& a, const BinarySearchTree<T>& b) {
    if (a.GetCount() != b.GetCount())
        return false;
    // GetEnumerator создаёт итератор в куче; unique_ptr удалит его сам, даже при исключении
    std::unique_ptr<IEnumerator<T>> first(a.GetEnumerator());
    std::unique_ptr<IEnumerator<T>> second(b.GetEnumerator());
    while (first->MoveNext() && second->MoveNext()) {
        const T& x = first->Current();
        const T& y = second->Current();
        if (x < y || y < x)
            return false;
    }
    return true;
}

template <typename T>
bool operator!=(const BinarySearchTree<T>& a, const BinarySearchTree<T>& b) {
    return !(a == b);
}

// Печать элементов по возрастанию: {1, 3, 4, 5}; пустое дерево - {}.
// Не может быть членом класса: у члена левый операнд - сам объект, а здесь слева поток.
template <typename T>
std::ostream& operator<<(std::ostream& out, const BinarySearchTree<T>& tree) {
    out << '{';
    bool first = true;
    tree.Traverse(TraversalOrder::kLNR, [&out, &first](const T& value) {
        out << (first ? "" : ", ") << ToText(value);
        first = false;
    });
    return out << '}';
}
