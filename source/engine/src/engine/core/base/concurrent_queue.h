#pragma once

namespace my {

template<typename T>
class ConcurrentQueue {
public:
    void push(const T& p_value) {
        m_mutex.lock();
        m_queue.emplace(p_value);
        m_mutex.unlock();
    }

    bool pop(T& p_out_value) {
        m_mutex.lock();
        if (m_queue.empty()) {
            m_mutex.unlock();
            return false;
        }

        p_out_value = m_queue.front();
        m_queue.pop();
        m_mutex.unlock();
        return true;
    }

    std::queue<T> pop_all() {
        std::queue<T> ret;
        m_mutex.lock();
        m_queue.swap(ret);
        m_mutex.unlock();
        return ret;
    }

private:
    std::queue<T> m_queue;
    mutable std::mutex m_mutex;
};

}  // namespace my
