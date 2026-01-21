#pragma once

#include "market-data/bookEvent.hpp"
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstring>
#include <memory>
#include <type_traits>

namespace utils {
template <typename T> class spsc_queue_shm {
    using size_type = std::size_t;

    static_assert(std::is_trivially_copyable_v<L2OrderBookUpdate>);
    static_assert(std::is_trivially_copyable_v<L3Update>);

public:
    explicit spsc_queue_shm(std::size_t capacity) {
        buffer_size_m = std::bit_ceil(capacity + 1);
        mask_m = buffer_size_m - 1;
        buffer_offset_m = sizeof(spsc_queue_shm);

        head_m.store(0, std::memory_order_relaxed);
        tail_m.store(0, std::memory_order_relaxed);
    }

    // producer calls this
    bool try_push(const T& item) {
        size_type current_tail = tail_m.load(std::memory_order_relaxed);
        size_type current_head = head_m.load(std::memory_order_acquire);

        size_type next_tail = current_tail + 1;

        if (next_tail - current_head >= buffer_size_m) {
            return false;
        }

        size_type index = current_tail & mask_m;
        T* buffer = get_buf_();
        std::memcpy(&buffer[index], &item, sizeof(T));
        tail_m.store(next_tail, std::memory_order_release);
        return true;
    }

    // consumer calls this
    bool try_pop(T& item) {
        size_type current_head = head_m.load(std::memory_order_relaxed);
        size_type current_tail = tail_m.load(std::memory_order_acquire);

        if (current_head == current_tail) {
            return false;
        }

        T* buffer = get_buf_();
        size_type index = current_head & mask_m;
        std::memcpy(&item, &buffer[index], sizeof(T));
        head_m.store(current_head + 1, std::memory_order_release);
        return true;
    }

    ~spsc_queue_shm() = default;

    spsc_queue_shm(const spsc_queue_shm&) = delete;
    spsc_queue_shm(spsc_queue_shm&&) = delete;
    spsc_queue_shm& operator=(const spsc_queue_shm&) = delete;
    spsc_queue_shm& operator=(spsc_queue_shm&&) = delete;

private:
    size_type buffer_offset_m; // offset from object pointer to the buffer
    size_type buffer_size_m;
    size_type mask_m;

    alignas(64) std::atomic<size_type> head_m; // consumer position
    alignas(64) std::atomic<size_type> tail_m; // producer position

    T* get_buf_() {
        return reinterpret_cast<T*>(reinterpret_cast<char*>(this) + buffer_offset_m);
    }
};

template <typename T, typename Allocator = std::allocator<T>> class spsc_queue {
    using alloc_traits = std::allocator_traits<Allocator>;
    using size_type = std::size_t;

    static_assert(std::is_trivially_copyable_v<T>,
                  "spsc_queue requires trivially copyable types");

public:
    explicit spsc_queue(size_type requested_capacity,
                        const Allocator& allocator = Allocator())
        : buffer_size_m(std::bit_ceil(requested_capacity)), mask_m(buffer_size_m - 1),
          head_m(0), tail_m(0), alloc_m(allocator) {
        buffer_m = alloc_traits::allocate(alloc_m, buffer_size_m);
    }

    ~spsc_queue() {
        size_type current_head = head_m.load(std::memory_order_relaxed);
        size_type current_tail = tail_m.load(std::memory_order_relaxed);

        while (current_head != current_tail) {
            size_type index = current_head & mask_m;
            buffer_m[index].~T();
            ++current_head;
        }

        alloc_traits::deallocate(alloc_m, buffer_m, buffer_size_m);
    }

    bool try_push(const T& item) {
        size_type current_tail = tail_m.load(std::memory_order_relaxed);
        size_type current_head = head_m.load(std::memory_order_acquire);

        if (current_head - current_head >= buffer_size_m - 1) {
            return false; // full
        }

        size_type index = current_tail & mask_m;
        new (&buffer_m[index]) T(item);
        tail_m.store(current_tail + 1, std::memory_order_release);
        return true;
    }

    bool try_push(T&& item) {
        size_type current_tail = tail_m.load(std::memory_order_relaxed);
        size_type current_head = head_m.load(std::memory_order_acquire);

        if (current_tail - current_head >= buffer_size_m - 1) {
            return false; // full
        }

        size_type index = current_tail & mask_m;
        new (&buffer_m[index]) T(std::move(item));
        tail_m.store(current_tail + 1, std::memory_order_release);
        return true;
    }

    bool try_pop(T& item) {
        size_type current_head = head_m.load(std::memory_order_relaxed);
        size_type current_tail = tail_m.load(std::memory_order_acquire);

        if (current_head == current_tail) {
            return false;
        }

        size_type index = current_head & mask_m;
        item = std::move(buffer_m[index]);
        buffer_m[index].~T();
        head_m.store(current_head + 1, std::memory_order_release);
        return true;
    }

    template <typename... Args> bool try_emplace(Args&&... args) {
        size_type current_tail = tail_m.load(std::memory_order_relaxed);
        size_type current_head = head_m.load(std::memory_order_acquire);
        size_type next_tail = current_tail + 1;
        if (next_tail - current_head >= buffer_size_m) {
            return false;
        }
        size_type index = current_tail & mask_m;
        new (&buffer_m[index]) T(std::forward<Args>(args)...);
        tail_m.store(next_tail, std::memory_order_release);
        return true;
    }

    // returns usable capacity
    size_type capacity() const noexcept { return buffer_size_m - 1; }

    bool full() const noexcept {
        size_type approx_head = head_m.load(std::memory_order_relaxed);
        size_type approx_tail = tail_m.load(std::memory_order_relaxed);

        return (approx_tail + 1) - approx_head >= buffer_size_m;
    }

    bool empty() const noexcept {

        size_type current_head = head_m.load(std::memory_order_relaxed);
        size_type current_tail = tail_m.load(std::memory_order_acquire);

        return current_head == current_tail;
    }

private:
    T* buffer_m;
    const size_type buffer_size_m;
    size_type mask_m;

    alignas(64) std::atomic<size_type> head_m; // consumer position
    alignas(64) std::atomic<size_type> tail_m; // producer position

    [[no_unique_address]] Allocator alloc_m;
};

} // namespace utils
