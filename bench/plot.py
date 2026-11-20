"""Графики по результатам замеров (bench/benchmarks.cpp).

Запуск из корня лабы:
    .venv/bin/python bench/plot.py   # читает results/*.csv, пишет results/benchmark_results/*.png

matplotlib стоит в виртуальном окружении .venv внутри папки лабы:
    python3 -m venv .venv && .venv/bin/pip install matplotlib

Графики:
    build.png     - время построения дерева от n (обе оси логарифмические) + n log n и n^2
    height.png    - высота дерева от n и теоретические границы AVL
    search.png    - время одного поиска: AVL, обычное дерево, "палка"
    ops.png       - время одной операции AVL: вставка, поиск, удаление, Get(i)
    traverse.png  - время на элемент для трёх способов обхода и для прошивки:
                    узлы разбросаны по памяти / лежат по порядку
"""

import csv
import math
import sys
from collections import defaultdict
from pathlib import Path

import matplotlib

matplotlib.use("Agg")  # без окна: только файлы
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.ticker import FuncFormatter  # noqa: E402

RESULTS = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("results")
PLOTS = RESULTS / "benchmark_results"  # сюда кладём графики (CSV остаются в results/)

# Цвета: фиксированный порядок категориальной палитры (проверена на различимость
# при дальтонизме), текст и сетка - нейтральные серые.
SURFACE = "#fcfcfb"
TEXT = "#0b0b0b"
TEXT_SECONDARY = "#52514e"
GRID = "#e4e3df"
REFERENCE = "#8a8984"
SERIES = ["#2a78d6", "#eb6834", "#1baf7a", "#eda100"]

plt.rcParams.update({
    "figure.facecolor": SURFACE,
    "axes.facecolor": SURFACE,
    "savefig.facecolor": SURFACE,
    "axes.edgecolor": GRID,
    "axes.labelcolor": TEXT_SECONDARY,
    "axes.titlecolor": TEXT,
    "axes.titlesize": 13,
    "axes.titleweight": "bold",
    "axes.titlelocation": "left",
    "axes.titlepad": 26,
    "axes.labelsize": 10.5,
    "axes.grid": True,
    "axes.spines.top": False,
    "axes.spines.right": False,
    "grid.color": GRID,
    "grid.linewidth": 0.8,
    "xtick.color": TEXT_SECONDARY,
    "ytick.color": TEXT_SECONDARY,
    "xtick.labelsize": 9.5,
    "ytick.labelsize": 9.5,
    "legend.frameon": False,
    "legend.fontsize": 9.5,
    "legend.labelcolor": TEXT,
    "lines.linewidth": 1.8,
    "lines.markersize": 5,
    "font.size": 10,
})


def load(path):
    """{(test, variant): [(n, seconds, height), ...]} отсортировано по n."""
    data = defaultdict(list)
    if not path.exists():
        return data
    with path.open(newline="") as file:
        for row in csv.DictReader(file):
            key = (row["test"], row["variant"])
            data[key].append((int(row["n"]), float(row["seconds"]), int(row["height"])))
    for points in data.values():
        points.sort()
    return data


def power_of_ten(value, _position=None):
    """Подписи оси n: 10³, 10⁴, ... (а не 1000, 10000)."""
    if value <= 0:
        return ""
    exponent = math.log10(value)
    if abs(exponent - round(exponent)) > 1e-9:
        return ""
    digits = str(round(exponent)).translate(str.maketrans("0123456789-", "⁰¹²³⁴⁵⁶⁷⁸⁹⁻"))
    return f"10{digits}"


def setup_n_axis(ax):
    ax.set_xscale("log")
    ax.xaxis.set_major_formatter(FuncFormatter(power_of_ten))
    ax.xaxis.set_minor_formatter(FuncFormatter(lambda *_: ""))
    ax.set_xlabel("число элементов n")


def subtitle(ax, text):
    ax.text(0, 1.02, text, transform=ax.transAxes, color=TEXT_SECONDARY, fontsize=9.5, va="bottom")


def line(ax, points, slot, label, per_element=False, scale=1.0):
    """Линия серии + подпись у последней точки (цвет серии - только у линии, текст серый)."""
    xs = [n for n, _, _ in points]
    ys = [(s / n if per_element else s) * scale for n, s, _ in points]
    color = SERIES[slot]
    ax.plot(xs, ys, color=color, marker="o", label=label,
            markeredgecolor=SURFACE, markeredgewidth=1.2, zorder=3)
    return xs, ys


def end_label(ax, x, y, text, dy=0):
    ax.annotate(text, (x, y), xytext=(8, dy), textcoords="offset points",
                color=TEXT, fontsize=9, va="center")


def reference(ax, xs, ys, text, dy=0):
    # поверх линий данных: на графике высоты AVL из отсортированных данных
    # совпадает с нижней границей точно, и пунктир иначе не виден
    ax.plot(xs, ys, color=REFERENCE, linestyle=(0, (4, 3)), linewidth=1.2, zorder=4)
    end_label(ax, xs[-1], ys[-1], text, dy)


def save(fig, name):
    fig.tight_layout()
    path = PLOTS / name
    fig.savefig(path, dpi=150)
    plt.close(fig)
    print(f"  {path}")


def scaled_reference(points, func):
    """Теоретическая кривая func(n), подогнанная к средней точке серии
    (крайние точки шумнее: на малых n время - микросекунды)."""
    n0, s0, _ = points[len(points) // 2]
    factor = s0 / func(n0)
    xs = [n for n, _, _ in points]
    return xs, [func(n) * factor for n in xs]


# ---------- графики ----------

def plot_build(data):
    fig, ax = plt.subplots(figsize=(8.6, 5.4))
    series = [
        (("build", "avl_random"), 0, "AVL, случайные"),
        (("build", "avl_sorted"), 1, "AVL, отсортированные"),
        (("build", "plain_random"), 2, "обычное, случайные"),
        (("build", "plain_sorted"), 3, "обычное, отсортированные"),
    ]
    for key, slot, label in series:
        if data[key]:
            line(ax, data[key], slot, label)

    if data[("build", "avl_random")]:
        xs, ys = scaled_reference(data[("build", "avl_random")], lambda n: n * math.log2(n))
        reference(ax, xs, ys, "∼ n log n", dy=-10)
    if data[("build", "plain_sorted")]:
        xs, ys = scaled_reference(data[("build", "plain_sorted")], lambda n: n * n)
        reference(ax, xs, ys, "∼ n²")

    ax.set_yscale("log")
    setup_n_axis(ax)
    ax.set_ylabel("время построения, с")
    ax.set_title("Построение дерева вставкой n элементов")
    subtitle(ax, "Обычное дерево на отсортированных данных вырождается в «палку»: время растёт как n²")
    ax.legend(loc="upper left")
    save(fig, "build.png")


def plot_height(data):
    fig, ax = plt.subplots(figsize=(8.6, 5.2))
    series = [
        (("build", "avl_random"), 0, "AVL, случайные"),
        (("build", "avl_sorted"), 1, "AVL, отсортированные"),
        (("build", "plain_random"), 2, "обычное, случайные"),
    ]
    xs_all = []
    for key, slot, label in series:
        points = data[key]
        if not points:
            continue
        xs = [n for n, _, _ in points]
        ax.plot(xs, [h for _, _, h in points], color=SERIES[slot], marker="o", label=label,
                markeredgecolor=SURFACE, markeredgewidth=1.2, zorder=3)
        xs_all = max(xs_all, xs, key=len)

    if xs_all:
        # Идеально заполненное дерево высоты h вмещает 2^h - 1 узлов, поэтому ниже
        # ⌈log₂(n+1)⌉ высота быть не может ни у какого дерева из n узлов
        reference(ax, xs_all, [math.ceil(math.log2(n + 1)) for n in xs_all], "⌈log₂(n+1)⌉ — меньше нельзя", dy=-8)
        reference(ax, xs_all, [1.44 * math.log2(n + 2) for n in xs_all], "1.44·log₂(n+2) — предел AVL", dy=8)

    setup_n_axis(ax)
    ax.set_ylim(bottom=0)
    ax.set_ylabel("высота дерева")
    ax.set_title("Высота дерева")
    subtitle(ax, "У «палки» из отсортированных данных высота равна n, она на график не помещается")
    ax.legend(loc="upper left")
    save(fig, "height.png")


def plot_search(data):
    fig, ax = plt.subplots(figsize=(8.6, 5.2))
    series = [
        (("search", "avl"), 0, "AVL"),
        (("search", "plain"), 2, "обычное, построено из случайных"),
        (("search", "plain_sorted"), 3, "обычное, построено из отсортированных"),
    ]
    for key, slot, label in series:
        if data[key]:
            line(ax, data[key], slot, label, per_element=True, scale=1e9)

    ax.set_yscale("log")
    setup_n_axis(ax)
    ax.set_ylabel("время одного поиска, нс")
    ax.set_title("Поиск элемента (Contains)")
    subtitle(ax, "Ищем каждый из n элементов в случайном порядке; у «палки» поиск — O(n)")
    ax.legend(loc="upper left")
    save(fig, "search.png")


def plot_ops(data):
    fig, ax = plt.subplots(figsize=(8.6, 5.2))
    series = [
        (("build", "avl_random"), 0, "вставка"),
        (("search", "avl"), 1, "поиск"),
        (("remove", "avl"), 2, "удаление"),
        (("ops", "get_index"), 3, "Get(i)"),
    ]
    for key, slot, label in series:
        points = data[key]
        if points:
            xs, ys = line(ax, points, slot, label, per_element=True, scale=1e9)
            end_label(ax, xs[-1], ys[-1], label)

    setup_n_axis(ax)
    ax.set_ylim(bottom=0)
    ax.set_ylabel("время одной операции, нс")
    ax.set_title("Операции AVL-дерева, время на одну")
    subtitle(ax, "Теория — O(log n); рост круче на больших n из-за промахов кэша: дерево перестаёт помещаться в кэш")
    ax.legend(loc="upper left")
    save(fig, "ops.png")


def plot_traverse(data):
    # Две панели с общей логарифмической осью y: одно и то же дерево (AVL),
    # но узлы в памяти лежат по-разному
    fig, axes = plt.subplots(1, 2, figsize=(11.5, 5.4), sharey=True)
    panels = [
        ("", "Узлы разбросаны по памяти", "вставка в случайном порядке"),
        ("_ordered", "Узлы лежат в памяти по порядку", "вставка по возрастанию"),
    ]
    series = [
        ("iterator", 0, "итератор (for по дереву)"),
        ("traverse", 1, "Traverse (свой стек)"),
        ("thread_build", 2, "Thread — построение прошивки"),
        ("threaded", 3, "обход по прошивке"),
    ]
    for ax, (suffix, title, note) in zip(axes, panels):
        for variant, slot, label in series:
            points = data[("traverse", variant + suffix)]
            if points:
                line(ax, points, slot, label, per_element=True, scale=1e9)
        setup_n_axis(ax)
        ax.set_yscale("log")
        ax.yaxis.set_major_formatter(FuncFormatter(lambda v, _: f"{v:g}"))
        ax.set_title(f"{title} ({note})", fontsize=10.5, pad=8)
    axes[0].set_ylabel("время на один элемент, нс")
    axes[0].legend(loc="upper left")
    fig.suptitle("Обход всех n элементов AVL-дерева", x=0.01, ha="left",
                 fontsize=13, fontweight="bold", color=TEXT)
    fig.text(0.01, 0.875, "Прошивка — цепочка next: каждый шаг ждёт загрузки следующего узла.\n"
             "Узлы разбросаны — промах кэша на каждом шаге; лежат подряд — прошивка самый быстрый обход",
             ha="left", color=TEXT_SECONDARY, fontsize=9.5, linespacing=1.5)
    fig.tight_layout(rect=(0, 0, 1, 0.86))
    path = PLOTS / "traverse.png"
    fig.savefig(path, dpi=150)
    plt.close(fig)
    print(f"  {path}")


def main():
    data = load(RESULTS / "benchmarks.csv")
    if not data:
        sys.exit(f"нет {RESULTS / 'benchmarks.csv'} - сначала ./build-release/benchmarks")
    # 10^8 (если запускали benchmarks --huge) - продолжение тех же линий
    for key, points in load(RESULTS / "huge.csv").items():
        data[key] = sorted(data[key] + points)

    PLOTS.mkdir(parents=True, exist_ok=True)
    print("Графики:")
    plot_build(data)
    plot_height(data)
    plot_search(data)
    plot_ops(data)
    plot_traverse(data)


if __name__ == "__main__":
    main()
