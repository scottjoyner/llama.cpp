// P0 #65: read-only HTTP header shape guard for a future native verifier.
//
// NOT INSTALLED in server-http.cpp. This fixture accepts ORIGINAL raw
// header pairs, including duplicates, BEFORE server_http_req headers are
// narrowed to std::map (last-value-wins). Header uniqueness DOES NOT
// authenticate a human, service, signature or model process.
//
// No process secrets, header values or decoded token contents are returned.
#pragma once

#include <cstddef>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace llama_server_p0_raw_header_shape {

enum class status {
    unique_but_unauthenticated,
    rejected,
};

struct result {
    status state;
    bool authenticated_forwarder;
    bool authenticated_human;
    bool physical_execution_proven;
};

inline bool token_char(unsigned char c) noexcept {
    if (c >= 'A' && c <= 'Z') return true;
    if (c >= 'a' && c <= 'z') return true;
    if (c >= '0' && c <= '9') return true;
    switch (c) {
        case '!': case '#': case '$': case '%': case '&': case '\'':
        case '*': case '+': case '-': case '.': case '^': case '_':
        case 96: case '|': case '~':
            return true;
        default:
            return false;
    }
}

inline bool protected_name(const std::string & lower) noexcept {
    return lower == "authorization"
        || lower == "x-api-key"
        || lower == "x-p0-native-admission"
        || lower == "x-p0-signature"
        || lower == "x-p0-original-request-sha256"
        || lower.compare(0, 5, "x-p0-") == 0;
}

inline result inspect_original_pairs(
        const std::vector<std::pair<std::string, std::string>> & raw) {
    constexpr result denied {
        status::rejected, false, false, false,
    };
    constexpr result unique {
        status::unique_but_unauthenticated, false, false, false,
    };
    if (raw.size() > 64) return denied;
    size_t cumulative_bytes = 0;
    std::set<std::string> seen;
    bool authorization_seen = false;
    bool api_key_seen = false;
    for (const auto & item : raw) {
        const std::string & name = item.first;
        const std::string & value = item.second;
        cumulative_bytes += name.size() + value.size();
        if (name.empty() || name.size() > 128 || value.size() > 4096 ||
                cumulative_bytes > 8192) return denied;
        std::string folded;
        folded.reserve(name.size());
        for (unsigned char c : name) {
            if (!token_char(c)) return denied;
            folded.push_back(
                (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A'))
                                      : static_cast<char>(c));
        }
        for (unsigned char c : value) {
            if (c < 0x20 || c == 0x7f) return denied;
        }
        if (!protected_name(folded)) continue;
        if (!seen.insert(folded).second) return denied;
        authorization_seen |= folded == "authorization";
        api_key_seen |= folded == "x-api-key";
        if (authorization_seen && api_key_seen) return denied;
    }
    return unique;
}

// A future P0 signature verifier MUST replace this deny-only fixture
// after proving operational trusted ingress identity. Until then, no
// reserved X-P0-* header grants any native-human or trace authority.
enum class fixture_admission {
    no_reserved_p0_header,
    rejected_invalid_raw_header_frame,
    rejected_unimplemented_signature_verification,
};

inline bool is_reserved_p0_header_name(const std::string & name) noexcept {
    if (name.size() < 5) return false;
    constexpr char prefix[] = "x-p0-";
    for (size_t i = 0; i < 5; ++i) {
        unsigned char c = static_cast<unsigned char>(name[i]);
        if (c >= 'A' && c <= 'Z') c = static_cast<unsigned char>(c + ('a' - 'A'));
        if (c != static_cast<unsigned char>(prefix[i])) return false;
    }
    return true;
}

inline fixture_admission evaluate_unimplemented_p0_admission(
        const std::vector<std::pair<std::string, std::string>> & raw) {
    bool reserved = false;
    for (const auto & [name, value] : raw) {
        (void) value;
        if (is_reserved_p0_header_name(name)) {
            reserved = true;
            break;
        }
    }
    if (!reserved) return fixture_admission::no_reserved_p0_header;
    if (inspect_original_pairs(raw).state == status::rejected) {
        return fixture_admission::rejected_invalid_raw_header_frame;
    }
    return fixture_admission::rejected_unimplemented_signature_verification;
}

} // namespace llama_server_p0_raw_header_shape
