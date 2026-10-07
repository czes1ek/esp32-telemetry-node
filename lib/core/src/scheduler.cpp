#include "scheduler.h"

namespace core {

Scheduler::Scheduler(const IClock& clock) : clock_(clock) {}

bool Scheduler::add(ITask& task, uint32_t intervalMs) {
    if (count_ == kMaxTasks || intervalMs == 0) return false;
    entries_[count_] = Entry{&task, intervalMs, clock_.nowMs()};
    ++count_;
    return true;
}

void Scheduler::tick() {
    const uint32_t nowMs = clock_.nowMs();
    for (size_t i = 0; i < count_; ++i) {
        Entry& entry = entries_[i];
        const uint32_t elapsedMs = nowMs - entry.lastDueMs;
        if (elapsedMs < entry.intervalMs) continue;

        const uint32_t periodsElapsed = elapsedMs / entry.intervalMs;
        entry.lastDueMs += periodsElapsed * entry.intervalMs;
        entry.task->run(nowMs);
    }
}

}
