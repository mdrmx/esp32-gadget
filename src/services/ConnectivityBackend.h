#pragma once

#include <stdint.h>

// Transport-neutral contract for a device network interface. Implementations
// own platform-specific configuration and status reads; the manager owns state.
enum class NetworkType : uint8_t {
  None = 0,
  WiFi,
  Cellular
};

class ConnectivityBackend {
public:
  virtual ~ConnectivityBackend() {}

  virtual NetworkType networkType() const = 0;
  virtual bool begin() = 0;
  virtual void update() = 0;
  virtual bool isConnected() const = 0;
  virtual void disconnect() = 0;
  virtual int32_t signalStrengthDbm() const = 0;
};
