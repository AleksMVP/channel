#include <memory>
#include <queue>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <iterator>
#include <cstddef> 

namespace Go {
class ChannelClosed : public std::exception {
 public:
    const char* what() const noexcept {
    	return "Channel is closed";
    }
};

template <typename T>
class Channel {
 public:
    Channel(int bufferSize = 0) noexcept : 
        terminated(false),
        bufferSize(bufferSize), 
        pushWaitCount(0), 
        popWaitCount(0) {}

    Channel(const Channel&) = delete;
    Channel(Channel&&)      = delete;

    Channel operator=(const Channel&) = delete;
    Channel operator=(Channel&&)      = delete;

    void pushWait(const T& source);
    void popWait(T& dist);
    void close() noexcept;

    class Iterator;
    Iterator begin();
    Iterator end();

    class Iterator {
     public:
        using iterator_category = std::input_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = T;
        using pointer           = T*;
        using reference         = const T&;

        Iterator(Channel<T>& chan);
        Iterator(Channel<T>& chan, bool isStopped);

        reference operator*() const;
        pointer   operator->();
        Iterator& operator++();
        Iterator  operator++(int);

        friend bool operator!= (const Iterator& lhs, const Iterator& rhs) {
            return lhs.isStopped != rhs.isStopped;
        }

     private:
        Channel<T>& chan;
        T           current;
        bool        isStopped;
    };

 private:
    int                             bufferSize;
    int                             pushWaitCount;  // unused
    int                             popWaitCount;
    std::atomic<bool>               terminated;
    std::queue<T>                   queue;

    mutable std::mutex              mut;
    mutable std::condition_variable pushCond;
    mutable std::condition_variable popCond;
};


template <typename T>
void Channel<T>::pushWait(const T& source) {
    std::unique_lock<std::mutex> m(mut);
    pushWaitCount++;

    m.unlock();
    popCond.notify_all();
    m.lock();

    pushCond.wait(m, [=]() -> bool {
        if (this->bufferSize == 0) {
            return (this->popWaitCount > 0 && this->queue.size() == 0) || this->terminated;
        }
        return this->queue.size() < this->bufferSize || this->terminated;
    });

    if (terminated.load(std::memory_order_acquire)) {
        throw ChannelClosed();
    }

    queue.push(source);
    pushWaitCount--;

    m.unlock();
    popCond.notify_all();
}

template <typename T>
void Channel<T>::popWait(T& dist) {
    std::unique_lock<std::mutex> m(mut);
    popWaitCount++;

    m.unlock();
    pushCond.notify_all();
    m.lock();

    popCond.wait(m, [=]() -> bool {
        return this->queue.size() > 0 || this->terminated;
    });

    if (terminated.load(std::memory_order_acquire)) {
        throw ChannelClosed();
    }

    dist = queue.front();
    queue.pop();
    popWaitCount--;

    m.unlock();
    pushCond.notify_all();
}

template <typename T>
void Channel<T>::close() noexcept {
    terminated.store(true, std::memory_order_release);

    pushCond.notify_all();
    popCond.notify_all();
}

template <typename T>
Channel<T>::Iterator::Iterator(Channel<T>& chan) : chan(chan), isStopped(false) {
    try {
        chan.popWait(current); 
    } catch (ChannelClosed& ex) {
        isStopped = true;
    }
}

template <typename T>
Channel<T>::Iterator::Iterator(Channel<T>& chan, bool isStopped) : 
        chan(chan), isStopped(isStopped) {}


template <typename T>
typename Channel<T>::Iterator::reference Channel<T>::Iterator::operator*() const { 
    if (isStopped) {
        throw std::runtime_error("Iterator ended");
    }

    return current; 
}

template <typename T>
typename Channel<T>::Iterator::pointer Channel<T>::Iterator::operator->() { 
    if (isStopped) {
        throw std::runtime_error("Iterator ended");
    }

    return &current; 
}

template <typename T>
typename Channel<T>::Iterator& Channel<T>::Iterator::operator++() { 
    try {
        chan.popWait(current); 
    } catch (ChannelClosed& ex) {
        isStopped = true;
    }

    return *this; 
}  

template <typename T>
typename Channel<T>::Iterator Channel<T>::Iterator::operator++(int) { 
    if (isStopped) {
        throw std::runtime_error("Iterator ended");
    }

    Iterator tmp = *this; 
    ++(*this); 

    return tmp; 
}

template <typename T>
typename Channel<T>::Iterator Channel<T>::begin() { 
    return typename Channel<T>::Iterator(*this); 
}

template <typename T>
typename Channel<T>::Iterator Channel<T>::end() { 
    return typename Channel<T>::Iterator(*this, true); 
}
}