#include <wss/observer.hpp>

namespace wss {

void MetricsCollector::on_start(std::uint64_t, const char*) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++data_.started;
}

void MetricsCollector::on_finish(std::uint64_t, const char*, TaskState state,
                                 std::chrono::nanoseconds elapsed) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++data_.finished;
    data_.total_ns += static_cast<std::uint64_t>(elapsed.count());
    if (state == TaskState::Cancelled) ++data_.cancelled;
    if (state == TaskState::Failed) ++data_.failed;
}

void MetricsCollector::on_steal(std::uint64_t, std::size_t, std::size_t) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++data_.stolen;
}

MetricsSnapshot MetricsCollector::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
}

LogObserver::LogObserver(FILE* out) : out_(out ? out : stderr) {}

void LogObserver::on_start(std::uint64_t id, const char* kind) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::fprintf(out_, "[wss] start id=%llu kind=%s\n",
                 static_cast<unsigned long long>(id), kind);
}

void LogObserver::on_finish(std::uint64_t id, const char* kind, TaskState state,
                            std::chrono::nanoseconds elapsed) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::fprintf(out_, "[wss] finish id=%llu kind=%s state=%.*s elapsed_ns=%lld\n",
                 static_cast<unsigned long long>(id), kind,
                 static_cast<int>(to_string(state).size()),
                 to_string(state).data(),
                 static_cast<long long>(elapsed.count()));
}

}  // namespace wss
