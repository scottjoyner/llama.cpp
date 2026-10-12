// P0 #65 — real cpp-httplib parser, loopback-only, NO model.
// This tests native pre-routing behavior without starting llama-server.
//
// The production Prism source's P0 pre-routing guard is only compiled
// with LLAMA_SERVER_P0_NATIVE_TASK_TEST_TAP and always DENIES reserved
// P0 headers until an independently reviewed signature verifier exists.
// This dummy test route intentionally never authenticates a human.
#include "../../server-p0-raw-header-shape.h"

#include <cpp-httplib/httplib.h>

#include <string>
#include <thread>
#include <utility>
#include <vector>

using llama_server_p0_raw_header_shape::fixture_admission;

int main() {
    httplib::Server server;
    server.set_pre_routing_handler([](const httplib::Request & req,
                                      httplib::Response & res) {
        if (req.path != "/v1/chat/completions") {
            return httplib::Server::HandlerResponse::Unhandled;
        }
        bool reserved = false;
        for (const auto & [name, value] : req.headers) {
            (void) value;
            if (llama_server_p0_raw_header_shape::is_reserved_p0_header_name(name)) {
                reserved = true;
                break;
            }
        }
        if (!reserved) return httplib::Server::HandlerResponse::Unhandled;
        if (req.headers.size() > 64) {
            res.status = 400;
            res.set_content("invalid P0 fixture header", "text/plain");
            return httplib::Server::HandlerResponse::Handled;
        }
        std::vector<std::pair<std::string, std::string>> raw;
        raw.reserve(req.headers.size());
        for (const auto & h : req.headers) raw.emplace_back(h.first, h.second);
        const auto decision =
            llama_server_p0_raw_header_shape::evaluate_unimplemented_p0_admission(raw);
        res.status = decision == fixture_admission::rejected_invalid_raw_header_frame
            ? 400 : 403;
        res.set_content("P0 native verifier not implemented", "text/plain");
        return httplib::Server::HandlerResponse::Handled;
    });
    // Dummy handlers have NO model, user, API key or trace store.
    server.Post("/v1/chat/completions", [](const httplib::Request &,
                                          httplib::Response & res) {
        res.status = 200;
        res.set_content("dummy_fixture_not_inference", "text/plain");
    });
    server.Get("/health", [](const httplib::Request &, httplib::Response & res) {
        res.status = 200;
        res.set_content("fixture_alive", "text/plain");
    });
    const int port = server.bind_to_any_port("127.0.0.1");
    if (port <= 0) return 1;
    std::thread thread([&server] { (void) server.listen_after_bind(); });
    httplib::Client client("127.0.0.1", port);
    client.set_connection_timeout(2);
    client.set_read_timeout(2);

    int failure = 0;
    auto send = [&client](const httplib::Headers & headers, int expected,
                          bool expect_dummy) -> bool {
        auto result = client.Post(
            "/v1/chat/completions", headers, "{}", "application/json");
        if (!result || result->status != expected) return false;
        if (expect_dummy && result->body != "dummy_fixture_not_inference") return false;
        if (!expect_dummy && result->body == "dummy_fixture_not_inference") return false;
        return true;
    };

    const auto health = client.Get("/health");
    if (!health || health->status != 200 || health->body != "fixture_alive") {
        failure = 2;
    }
    if (!failure && !send({}, 200, true)) failure = 3;
    if (!failure && !send({
            {"Authorization", "test_service_label_only"},
        }, 200, true)) failure = 4;
    if (!failure && !send({
            {"X-P0-Native-Admission", "signed_looking_but_unverified"},
        }, 403, false)) failure = 5;
    if (!failure && !send({
            {"X-P0-Signature", "A"}, {"x-p0-signature", "B"},
        }, 400, false)) failure = 6;
    if (!failure && !send({
            {"Authorization", "service_bearer"},
            {"X-P0-Native-Admission", "untrusted"},
        }, 403, false)) failure = 7;
    if (!failure && !send({
            {"X-P0-Original-Request-SHA256", std::string(64, 'a')},
        }, 403, false)) failure = 8;

    // Ensure the fixture listener leaves no socket behind.
    server.stop();
    thread.join();
    return failure;
}
