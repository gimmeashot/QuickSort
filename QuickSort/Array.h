// Динамический массив на malloc()/free() с итераторами.
#pragma once
#include <cassert>
#include <climits>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

template <typename T>
class Array final {
    // malloc() не гарантирует выравнивание больше max_align_t
    static_assert(alignof(T) <= alignof(std::max_align_t), "Over-aligned types are not supported");

    static const int kDefaultCapacity = 8;
    static const int kGrowthFactor = 2;

    // Перемещение, а если T его не поддерживает — копирование
    using MoveRef = std::conditional_t<std::is_move_constructible<T>::value, T&&, const T&>;

public:
    class ConstIterator {
    public:
        const T& get() const {
            assert(hasNext());
            return (*array_)[index_];
        }

        void next() {
            assert(hasNext());
            index_ += step_;
        }

        // true, пока итератор стоит на элементе: иначе цикл из задания
        // for (auto it = a.iterator(); it.hasNext(); it.next()) пропустил бы последний элемент
        bool hasNext() const {
            return index_ >= 0 && index_ < array_->size();
        }

        const T& operator*() const { return get(); }
        const T* operator->() const { return &get(); }

        ConstIterator& operator++() {
            next();
            return *this;
        }

        ConstIterator operator++(int) {
            ConstIterator old = *this;
            next();
            return old;
        }

        bool operator==(const ConstIterator& other) const {
            return array_ == other.array_ && index_ == other.index_;
        }

        bool operator!=(const ConstIterator& other) const { return !(*this == other); }

        // Операции произвольного доступа — нужны для algo::sort (работа 3).
        // Смещения считаются в направлении обхода, поэтому работают и для reverseIterator().
        ConstIterator& operator--() {
            index_ -= step_;
            return *this;
        }

        ConstIterator& operator+=(int n) {
            index_ += n * step_;
            return *this;
        }

        ConstIterator& operator-=(int n) { return *this += -n; }
        ConstIterator operator+(int n) const { return ConstIterator(*this) += n; }
        ConstIterator operator-(int n) const { return ConstIterator(*this) -= n; }

        int operator-(const ConstIterator& other) const {
            assert(array_ == other.array_ && step_ == other.step_);
            return (index_ - other.index_) * step_;
        }

        bool operator<(const ConstIterator& other) const { return *this - other < 0; }
        bool operator>(const ConstIterator& other) const { return other < *this; }
        bool operator<=(const ConstIterator& other) const { return !(other < *this); }
        bool operator>=(const ConstIterator& other) const { return !(*this < other); }

    protected:
        ConstIterator(const Array* array, int index, int step)
            : array_(array), index_(index), step_(step) {}

        friend class Array<T>;

        const Array* array_;
        int index_;
        int step_;  // +1 при прямом обходе, -1 при обратном
    };

    class Iterator : public ConstIterator {
    public:
        void set(const T& value) { **this = value; }

        // const_cast безопасен: Iterator создаётся только из неконстантного Array
        T& operator*() const { return const_cast<T&>(ConstIterator::get()); }
        T* operator->() const { return &**this; }

        Iterator& operator++() {
            this->next();
            return *this;
        }

        Iterator operator++(int) {
            Iterator old = *this;
            this->next();
            return old;
        }

        Iterator& operator--() {
            ConstIterator::operator--();
            return *this;
        }

        Iterator& operator+=(int n) {
            ConstIterator::operator+=(n);
            return *this;
        }

        Iterator& operator-=(int n) { return *this += -n; }
        Iterator operator+(int n) const { return Iterator(*this) += n; }
        Iterator operator-(int n) const { return Iterator(*this) -= n; }
        int operator-(const ConstIterator& other) const { return ConstIterator::operator-(other); }

    private:
        friend class Array<T>;

        Iterator(Array* array, int index, int step) : ConstIterator(array, index, step) {}
    };

    Array() : Array(kDefaultCapacity) {}

    explicit Array(int capacity) : data_(allocate(capacity)), capacity_(capacity), size_(0) {}

    // Делегирующий конструктор уже завершён, поэтому при исключении в теле
    // сработает ~Array() и освободит всё, что успели скопировать
    Array(const Array& other) : Array(other.capacity_) {
        for (; size_ < other.size_; ++size_) {
            new (data_ + size_) T(other.data_[size_]);
        }
    }

    Array(Array&& other) noexcept
        : data_(other.data_), capacity_(other.capacity_), size_(other.size_) {
        other.data_ = nullptr;
        other.capacity_ = 0;
        other.size_ = 0;
    }

    // Параметр по значению: один оператор и для копирования, и для перемещения,
    // присваивание самому себе тоже работает
    Array& operator=(Array other) noexcept {
        std::swap(data_, other.data_);
        std::swap(capacity_, other.capacity_);
        std::swap(size_, other.size_);
        return *this;
    }

    ~Array() {
        destroy(data_, 0, size_);
        std::free(data_);
    }

    int insert(const T& value) { return insert(size_, value); }

    int insert(int index, const T& value) {
        assert(index >= 0 && index <= size_);
        if (size_ == capacity_) {
            insertWithGrowth(index, value);
        } else if (index == size_) {
            new (data_ + size_) T(value);
            ++size_;
        } else {
            T copy(value);  // value может лежать в этом же массиве и уехать при сдвиге
            ++size_;
            for (int i = size_ - 1; i > index; --i) {
                relocate(i - 1, i);
            }
            fill(index, static_cast<MoveRef>(copy));
        }
        return index;
    }

    void remove(int index) {
        assert(index >= 0 && index < size_);
        data_[index].~T();
        for (int i = index; i + 1 < size_; ++i) {
            relocate(i + 1, i);
        }
        --size_;
    }

    const T& operator[](int index) const {
        assert(index >= 0 && index < size_);
        return data_[index];
    }

    T& operator[](int index) {
        assert(index >= 0 && index < size_);
        return data_[index];
    }

    int size() const { return size_; }
    int capacity() const { return capacity_; }

    Iterator iterator() { return Iterator(this, 0, 1); }
    ConstIterator iterator() const { return ConstIterator(this, 0, 1); }

    Iterator reverseIterator() { return Iterator(this, size_ - 1, -1); }
    ConstIterator reverseIterator() const { return ConstIterator(this, size_ - 1, -1); }

    Iterator begin() { return Iterator(this, 0, 1); }
    Iterator end() { return Iterator(this, size_, 1); }
    ConstIterator begin() const { return cbegin(); }
    ConstIterator end() const { return cend(); }
    ConstIterator cbegin() const { return ConstIterator(this, 0, 1); }
    ConstIterator cend() const { return ConstIterator(this, size_, 1); }

private:
    static T* allocate(int capacity) {
        assert(capacity >= 0);
        if (capacity == 0) return nullptr;
        // Отрицательная ёмкость после приведения тоже не пройдёт эту проверку
        const std::size_t count = static_cast<std::size_t>(capacity);
        if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) throw std::bad_alloc();
        void* block = std::malloc(count * sizeof(T));
        if (block == nullptr) throw std::bad_alloc();
        return static_cast<T*>(block);
    }

    static void destroy(T* block, int from, int to) {
        for (int i = from; i < to; ++i) {
            block[i].~T();
        }
    }

    int grownCapacity() const {
        if (capacity_ == 0) return kDefaultCapacity;
        if (capacity_ > INT_MAX / kGrowthFactor) throw std::length_error("Array is too large");
        return capacity_ * kGrowthFactor;
    }

    void insertWithGrowth(int index, const T& value) {
        const int newCapacity = grownCapacity();
        T* block = allocate(newCapacity);
        // Новый элемент создаётся первым, пока value (возможно, из этого массива) ещё жив.
        // Затем [lo, hi) растёт от него в обе стороны, поэтому при исключении его легко убрать.
        int lo = index;
        int hi = index;
        try {
            new (block + hi) T(value);
            ++hi;
            for (; hi <= size_; ++hi) {
                new (block + hi) T(static_cast<MoveRef>(data_[hi - 1]));
            }
            for (; lo > 0; --lo) {
                new (block + lo - 1) T(static_cast<MoveRef>(data_[lo - 1]));
            }
        } catch (...) {
            destroy(block, lo, hi);
            std::free(block);
            throw;
        }
        destroy(data_, 0, size_);
        std::free(data_);
        data_ = block;
        capacity_ = newCapacity;
        ++size_;
    }

    // Создаёт элемент в пустой ячейке to. Если конструктор бросил исключение, в массиве
    // остаётся дыра, поэтому хвост за ней отбрасывается: ~Array() не должен её разрушать.
    template <typename Source>
    void fill(int to, Source&& source) {
        try {
            new (data_ + to) T(std::forward<Source>(source));
        } catch (...) {
            destroy(data_, to + 1, size_);
            size_ = to;
            throw;
        }
    }

    void relocate(int from, int to) {
        fill(to, static_cast<MoveRef>(data_[from]));
        data_[from].~T();
    }

    T* data_;
    int capacity_;
    int size_;
};
