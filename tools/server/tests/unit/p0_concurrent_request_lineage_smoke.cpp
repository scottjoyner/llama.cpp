// Standalone concurrent native-request allocation fixture.
// Never starts a server, sends a prompt, or signs user claims.
#include "../../server-p0-native-task-tap.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace llama_server_p0_native_task;

#ifdef LLAMA_SERVER_P0_NATIVE_TASK_TEST_TAP
static constexpr int N = 24;
static constexpr int MAX_EVENTS = 160;
static std::array<event, MAX_EVENTS> observations {};
static std::atomic<int> count {0};

static void capture(const event & value) noexcept {
    const auto index = count.fetch_add(1, std::memory_order_relaxed);
    if (index < MAX_EVENTS) {
        observations[static_cast<size_t>(index)] = value;
    }
}
#endif

int main() {
#ifdef LLAMA_SERVER_P0_NATIVE_TASK_TEST_TAP
    register_test_observer(capture);
    std::array<std::thread, N> threads;
    for (int n = 0; n < N; ++n) {
        threads[n] = std::thread([n] {
            const auto key = next_test_request_key();
            const int task = n * 3;
            const bool forked = n % 2 == 0;
            emit(stage::task_allocated, task, -1, 0, -1, key);
            if (forked) {
                emit(stage::task_allocated, task + 1, -1, 0, task, key);
            }
            // Close the WHOLE request-local allocation set BEFORE
            // queueing or assigning any simulated native slot.
            emit(stage::allocation_set_closed, -1, -1, 0, -1, key,
                 forked ? 2U : 1U);
            std::this_thread::yield();
            emit(stage::slot_assigned, task, n, 0, -1, key);
            if (forked) {
                emit(stage::slot_assigned, task + 1, n + 200, 0, task, key);
            }
        });
    }
    for (auto & thread : threads) thread.join();
    register_test_observer(nullptr);
    const int total = count.load(std::memory_order_acquire);
    if (total != (N * 3 + N)) return 1; // 12 extra child allocation/slot pairs
    std::unordered_map<std::uint64_t, std::vector<event>> groups;
    for (int i = 0; i < total; ++i) {
        const auto & item = observations[static_cast<size_t>(i)];
        if (item.request_key == 0) return 2;
        groups[item.request_key].push_back(item);
    }
    if (groups.size() != N) return 3;
    std::unordered_set<int> all_native_ids;
    int closed_groups = 0;
    for (const auto & [key, events] : groups) {
        if (key == 0 || (events.size() != 3 && events.size() != 5)) return 4;
        int allocated = 0;
        int observed_slots = 0;
        bool closed = false;
        std::unordered_set<int> local_ids;
        for (const auto & item : events) {
            if (item.request_key != key) return 5;
            switch (item.stage_value) {
                case stage::task_allocated:
                    if (closed || item.slot_id != -1 ||
                            !local_ids.insert(item.task_id).second ||
                            !all_native_ids.insert(item.task_id).second) {
                        return 6;
                    }
                    ++allocated;
                    break;
                case stage::allocation_set_closed:
                    if (closed || item.allocated_task_count !=
                            static_cast<unsigned>(allocated)) return 7;
                    closed = true;
                    ++closed_groups;
                    break;
                case stage::slot_assigned:
                    if (!closed || !local_ids.count(item.task_id)) return 8;
                    ++observed_slots;
                    break;
                default: return 9;
            }
        }
        if (!closed || observed_slots != allocated) return 10;
    }
    return closed_groups == N ? 0 : 11;
#else
    emit(stage::task_allocated, 17, -1, 0);
    // Default binary has no test-key minting or observer registration.
    return 0;
#endif
}
