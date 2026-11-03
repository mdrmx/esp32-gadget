#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <stdint.h>

// Owns one synchronous HTTPS request session. Domain services retain endpoint
// construction, status interpretation and response parsing; this class owns
// the secure socket, HTTPClient lifetime, timeouts and response access.
//
// Each instance is intentionally single-use. The caller must explicitly opt
// into the existing insecure TLS mode so that this compatibility behaviour is
// visible at each call site rather than becoming an implicit platform default.
class HttpRequestClient {
public:
  enum class TlsPolicy : uint8_t {
    InsecureCompatibility,
    ConfiguredTrustAnchor
  };

  HttpRequestClient() = default;

  ~HttpRequestClient() {
    end();
  }

  HttpRequestClient(const HttpRequestClient&) = delete;
  HttpRequestClient& operator=(const HttpRequestClient&) = delete;
  HttpRequestClient(HttpRequestClient&&) = delete;
  HttpRequestClient& operator=(HttpRequestClient&&) = delete;

  // Configure certificate validation before begin() when the caller has
  // a trusted root CA. This is intentionally separate from the legacy
  // insecure-TLS compatibility policy below.
  bool setCACert(const char* rootCaCertificate) {
    if (beginAttempted_ || rootCaCertificate == nullptr ||
        rootCaCertificate[0] == '\0') {
      return false;
    }

    secureClient_.setCACert(rootCaCertificate);
    trustAnchorConfigured_ = true;
    return true;
  }

  bool begin(
    const String& url,
    int32_t connectTimeoutMs,
    uint16_t responseTimeoutMs,
    TlsPolicy tlsPolicy
  ) {
    if (beginAttempted_ || url.length() == 0 ||
        (tlsPolicy == TlsPolicy::ConfiguredTrustAnchor &&
         !trustAnchorConfigured_)) {
      return false;
    }

    beginAttempted_ = true;
    if (tlsPolicy == TlsPolicy::InsecureCompatibility) {
      secureClient_.setInsecure();
    }

    // Mark the session active before begin(): HTTPClient may retain the client
    // reference even when URL initialisation fails, and end() is safe there.
    active_ = true;
    if (!http_.begin(secureClient_, url)) {
      end();
      return false;
    }

    http_.setConnectTimeout(connectTimeoutMs);
    http_.setTimeout(responseTimeoutMs);
    return true;
  }

  void addHeader(const String& name, const String& value) {
    http_.addHeader(name, value);
  }

  int get() {
    return http_.GET();
  }

  int post(const String& payload) {
    return http_.POST(payload);
  }

  int getSize() {
    return http_.getSize();
  }

  bool connected() {
    return http_.connected();
  }

  // The response stream is owned by HTTPClient and remains valid only until
  // end() or destruction of this request session.
  NetworkClient& getStream() {
    return http_.getStream();
  }

  NetworkClient* getStreamPtr() {
    return http_.getStreamPtr();
  }

  String getString() {
    return http_.getString();
  }

  void end() {
    if (!active_) {
      return;
    }

    http_.end();
    active_ = false;
  }

private:
  // Declare the socket before HTTPClient so it outlives HTTPClient destruction.
  WiFiClientSecure secureClient_;
  HTTPClient http_;
  bool beginAttempted_ = false;
  bool active_ = false;
  bool trustAnchorConfigured_ = false;
};
