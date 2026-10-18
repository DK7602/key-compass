// Key Compass - single-producer / single-consumer lock-free ring buffer
#pragma once

#include <atomic>
#include <cstddef>
#include <vector>

namespace kc
{
    template <typename T>
    class SpscRing
    {
    public:
        explicit SpscRing (size_t capacityPowerOfTwo = 1 << 16)
        {
            size_t cap = 1;
            while (cap < capacityPowerOfTwo) cap <<= 1;
            buffer.resize (cap);
            mask = cap - 1;
        }

        // Producer: pushes up to n items, returns how many were written (drops the rest when full).
        size_t push (const T* items, size_t n) noexcept
        {
            const size_t w = writePos.load (std::memory_order_relaxed);
            const size_t r = readPos.load (std::memory_order_acquire);
            const size_t space = buffer.size() - (w - r);
            if (n > space) n = space;
            for (size_t i = 0; i < n; ++i)
                buffer[(w + i) & mask] = items[i];
            writePos.store (w + n, std::memory_order_release);
            return n;
        }

        bool push (const T& item) noexcept   { return push (&item, 1) == 1; }

        // Consumer
        size_t pop (T* dest, size_t maxItems) noexcept
        {
            const size_t r = readPos.load (std::memory_order_relaxed);
            const size_t w = writePos.load (std::memory_order_acquire);
            size_t n = w - r;
            if (n > maxItems) n = maxItems;
            for (size_t i = 0; i < n; ++i)
                dest[i] = buffer[(r + i) & mask];
            readPos.store (r + n, std::memory_order_release);
            return n;
        }

        // Consumer: look at the next item without removing it
        bool peek (T& dest) const noexcept
        {
            const size_t r = readPos.load (std::memory_order_relaxed);
            const size_t w = writePos.load (std::memory_order_acquire);
            if (w == r) return false;
            dest = buffer[r & mask];
            return true;
        }

        void discardOne() noexcept
        {
            const size_t r = readPos.load (std::memory_order_relaxed);
            if (writePos.load (std::memory_order_acquire) != r)
                readPos.store (r + 1, std::memory_order_release);
        }

        size_t available() const noexcept
        {
            return writePos.load (std::memory_order_acquire) - readPos.load (std::memory_order_relaxed);
        }

    private:
        std::vector<T> buffer;
        size_t mask = 0;
        std::atomic<size_t> writePos { 0 }, readPos { 0 };
    };
}
