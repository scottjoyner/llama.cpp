// P0 #37: compile-time-OFF native inference task instrumentation seam.
//
// This header contains NO request-body, token-text, user-identity, network,
// filesystem, logging, signing-key, or persistence capability.
//
// The test callback exists ONLY with explicit
// -DLLAMA_SERVER_P0_NATIVE_TASK_TEST_TAP. No production build definition,
// configuration knob, or app route installs it. These are internal native
// task hints, NOT provider attestation and NOT per-human receipts.
//
// IMPORTANT: the callback must be nonblocking and noexcept: it is called
// on the server's inference scheduling thread. A real signed node observer
// will require a separately reviewed bounded queue, trusted key custody,
// process/model attestation and correlation to a verified root+attempt.
#pragma once

#include <cstdint>

#ifdef LLAMA_SERVER_P0_NATIVE_TASK_TEST_TAP
#  include <atomic>
#endif

namespace llama_server_p0_native_task {

enum class stage : std::uint8_t {
    // Created at the real completion-route task allocation site. It is
    // NOT queued, admitted, authenticated to a human or executed yet.
    task_allocated = 0,
    slot_assigned = 1,
    sampled_token = 2,
    final_result_enqueued = 3,
    // All task IDs for ONE local request have been enumerated. This is
    // BEFORE posting; it does NOT prove queue admission or execution.
    allocation_set_closed = 4,
};

struct event {
    stage stage_value;
    int task_id;
    int slot_id;
    int parent_task_id; // -1: top-level task; child IDs share a request, not an actor.
    std::uint64_t n_generated;
    // Process-local ephemeral grouping key, NOT an authenticated human ID.
    std::uint64_t request_key;
    // Meaningful ONLY for allocation_set_closed.
    std::uint32_t allocated_task_count;
};

#ifdef LLAMA_SERVER_P0_NATIVE_TASK_TEST_TAP
using observer_fn = void (*)(const event &) noexcept;

inline std::uint64_t next_test_request_key() noexcept {
    static std::atomic<std::uint64_t> current {1};
    const auto key = current.fetch_add(1, std::memory_order_relaxed);
    // Refuse wraparound; real production epoch/fencing is NOT implemented.
    return key == 0 ? 0 : key;
}

inline std::atomic<observer_fn> & fixture_observer() noexcept {
    static std::atomic<observer_fn> callback {nullptr};
    return callback;
}

inline void register_test_observer(observer_fn callback) noexcept {
    fixture_observer().store(callback, std::memory_order_release);
}
#endif

inline void emit(stage which, int task_id, int slot_id, std::uint64_t n_generated,
                 int parent_task_id = -1, std::uint64_t request_key = 0,
                 std::uint32_t allocated_task_count = 0) noexcept {
#ifdef LLAMA_SERVER_P0_NATIVE_TASK_TEST_TAP
    if (request_key == 0 || parent_task_id < -1 ||
            (which == stage::allocation_set_closed
                ? (task_id != -1 || slot_id != -1 || parent_task_id != -1
                    || n_generated != 0 || allocated_task_count == 0
                    || allocated_task_count > 64)
                : (task_id < 0 || parent_task_id == task_id
                    || allocated_task_count != 0
                    || (which == stage::task_allocated
                        ? (slot_id != -1 || n_generated != 0)
                        : slot_id < 0)))) {
        return;
    }
    const auto observer = fixture_observer().load(std::memory_order_acquire);
    if (observer != nullptr) {
        observer({
            which, task_id, slot_id, parent_task_id, n_generated,
            request_key, allocated_task_count,
        });
    }
#else
    // Default build cannot register an observer or emit native events.
    (void) which;
    (void) task_id;
    (void) slot_id;
    (void) n_generated;
    (void) parent_task_id;
    (void) request_key;
    (void) allocated_task_count;
#endif
}

} // namespace llama_server_p0_native_task
