#ifndef SORT_H
#define SORT_H

#include <algorithm>
#include <functional>
#include <vector>

// Generic insertion sort — O(n²), stable, used for small or nearly-sorted ranges.
template <typename T, typename Compare = std::less<T>>
void insertion_sort(std::vector<T>& data, Compare cmp = Compare())
{
    for (std::size_t i = 1; i < data.size(); ++i) {
        T key = std::move(data[i]);
        std::size_t j = i;
        while (j > 0 && cmp(key, data[j - 1])) {
            data[j] = std::move(data[j - 1]);
            --j;
        }
        data[j] = std::move(key);
    }
}

// Generic merge sort — O(n log n), stable.
template <typename T, typename Compare = std::less<T>>
void merge_sort(std::vector<T>& data, Compare cmp = Compare())
{
    if (data.size() <= 1) {
        return;
    }

    const std::size_t mid = data.size() / 2;
    std::vector<T> left(data.begin(), data.begin() + static_cast<std::ptrdiff_t>(mid));
    std::vector<T> right(data.begin() + static_cast<std::ptrdiff_t>(mid), data.end());

    merge_sort(left, cmp);
    merge_sort(right, cmp);

    std::size_t i = 0;
    std::size_t j = 0;
    std::size_t k = 0;
    while (i < left.size() && j < right.size()) {
        if (cmp(right[j], left[i])) {
            data[k++] = std::move(right[j++]);
        } else {
            data[k++] = std::move(left[i++]);
        }
    }
    while (i < left.size()) {
        data[k++] = std::move(left[i++]);
    }
    while (j < right.size()) {
        data[k++] = std::move(right[j++]);
    }
}

#endif // SORT_H