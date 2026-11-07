#pragma once

#include <thread>
#include <utility>

namespace perf_lab {

#if defined(__cpp_lib_jthread) && __cpp_lib_jthread >= 201911L
using ManagedThread = std::jthread;
#else
// RAII wrapper around std::thread that automatically joins on destruction,
// matching the lifecycle semantics of C++20 std::jthread.
class ManagedThread {
public:
    ManagedThread() noexcept = default;

    template <typename Callable, typename... Args>
    explicit ManagedThread(Callable&& f, Args&&... args)
        : m_thread(std::forward<Callable>(f), std::forward<Args>(args)...) {}

    ~ManagedThread() {
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    ManagedThread(const ManagedThread&) = delete;
    ManagedThread& operator=(const ManagedThread&) = delete;

    ManagedThread(ManagedThread&& other) noexcept : m_thread(std::move(other.m_thread)) {}

    ManagedThread& operator=(ManagedThread&& other) noexcept {
        if (this != &other) {
            if (m_thread.joinable()) {
                m_thread.join();
            }
            m_thread = std::move(other.m_thread);
        }
        return *this;
    }

    [[nodiscard]] bool joinable() const noexcept {
        return m_thread.joinable();
    }

    void join() {
        m_thread.join();
    }

    [[nodiscard]] std::thread::id get_id() const noexcept {
        return m_thread.get_id();
    }

private:
    std::thread m_thread;
};
#endif

} // namespace perf_lab
