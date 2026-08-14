#pragma once
/**
 * @file stub_backend.hpp
 * @brief Stub guidance backend for testing
 *
 * Returns immediate, configurable responses without any external
 * communication. Used for unit tests and development.
 */

#include <opencog/endocrine/guidance_types.hpp>

#include <future>

namespace opencog::endo {

/**
 * @brief Testing backend that returns preset responses immediately
 *
 * By default returns NO_ACTION. Can be configured with a preset
 * response or a custom callback for dynamic test scenarios.
 */
class StubGuidanceBackend : public GuidanceBackend {
public:
    StubGuidanceBackend() = default;

    /// Set a preset response to return for all requests
    void set_response(const GuidanceResponse& resp) {
        preset_response_ = resp;
        preset_response_.valid = true;
    }

    /// Set a callback for dynamic responses
    void set_callback(std::function<GuidanceResponse(const GuidanceRequest&)> cb) {
        callback_ = std::move(cb);
    }

    std::future<GuidanceResponse> request(const GuidanceRequest& req) override {
        ++request_count_;
        last_request_ = req;

        std::promise<GuidanceResponse> promise;

        if (callback_) {
            promise.set_value(callback_(req));
        } else {
            promise.set_value(preset_response_);
        }

        return promise.get_future();
    }

    [[nodiscard]] bool is_available() const noexcept override {
        return available_;
    }

    [[nodiscard]] std::string_view name() const noexcept override {
        return "StubGuidanceBackend";
    }

    // === Test helpers ===

    void set_available(bool avail) noexcept { available_ = avail; }
    [[nodiscard]] size_t request_count() const noexcept { return request_count_; }
    [[nodiscard]] const GuidanceRequest& last_request() const noexcept { return last_request_; }

private:
    GuidanceResponse preset_response_{};
    std::function<GuidanceResponse(const GuidanceRequest&)> callback_;
    bool available_{true};
    size_t request_count_{0};
    GuidanceRequest last_request_;
};

} // namespace opencog::endo
