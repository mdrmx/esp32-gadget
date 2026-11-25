#pragma once

#include "ConnectivityBackend.h"
#include <stdint.h>

// Coordinates one active connectivity backend. connect() starts an attempt
// and returns immediately; update() advances association and recovery without
// waiting or delaying. Backend-specific profile/credential handling stays in
// the backend and platform composition root.
class ConnectivityManager {
public:
  enum class State : uint8_t {
    Disconnected = 0,
    Connecting,
    RetryWaiting,
    Connected,
    Failed,
    TimedOut
  };

  static const uint32_t DefaultConnectionTimeoutMs = 20000UL;
  static const uint8_t DefaultMaxRetryAttempts = 2;
  static const uint32_t DefaultRetryInitialBackoffMs = 2000UL;
  static const uint32_t DefaultRetryMaxBackoffMs = 8000UL;

  explicit ConnectivityManager(
    ConnectivityBackend& backend,
    uint32_t connectionTimeoutMs = DefaultConnectionTimeoutMs,
    uint8_t maxRetryAttempts = DefaultMaxRetryAttempts,
    uint32_t retryInitialBackoffMs = DefaultRetryInitialBackoffMs,
    uint32_t retryMaxBackoffMs = DefaultRetryMaxBackoffMs
  )
      : backend_(backend),
        connectionTimeoutMs_(connectionTimeoutMs),
        maxRetryAttempts_(maxRetryAttempts),
        retryInitialBackoffMs_(retryInitialBackoffMs),
        retryMaxBackoffMs_(retryMaxBackoffMs),
        connectionStartedAtMs_(0),
        retryStartedAtMs_(0),
        nextRetryDelayMs_(0),
        retryAttempts_(0),
        retryTerminalState_(State::Failed),
        state_(State::Disconnected) {}

  ConnectivityManager(const ConnectivityManager&) = delete;
  ConnectivityManager& operator=(const ConnectivityManager&) = delete;
  ConnectivityManager(ConnectivityManager&&) = delete;
  ConnectivityManager& operator=(ConnectivityManager&&) = delete;

  // Starts a connection/recovery sequence. A successful return means the
  // sequence was accepted; the first backend begin may fail immediately and
  // still be followed by a scheduled retry.
  bool connect(uint32_t nowMs) {
    if (isConnecting()) {
      return false;
    }

    if (backend_.isConnected()) {
      markConnected();
      return true;
    }

    retryAttempts_ = 0;
    nextRetryDelayMs_ = 0;
    retryTerminalState_ = State::Failed;
    return startAttempt(nowMs);
  }

  // Call regularly from the device loop. All elapsed-time checks use unsigned
  // subtraction so connection timeouts and retry deadlines survive millis()
  // rollover.
  void update(uint32_t nowMs) {
    backend_.update();

    if (state_ == State::Connecting) {
      if (backend_.isConnected()) {
        markConnected();
        return;
      }

      if (elapsed(nowMs, connectionStartedAtMs_) >=
          connectionTimeoutMs_) {
        backend_.disconnect();
        scheduleRetry(nowMs, State::TimedOut);
      }
      return;
    }

    if (state_ == State::RetryWaiting) {
      // Accept a driver-managed reconnect while the manager's backoff timer
      // is running. Do not issue a redundant begin() if the link recovered.
      if (backend_.isConnected()) {
        markConnected();
        return;
      }

      if (elapsed(nowMs, retryStartedAtMs_) >= nextRetryDelayMs_) {
        startAttempt(nowMs);
      }
      return;
    }

    if (backend_.isConnected()) {
      markConnected();
      return;
    }

    if (state_ == State::Connected) {
      // Link loss starts a new bounded recovery sequence for the same
      // configured profile. A successful recovery resets the retry budget.
      retryAttempts_ = 0;
      scheduleRetry(nowMs, State::Disconnected);
    }
  }

  // Explicit user/platform disconnect cancels any pending automatic retries.
  void disconnect() {
    backend_.disconnect();
    retryAttempts_ = 0;
    nextRetryDelayMs_ = 0;
    state_ = State::Disconnected;
  }

  State state() const {
    return state_;
  }

  // RetryWaiting is pending connectivity work too. Treat it as connecting so
  // WeatherApp and the Wi-Fi settings flow continue to wait without blocking.
  bool isConnecting() const {
    return state_ == State::Connecting || state_ == State::RetryWaiting;
  }

  bool isRetryPending() const {
    return state_ == State::RetryWaiting;
  }

  uint8_t retryAttempts() const {
    return retryAttempts_;
  }

  uint32_t nextRetryDelayMs() const {
    return nextRetryDelayMs_;
  }

  bool isConnected() const {
    return state_ == State::Connected && backend_.isConnected();
  }

  NetworkType activeNetworkType() const {
    return (isConnecting() || isConnected())
      ? backend_.networkType()
      : NetworkType::None;
  }

  bool hasSignalStrength() const {
    return isConnected();
  }

  int32_t signalStrengthDbm() const {
    return hasSignalStrength() ? backend_.signalStrengthDbm() : 0;
  }

private:
  static uint32_t elapsed(uint32_t nowMs, uint32_t startMs) {
    return static_cast<uint32_t>(nowMs - startMs);
  }

  void markConnected() {
    state_ = State::Connected;
    retryAttempts_ = 0;
    retryStartedAtMs_ = 0;
    nextRetryDelayMs_ = 0;
    retryTerminalState_ = State::Failed;
  }

  bool startAttempt(uint32_t nowMs) {
    connectionStartedAtMs_ = nowMs;
    nextRetryDelayMs_ = 0;
    state_ = State::Connecting;

    if (!backend_.begin()) {
      scheduleRetry(nowMs, State::Failed);
      return isConnecting() || isConnected();
    }

    if (backend_.isConnected()) {
      markConnected();
    }
    return true;
  }

  bool scheduleRetry(uint32_t nowMs, State terminalState) {
    retryTerminalState_ = terminalState;
    if (retryAttempts_ >= maxRetryAttempts_) {
      state_ = retryTerminalState_;
      nextRetryDelayMs_ = 0;
      return false;
    }

    uint32_t delayMs = retryInitialBackoffMs_;
    for (uint8_t i = 0; i < retryAttempts_; ++i) {
      if (delayMs >= retryMaxBackoffMs_ ||
          delayMs > retryMaxBackoffMs_ - delayMs) {
        delayMs = retryMaxBackoffMs_;
        break;
      }
      delayMs *= 2U;
    }
    if (delayMs > retryMaxBackoffMs_) {
      delayMs = retryMaxBackoffMs_;
    }

    ++retryAttempts_;
    retryStartedAtMs_ = nowMs;
    nextRetryDelayMs_ = delayMs;
    state_ = State::RetryWaiting;
    return true;
  }

  ConnectivityBackend& backend_;
  uint32_t connectionTimeoutMs_;
  uint8_t maxRetryAttempts_;
  uint32_t retryInitialBackoffMs_;
  uint32_t retryMaxBackoffMs_;
  uint32_t connectionStartedAtMs_;
  uint32_t retryStartedAtMs_;
  uint32_t nextRetryDelayMs_;
  uint8_t retryAttempts_;
  State retryTerminalState_;
  State state_;
};
