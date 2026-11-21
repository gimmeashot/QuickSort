// Обобщённая быстрая сортировка с оптимизациями:
//  - опорный элемент — медиана первого, среднего и последнего элементов;
//  - рекурсия только в меньшую часть, большая обрабатывается в цикле (глубина стека O(log n));
//  - короткие интервалы досортировываются вставками;
//  - элементы перемещаются через move-семантику (std::move / std::swap).
//
// Работает с любым итератором произвольного доступа (*, ++, --, it + n, it - it, ==, !=, <),
// в том числе с T* и с Array<T>::Iterator из работы 2.
#pragma once
#include <cstddef>
#include <utility>

namespace algo {

// порог перехода на вставки, подобран экспериментально (см. main.cpp)
constexpr std::ptrdiff_t kInsertionSortThreshold = 16;

namespace detail {

// обмен через move, без самоприсваивания
template <typename Iterator>
void swapValues(Iterator a, Iterator b) {
    if (a != b) {
        using std::swap;
        swap(*a, *b);
    }
}

template <typename Iterator, typename Compare>
void insertionSort(Iterator first, Iterator last, Compare& comp) {
    if (first == last) return;
    for (Iterator i = first + 1; i != last; ++i) {
        auto value = std::move(*i);
        Iterator hole = i;
        while (hole != first) {
            Iterator prev = hole;
            --prev;
            if (!comp(value, *prev)) break;
            *hole = std::move(*prev);
            hole = prev;
        }
        *hole = std::move(value);
    }
}

// упорядочивает *a, *b, *c
template <typename Iterator, typename Compare>
void sort3(Iterator a, Iterator b, Iterator c, Compare& comp) {
    if (comp(*b, *a)) detail::swapValues(a, b);
    if (comp(*c, *b)) {
        detail::swapValues(b, c);
        if (comp(*b, *a)) detail::swapValues(a, b);
    }
}

// разбиение Хоара, длина интервала >= 3; возвращает итоговую позицию опорного
template <typename Iterator, typename Compare>
Iterator partition(Iterator first, Iterator last, Compare& comp) {
    Iterator back = last;
    --back;
    Iterator mid = first + (last - first) / 2;

    // медиана уходит в начало, *back и сам опорный служат барьерами для сканирования
    detail::sort3(first, mid, back, comp);
    detail::swapValues(first, mid);
    const Iterator pivot = first;

    Iterator i = first + 1;
    Iterator j = back;
    while (true) {
        while (comp(*i, *pivot)) ++i;
        while (comp(*pivot, *j)) --j;
        if (!(i < j)) break;
        detail::swapValues(i, j);  // равные опорному тоже меняются — баланс на дубликатах
        ++i;
        --j;
    }
    detail::swapValues(pivot, j);
    return j;
}

template <typename Iterator, typename Compare>
void quickSort(Iterator first, Iterator last, Compare& comp, std::ptrdiff_t threshold) {
    if (threshold < 2) threshold = 2;  // partition требует хотя бы 3 элемента
    while (last - first > threshold) {
        Iterator p = detail::partition(first, last, comp);
        Iterator afterP = p + 1;
        // меньшая часть — рекурсивно, большая — следующей итерацией
        if (p - first < last - afterP) {
            detail::quickSort(first, p, comp, threshold);
            first = afterP;
        } else {
            detail::quickSort(afterP, last, comp, threshold);
            last = p;
        }
    }
    detail::insertionSort(first, last, comp);
}

}  // namespace detail

// сортирует [first, last) по предикату comp(a, b) == (a < b)
template <typename Iterator, typename Compare>
void sort(Iterator first, Iterator last, Compare comp) {
    detail::quickSort(first, last, comp, kInsertionSortThreshold);
}

template <typename Iterator, typename Compare>
void insertionSort(Iterator first, Iterator last, Compare comp) {
    detail::insertionSort(first, last, comp);
}

// для подбора порога
template <typename Iterator, typename Compare>
void sortWithThreshold(Iterator first, Iterator last, Compare comp, std::ptrdiff_t threshold) {
    detail::quickSort(first, last, comp, threshold);
}

}  // namespace algo

// сигнатура из задания; с T* шаблон специализированнее std::sort, поэтому вызов без algo:: однозначен
template <typename T, typename Compare>
void sort(T* first, T* last, Compare comp) {
    algo::sort(first, last, comp);
}
