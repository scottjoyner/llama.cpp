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
static event captured[4] {};
static int captured_count = 0;

static void collect(const event & observation) noexcept {
    if (captured_count < 4) {
        captured[captured_count++] = observation;
    }
}
#endif

int main() {
#ifdef LLAMA_SERVER_P0_NATIVE_TASK_TEST_TAP
    register_test_observer(collect);

    emit(stage::slot_assigned, 42, 3, 0);
    emit(stage::sampled_token, 42, 3, 1);
    emit(stage::final_result_enqueued, 42, 3, 3);
    // Malformed/native-unknown identifiers cannot produce an event.
    emit(stage::slot_assigned, -1, 3, 0);
    emit(stage::sampled_token, 42, -1, 1);

    if (captured_count != 3 ||
            captured[0].stage_value != stage::slot_assigned ||
            captured[1].stage_value != stage::sampled_token ||
            captured[2].stage_value != stage::final_result_enqueued ||
            captured[0].task_id != 42 ||
            captured[2].slot_id != 3 ||
            captured[2].n_generated != 3) {
        return 1;
    }
    register_test_observer(nullptr);
    emit(stage::slot_assigned, 51, 4, 0);
    return captured_count == 3 ? 0 : 2;
#else
    // By default, there is no observer registration API or active sink:
    // calls compile to no-op even in a program that invokes all stages.
    emit(stage::slot_assigned, 42, 3, 0);
    emit(stage::sampled_token, 42, 3, 1);
    emit(stage::final_result_enqueued, 42, 3, 3);
    return 0;
#endif
}
