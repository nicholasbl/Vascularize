#ifndef XRANGE_H
#define XRANGE_H

#include <iterator>
#include <type_traits>

namespace xrange_detail {

template <class T>
struct increment_iterator {
    T value;

    using iterator_category = std::input_iterator_tag;
    using value_type        = T;
    using difference_type   = ptrdiff_t;
    using pointer           = T const*;
    using reference         = T const&;

    increment_iterator& operator++() {
        ++value;
        return *this;
    }

    increment_iterator operator++(int) = delete;

    bool operator==(increment_iterator const& other) const {
        return value == other.value;
    }

    bool operator!=(increment_iterator const& other) const {
        return !(*this == other);
    }

    T const& operator*() const {
        return value;
    }

    T const* operator->() const {
        return std::addressof(value);
    }
};

template <class T>
struct stepping_iterator {
    T value;
    T steps_left;
    T step_size;

    using iterator_category = std::input_iterator_tag;
    using value_type        = T;
    using difference_type   = ptrdiff_t;
    using pointer           = T const*;
    using reference         = T const&;

    stepping_iterator& operator++() {
        --steps_left;
        value += step_size;
        return *this;
    }

    stepping_iterator operator++(int) = delete;

    bool operator==(stepping_iterator const& other) const {
        return steps_left == other.steps_left;
    }

    bool operator!=(stepping_iterator const& other) const {
        return !(*this == other);
    }

    T const& operator*() const {
        return value;
    }

    T const* operator->() const {
        return std::addressof(value);
    }
};

template <class T>
struct range_generator_single {
    T last;

    increment_iterator<T> begin() {
        return { 0 };
    }

    increment_iterator<T> end() {
        return { last };
    }
};


template <class T>
struct range_generator {
    T first;
    T last;

    increment_iterator<T> begin() {
        return { first };
    }

    increment_iterator<T> end() {
        return { last };
    }
};

template <class T>
struct stepping_generator {
    T first;
    T last;
    T step_size;

    stepping_iterator<T> begin() {
        return { first, (last - first) / step_size + 1, step_size };
    }

    stepping_iterator<T> end() {
        return { last, 0, step_size };
    }
};

} // namespace xrange_detail

template <class T>
xrange_detail::range_generator_single<T> xrange(T last) {
    static_assert(std::is_integral<T>::value, "XRange only supports integral values");
    return { last };
}

template <class T>
xrange_detail::range_generator<T> xrange(T first, T last) {
    static_assert(std::is_integral<T>::value, "XRange only supports integral values");
    return { first, last };
}

//TODO FIX. exhibits weirdness
//template <class T>
//xrange_detail::stepping_generator<T> xrange(T first, T last, T step_by) {
//   static_assert(std::is_integral<T>::value, "XRange only supports integral values");
//    return { first, last, step_by };
//}

template <class Container>
auto xrange_over(Container const& c) {
    return xrange(c.size());
}

#endif // XRANGE_H
