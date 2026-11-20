// ========== Замеры скорости дерева ==========
// Собирать ТОЛЬКО в Release без санитайзеров, иначе цифры бессмысленны:
//   cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DENABLE_SANITIZERS=OFF
//   cmake --build build-release --target benchmarks
//   ./build-release/benchmarks            -> results/benchmarks.csv, results/memory.csv
//   ./build-release/benchmarks --huge     -> results/huge.csv (10^8 элементов, нужно ~6 ГБ памяти)
//   ./build-release/benchmarks --max 100000   (быстрый прогон до 10^5)
// Графики по CSV рисует bench/plot.py (matplotlib).
//
// Как меряем:
// * часы std::chrono::steady_clock - монотонные, их не сдвигает синхронизация времени;
// * каждое измерение повторяется несколько раз, в CSV идёт МЕДИАНА (выбросы от
//   других процессов на неё почти не влияют, в отличие от среднего);
// * в замер входит только сама операция: подготовка данных и удаление дерева - вне замера;
// * результаты обходов и поиска складываются в volatile-переменную, чтобы компилятор
//   не выкинул "бесполезный" цикл;
// * входные данные - числа 0..n-1, перемешанные генератором с фиксированным seed:
//   все запуски получают одни и те же данные.

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>

#if defined(__APPLE__)
#include <malloc/malloc.h> // malloc_zone_statistics
#elif defined(__GLIBC__)
#include <malloc.h> // mallinfo2
#endif

#include "lab2_files/DynamicArray.hpp"
#include "tree/BinarySearchTree.hpp"

namespace {

using Tree = BinarySearchTree<int>;
using Clock = std::chrono::steady_clock;

volatile long long g_sink = 0; // сюда складываем результаты, чтобы цикл не выкинули

class Stopwatch {
public:
    Stopwatch() : start_(Clock::now()) {}
    double Seconds() const { return std::chrono::duration<double>(Clock::now() - start_).count(); }

private:
    Clock::time_point start_;
};

// Медиана: сортировка вставками (повторов всего 1..7) и средний элемент
double Median(DynamicArray<double> values) {
    for (std::size_t i = 1; i < values.GetSize(); ++i) {
        for (std::size_t j = i; j > 0 && values[j] < values[j - 1]; --j)
            std::swap(values[j], values[j - 1]);
    }
    return values[values.GetSize() / 2];
}

// Больше повторов там, где один замер быстрый и шумный
int RepeatsFor(std::size_t n) {
    if (n <= 100'000)
        return 7;
    if (n <= 1'000'000)
        return 5;
    return 3;
}

// Повторить measure() repeats раз (measure сам возвращает время в секундах) и взять медиану.
// Долгие замеры (обычное дерево на отсортированных данных - секунды на один прогон)
// останавливаем после трёх повторов, если уже потрачено больше kTimeBudget.
constexpr double kTimeBudget = 3.0;

template <typename Measure>
double MedianOf(int repeats, Measure measure) {
    DynamicArray<double> times;
    double total = 0;
    for (int i = 0; i < repeats; ++i) {
        times.Append(measure());
        total += times.GetLast();
        if (times.GetSize() >= 3 && total > kTimeBudget)
            break;
    }
    return Median(times);
}

// ---------- Входные данные ----------
DynamicArray<int> Sorted(std::size_t n) {
    DynamicArray<int> values(n);
    for (std::size_t i = 0; i < n; ++i)
        values[i] = static_cast<int>(i);
    return values;
}

DynamicArray<int> Reversed(std::size_t n) {
    DynamicArray<int> values(n);
    for (std::size_t i = 0; i < n; ++i)
        values[i] = static_cast<int>(n - 1 - i);
    return values;
}

// Перемешивание Фишера-Йетса: каждая перестановка равновероятна
DynamicArray<int> Shuffled(std::size_t n, unsigned seed) {
    DynamicArray<int> values = Sorted(n);
    std::mt19937_64 rng(seed);
    for (std::size_t i = n; i > 1; --i) {
        std::uniform_int_distribution<std::size_t> pick(0, i - 1);
        std::swap(values[i - 1], values[pick(rng)]);
    }
    return values;
}

Tree Build(const DynamicArray<int>& input, Balancing balancing) {
    Tree tree(balancing);
    for (std::size_t i = 0; i < input.GetSize(); ++i)
        tree.Insert(input[i]);
    return tree;
}

// Размеры 1-2-5: 1000, 2000, 5000, 10000, ... - равномерно на логарифмической оси
DynamicArray<std::size_t> Sizes(std::size_t from, std::size_t to) {
    DynamicArray<std::size_t> sizes;
    const std::size_t steps[] = {1, 2, 5};
    for (std::size_t power = 1; power <= to; power *= 10) {
        for (std::size_t step : steps) {
            std::size_t n = power * step;
            if (n >= from && n <= to)
                sizes.Append(n);
        }
    }
    return sizes;
}

// ---------- Вывод ----------
class Results {
public:
    explicit Results(const std::string& path) : out_(path) {
        if (!out_)
            throw std::runtime_error("cannot open " + path);
        out_ << "test,variant,n,seconds,height\n";
    }

    void Add(const char* test, const std::string& variant, std::size_t n, double seconds, std::size_t height = 0) {
        out_ << test << ',' << variant << ',' << n << ',' << seconds << ',' << height << '\n';
        out_.flush(); // если прогон прервут, уже снятые точки останутся в файле
        std::printf("  %-10s %-16s n = %10zu  %10.4f с  (%7.1f нс на элемент)", test, variant.c_str(), n, seconds,
                    seconds * 1e9 / static_cast<double>(n));
        if (height > 0)
            std::printf("  высота %zu", height);
        std::printf("\n");
        std::fflush(stdout);
    }

private:
    std::ofstream out_;
};

// ---------- Замеры ----------

// Построение дерева вставками по одной. Высота - у построенного дерева.
void BenchBuild(Results& results, const char* variant, const DynamicArray<int>& input, Balancing balancing) {
    std::size_t height = 0;
    double seconds = MedianOf(RepeatsFor(input.GetSize()), [&] {
        Tree tree(balancing);
        Stopwatch watch;
        for (std::size_t i = 0; i < input.GetSize(); ++i)
            tree.Insert(input[i]);
        double elapsed = watch.Seconds();
        height = tree.GetHeight();
        return elapsed; // дерево удаляется здесь, вне замера
    });
    results.Add("build", variant, input.GetSize(), seconds, height);
}

// Поиск каждого из n элементов в случайном порядке
void BenchSearch(Results& results, const char* variant, const Tree& tree, const DynamicArray<int>& queries) {
    double seconds = MedianOf(RepeatsFor(queries.GetSize()), [&] {
        long long found = 0;
        Stopwatch watch;
        for (std::size_t i = 0; i < queries.GetSize(); ++i)
            found += tree.Contains(queries[i]) ? 1 : 0;
        double elapsed = watch.Seconds();
        g_sink = g_sink + found;
        return elapsed;
    });
    results.Add("search", variant, queries.GetSize(), seconds, tree.GetHeight());
}

// Удаление всех n элементов в случайном порядке (дерево строится заново вне замера)
void BenchRemove(Results& results, const char* variant, const DynamicArray<int>& input,
                 const DynamicArray<int>& order, Balancing balancing) {
    double seconds = MedianOf(RepeatsFor(input.GetSize()), [&] {
        Tree tree = Build(input, balancing);
        Stopwatch watch;
        for (std::size_t i = 0; i < order.GetSize(); ++i)
            tree.Remove(order[i]);
        return watch.Seconds();
    });
    results.Add("remove", variant, input.GetSize(), seconds);
}

// Get(i) для всех i: каждый вызов - спуск от корня по размерам поддеревьев, O(log n)
void BenchGet(Results& results, const Tree& tree) {
    double seconds = MedianOf(RepeatsFor(tree.GetCount()), [&] {
        long long sum = 0;
        Stopwatch watch;
        for (std::size_t i = 0; i < tree.GetCount(); ++i)
            sum += tree.Get(i);
        double elapsed = watch.Seconds();
        g_sink = g_sink + sum;
        return elapsed;
    });
    results.Add("ops", "get_index", tree.GetCount(), seconds);
}

// Три способа обойти все n элементов + сама прошивка.
// Важно, КАК узлы лежат в памяти:
// * дерево из случайных данных - узлы, соседние по порядку, разбросаны по куче.
//   Обход по прошивке - цепочка "прочитай узел -> узнай адрес следующего": каждый шаг
//   ждёт загрузки из оперативной памяти (~80 нс), процессор не может забежать вперёд.
//   Обход со стеком быстрее: адреса правых поддеревьев уже известны, и процессор
//   загружает несколько узлов параллельно;
// * дерево из отсортированных данных (suffix "_ordered") - узлы выделены подряд,
//   цепочка next идёт по памяти почти последовательно, и прошивка - самый быстрый обход.
void BenchTraverse(Results& results, Tree& tree, const std::string& suffix) {
    const std::size_t n = tree.GetCount();
    const int repeats = RepeatsFor(n);

    double iterator = MedianOf(repeats, [&] {
        long long sum = 0;
        Stopwatch watch;
        for (const int& value : tree) // IEnumerator: стек в куче, виртуальные вызовы
            sum += value;
        double elapsed = watch.Seconds();
        g_sink = g_sink + sum;
        return elapsed;
    });
    results.Add("traverse", "iterator" + suffix, n, iterator);

    double traverse = MedianOf(repeats, [&] {
        long long sum = 0;
        Stopwatch watch;
        tree.Traverse(TraversalOrder::kLNR, [&sum](const int& value) { sum += value; });
        double elapsed = watch.Seconds();
        g_sink = g_sink + sum;
        return elapsed;
    });
    results.Add("traverse", "traverse" + suffix, n, traverse);

    double thread = MedianOf(repeats, [&] {
        Stopwatch watch;
        tree.Thread(TraversalOrder::kLNR);
        return watch.Seconds();
    });
    results.Add("traverse", "thread_build" + suffix, n, thread);

    double threaded = MedianOf(repeats, [&] {
        long long sum = 0;
        Stopwatch watch;
        tree.TraverseThreaded([&sum](const int& value) { sum += value; });
        double elapsed = watch.Seconds();
        g_sink = g_sink + sum;
        return elapsed;
    });
    results.Add("traverse", "threaded" + suffix, n, threaded);
}

// ---------- Память ----------

// Сколько байт сейчас занято в куче (0, если на этой системе узнать нельзя)
std::size_t HeapInUse() {
#if defined(__APPLE__)
    malloc_statistics_t stats{};
    malloc_zone_statistics(nullptr, &stats); // nullptr - сумма по всем зонам
    return stats.size_in_use;
#elif defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 33))
    return mallinfo2().uordblks;
#else
    return 0;
#endif
}

// Реальный расход памяти на узел: занятая куча до и после построения AVL из n элементов.
// Больше sizeof(узла), потому что malloc округляет размер блока вверх.
void BenchMemory(const std::string& path, std::size_t max_n) {
    std::ofstream out(path);
    out << "n,heap_bytes,bytes_per_node,sizeof_node\n";
    std::printf("\nПамять: sizeof(TreeNode<int>) = %zu байт\n", sizeof(TreeNode<int>));
    double last_per_node = 0;
    for (std::size_t n = 1000; n <= max_n; n *= 10) {
        DynamicArray<int> input = Shuffled(n, 7);
        std::size_t before = HeapInUse();
        Tree tree = Build(input, Balancing::kAvl);
        std::size_t after = HeapInUse();
        if (before == 0 && after == 0) {
            std::printf("  на этой системе занятую кучу узнать нельзя - замер памяти пропущен\n");
            return;
        }
        std::size_t used = after - before;
        last_per_node = static_cast<double>(used) / static_cast<double>(n);
        out << n << ',' << used << ',' << last_per_node << ',' << sizeof(TreeNode<int>) << '\n';
        std::printf("  n = %10zu  куча +%12zu байт  = %.1f байт на узел\n", n, used, last_per_node);
    }
    std::printf("  => 10^8 узлов займут около %.1f ГБ\n", last_per_node * 1e8 / 1e9);
}

// ---------- Режимы ----------

void RunAll(const std::string& dir, std::size_t max_n) {
    Results results(dir + "/benchmarks.csv");

    // Обычное дерево на отсортированных данных - "палка": O(n^2) на построение
    // и рекурсия InsertInto глубиной n. Поэтому только до 50 000, а обратный порядок
    // (зеркальный случай, та же "палка", только влево) - до 20 000.
    const std::size_t degenerate_max = max_n < 50'000 ? max_n : 50'000;
    const std::size_t reversed_max = 20'000;

    // Прогрев: первый замер в процессе всегда медленнее (malloc ещё не набрал памяти
    // у системы, процессор не поднял частоту). Делаем холостую работу и выкидываем её.
    {
        Tree warmup = Build(Shuffled(1'000'000, 3), Balancing::kAvl);
        g_sink = g_sink + static_cast<long long>(warmup.GetHeight());
    }

    std::printf("Построение дерева (вставка n элементов)\n");
    for (std::size_t n : Sizes(1000, max_n)) {
        DynamicArray<int> random = Shuffled(n, 1);
        BenchBuild(results, "avl_random", random, Balancing::kAvl);
        BenchBuild(results, "avl_sorted", Sorted(n), Balancing::kAvl);
        BenchBuild(results, "avl_reversed", Reversed(n), Balancing::kAvl);
        BenchBuild(results, "plain_random", random, Balancing::kNone);
        if (n <= degenerate_max)
            BenchBuild(results, "plain_sorted", Sorted(n), Balancing::kNone);
        if (n <= degenerate_max && n <= reversed_max)
            BenchBuild(results, "plain_reversed", Reversed(n), Balancing::kNone);
    }

    std::printf("\nПоиск, удаление, Get(i), обходы\n");
    for (std::size_t n : Sizes(1000, max_n)) {
        DynamicArray<int> input = Shuffled(n, 1);
        DynamicArray<int> queries = Shuffled(n, 2); // другой порядок, чем при вставке

        Tree avl = Build(input, Balancing::kAvl);
        Tree plain = Build(input, Balancing::kNone);
        BenchSearch(results, "avl", avl, queries);
        BenchSearch(results, "plain", plain, queries);
        if (n <= degenerate_max) {
            Tree degenerate = Build(Sorted(n), Balancing::kNone);
            BenchSearch(results, "plain_sorted", degenerate, queries);
        }
        BenchRemove(results, "avl", input, queries, Balancing::kAvl);
        BenchRemove(results, "plain", input, queries, Balancing::kNone);
        if (n <= 1'000'000)
            BenchGet(results, avl); // n log n с большой константой - 10^7 не добавляет смысла
        BenchTraverse(results, avl, "");
        Tree ordered = Build(Sorted(n), Balancing::kAvl); // узлы в памяти по порядку
        BenchTraverse(results, ordered, "_ordered");
    }

    BenchMemory(dir + "/memory.csv", max_n);
}

// 10^8 элементов: по одному повтору, только AVL (обычное дерево на случайных
// данных ведёт себя так же, а на отсортированных до 10^8 не дойти - n^2).
void RunHuge(const std::string& dir) {
    const std::size_t n = 100'000'000;
    Results results(dir + "/huge.csv");
    std::printf("10^8 элементов (по одному повтору, займёт несколько минут)\n");

    DynamicArray<int> sorted = Sorted(n);
    {
        Stopwatch watch;
        Tree tree = Build(sorted, Balancing::kAvl);
        results.Add("build", "avl_sorted", n, watch.Seconds(), tree.GetHeight());
    }

    DynamicArray<int> random = Shuffled(n, 1);
    Stopwatch watch;
    Tree tree = Build(random, Balancing::kAvl);
    results.Add("build", "avl_random", n, watch.Seconds(), tree.GetHeight());

    DynamicArray<int> queries = Shuffled(n, 2);
    long long found = 0;
    Stopwatch search;
    for (std::size_t i = 0; i < n; ++i)
        found += tree.Contains(queries[i]) ? 1 : 0;
    results.Add("search", "avl", n, search.Seconds(), tree.GetHeight());
    g_sink = g_sink + found;

    long long sum = 0;
    Stopwatch traverse;
    tree.Traverse(TraversalOrder::kLNR, [&sum](const int& value) { sum += value; });
    results.Add("traverse", "traverse", n, traverse.Seconds());
    g_sink = g_sink + sum;
}

} // namespace

int main(int argc, char** argv) {
    std::string dir = "results";
    std::size_t max_n = 10'000'000;
    bool huge = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--huge") == 0) {
            huge = true;
        } else if (std::strcmp(argv[i], "--max") == 0 && i + 1 < argc) {
            max_n = std::strtoull(argv[++i], nullptr, 10);
        } else if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            dir = argv[++i];
        } else {
            std::fprintf(stderr, "usage: %s [--max N] [--out DIR] [--huge]\n", argv[0]);
            return 2;
        }
    }
    if (max_n < 1000) {
        std::fprintf(stderr, "--max must be at least 1000\n");
        return 2;
    }

#ifndef NDEBUG
    std::printf("ВНИМАНИЕ: сборка без оптимизаций (Debug) - цифры будут завышены.\n"
                "Нужно: cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DENABLE_SANITIZERS=OFF\n\n");
#endif

    try {
        std::filesystem::create_directories(dir);
        if (huge)
            RunHuge(dir);
        else
            RunAll(dir, max_n);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "error: %s\n", error.what());
        return 1;
    }
    std::printf("\nГотово, CSV в папке %s/\n", dir.c_str());
    return 0;
}
