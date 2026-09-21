#include <wss/work_queue.hpp>

namespace wss {

MutexQueue::MutexQueue(bool lifo) : lifo_(lifo) {}

bool MutexQueue::push(TaskPtr task) {
    if (!task) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    deque_.push_back(std::move(task));
    return true;
}

bool MutexQueue::try_pop(TaskPtr& out) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (deque_.empty()) return false;
    if (lifo_) {
        out = std::move(deque_.back());
        deque_.pop_back();
    } else {
        out = std::move(deque_.front());
        deque_.pop_front();
    }
    return true;
}

bool MutexQueue::try_steal(TaskPtr& out) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (deque_.empty()) return false;
    out = std::move(deque_.front());
    deque_.pop_front();
    return true;
}

std::size_t MutexQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return deque_.size();
}

bool MutexQueue::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return deque_.empty();
}

void MutexQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    deque_.clear();
}

ChaseLevDeque::ChaseLevDeque() : slots_(kCapacity) {}

ChaseLevDeque::~ChaseLevDeque() = default;

bool ChaseLevDeque::push(TaskPtr task) {
    if (!task) return false;
    std::int64_t b = bottom_.load(std::memory_order_relaxed);
    std::int64_t t = top_.load(std::memory_order_acquire);
    if (b - t >= static_cast<std::int64_t>(kCapacity)) return false;
    slots_[static_cast<std::size_t>(b) % kCapacity] = std::move(task);
    bottom_.store(b + 1, std::memory_order_release);
    return true;
}

bool ChaseLevDeque::try_pop(TaskPtr& out) {
    std::int64_t b = bottom_.load(std::memory_order_relaxed) - 1;
    bottom_.store(b, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    std::int64_t t = top_.load(std::memory_order_acquire);
    if (t <= b) {
        if (t == b) {
            if (!top_.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst,
                                              std::memory_order_relaxed)) {
                bottom_.store(b + 1, std::memory_order_relaxed);
                return false;
            }
            bottom_.store(b + 1, std::memory_order_relaxed);
        }
        // Slots are never reset: a loser may still be copying this shared_ptr
        // while the winner returns, and resetting under a concurrent copy is
        // a data race. Stale entries are overwritten by later pushes.
        out = slots_[static_cast<std::size_t>(b) % kCapacity];
        return true;
    }
    bottom_.store(b + 1, std::memory_order_relaxed);
    return false;
}

bool ChaseLevDeque::try_steal(TaskPtr& out) {
    std::int64_t t = top_.load(std::memory_order_acquire);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    std::int64_t b = bottom_.load(std::memory_order_acquire);
    if (t >= b) return false;
    out = slots_[static_cast<std::size_t>(t) % kCapacity];
    if (!top_.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst,
                                      std::memory_order_relaxed)) {
        return false;
    }
    return true;
}

std::size_t ChaseLevDeque::size() const {
    std::int64_t b = bottom_.load(std::memory_order_acquire);
    std::int64_t t = top_.load(std::memory_order_acquire);
    std::int64_t n = b - t;
    return n > 0 ? static_cast<std::size_t>(n) : 0;
}

bool ChaseLevDeque::empty() const { return size() == 0; }

void ChaseLevDeque::clear() {
    TaskPtr tmp;
    while (try_pop(tmp)) {
    }
    while (try_steal(tmp)) {
    }
}

std::unique_ptr<IWorkQueue> make_queue(QueueKind kind, bool lifo) {
    if (kind == QueueKind::Mutex) return std::make_unique<MutexQueue>(lifo);
    return std::make_unique<ChaseLevDeque>();
}

}  // namespace wss
