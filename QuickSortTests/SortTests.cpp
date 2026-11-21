#include "pch.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "Array.h"
#include "Sort.h"

namespace {

const auto kLess = [](int a, int b) { return a < b; };
const auto kGreater = [](int a, int b) { return a > b; };

std::vector<int> randomInts(int n, int maxValue, unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(0, maxValue);
    std::vector<int> v(n);
    for (int& x : v) x = dist(rng);
    return v;
}

// Сортирует копию нашей функцией и сверяет с std::sort
template <typename T, typename Compare>
void expectSortedLikeStd(std::vector<T> v, Compare comp) {
    std::vector<T> expected = v;
    std::sort(expected.begin(), expected.end(), comp);
    algo::sort(v.data(), v.data() + v.size(), comp);
    EXPECT_EQ(expected, v);
}

// Тип без копирования: проверяет, что сортировка использует только перемещения
struct MoveOnly {
    explicit MoveOnly(int v) : value(std::make_unique<int>(v)) {}
    MoveOnly(MoveOnly&&) = default;
    MoveOnly& operator=(MoveOnly&&) = default;
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;
    std::unique_ptr<int> value;
};

// Считает копирования
struct Counted {
    int value = 0;
    static int copies;
    Counted(int v = 0) : value(v) {}
    Counted(const Counted& o) : value(o.value) { ++copies; }
    Counted& operator=(const Counted& o) {
        value = o.value;
        ++copies;
        return *this;
    }
    Counted(Counted&&) = default;
    Counted& operator=(Counted&&) = default;
};
int Counted::copies = 0;

struct Person {
    std::string name;
    int age;
    bool operator==(const Person& o) const { return name == o.name && age == o.age; }
};

}  // namespace

// ---------- граничные случаи ----------

TEST(SortTest, EmptyRange) {
    int a[1] = {42};
    algo::sort(a, a, kLess);
    EXPECT_EQ(42, a[0]);
    int* null = nullptr;
    algo::sort(null, null, kLess);  // пустой интервал без памяти
}

TEST(SortTest, SingleElement) {
    int a[] = {5};
    algo::sort(a, a + 1, kLess);
    EXPECT_EQ(5, a[0]);
}

TEST(SortTest, TwoAndThreeElements) {
    expectSortedLikeStd(std::vector<int>{2, 1}, kLess);
    expectSortedLikeStd(std::vector<int>{1, 2}, kLess);
    std::vector<int> p = {1, 2, 3};
    do {
        expectSortedLikeStd(p, kLess);
    } while (std::next_permutation(p.begin(), p.end()));
}

TEST(SortTest, AllPermutationsOfEight) {
    std::vector<int> p = {1, 2, 3, 4, 5, 6, 7, 8};
    do {
        std::vector<int> v = p;
        algo::sort(v.data(), v.data() + v.size(), kLess);
        ASSERT_TRUE(std::is_sorted(v.begin(), v.end()));
    } while (std::next_permutation(p.begin(), p.end()));
}

TEST(SortTest, ExampleFromTask) {
    int a[100];
    for (int i = 0; i < 100; ++i) a[i] = (i * 37) % 100;
    algo::sort(a, a + 100, [](int a, int b) { return a < b; });
    for (int i = 0; i < 100; ++i) EXPECT_EQ(i, a[i]);
}

// Вызов дословно как в задании — неквалифицированный sort с требуемой сигнатурой
TEST(SortTest, ExampleFromTaskVerbatim) {
    int a[100];
    for (int i = 0; i < 100; ++i) a[i] = 99 - i;
    sort(a, a + 100, [](int a, int b) { return a < b; });
    for (int i = 0; i < 100; ++i) EXPECT_EQ(i, a[i]);
}

// Неквалифицированный вызов для std::string* не конфликтует со std::sort (ADL)
TEST(SortTest, UnqualifiedCallWithStdTypes) {
    std::string s[] = {"c", "a", "b"};
    sort(s, s + 3, [](const std::string& a, const std::string& b) { return a < b; });
    EXPECT_EQ("a", s[0]);
    EXPECT_EQ("b", s[1]);
    EXPECT_EQ("c", s[2]);
}

TEST(SortTest, SortsOnlyGivenSubrange) {
    int a[] = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
    algo::sort(a + 2, a + 8, kLess);
    int expected[] = {9, 8, 2, 3, 4, 5, 6, 7, 1, 0};
    for (int i = 0; i < 10; ++i) EXPECT_EQ(expected[i], a[i]);
}

// Размеры вокруг порога вставок, чтобы задеть обе ветки
TEST(SortTest, SizesAroundThreshold) {
    const int t = static_cast<int>(algo::kInsertionSortThreshold);
    for (int n = 0; n <= 3 * t + 3; ++n) {
        expectSortedLikeStd(randomInts(n, 1000, 100 + n), kLess);
    }
}

// ---------- характерные входные данные ----------

TEST(SortTest, AlreadySorted) {
    std::vector<int> v(10000);
    for (int i = 0; i < 10000; ++i) v[i] = i;
    expectSortedLikeStd(v, kLess);
}

TEST(SortTest, ReverseSorted) {
    std::vector<int> v(10000);
    for (int i = 0; i < 10000; ++i) v[i] = 10000 - i;
    expectSortedLikeStd(v, kLess);
}

TEST(SortTest, AllEqual) {
    expectSortedLikeStd(std::vector<int>(10000, 7), kLess);
}

TEST(SortTest, FewDistinctValues) {
    expectSortedLikeStd(randomInts(10000, 3, 1), kLess);
}

TEST(SortTest, OrganPipe) {
    std::vector<int> v;
    for (int i = 0; i < 5000; ++i) v.push_back(i);
    for (int i = 5000; i > 0; --i) v.push_back(i);
    expectSortedLikeStd(v, kLess);
}

TEST(SortTest, RandomLarge) {
    for (unsigned seed = 0; seed < 5; ++seed) {
        expectSortedLikeStd(randomInts(100000, 1000000, seed), kLess);
    }
}

TEST(SortTest, NegativeAndExtremeValues) {
    expectSortedLikeStd(std::vector<int>{INT_MAX, -1, 0, INT_MIN, 5, INT_MIN, INT_MAX, -100}, kLess);
}

TEST(SortTest, IsPermutationOfInput) {
    std::vector<int> v = randomInts(5000, 100, 9);
    std::vector<int> original = v;
    algo::sort(v.data(), v.data() + v.size(), kLess);
    EXPECT_TRUE(std::is_permutation(v.begin(), v.end(), original.begin()));
}

// ---------- предикаты разных видов ----------

TEST(SortTest, DescendingLambda) {
    expectSortedLikeStd(randomInts(1000, 1000, 2), kGreater);
}

bool lessFunction(const int& a, const int& b) { return a < b; }

TEST(SortTest, FunctionPointer) {
    expectSortedLikeStd(randomInts(1000, 1000, 3), &lessFunction);
}

TEST(SortTest, FunctionalObject) {
    expectSortedLikeStd(randomInts(1000, 1000, 4), std::greater<int>());
}

TEST(SortTest, StatefulComparatorIsUsed) {
    int calls = 0;
    std::vector<int> v = randomInts(1000, 1000, 5);
    algo::sort(v.data(), v.data() + v.size(), [&calls](int a, int b) {
        ++calls;
        return a < b;
    });
    EXPECT_TRUE(std::is_sorted(v.begin(), v.end()));
    EXPECT_GT(calls, 0);
}

// ---------- нетривиальные типы ----------

TEST(SortTest, Doubles) {
    expectSortedLikeStd(std::vector<double>{3.5, -1.25, 0.0, 2.0, -7.75, 3.5, 1e10, -1e-10},
                        [](double a, double b) { return a < b; });
}

TEST(SortTest, Strings) {
    std::vector<std::string> v;
    std::mt19937 rng(6);
    for (int i = 0; i < 3000; ++i) {
        std::string s(rng() % 20, 'a');
        for (char& c : s) c = static_cast<char>('a' + rng() % 26);
        v.push_back(s);
    }
    expectSortedLikeStd(v, [](const std::string& a, const std::string& b) { return a < b; });
}

TEST(SortTest, StringsByLength) {
    std::vector<std::string> v = {"ccc", "a", "bb", "", "dddd", "e", "ff"};
    algo::sort(v.data(), v.data() + v.size(),
               [](const std::string& a, const std::string& b) { return a.size() < b.size(); });
    for (std::size_t i = 1; i < v.size(); ++i) EXPECT_LE(v[i - 1].size(), v[i].size());
}

TEST(SortTest, StructsByMultipleKeys) {
    std::vector<Person> v;
    std::mt19937 rng(7);
    const char* names[] = {"Anna", "Boris", "Vera", "Gleb"};
    for (int i = 0; i < 500; ++i) v.push_back({names[rng() % 4], static_cast<int>(rng() % 50)});
    auto byAgeThenName = [](const Person& a, const Person& b) {
        return a.age != b.age ? a.age < b.age : a.name < b.name;
    };
    expectSortedLikeStd(v, byAgeThenName);
}

TEST(SortTest, MoveOnlyType) {
    std::vector<MoveOnly> v;
    std::vector<int> values = randomInts(2000, 500, 8);
    for (int x : values) v.emplace_back(x);
    algo::sort(v.data(), v.data() + v.size(),
               [](const MoveOnly& a, const MoveOnly& b) { return *a.value < *b.value; });
    std::sort(values.begin(), values.end());
    for (std::size_t i = 0; i < v.size(); ++i) {
        ASSERT_NE(nullptr, v[i].value);  // ничего не потерялось при перемещениях
        EXPECT_EQ(values[i], *v[i].value);
    }
}

TEST(SortTest, NoCopiesAreMade) {
    std::vector<Counted> v;
    for (int x : randomInts(2000, 1000, 10)) v.emplace_back(x);
    Counted::copies = 0;
    algo::sort(v.data(), v.data() + v.size(),
               [](const Counted& a, const Counted& b) { return a.value < b.value; });
    EXPECT_EQ(0, Counted::copies);
    EXPECT_TRUE(std::is_sorted(v.begin(), v.end(),
                               [](const Counted& a, const Counted& b) { return a.value < b.value; }));
}

TEST(SortTest, PointersToObjects) {
    int storage[] = {50, 10, 40, 20, 30};
    std::vector<int*> v;
    for (int& x : storage) v.push_back(&x);
    algo::sort(v.data(), v.data() + v.size(), [](const int* a, const int* b) { return *a < *b; });
    for (int i = 0; i < 5; ++i) EXPECT_EQ((i + 1) * 10, *v[i]);
}

// ---------- отдельные шаги ----------

TEST(InsertionSortTest, SortsSmallAndLarge) {
    for (int n : {0, 1, 2, 5, 100, 1000}) {
        std::vector<int> v = randomInts(n, 100, n);
        std::vector<int> expected = v;
        std::sort(expected.begin(), expected.end());
        algo::insertionSort(v.data(), v.data() + v.size(), kLess);
        EXPECT_EQ(expected, v);
    }
}

TEST(SortTest, AnyThresholdGivesSameResult) {
    std::vector<int> base = randomInts(3000, 100, 11);
    std::vector<int> expected = base;
    std::sort(expected.begin(), expected.end());
    for (int t : {0, 1, 2, 3, 8, 16, 64, 5000}) {
        std::vector<int> v = base;
        algo::sortWithThreshold(v.data(), v.data() + v.size(), kLess, t);
        EXPECT_EQ(expected, v) << "threshold = " << t;
    }
}

// ---------- (*) динамический массив из работы 2 ----------

TEST(ArraySortTest, SortsDynamicArray) {
    Array<int> a;
    std::vector<int> values = randomInts(1000, 1000, 12);
    for (int x : values) a.insert(x);
    algo::sort(a.begin(), a.end(), kLess);
    std::sort(values.begin(), values.end());
    ASSERT_EQ(static_cast<int>(values.size()), a.size());
    for (int i = 0; i < a.size(); ++i) EXPECT_EQ(values[i], a[i]);
}

TEST(ArraySortTest, SortsDynamicArrayOfStrings) {
    Array<std::string> a;
    for (const char* s : {"pear", "apple", "fig", "kiwi", "banana", "apple", "cherry"}) a.insert(s);
    algo::sort(a.begin(), a.end(), [](const std::string& x, const std::string& y) { return x < y; });
    const char* expected[] = {"apple", "apple", "banana", "cherry", "fig", "kiwi", "pear"};
    for (int i = 0; i < a.size(); ++i) EXPECT_EQ(expected[i], a[i]);
}

TEST(ArraySortTest, ReverseIteratorSortsDescending) {
    Array<int> a;
    for (int x : {3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5, 8, 9, 7, 9, 3, 2, 3, 8, 4}) a.insert(x);
    // Сортировка по возрастанию в обратном порядке обхода = по убыванию в прямом
    algo::sort(a.reverseIterator(), a.reverseIterator() + a.size(), kLess);
    for (int i = 1; i < a.size(); ++i) EXPECT_GE(a[i - 1], a[i]);
}

TEST(ArraySortTest, EmptyAndSingle) {
    Array<int> empty;
    algo::sort(empty.begin(), empty.end(), kLess);
    EXPECT_EQ(0, empty.size());
    Array<int> one;
    one.insert(1);
    algo::sort(one.begin(), one.end(), kLess);
    EXPECT_EQ(1, one[0]);
}

TEST(ArraySortTest, IteratorArithmetic) {
    Array<int> a;
    for (int i = 0; i < 10; ++i) a.insert(i);
    auto it = a.begin() + 3;
    EXPECT_EQ(3, *it);
    EXPECT_EQ(7, a.end() - it);
    EXPECT_TRUE(it < a.end());
    --it;
    EXPECT_EQ(2, *it);
    EXPECT_EQ(5, *(it + 3));
    EXPECT_EQ(0, *(it - 2));
}
