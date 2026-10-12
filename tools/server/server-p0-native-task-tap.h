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
    slot_assigned = 1,
    sampled_token = 2,
    final_result_enqueued = 3,
};

struct event {
    stage stage_value;
    int task_id;
    int slot_id;
    std::uint64_t n_generated;
};

#ifdef LLAMA_SERVER_P0_NATIVE_TASK_TEST_TAP
using observer_fn = void (*)(const event &) noexcept;

inline std::atomic<observer_fn> & fixture_observer() noexcept {
    static std::atomic<observer_fn> callback {nullptr};
    return callback;
}

inline void register_test_observer(observer_fn callback) noexcept {
    fixture_observer().store(callback, std::memory_order_release);
}
#endif

inline void emit(stage which, int task_id, int slot_id, std::uint64_t n_generated) noexcept {
#ifdef LLAMA_SERVER_P0_NATIVE_TASK_TEST_TAP
    if (task_id < 0 || slot_id < 0) {
        return;
    }
    const auto observer = fixture_observer().load(std::memory_order_acquire);
    if (observer != nullptr) {
        observer({which, task_id, slot_id, n_generated});
    }
#else
    // A default build cannot install an observer or emit native events.
    (void) which;
    (void) task_id;
    (void) slot_id;
    (void) n_generated;
#endif
}

} // namespace llama_server_p0_native_task
