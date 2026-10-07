#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "clock.h"

namespace core {

class ITask {
public:
    virtual void run(uint32_t nowMs) = 0;

protected:
    ~ITask() = default;
};

template <typename T, void (T::*Method)(uint32_t)>
class MethodTask final : public ITask {
public:
    explicit MethodTask(T& target) : target_(target) {}

    void run(uint32_t nowMs) override { (target_.*Method)(nowMs); }

private:
    T& target_;
};

class Scheduler {
public:
    static constexpr size_t kMaxTasks = 8;
    static constexpr uint32_t kNothingScheduledMs = UINT32_MAX;

    explicit Scheduler(const IClock& clock);

    bool add(ITask& task, uint32_t intervalMs);
    void tick();
    uint32_t timeUntilNextDueMs() const;

private:
    struct Entry {
        ITask*   task;
        uint32_t intervalMs;
        uint32_t lastDueMs;
    };

    const IClock& clock_;
    std::array<Entry, kMaxTasks> entries_{};
    size_t count_ = 0;
};

}
