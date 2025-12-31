#pragma once

#include <atomic>
#include <bit>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace utils {
template <typename T> class spsc_queue_shm {
    static_assert(std::is_trivially_copyable_v<T>,
                  "spsc_queue_shm requires trivially copyable types");
    using size_type = std::size_t;

public:
    void init(size_type capacity) {
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

    /*
     * Since this object is meant to exist in a shared memory space, regular RAII rules
     * don't apply, dtor, ctor and other special members are deleted, since they would
     * cause UB
     */
    spsc_queue_shm() = delete;
    ~spsc_queue_shm() = delete;
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

} // namespace utils
