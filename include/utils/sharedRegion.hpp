#pragma once
#include <cstddef>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <unistd.h>

class SharedRegion {
public:
    // Create a shared memory region of given size
    // name: optional name for POSIX shm ("/myqueue"), empty string = anonymous
    explicit SharedRegion(std::size_t size, const std::string& name = "")
        : size_(size), name_(name) {
        if (!name_.empty()) {
            // POSIX named shared memory
            fd_ = shm_open(name_.c_str(), O_CREAT | O_RDWR, 0600);
            if (fd_ == -1) throw std::runtime_error("shm_open failed");

            if (ftruncate(fd_, static_cast<long>(size_)) == -1) {
                ::close(fd_);
                throw std::runtime_error("ftruncate failed");
            }

            ptr_ = mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
            if (ptr_ == MAP_FAILED) {
                ::close(fd_);
                throw std::runtime_error("mmap failed");
            }
        } else {
            // Anonymous shared memory
            ptr_ = mmap(nullptr, size_, PROT_READ | PROT_WRITE,
                        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
            if (ptr_ == MAP_FAILED) {
                throw std::runtime_error("anonymous mmap failed");
            }
            fd_ = -1;
        }
    }

    ~SharedRegion() {
        if (ptr_) {
            munmap(ptr_, size_);
            ptr_ = nullptr;
        }

        if (fd_ != -1) {
            ::close(fd_);
            if (!name_.empty()) {
                shm_unlink(name_.c_str());
            }
        }
    }

    // Non-copyable
    SharedRegion(const SharedRegion&) = delete;
    SharedRegion& operator=(const SharedRegion&) = delete;

    // Movable
    SharedRegion(SharedRegion&& other) = delete;

    SharedRegion& operator=(SharedRegion&& other) = delete;

    void* data() noexcept { return ptr_; }
    const void* data() const noexcept { return ptr_; }
    std::size_t size() const noexcept { return size_; }

private:
    void* ptr_ = nullptr;
    std::size_t size_ = 0;
    int fd_ = -1;
    std::string name_;
};
