#pragma once

#include "ConnectivityBackend.h"
#include <stdint.h>

// Coordinates one active connectivity backend. connect() starts an attempt
// and returns immediately; update() advances it without waiting or delaying.
// Backend-specific profile/credential handling stays inside the backend.
class ConnectivityManager {
public:
  enum class State : uint8_t {
    Disconnected = 0,
    Connecting,
    Connected,
    Failed,
    TimedOut
  };

  static const uint32_t DefaultConnectionTimeoutMs = 20000UL;

  explicit ConnectivityManager(
    ConnectivityBackend& backend,
    uint32_t connectionTimeoutMs = DefaultConnectionTimeoutMs
  )
      : backend_(backend),
        connectionTimeoutMs_(connectionTimeoutMs),
        connectionStartedAtMs_(0),
        state_(State::Disconnected) {}

  ConnectivityManager(const ConnectivityManager&) = delete;
  ConnectivityManager& operator=(const ConnectivityManager&) = delete;
  ConnectivityManager(ConnectivityManager&&) = delete;
  ConnectivityManager& operator=(ConnectivityManager&&) = delete;

  // Starts a connection attempt. The call never polls or waits for association.
  bool connect(uint32_t nowMs) {
    if (state_ == State::Connecting) {
      return false;
    }

    if (backend_.isConnected()) {
      state_ = State::Connected;
      return true;
    }

    if (!backend_.begin()) {
      state_ = State::Failed;
      return false;
    }

    connectionStartedAtMs_ = nowMs;
    state_ = backend_.isConnected() ? State::Connected : State::Connecting;
    return true;
  }

  // Call regularly from the device loop. Uses unsigned elapsed-time arithmetic
  // so the timeout remains correct across millis() rollover.
  void update(uint32_t nowMs) {
    backend_.update();

    if (state_ == State::Connecting) {
      if (backend_.isConnected()) {
        state_ = State::Connected;
        return;
      }

      if ((uint32_t)(nowMs - connectionStartedAtMs_) >=
          connectionTimeoutMs_) {
        backend_.disconnect();
        state_ = State::TimedOut;
      }
      return;
    }

    if (state_ == State::Connected && !backend_.isConnected()) {
      state_ = State::Disconnected;
    }
  }

  void disconnect() {
    backend_.disconnect();
    state_ = State::Disconnected;
  }

  State state() const {
    return state_;
  }

  bool isConnecting() const {
    return state_ == State::Connecting;
  }

  bool isConnected() const {
    return state_ == State::Connected && backend_.isConnected();
  }

  NetworkType activeNetworkType() const {
    return (state_ == State::Connecting || isConnected())
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
  ConnectivityBackend& backend_;
  uint32_t connectionTimeoutMs_;
  uint32_t connectionStartedAtMs_;
  State state_;
};
