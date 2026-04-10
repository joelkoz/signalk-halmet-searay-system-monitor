#pragma once
#include <vector>
// #include <algorithm>

template<typename T>
class MedianBuffer {
public:
    explicit MedianBuffer(std::size_t maxSamples)
        : _maxSamples(maxSamples), _buf(maxSamples), _count(0), _head(0) {}

    void clear() {
        _count = 0;
        _head  = 0;
    }

    void add(const T& sample) {
        _buf[_head] = sample;
        _head = (_head + 1) % _maxSamples;
        if (_count < _maxSamples) ++_count;          // grow until full
    }

    T getMedian() const {
        if (_count == 0) return T();          // empty → default‑constructed value
        std::vector<T> tmp(_buf.begin(), _buf.begin() + _count);
        std::sort(tmp.begin(), tmp.end());
        const std::size_t mid = _count / 2;
        return (_count & 1) ? tmp[mid]
                            : (tmp[mid - 1] + tmp[mid]) / static_cast<T>(2);
    }

    std::size_t size() const { return _count; }

private:
    std::size_t _maxSamples;
    std::vector<T>    _buf;
    std::size_t       _count;
    std::size_t       _head;
};
