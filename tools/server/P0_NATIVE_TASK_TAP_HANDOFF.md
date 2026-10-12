# P0 #37 / #61 — Native inference-task observability seam

**Research date:** October 11, 2026 (EDT)  
**Status:** Draft, test-only instrumentation, NOT deployed.  
**Target:** `scottjoyner/llama.cpp`, fork of the Prism native server.  
**Upstream evidence:** [Auto-router physical witness issue #61](https://github.com/scottjoyner/auto-router/issues/61), [signed observer verifier draft #60](https://github.com/scottjoyner/auto-router/pull/60), [read-only physical preflight draft #62](https://github.com/scottjoyner/auto-router/pull/62), [root ownership #37](https://github.com/scottjoyner/auto-router/issues/37).

## What is actually implemented

`tools/server/server-p0-native-task-tap.h` adds a tiny metadata-only
observer callback seam. Without the explicitly test-only
`LLAMA_SERVER_P0_NATIVE_TASK_TEST_TAP` compile definition, all hook
calls compile to no-ops and **no registration interface exists**. The
repository does not enable that definition for the normal server build.

Three hook locations in `tools/server/server-context.cpp` are the
actual native inference-task path (not HTTP response heuristics):

1. **`slot_assigned`**: `launch_slot_with_task` returns a
   successfully scheduled slot, after moving the native task into it.
2. **`sampled_token`**: `process_token` has accepted a sampled token
   into the slot's native token queue. This occurs even if a stream
   does not emit its textual fragment to the HTTP client.
3. **`final_result_enqueued`**: `send_final_response` has enqueued
   the native final-result object. This does **NOT** prove client
   delivery or acknowledgment, and errors/cancellations can take
   other paths that are not yet instrumented.

Only `{stage, native task_id, slot_id, generated-token count}` is
available to the fixture. **No prompt, token text, user subject, tenant,
API key, GGUF path, raw HTTP request, root/attempt signature or user
trace is read or emitted**. The callback is `noexcept`; it must never
perform blocking/expensive work on the inference thread.

A standalone fixture-only callback can be registered with the macro
in a unit test. No production callback, collector, socket, persistent
queue, network service, signing key, observer credential or runtime
configuration is provided. No hooks are installed on xwing.

## Source provenance and important limitation

Read-only strict-SSH inspection of the running xwing
`llama-server` at PID `931526` showed its *working tree* is
`PrismML-Eng/llama.cpp` on branch `prism`, source HEAD
`2459f68b5c0eb26261fd5a81682004b93cd645ba`.
Selected source Git blob hashes are identical to the fork's master
(`server-task.h`, `server.cpp`, `server-http.cpp`). **However,
`server-context.cpp` differs between that checkout and this fork's
master**, and **no reproducible binary-build/provenance attestation
ties the running executable to either tree**. The patch targets the
accessible fork's current source; do not assert these new hooks are
currently present in or tested inside xwing's live binary.

That running process was independently observed (no inference):
boot UUID `92ba916a-6301-4a80-b234-f3a9f995a54e`,
PID `931526`, start ticks `8211850`,
TCP 18270 socket inode `71286212`, GGUF mmap file
`Ternary-Bonsai-2-27B-PTQ1_0.gguf` inode `79954950`.
This is only process/socket/file mapping, NOT native per-attempt proof.

## Explicit P0 blockers before an actual provider witness

A **native task ID** is a local, recyclable integer. It is *not*
the router's authenticated `root_request_id` or `attempt_id`, and
this patch does not attempt to mint/verify those fields from unsigned
HTTP headers.

Before actual A/B user proof, a separate, reviewed implementation must:

- securely bind authenticated gateway/router attempt to **one native
  server task ID** across the protected inbound HTTP boundary; no
  unsanitized client-provided header may establish ownership;
- independently capture node boot/PID/start ticks, exact model GGUF
  artifact identity and native model residency, with isolated
  observer signing-key custody and anti-replay/fencing semantics;
- emit bounded, asynchronous, redacted native slot/first-token/
  terminal/error/cancellation events without stalling inference.
  Current `sampled_token` is not a guaranteed first-token event and
  does not prove network emission; `final_result_enqueued` is not
  necessarily delivered;
- separate trusted native process-source provenance from an actor's
  signed delegation and from encrypted, tenant/subject/producer/node
  trace custody; verify readback ACLs and client delivery separately;
- obtain actual independent two-human IdP sessions, operator-approved
  host pressure, real model residency and a distinct physical-device
  acceptance. **Beelink remains NO-GO.** xwing's protected model
  endpoint returned 401 to unauthenticated inventory; no credentials
  were copied or bypassed.

There is **no release promotion switch** and no automatic model dispatch.

## Local isolated test

No inference model, network listener or GGUF file is required:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -pedantic \
    tools/server/tests/unit/p0_native_task_tap_smoke.cpp \
    -o /tmp/p0-native-tap-default
/tmp/p0-native-tap-default

g++ -std=c++17 -Wall -Wextra -Werror -pedantic \
    -DLLAMA_SERVER_P0_NATIVE_TASK_TEST_TAP \
    tools/server/tests/unit/p0_native_task_tap_smoke.cpp \
    -o /tmp/p0-native-tap-fixture
/tmp/p0-native-tap-fixture
```

Both standalone C++17 tests passed on x1-370 using GNU g++ 13.3.
**The complete llama-server binary was NOT rebuilt**, and the xwing
running process was NOT restarted, patched, inspected for credentials,
or sent any inference traffic. Full CMake build and native callback
performance review remain separate acceptance gates.

**Never treat a passing native fixture callback as process execution
attestation or human-scoped receipt acceptance.**
