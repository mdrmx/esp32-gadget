#pragma once

#include <stddef.h>
#include <stdint.h>

class Preferences {
 public:
  Preferences() : active_(false), readOnly_(false) {}

  bool begin(const char* name, bool readOnly = false);
  size_t getBytesLength(const char* key);
  size_t getBytes(const char* key, void* destination, size_t length);
  size_t putBytes(const char* key, const void* source, size_t length);
  void end();

  static void reset();

  static uint8_t storedBytes[1024];
  static size_t storedLength;
  static bool failBegin;
  static bool failRead;
  static bool failWrite;
  static bool lastBeginReadOnly;
  static unsigned int beginCalls;
  static unsigned int endCalls;
  static char lastNamespace[16];
  static char lastKey[16];

 private:
  bool active_;
  bool readOnly_;
};
