#include "src/services/PreferencesWiFiProfileStorage.h"
#include "src/services/WiFiProfileStore.h"
#include <stdio.h>
#include <string.h>

uint8_t Preferences::storedBytes[1024] = {};
size_t Preferences::storedLength = 0;
bool Preferences::failBegin = false;
bool Preferences::failRead = false;
bool Preferences::failWrite = false;
unsigned int Preferences::beginCalls = 0;
unsigned int Preferences::endCalls = 0;
char Preferences::lastNamespace[16] = {};
char Preferences::lastKey[16] = {};

bool Preferences::begin(const char* name, bool readOnly) {
  if (failBegin || name == nullptr || strlen(name) >= sizeof(lastNamespace)) {
    return false;
  }
  active_ = true;
  readOnly_ = readOnly;
  memset(lastNamespace, 0, sizeof(lastNamespace));
  memcpy(lastNamespace, name, strlen(name));
  ++beginCalls;
  return true;
}

size_t Preferences::getBytesLength(const char* key) {
  if (!active_ || key == nullptr || strlen(key) >= sizeof(lastKey)) {
    return 0;
  }
  memset(lastKey, 0, sizeof(lastKey));
  memcpy(lastKey, key, strlen(key));
  return storedLength;
}

size_t Preferences::getBytes(
    const char* key,
    void* destination,
    size_t length) {
  if (!active_ || failRead || key == nullptr || destination == nullptr ||
      length > storedLength) {
    return 0;
  }
  memset(lastKey, 0, sizeof(lastKey));
  memcpy(lastKey, key, strlen(key));
  memcpy(destination, storedBytes, length);
  return length;
}

size_t Preferences::putBytes(
    const char* key,
    const void* source,
    size_t length) {
  if (!active_ || readOnly_ || failWrite || key == nullptr ||
      source == nullptr || length > sizeof(storedBytes)) {
    return 0;
  }
  memset(lastKey, 0, sizeof(lastKey));
  memcpy(lastKey, key, strlen(key));
  memcpy(storedBytes, source, length);
  storedLength = length;
  return length;
}

void Preferences::end() {
  if (active_) {
    active_ = false;
    ++endCalls;
  }
}

void Preferences::reset() {
  memset(storedBytes, 0, sizeof(storedBytes));
  storedLength = 0;
  failBegin = false;
  failRead = false;
  failWrite = false;
  beginCalls = 0;
  endCalls = 0;
  memset(lastNamespace, 0, sizeof(lastNamespace));
  memset(lastKey, 0, sizeof(lastKey));
}

namespace {
int failures = 0;
#define CHECK(condition) do { \
  if (!(condition)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    ++failures; \
  } \
} while (0)

void readsMissingAndOversizedRecordsSafely() {
  Preferences::reset();
  PreferencesWiFiProfileStorage storage;
  uint8_t buffer[8] = {};
  size_t length = 99;

  CHECK(storage.read(buffer, sizeof(buffer), length));
  CHECK(length == 0);
  CHECK(Preferences::endCalls == 1);
  CHECK(strcmp(Preferences::lastNamespace, "wifi_profiles") == 0);
  CHECK(strcmp(Preferences::lastKey, "profiles") == 0);

  const uint8_t largeRecord[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  CHECK(storage.write(largeRecord, sizeof(largeRecord)));
  memset(buffer, 0, sizeof(buffer));
  CHECK(storage.read(buffer, sizeof(buffer), length));
  CHECK(length == sizeof(largeRecord));
  CHECK(buffer[0] == 0);
}

void checksBeginReadWriteAndEndFailures() {
  Preferences::reset();
  PreferencesWiFiProfileStorage storage;
  const uint8_t record[] = {9, 8, 7, 6};

  Preferences::failBegin = true;
  size_t length = 0;
  uint8_t buffer[8] = {};
  CHECK(!storage.read(buffer, sizeof(buffer), length));
  CHECK(!storage.write(record, sizeof(record)));
  CHECK(Preferences::endCalls == 0);

  Preferences::failBegin = false;
  Preferences::failWrite = true;
  CHECK(!storage.write(record, sizeof(record)));
  CHECK(Preferences::endCalls == 1);
  CHECK(Preferences::storedLength == 0);

  Preferences::failWrite = false;
  CHECK(storage.write(record, sizeof(record)));
  CHECK(Preferences::storedLength == sizeof(record));
  CHECK(Preferences::endCalls == 2);

  Preferences::failRead = true;
  CHECK(!storage.read(buffer, sizeof(buffer), length));
  CHECK(Preferences::endCalls == 3);
}

void profileStoreUsesPreferencesForVersionedRecords() {
  Preferences::reset();
  PreferencesWiFiProfileStorage backend;
  typedef WiFiProfileStore<PreferencesWiFiProfileStorage> Store;
  Store store(backend);

  CHECK(store.begin() == Store::Status::NotFound);
  CHECK(store.save("nvs-network", "nvs-password-123", true) ==
        Store::Status::Success);

  Store reloaded(backend);
  CHECK(reloaded.begin() == Store::Status::Success);
  Store::Profile preferred = {};
  CHECK(reloaded.getPreferred(preferred));
  CHECK(strcmp(preferred.ssid, "nvs-network") == 0);
  CHECK(strcmp(preferred.password, "nvs-password-123") == 0);
}
}

int main() {
  readsMissingAndOversizedRecordsSafely();
  checksBeginReadWriteAndEndFailures();
  profileStoreUsesPreferencesForVersionedRecords();

  if (failures != 0) {
    fprintf(stderr, "%d Preferences storage test(s) failed.\n", failures);
    return 1;
  }

  printf("PreferencesWiFiProfileStorage tests passed.\n");
  return 0;
}
