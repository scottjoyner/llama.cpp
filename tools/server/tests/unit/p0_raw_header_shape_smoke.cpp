// Standalone raw-header test; no model, listener, cryptography or sockets.
// Compile with -std=c++17 -Wall -Wextra -Werror -pedantic.
#include "../../server-p0-raw-header-shape.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

using namespace llama_server_p0_raw_header_shape;

using pair_t = std::pair<std::string, std::string>;
using pairs_t = std::vector<pair_t>;

static bool rejected(const pairs_t & raw) {
    const auto x = inspect_original_pairs(raw);
    return x.state == status::rejected
        && !x.authenticated_forwarder
        && !x.authenticated_human
        && !x.physical_execution_proven;
}

int main() {
    const pairs_t safe {
        {"Host", "test.internal"},
        {"Authorization", "dummy fixture-bearer private-value-must-never-log"},
        {"x-p0-native-admission", "opaque signed bytes not verified"},
        {"Content-Type", "application/json"},
    };
    const auto result = inspect_original_pairs(safe);
    if (result.state != status::unique_but_unauthenticated ||
            result.authenticated_forwarder ||
            result.authenticated_human ||
            result.physical_execution_proven) return 1;

    const pairs_t duplicate_header {
        {"Authorization", "AUTHORITY-A"},
        {"Authorization", "AUTHORITY-B"},
    };
    const pairs_t mixed_case {
        {"X-P0-Signature", "SIGNATURE-A"},
        {"x-p0-signature", "SIGNATURE-B"},
    };
    const pairs_t mixed_auth {
        {"Authorization", "shared-service-actor"},
        {"X-Api-Key", "other-service-actor"},
    };
    if (!rejected(duplicate_header) || !rejected(mixed_case) ||
            !rejected(mixed_auth)) return 2;

    // Reproduce precisely why the actual Prism get_headers() map-copy
    // cannot be used to check duplicated signed-admission headers.
    std::map<std::string, std::string> narrowed;
    for (const auto & h : duplicate_header) {
        narrowed[h.first] = h.second;
    }
    if (narrowed.size() != 1 || narrowed["Authorization"] != "AUTHORITY-B") return 3;
    if (!rejected(duplicate_header)) return 4;

    if (!rejected({{" x-p0-signature", "injected"}})) return 5;
    if (!rejected({{"x-p0-signature", "line1\r\nline2"}})) return 6;
    if (!rejected({{"x-p0-signature", std::string("a\0b", 3)}})) return 7;
    if (!rejected({{"x-p0-signature", "tab\tvalue"}})) return 11;
    if (!rejected({{"x-p0-signature", std::string("a\x7f" "b", 3)}})) return 12;
    if (!rejected({{"x-p0-signature", std::string(4097, 'x')}})) return 8;

    pairs_t too_many;
    for (int i = 0; i < 65; i++) {
        too_many.emplace_back("X-Unimportant", "safe");
    }
    if (!rejected(too_many)) return 9;

    const pairs_t unique_other_headers {
        {"X-Custom-Information", "nonsecret"},
        {"x-p0-native-admission", "not actually authenticated"},
    };
    if (inspect_original_pairs(unique_other_headers).state !=
            status::unique_but_unauthenticated) return 10;
    // Real pre-routing integration is compile-time TEST-ONLY and must
    // refuse even a well-framed X-P0-* claim until a trusted signer exists.
    if (evaluate_unimplemented_p0_admission(safe) !=
            fixture_admission::rejected_unimplemented_signature_verification) {
        return 13;
    }
    if (evaluate_unimplemented_p0_admission(duplicate_header) !=
            fixture_admission::no_reserved_p0_header) {
        return 14; // ordinary service header duplicate not a P0 signature
    }
    if (evaluate_unimplemented_p0_admission(mixed_case) !=
            fixture_admission::rejected_invalid_raw_header_frame) {
        return 15;
    }
    if (evaluate_unimplemented_p0_admission(unique_other_headers) !=
            fixture_admission::rejected_unimplemented_signature_verification) {
        return 16;
    }
    if (evaluate_unimplemented_p0_admission({
            {"Host", "test.internal"}, {"Content-Type", "application/json"},
        }) != fixture_admission::no_reserved_p0_header) {
        return 17;
    }
    if (evaluate_unimplemented_p0_admission({
            {"X-P0-Signature", "bad\r\nframing"},
        }) != fixture_admission::rejected_invalid_raw_header_frame) {
        return 18;
    }
    pairs_t many_reserved;
    many_reserved.emplace_back("X-P0-Signature", "fake");
    for (int i = 0; i < 65; i++) many_reserved.emplace_back("X-Normal", "v");
    if (evaluate_unimplemented_p0_admission(many_reserved) !=
            fixture_admission::rejected_invalid_raw_header_frame) {
        return 19;
    }
    return 0;
}
