#include <unity.h>

#include <cstdint>
#include <vector>

#include "fake_clock.h"
#include "scheduler.h"

namespace {

class CountingTask final : public core::ITask {
public:
    void run(uint32_t nowMs) override {
        ++runs;
        lastRunMs = nowMs;
    }

    uint32_t runs = 0;
    uint32_t lastRunMs = 0;
};

class OrderTask final : public core::ITask {
public:
    OrderTask(std::vector<int>& order, int id) : order_(order), id_(id) {}

    void run(uint32_t) override { order_.push_back(id_); }

private:
    std::vector<int>& order_;
    int id_;
};

class Accumulator {
public:
    void add(uint32_t nowMs) { total += nowMs; }

    uint32_t total = 0;
};

void tickAt(FakeClock& clock, core::Scheduler& scheduler, uint32_t nowMs) {
    clock.now = nowMs;
    scheduler.tick();
}

void tickEveryMillisecond(FakeClock& clock, core::Scheduler& scheduler, uint32_t durationMs) {
    for (uint32_t i = 0; i < durationMs; ++i) {
        ++clock.now;
        scheduler.tick();
    }
}

}

void setUp() {}
void tearDown() {}

void test_task_waits_one_interval_before_first_run() {
    FakeClock clock;
    core::Scheduler scheduler(clock);
    CountingTask task;
    scheduler.add(task, 100);

    tickAt(clock, scheduler, 99);
    TEST_ASSERT_EQUAL_UINT32(0, task.runs);

    tickAt(clock, scheduler, 100);
    TEST_ASSERT_EQUAL_UINT32(1, task.runs);
    TEST_ASSERT_EQUAL_UINT32(100, task.lastRunMs);
}

void test_task_runs_once_per_interval() {
    FakeClock clock;
    core::Scheduler scheduler(clock);
    CountingTask task;
    scheduler.add(task, 100);

    tickEveryMillisecond(clock, scheduler, 1000);

    TEST_ASSERT_EQUAL_UINT32(10, task.runs);
}

void test_tasks_keep_independent_intervals() {
    FakeClock clock;
    core::Scheduler scheduler(clock);
    CountingTask fast;
    CountingTask slow;
    scheduler.add(fast, 200);
    scheduler.add(slow, 500);

    tickEveryMillisecond(clock, scheduler, 1000);

    TEST_ASSERT_EQUAL_UINT32(5, fast.runs);
    TEST_ASSERT_EQUAL_UINT32(2, slow.runs);
}

void test_tasks_due_together_run_in_registration_order() {
    FakeClock clock;
    core::Scheduler scheduler(clock);
    std::vector<int> order;
    OrderTask first(order, 1);
    OrderTask second(order, 2);
    scheduler.add(first, 100);
    scheduler.add(second, 100);

    tickAt(clock, scheduler, 100);

    TEST_ASSERT_EQUAL_UINT32(2, order.size());
    TEST_ASSERT_EQUAL_INT(1, order[0]);
    TEST_ASSERT_EQUAL_INT(2, order[1]);
}

void test_late_tick_runs_once_and_keeps_the_original_grid() {
    FakeClock clock;
    core::Scheduler scheduler(clock);
    CountingTask task;
    scheduler.add(task, 100);

    tickAt(clock, scheduler, 350);
    TEST_ASSERT_EQUAL_UINT32(1, task.runs);

    tickAt(clock, scheduler, 399);
    TEST_ASSERT_EQUAL_UINT32(1, task.runs);

    tickAt(clock, scheduler, 400);
    TEST_ASSERT_EQUAL_UINT32(2, task.runs);

    tickAt(clock, scheduler, 499);
    TEST_ASSERT_EQUAL_UINT32(2, task.runs);

    tickAt(clock, scheduler, 500);
    TEST_ASSERT_EQUAL_UINT32(3, task.runs);
}

void test_grid_is_anchored_at_registration_time() {
    FakeClock clock;
    clock.now = 1030;
    core::Scheduler scheduler(clock);
    CountingTask task;
    scheduler.add(task, 100);

    tickAt(clock, scheduler, 1129);
    TEST_ASSERT_EQUAL_UINT32(0, task.runs);

    tickAt(clock, scheduler, 1130);
    TEST_ASSERT_EQUAL_UINT32(1, task.runs);
}

void test_intervals_survive_millisecond_counter_rollover() {
    FakeClock clock;
    clock.now = 0xFFFFFF00u;
    core::Scheduler scheduler(clock);
    CountingTask task;
    scheduler.add(task, 100);

    tickAt(clock, scheduler, 0xFFFFFF63u);
    TEST_ASSERT_EQUAL_UINT32(0, task.runs);
    tickAt(clock, scheduler, 0xFFFFFF64u);
    TEST_ASSERT_EQUAL_UINT32(1, task.runs);
    tickAt(clock, scheduler, 0xFFFFFFC8u);
    TEST_ASSERT_EQUAL_UINT32(2, task.runs);

    tickAt(clock, scheduler, 0x0000002Bu);
    TEST_ASSERT_EQUAL_UINT32(2, task.runs);
    tickAt(clock, scheduler, 0x0000002Cu);
    TEST_ASSERT_EQUAL_UINT32(3, task.runs);
}

void test_add_rejects_zero_interval() {
    FakeClock clock;
    core::Scheduler scheduler(clock);
    CountingTask task;

    TEST_ASSERT_FALSE(scheduler.add(task, 0));
}

void test_add_rejects_tasks_beyond_capacity() {
    FakeClock clock;
    core::Scheduler scheduler(clock);
    CountingTask task;

    for (size_t i = 0; i < core::Scheduler::kMaxTasks; ++i) {
        TEST_ASSERT_TRUE(scheduler.add(task, 100));
    }
    TEST_ASSERT_FALSE(scheduler.add(task, 100));
}

void test_method_task_forwards_to_member_function() {
    FakeClock clock;
    core::Scheduler scheduler(clock);
    Accumulator accumulator;
    core::MethodTask<Accumulator, &Accumulator::add> task(accumulator);
    scheduler.add(task, 100);

    tickAt(clock, scheduler, 100);
    tickAt(clock, scheduler, 200);

    TEST_ASSERT_EQUAL_UINT32(300, accumulator.total);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_task_waits_one_interval_before_first_run);
    RUN_TEST(test_task_runs_once_per_interval);
    RUN_TEST(test_tasks_keep_independent_intervals);
    RUN_TEST(test_tasks_due_together_run_in_registration_order);
    RUN_TEST(test_late_tick_runs_once_and_keeps_the_original_grid);
    RUN_TEST(test_grid_is_anchored_at_registration_time);
    RUN_TEST(test_intervals_survive_millisecond_counter_rollover);
    RUN_TEST(test_add_rejects_zero_interval);
    RUN_TEST(test_add_rejects_tasks_beyond_capacity);
    RUN_TEST(test_method_task_forwards_to_member_function);
    return UNITY_END();
}
