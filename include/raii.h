#ifndef X_PILOT_RAII_H
#define X_PILOT_RAII_H

#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string>

// RAII 封装文件描述符
// 构造时接管 fd，析构时自动 close，不用手动管
class ScopedFd {
public:
    explicit ScopedFd(int fd = -1) : m_fd(fd) {}
    ~ScopedFd() { reset(); }

    // 禁止拷贝（fd 不能被两个对象同时管）
    ScopedFd(const ScopedFd&) = delete;
    ScopedFd& operator=(const ScopedFd&) = delete;

    // 允许移动（转移所有权）
    ScopedFd(ScopedFd&& other) : m_fd(other.m_fd) { other.m_fd = -1; }
    ScopedFd& operator=(ScopedFd&& other) {
        if (this != &other) {
            reset();
            m_fd = other.m_fd;
            other.m_fd = -1;
        }
        return *this;
    }

    // 获取原始 fd
    int get() const { return m_fd; }

    // 是否有效
    bool valid() const { return m_fd != -1; }

    // 释放当前 fd 并接管新的
    void reset(int new_fd = -1) {
        if (m_fd != -1) ::close(m_fd);
        m_fd = new_fd;
    }

    // 放弃所有权（返回 fd，自己不再管）
    int release() {
        int tmp = m_fd;
        m_fd = -1;
        return tmp;
    }

private:
    int m_fd;
};

// RAII 封装共享内存
// 构造时 shm_open + mmap，析构时自动 munmap + close + shm_unlink
class ScopedShm {
public:
    ScopedShm() = default;
    ~ScopedShm() { reset(); }

    ScopedShm(const ScopedShm&) = delete;
    ScopedShm& operator=(const ScopedShm&) = delete;

    // 创建并映射共享内存
    // name: 如 "/x_pilot_shm"
    // size: 映射大小
    // read_only: true 只读，false 读写
    bool open(const std::string& name, size_t size, bool read_only) {
        reset();

        int flags = read_only ? O_RDONLY : (O_CREAT | O_RDWR);
        m_fd = ScopedFd(::shm_open(name.c_str(), flags, 0666));
        if (!m_fd.valid()) return false;

        if (!read_only) {
            if (ftruncate(m_fd.get(), size) == -1) return false;
        }

        int prot = read_only ? PROT_READ : (PROT_READ | PROT_WRITE);
        m_ptr = ::mmap(nullptr, size, prot, MAP_SHARED, m_fd.get(), 0);
        if (m_ptr == MAP_FAILED) {
            m_ptr = nullptr;
            m_fd.reset();
            return false;
        }

        m_size = size;
        m_name = name;
        m_owner = !read_only;  // 读写创建者是 owner，负责 shm_unlink
        return true;
    }

    void* get() const { return m_ptr; }

    void reset() {
        if (m_ptr && m_ptr != MAP_FAILED) {
            ::munmap(m_ptr, m_size);
            m_ptr = nullptr;
        }
        m_fd.reset();
        // 只有创建者才 unlink，避免 reader 把 shared memory 删了
        if (m_owner && !m_name.empty()) {
            ::shm_unlink(m_name.c_str());
            m_name.clear();
            m_owner = false;
        }
        m_size = 0;
    }

private:
    ScopedFd m_fd;
    void* m_ptr = nullptr;
    size_t m_size = 0;
    std::string m_name;
    bool m_owner = false;
};

#endif