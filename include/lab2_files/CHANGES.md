# CHANGES — ревизия контейнеров `lab2_files`

Дата: 2026-11-01. Исходные версии всех файлов сохранены в `backup_original/`
(после проверки эту папку можно удалить).

Цель ревизии: контейнеры из этой папки заменяют `std::vector` / `std::list` во всех
лабораторных 3-го семестра (бенчмарки ДЗ-1, тесты, вспомогательные коллекции).
Поэтому главное: чтобы они **компилировались**, были **корректны** (без утечек,
двойных удалений и висячих ссылок), **быстры** и **удобны** в роли «вектора».

---

## 1. Итог в двух словах

* **В исходном виде папка не компилировалась.** Код застрял посреди рефакторинга:
  `ArraySequence` уже перешёл на схему `Instance()` + `...Internal`, а `Sequence`,
  `DynamicArray` и `ListSequence` — нет. Вдобавок были опечатки и перемешаны `int` и `size_t`
  (подробный список — в разделе 2).
* Все файлы переписаны в едином стиле. Индексы и размеры везде `std::size_t`.
  Константность соблюдена: `const`-методы возвращают `const T&`.
* `DynamicArray` теперь полноценная замена `std::vector`: `Append` за амортизированное O(1),
  `Reserve`, `Resize`, `Clear`, `RemoveAt`, `InsertAt`, `Swap`, обход `for (auto& x : arr)`,
  поддержка некопируемых типов (`UnqPtr` и т.п.). Скорость на 10⁷ элементов почти как у `std::vector`.
* Лямбды и `std::function` из библиотеки убраны. `Map/Where/Reduce` принимают обычную функцию
  (или лямбду, если удобно в вызывающем коде).
* Добавлен набор тестов `tests/test_containers.cpp`. Проходит под AddressSanitizer + UBSan
  (C++17 и C++20); `leaks --atExit` показывает 0 утечек.

---

## 2. Найденные ошибки (по файлам)

### `DynamicArray.hpp`
| Где | Ошибка | Последствие |
|---|---|---|
| `operator=(DynamicArray&&)` | `std::excange` вместо `std::exchange` и нет `;` | не компилируется |
| `ArrayIterator` | конструктор инициализирует `arr_`, а поле называется `array_` | не компилируется |
| `ArrayIterator::Current() const override` | сигнатура не совпадала с `IEnumerator::Current()` | не компилируется |
| `T& Get(size_t) override` | в `ICollection` нет неконстантного `Get` — переопределять нечего | не компилируется |
| `operator=(const DynamicArray&)` | вызывается `Swap`, которого не было | не компилируется |
| — | нет `IsEmpty`, `InsertAt`, `RemoveAt`, `Prepend`, хотя их вызывал `ArraySequence` | не компилируется |
| `Append(const T& value)` | при перевыделении памяти старый буфер удалялся **до** копирования `value`; `arr.Append(arr[0])` читал освобождённую память | use-after-free |
| `Resize` | при росте в пределах capacity «новые» элементы содержали старый мусор | неверные данные |
| везде | `int` и `size_t` вперемешку (`CheckIndex(int)`, `Resize(int)`, `GetCapacity()` возвращал `int`) | предупреждения, переполнение на больших N |
| конструкторы | при исключении в копировании элемента буфер утекал | утечка |

### `LinkedList.hpp`
| Где | Ошибка | Последствие |
|---|---|---|
| `GetEnumerator() override` | без `const`, а в `IEnumerable` он `const` | не компилируется |
| `ListIterator::Current()` | возвращал `T&` без `const`, не совпадал с интерфейсом | не компилируется |
| `ListIterator::MoveNext` | после конца списка `current_ == nullptr`, и следующий `MoveNext()` **начинал обход заново** | бесконечный цикл у кода вида `while (it->MoveNext())`, если его вызвать повторно |
| `ListIterator` | порядок инициализации полей не совпадал с порядком объявления | предупреждение |
| `GetFirst() const` / `GetLast() const` | возвращали `T&` из `const`-метода | можно было менять константный список |
| `operator=` | сначала `Clear()`, потом копирование: при исключении список оставался пустым | потеря данных |
| — | не было конструктора/присваивания перемещением и `RemoveAt` | — |
| `GetSubList` / `Concat` | возвращали `new LinkedList` — легко забыть `delete` | утечки |
| `LinkedList(T* items, int count)` | неконстантный указатель — нельзя передать `const`-массив | неудобство |

### `Sequence.hpp`
| Где | Ошибка | Последствие |
|---|---|---|
| `T& Get(size_t) override = 0` | в базовом классе нет такого метода | не компилируется |
| — | не было `Instance()` и `...Internal`, которые переопределял `ArraySequence` | не компилируется |
| `Map`, `Where`, `ListSequence::Concat/GetSubsequence` | для Immutable на каждом шаге создавалась новая копия и удалялась старая: **O(n²)** | очень медленно |
| то же | для `ListSequence` использовался `Get(idx)` в цикле — тоже **O(n²)** | очень медленно |
| `MapIndexedTo`, `FlatMap`, `Zip` | используют `MutableArraySequence`, но `Sequence.hpp` его не подключал | компилировалось только при «удачном» порядке `#include` |
| `GetMinMaxAvg`, `ZipWithReversed` | лямбды внутри библиотеки, требования к `T` (`+`, `<`, приведение к `double`) | сложно читать |

### `ArraySequence.hpp`
* Переопределял (`override`) методы, которых не было в `Sequence`. Вызывал `items_.IsEmpty()`,
  `items_.InsertAt()`, `items_.RemoveAt()`, которых не было в `DynamicArray`. Не компилировался.
* Не было `GetSubsequence`, `Concat`, `Append` и т.д., поэтому класс оставался абстрактным.

### `ListSequence.hpp`
* `LinkedList<T>* items_` хранился **по указателю** с ручным `new/delete`, но без
  оператора присваивания. Сгенерированный компилятором `operator=` копировал указатель,
  и у двух объектов оказывался общий список → **двойное удаление** в деструкторах.
* `begin()/end()` делегировали в `LinkedList::begin()`, которого не существовало.
* `GetEnumerator()` без `const` и через `SequenceIterator` (индексный доступ) → O(n²) на обход.

### `SequenceIterator.hpp`
* `Current()` возвращал `T&` и не совпадал с интерфейсом; сравнение `int` с длиной.
* Обход по индексу через `Get(i)` — для списков O(n²). **Файл удалён**: у каждого контейнера
  теперь свой итератор за O(1) на шаг.

---

## 3. Что изменено и добавлено (по файлам)

### `Exceptions.hpp`
* `IndexOutOfRange` принимает и `const char*`, и `std::string`.
* **Новое:** `EmptyCollection` (наследник `IndexOutOfRange`) — для `GetFirst/GetLast/RemoveFirst/RemoveLast`
  на пустой коллекции. Старый `catch (IndexOutOfRange&)` ловит и его.

### `IEnumerator.hpp`
* `Current()` стал `const` и возвращает `const T&`. Итератор только читает.

### `IEnumerable.hpp`
* `GetEnumerator() const` — без изменений по смыслу. Итератор удаляет вызывающий.
* **Новое:** `EnumeratorIterator<T>` и методы `begin()/end()` в `IEnumerable`.
  Теперь **любую** коллекцию можно обходить циклом, даже через базовый тип:
  ```cpp
  const Sequence<int>& seq = ...;
  for (const int& x : seq) { ... }   // итератор удалится сам, даже при break / исключении
  ```
  Конкретные классы объявляют свои, более быстрые `begin()/end()`. Они перекрывают эти.

### `ICollection.hpp`
* **Новое:** `IsEmpty()` — общий для всех коллекций.

### `DynamicArray.hpp` (замена `std::vector`)
* Конструкторы: `DynamicArray()`, `DynamicArray(count)` (count нулей / `T()`),
  **новый** `DynamicArray(count, value)`, `DynamicArray(const T* items, count)`.
* Копирование (copy-and-swap), перемещение, деструктор — исключение-безопасные.
* Доступ: `Get`, `operator[]`, `GetFirst`, `GetLast` (константные и нет; все с проверкой),
  `Set(index, value)` (+ версия с перемещением).
* Изменение: `Append`, **новые** `Prepend`, `InsertAt(value, index)` (все три — и с копированием,
  и с перемещением `T&&`), `RemoveAt`, `RemoveLast`, `Clear`, `Reserve`, `Swap`. `Resize` исправлен.
* **Новое:** `GetSize()` — синоним `GetCount()` (название из задания).
* Обход: `begin()/end()` — обычные указатели, самый быстрый вариант.
* Рост ёмкости: 0 → 4 → 8 → 16 … (вдвое). `Clear()` ёмкость сохраняет, как `std::vector::clear`.
* Удалённые элементы (`RemoveAt`, `RemoveLast`, `Clear`, уменьшение `Resize`) сбрасываются в `T()`,
  поэтому `UnqPtr`/строки сразу освобождают память.
* **Некопируемые типы** (`UnqPtr<int>` и т.п.) поддерживаются: при росте элементы перемещаются.
  Нужен только конструктор по умолчанию у `T`, потому что буфер создаётся через `new T[]`.

### `LinkedList.hpp` (замена `std::list`)
* Индексы `std::size_t`, конструктор принимает `const T*`.
* Перемещение, copy-and-swap присваивание.
* **Новое:** `RemoveFirst` (O(1)), `RemoveLast`, `RemoveAt`, `Set`, `Clear` (стал публичным), `Swap`,
  `operator[]`, `Append/Prepend` с перемещением.
* **Новое:** `Iterator`/`ConstIterator` и `begin()/end()` — обход `for (auto& x : list)` за O(n).
* `GetSubList(start, end)` и `Concat(other)` теперь возвращают **по значению**
  (`LinkedList<T>`, а не `LinkedList<T>*`) — утечка невозможна, лишнего копирования нет (перемещение).
* `GetLength()` оставлен как синоним `GetCount()` (название из задания).
* `ListEnumerator` исправлен: после конца больше не начинает заново.
* Узел списка вынесен из класса в отдельную структуру `LinkedListNode<T>` (внутри класса — псевдоним
  `LinkedList<T>::Node`). Иначе класс начинался бы с `private:`: итераторам тип узла нужен раньше,
  чем они объявлены, а порядок секций должен быть `public` → `protected` → `private`.

### `Sequence.hpp`
* Единая схема изменения, реализованная **один раз** в `Sequence`:
  `Append`, `Prepend`, `InsertAt`, `RemoveAt`, `Set`, **новый** `Clear` вызывают `Instance()`
  (Mutable → `this`, Immutable → новая копия) и затем защищённый `...Internal`.
  Индекс проверяется **до** копирования, поэтому у Immutable при ошибке ничего не утекает.
* `GetSubsequence`, `Concat`, `Skip`, `Take`, `Map`, `Where`, `Reduce` и **новый** `Clone`
  реализованы в базовом классе за **O(n)** через итератор. Результат — тот же вид последовательности
  (через `CreateEmpty()`).
* `Concat` принимает `const Sequence<T>&` вместо `Sequence<T>*` — `nullptr` передать нельзя.
* `Map/Where/Reduce` — шаблоны, принимают обычную функцию (или лямбду), без `std::function`:
  ```cpp
  int Square(const int& x) { return x * x; }
  long long Sum(const long long& acc, const int& x) { return acc + x; }
  Sequence<int>* squares = seq.Map(Square);
  long long total = seq.Reduce(Sum, 0LL);   // тип результата берётся из начального значения
  ```
* **Новое:** `IndexOf` (возвращает `Sequence<T>::kNotFound`, если нет), `Contains`,
  `operator==`/`operator!=` (сравнивают элементы, вид последовательности не важен — удобно в тестах).
* `operator[]` в `Sequence` — только чтение. Запись — через `Set` или `operator[]` у Mutable-классов.

### `ArraySequence.hpp`, `ListSequence.hpp`
* `ArraySequence` хранит `DynamicArray`, `ListSequence` хранит `LinkedList` — **по значению**.
  Копирование, присваивание и деструктор теперь корректно генерирует компилятор.
* **Новое:** конструктор из любой `Sequence<T>` — `MutableArraySequence<int> a(some_list_sequence);`.
* **Новое (из задания):** `ArraySequence(const DynamicArray<T>&)` и `ListSequence(const LinkedList<T>&)`.
  Они есть у всех четырёх классов `Mutable/Immutable × Array/List` и делают копию переданного контейнера.
* Быстрый обход `for (const T& x : seq)` (указатели / узлы списка, без виртуальных вызовов).
* **Только у Mutable-классов:** `T& Get(i)`, `T& operator[](i)`, неконстантные `begin()/end()`:
  `seq[0] = 5; for (int& x : seq) x *= 2;`. У Immutable менять элементы по ссылке нельзя — так и задумано.
* **Новое у `MutableArraySequence`:** `Reserve(capacity)`, `GetCapacity()`.
* `GetEnumerator()` делегирует в итератор контейнера (O(1) на шаг, для списка тоже).

---

## 4. Удалено (есть в `backup_original/`)

Контейнеры теперь служат заменой `std::vector`, поэтому методы под варианты заданий
2-го семестра из общей библиотеки убраны:

* `MapIndexed`, `MapIndexedTo`, `FlatMap`, `Zip`, `ZipWithReversed`;
* `GetMinMaxAvg`, `GetAvg`, `GetMin`, `GetMax` и структура `MinMaxAvg`;
* `SequenceIterator.hpp` (см. раздел 2).

Если что-то из этого понадобится, проще написать обычную функцию в своей лабораторной
поверх `for (const T& x : seq)`.

---

## 5. Изменения API, которые затронут старый код

| Было | Стало |
|---|---|
| индексы `int` | `std::size_t` |
| `seq->Concat(otherPtr)` | `seq->Concat(*otherPtr)` |
| `LinkedList<T>* sub = list.GetSubList(a, b)` (+ `delete`) | `LinkedList<T> sub = list.GetSubList(a, b)` |
| `LinkedList<T>* c = list.Concat(&other)` | `LinkedList<T> c = list.Concat(other)` |
| `T& Sequence::Get(i)` у любой последовательности | `const T& Get(i)`; `T&` — только у `Mutable*` |
| `T& GetFirst() const` | `const T& GetFirst() const` (у `DynamicArray`/`LinkedList` есть и неконстантные) |
| `IEnumerator::Current()` → `T&` | `const T& Current() const` |
| `Map(std::function<...>)` | `Map(функция)` — шаблон |
| `Reduce<T2>(func, init)` | `Reduce(func, init)` — `T2` выводится из `init` |
| пустая коллекция → `IndexOutOfRange` | `EmptyCollection` (это наследник `IndexOutOfRange`) |
| `LinkedList<T>::Node` (приватная структура) | `LinkedListNode<T>` (псевдоним `LinkedList<T>::Node` сохранён) |

---

## 6. Шпаргалка: `std::vector` → `DynamicArray`

| `std::vector<T>` | `DynamicArray<T>` |
|---|---|
| `v.push_back(x)` / `v.emplace_back(...)` | `a.Append(x)` / `a.Append(T(...))` |
| `v.reserve(n)` | `a.Reserve(n)` |
| `v.size()`, `v.capacity()`, `v.empty()` | `a.GetCount()`, `a.GetCapacity()`, `a.IsEmpty()` |
| `v[i]`, `v.at(i)` | `a[i]`, `a.Get(i)` (оба с проверкой индекса) |
| `v.front()`, `v.back()` | `a.GetFirst()`, `a.GetLast()` |
| `v.insert(v.begin() + i, x)` | `a.InsertAt(x, i)` |
| `v.erase(v.begin() + i)`, `v.pop_back()` | `a.RemoveAt(i)`, `a.RemoveLast()` |
| `v.clear()`, `v.resize(n)`, `v.swap(w)` | `a.Clear()`, `a.Resize(n)`, `a.Swap(b)` |
| `for (auto& x : v)` | `for (auto& x : a)` |
| `std::vector<UnqPtr<int>>` | `DynamicArray<UnqPtr<int>>` + `a.Append(std::move(p))` |

**Какой контейнер брать:**
* `DynamicArray<T>` — по умолчанию, вместо `std::vector` (бенчмарки, буферы, результаты замеров).
  Единственный вариант для некопируемых типов (`UnqPtr`).
* `MutableArraySequence<T>` — если нужен интерфейс `Sequence` (`Map`, `Where`, полиморфизм).
* `LinkedList<T>` / `MutableListSequence<T>` — если часто добавлять/удалять в начале (очередь, стек).
* `Immutable*` — только когда нужна именно неизменяемость. Каждое изменение копирует всю последовательность.

**Важно про `Sequence`:** `Append` и т.п. у Mutable возвращают `this` (удалять нельзя), у Immutable —
новый объект (удалять обязательно). `Map/Where/Concat/GetSubsequence/Skip/Take/Clone` всегда
возвращают новый объект — его нужно удалить. `Sequence` требует копируемый `T`, поэтому
некопируемые типы храните в `DynamicArray`.

---

## 7. Проверка

```bash
clang++ -std=c++17 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -g tests/test_containers.cpp -o test_containers && ./test_containers
```

* `tests/test_containers.cpp`: все методы всех классов, граничные случаи и исключения,
  Mutable/Immutable, обход через базовый тип, некопируемый тип, `Append(arr[0])` при росте,
  подсчёт живых объектов (утечки / двойное удаление).
* Результат: **All tests passed** на C++17 и C++20 с ASan + UBSan.
  Без предупреждений с `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion`.
  `leaks --atExit` на сборке `-O2`: **0 leaks**.
* Скорость (`-O2`, 10⁷ `Append` + обход): `DynamicArray` ≈ 22 мс, `std::vector` ≈ 20 мс,
  `MutableArraySequence` ≈ 31 мс (виртуальные вызовы).

---

## 8. Повторная проверка (3 прохода): порядок секций, имена, полнота методов

**Проход 1 — вычитка всех файлов с нуля и сверка с заданием 2-го семестра.**
* Не было `DynamicArray::GetSize()` и конструкторов `ArraySequence(const DynamicArray<T>&)`,
  `ListSequence(const LinkedList<T>&)` из задания → добавлены (см. раздел 3).
* У `DynamicArray` `InsertAt` и `Prepend` были только с копированием: для `UnqPtr` они не компилировались.
  Добавлены версии с перемещением `T&&`.
* Конструктор `DynamicArray(count)` при исключении не освобождал буфер (другие конструкторы
  освобождали) → исправлено так же, как в остальных.

**Проход 2 — порядок спецификаторов доступа `public` → `protected` → `private`.**
* Нарушение было только в `LinkedList`: класс начинался с `private: struct Node`.
  Узел вынесен в `LinkedListNode<T>` (см. раздел 3). Остальные классы, включая вложенные
  (`ArrayEnumerator`, `Iterator`, `ConstIterator`, `ListEnumerator`, `EnumeratorIterator`) и классы в тестах,
  уже были в правильном порядке.

**Проход 3 — имена по Google C++ Style Guide** (менялись только имена, остальной стиль файлов не трогался):

| Что | Правило Google | Пример |
|---|---|---|
| классы, структуры, псевдонимы типов | `PascalCase` | `DynamicArray`, `LinkedListNode`, `Node` |
| функции и методы | `PascalCase` | `Append`, `GetSubsequence`, `CheckIndex` |
| методы STL-интерфейса | как в STL (исключение из правил) | `begin()`, `end()` |
| локальные переменные и параметры | `snake_case` | `new_capacity`, `start_index`, `other_item` |
| поля классов | `snake_case_` (с `_` в конце) | `data_`, `size_`, `head_`, `enumerator_` |
| поля структур | `snake_case` (без `_`) | `LinkedListNode::data`, `LinkedListNode::next` |
| константы | `k` + `PascalCase` | `kNotFound` |
| макросы | `UPPER_CASE` | `CHECK`, `CHECK_THROWS` (в тестах) |

Переименовано:
* `DynamicArray`: `newSize` → `new_size`, `newCapacity` → `new_capacity`, `newData` → `new_data`,
  `minCapacity` → `min_capacity`;
* `LinkedList`: `startIndex`/`endIndex` → `start_index`/`end_index`;
* `Sequence`: `otherItem` → `other_item`, параметры `GetSubsequence(start, end)` → `(start_index, end_index)`
  (как в `LinkedList::GetSubList`), **`NotFound` → `kNotFound`** (затрагивает код, который его использует);
* тесты: `sameCopy`, `skippedAll`, `takenAll`, `emptyBase`, `frozenList` → `snake_case`.

Имена файлов (`DynamicArray.hpp` и т.д.) оставлены как есть: по Google они были бы `dynamic_array.h`,
но переименование сломало бы `#include` в других проектах.

**Проверка после всех трёх проходов:**
* явное инстанцирование всех классов для `int` и `std::string` (`template class ...;`) — компилятор
  проверяет **каждый** метод, а не только вызванные в тестах: без ошибок и предупреждений;
* каждый заголовок компилируется сам по себе (все нужные `#include` на месте);
* тесты дополнены (`GetSize`, новые конструкторы, `InsertAt`/`Prepend` с некопируемым типом):
  **All tests passed** на C++17 и C++20 с ASan + UBSan и флагами
  `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion`; `leaks --atExit`: **0 leaks**.
