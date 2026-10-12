// Standalone test: compile this file TWICE, with and without
// -DLLAMA_SERVER_P0_NATIVE_TASK_TEST_TAP, using only a standard C++17 compiler.
//
// This is a fixture, NOT an observer and NOT proof of real provider execution.
#include "../../server-p0-native-task-tap.h"

#include <cstdint>
#include <type_traits>

using namespace llama_server_p0_native_task;

static_assert(std::is_trivially_copyable<event>::value, "no event payload buffers");
static_assert(sizeof(event) <= 32, "bounded metadata-only observer event");

#ifdef LLAMA_SERVER_P0_NATIVE_TASK_TEST_TAP
static event captured[10] {};
static int captured_count = 0;

static void collect(const event & observation) noexcept {
    if (captured_count < 10) {
        captured[captured_count++] = observation;
    }
}
#endif

int main() {
#ifdef LLAMA_SERVER_P0_NATIVE_TASK_TEST_TAP
    register_test_observer(collect);

    // Allocation happens at the real completion route after get_new_id(),
    // before work is posted to the queue, and cannot claim slot admission.
    emit(stage::task_allocated, 42, -1, 0);
    emit(stage::task_allocated, 44, -1, 0, 42); // n_cmpl child task
    emit(stage::slot_assigned, 42, 3, 0);
    emit(stage::slot_assigned, 44, 4, 0, 42);
    emit(stage::sampled_token, 44, 4, 1, 42);
    emit(stage::final_result_enqueued, 44, 4, 3, 42);
    // Invalid identifiers/parent relationships must never enter test observer.
    emit(stage::slot_assigned, -1, 3, 0);
    emit(stage::sampled_token, 42, -1, 1);
    emit(stage::task_allocated, 42, 3, 0);
    emit(stage::slot_assigned, 42, 3, 0, -2);
    emit(stage::slot_assigned, 42, 3, 0, 42);

    if (captured_count != 6 ||
            captured[0].stage_value != stage::task_allocated ||
            captured[1].stage_value != stage::task_allocated ||
            captured[2].stage_value != stage::slot_assigned ||
            captured[3].stage_value != stage::slot_assigned ||
            captured[4].stage_value != stage::sampled_token ||
            captured[5].stage_value != stage::final_result_enqueued ||
            captured[0].task_id != 42 ||
            captured[0].slot_id != -1 ||
            captured[1].task_id != 44 ||
            captured[1].parent_task_id != 42 ||
            captured[3].parent_task_id != 42 ||
            captured[5].n_generated != 3) {
        return 1;
    }
    register_test_observer(nullptr);
    emit(stage::task_allocated, 51, -1, 0);
    return captured_count == 6 ? 0 : 2;
#else
    // By default, there is no observer registration API or active sink:
    // calls compile to no-op even in a program that invokes all stages.
    emit(stage::task_allocated, 42, -1, 0);
    emit(stage::task_allocated, 44, -1, 0, 42);
    emit(stage::slot_assigned, 42, 3, 0);
    emit(stage::sampled_token, 44, 4, 1, 42);
    emit(stage::final_result_enqueued, 44, 4, 3, 42);
    return 0;
#endif
}
