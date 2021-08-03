#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>

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

    void pushWait(const T& source) {
        std::unique_lock<std::mutex> m(mut);
        pushWaitCount++;

        m.unlock();
        popCond.notify_all();
        m.lock();

        pushCond.wait(m, [=]{
            if (this->bufferSize == 0) {
                return this->popWaitCount > 0 && this->queue.size() == 0;
            }
            return this->queue.size() < this->bufferSize;
        });

        queue.push(source);
        pushWaitCount--;

        m.unlock();
        popCond.notify_all();
    }

    void popWait(T& dist) {
        std::unique_lock<std::mutex> m(mut);
        popWaitCount++;
    
        m.unlock();
        pushCond.notify_all();
        m.lock();

        popCond.wait(m, [=]{
            return this->queue.size() > 0;
        });

        dist = queue.front();
        queue.pop();
        popWaitCount--;

        m.unlock();
        pushCond.notify_all();
    }

 private:
    bool                            terminated;
    int                             bufferSize;
    int                             pushWaitCount;  // unused
    int                             popWaitCount;
    std::queue<T>                   queue;

    mutable std::mutex              mut;
    mutable std::condition_variable pushCond;
    mutable std::condition_variable popCond;
};