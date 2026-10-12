// Compile TWICE with plain C++17, default-OFF and fixture-ON.
// No actual server/process/model/socket or client input is exercised.
#include "../../server-p0-native-task-tap.h"

#include <cstdint>
#include <type_traits>

using namespace llama_server_p0_native_task;

static_assert(std::is_trivially_copyable<event>::value, "no payload buffers");
static_assert(sizeof(event) <= 40, "bounded metadata-only event record");

#ifdef LLAMA_SERVER_P0_NATIVE_TASK_TEST_TAP
static event captured[16] {};
static int captured_count = 0;

static void collect(const event & observation) noexcept {
    if (captured_count < 16) {
        captured[captured_count++] = observation;
    }
}
#endif

int main() {
#ifdef LLAMA_SERVER_P0_NATIVE_TASK_TEST_TAP
    register_test_observer(collect);
    const auto key = next_test_request_key();
    const auto second = next_test_request_key();
    if (key == 0 || second == 0 || key == second) return 1;

    emit(stage::task_allocated, 42, -1, 0, -1, key);
    emit(stage::task_allocated, 44, -1, 0, 42, key);
    emit(stage::allocation_set_closed, -1, -1, 0, -1, key, 2);
    emit(stage::slot_assigned, 42, 3, 0, -1, key);
    emit(stage::slot_assigned, 44, 4, 0, 42, key);
    emit(stage::sampled_token, 44, 4, 1, 42, key);
    emit(stage::final_result_enqueued, 44, 4, 3, 42, key);

    // All the following invalid events must be rejected without side effects.
    emit(stage::slot_assigned, -1, 3, 0, -1, key);
    emit(stage::sampled_token, 42, -1, 1, -1, key);
    emit(stage::task_allocated, 42, 3, 0, -1, key);
    emit(stage::slot_assigned, 42, 3, 0, -2, key);
    emit(stage::slot_assigned, 42, 3, 0, 42, key);
    emit(stage::task_allocated, 43, -1, 0, -1, 0);
    emit(stage::allocation_set_closed, -1, -1, 0, -1, 0, 1);
    emit(stage::allocation_set_closed, -1, 0, 0, -1, key, 1);
    emit(stage::allocation_set_closed, -1, -1, 0, -1, key, 0);
    emit(stage::allocation_set_closed, -1, -1, 0, -1, key, 65);
    emit(stage::allocation_set_closed, -1, -1, 1, -1, key, 1);
    emit(stage::slot_assigned, 44, 4, 0, 42, key, 1);

    if (captured_count != 7 ||
            captured[0].stage_value != stage::task_allocated ||
            captured[1].stage_value != stage::task_allocated ||
            captured[2].stage_value != stage::allocation_set_closed ||
            captured[2].allocated_task_count != 2 ||
            captured[2].request_key != key ||
            captured[3].stage_value != stage::slot_assigned ||
            captured[4].stage_value != stage::slot_assigned ||
            captured[5].stage_value != stage::sampled_token ||
            captured[6].stage_value != stage::final_result_enqueued ||
            captured[0].task_id != 42 ||
            captured[0].slot_id != -1 ||
            captured[1].task_id != 44 ||
            captured[1].parent_task_id != 42 ||
            captured[4].parent_task_id != 42 ||
            captured[6].n_generated != 3 ||
            captured[6].request_key != key) {
        return 2;
    }
    register_test_observer(nullptr);
    emit(stage::task_allocated, 51, -1, 0, -1, second);
    return captured_count == 7 ? 0 : 3;
#else
    // No registration API or allocation-key API is compiled by default.
    emit(stage::task_allocated, 42, -1, 0);
    emit(stage::task_allocated, 44, -1, 0, 42);
    emit(stage::allocation_set_closed, -1, -1, 0, -1, 0, 2);
    emit(stage::slot_assigned, 42, 3, 0);
    emit(stage::sampled_token, 44, 4, 1, 42);
    emit(stage::final_result_enqueued, 44, 4, 3, 42);
    return 0;
#endif
}
