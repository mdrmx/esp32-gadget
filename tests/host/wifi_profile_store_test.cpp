#include "src/services/WiFiProfileStore.h"
#include <stdio.h>
#include <string.h>

namespace {
class MemoryStorage {
 public:
  MemoryStorage()
      : length(0), failRead(false), failWrite(false) {
    memset(bytes, 0, sizeof(bytes));
  }

  bool read(uint8_t* destination, size_t capacity, size_t& outputLength) {
    outputLength = length;
    if (failRead || destination == nullptr) {
      return false;
    }
    if (length > capacity) {
      return true;
    }
    if (length > 0) {
      memcpy(destination, bytes, length);
    }
    return true;
  }

  bool write(const uint8_t* source, size_t inputLength) {
    if (failWrite || source == nullptr || inputLength > sizeof(bytes)) {
      return false;
    }
    memcpy(bytes, source, inputLength);
    length = inputLength;
    return true;
  }

  uint8_t bytes[1024];
  size_t length;
  bool failRead;
  bool failWrite;
};

typedef WiFiProfileStore<MemoryStorage> Store;

int failures = 0;
#define CHECK(condition) do { \
  if (!(condition)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    ++failures; \
  } \
} while (0)

void startsEmptyAndPersistsProfilesAcrossInstances() {
  MemoryStorage storage;
  Store store(storage);
  CHECK(store.begin() == Store::Status::NotFound);
  CHECK(store.isLoaded());
  CHECK(store.count() == 0);
  CHECK(!store.hasPreferred());

  CHECK(store.save("studio-wifi", "studio-pass-01", true) ==
        Store::Status::Success);
  CHECK(store.count() == 1);
  CHECK(store.hasPreferred());

  Store reloaded(storage);
  CHECK(reloaded.begin() == Store::Status::Success);
  CHECK(reloaded.count() == 1);

  Store::Profile preferred = {};
  CHECK(reloaded.getPreferred(preferred));
  CHECK(strcmp(preferred.ssid, "studio-wifi") == 0);
  CHECK(strcmp(preferred.password, "studio-pass-01") == 0);
}

void replacesCredentialsWithoutDuplicatingTheSsid() {
  MemoryStorage storage;
  Store store(storage);
  CHECK(store.begin() == Store::Status::NotFound);
  CHECK(store.save("home-network", "old-pass-123", true) ==
        Store::Status::Success);
  CHECK(store.save("work-network", "work-pass-123") ==
        Store::Status::Success);
  CHECK(store.setPreferred("work-network") == Store::Status::Success);

  CHECK(store.save("home-network", "new-pass-456", true) ==
        Store::Status::Success);
  CHECK(store.count() == 2);

  Store::Profile profile = {};
  CHECK(store.getBySsid("home-network", profile));
  CHECK(strcmp(profile.password, "new-pass-456") == 0);
  CHECK(store.getPreferred(profile));
  CHECK(strcmp(profile.ssid, "home-network") == 0);

  Store reloaded(storage);
  CHECK(reloaded.begin() == Store::Status::Success);
  CHECK(reloaded.count() == 2);
  CHECK(reloaded.getBySsid("home-network", profile));
  CHECK(strcmp(profile.password, "new-pass-456") == 0);
}

void supportsOpenNetworksAndEnforcesInputBounds() {
  MemoryStorage storage;
  Store store(storage);
  CHECK(store.begin() == Store::Status::NotFound);

  CHECK(store.save(nullptr, "password-123") ==
        Store::Status::InvalidArgument);
  CHECK(store.save("", "password-123") ==
        Store::Status::InvalidArgument);
  CHECK(store.save("ssid", "short") ==
        Store::Status::InvalidArgument);
  CHECK(store.save("ssid", "1234567890123456789012345678901234567890123456789012345678901234") ==
        Store::Status::InvalidArgument);

  char tooLongSsid[Store::MaxSsidLength + 2];
  memset(tooLongSsid, 'S', sizeof(tooLongSsid));
  tooLongSsid[sizeof(tooLongSsid) - 1] = '\0';
  CHECK(store.save(tooLongSsid, "valid-pass-123") ==
        Store::Status::InvalidArgument);

  CHECK(store.save("open-network", nullptr, true) ==
        Store::Status::Success);
  Store::Profile profile = {};
  CHECK(store.getPreferred(profile));
  CHECK(strcmp(profile.ssid, "open-network") == 0);
  CHECK(profile.password[0] == '\0');
}

void enforcesBoundedProfileCountButAllowsReplacementAtCapacity() {
  MemoryStorage storage;
  Store store(storage);
  CHECK(store.begin() == Store::Status::NotFound);

  CHECK(store.save("network-1", "password-001", true) == Store::Status::Success);
  CHECK(store.save("network-2", "password-002") == Store::Status::Success);
  CHECK(store.save("network-3", "password-003") == Store::Status::Success);
  CHECK(store.save("network-4", "password-004") == Store::Status::Success);
  CHECK(store.save("network-5", "password-005") == Store::Status::Success);
  CHECK(store.count() == Store::MaxProfiles);
  CHECK(store.save("network-6", "password-006") == Store::Status::Full);

  CHECK(store.save("network-4", "replacement-44") == Store::Status::Success);
  CHECK(store.count() == Store::MaxProfiles);
  Store::Profile profile = {};
  CHECK(store.getBySsid("network-4", profile));
  CHECK(strcmp(profile.password, "replacement-44") == 0);
}

void forgetsProfilesAndMaintainsPreferredIndex() {
  MemoryStorage storage;
  Store store(storage);
  CHECK(store.begin() == Store::Status::NotFound);

  CHECK(store.save("first", "first-pass-123", true) == Store::Status::Success);
  CHECK(store.save("second", "second-pass-12") == Store::Status::Success);
  CHECK(store.save("third", "third-pass-123") == Store::Status::Success);
  CHECK(store.setPreferred("third") == Store::Status::Success);

  CHECK(store.remove("second") == Store::Status::Success);
  CHECK(store.count() == 2);
  Store::Profile profile = {};
  CHECK(store.getPreferred(profile));
  CHECK(strcmp(profile.ssid, "third") == 0);

  CHECK(store.remove("third") == Store::Status::Success);
  CHECK(store.getPreferred(profile));
  CHECK(strcmp(profile.ssid, "first") == 0);

  CHECK(store.remove("missing") == Store::Status::ProfileNotFound);
  CHECK(store.remove("first") == Store::Status::Success);
  CHECK(store.count() == 0);
  CHECK(!store.hasPreferred());

  Store reloaded(storage);
  CHECK(reloaded.begin() == Store::Status::Success);
  CHECK(reloaded.count() == 0);
  CHECK(!reloaded.hasPreferred());
  CHECK(reloaded.save("new-network", "new-password-123", true) ==
        Store::Status::Success);
}

void failedCredentialReplacementLeavesPreviousRecordUntouched() {
  MemoryStorage storage;
  Store store(storage);
  CHECK(store.begin() == Store::Status::NotFound);
  CHECK(store.save("stable-network", "original-pass-1", true) ==
        Store::Status::Success);

  uint8_t oldBytes[sizeof(storage.bytes)];
  memcpy(oldBytes, storage.bytes, sizeof(oldBytes));
  const size_t oldLength = storage.length;

  storage.failWrite = true;
  CHECK(store.save("stable-network", "replacement-pass", true) ==
        Store::Status::StorageError);

  Store::Profile profile = {};
  CHECK(store.getPreferred(profile));
  CHECK(strcmp(profile.password, "original-pass-1") == 0);
  CHECK(storage.length == oldLength);
  CHECK(memcmp(storage.bytes, oldBytes, sizeof(oldBytes)) == 0);

  storage.failWrite = false;
  Store reloaded(storage);
  CHECK(reloaded.begin() == Store::Status::Success);
  CHECK(reloaded.getPreferred(profile));
  CHECK(strcmp(profile.password, "original-pass-1") == 0);
}

void rejectsCorruptAndUnsupportedRecordsWithoutOverwritingThem() {
  MemoryStorage storage;
  Store writer(storage);
  CHECK(writer.begin() == Store::Status::NotFound);
  CHECK(writer.save("protected-network", "protected-pass", true) ==
        Store::Status::Success);

  const size_t originalLength = storage.length;
  uint8_t originalBytes[sizeof(storage.bytes)];
  memcpy(originalBytes, storage.bytes, sizeof(originalBytes));

  storage.bytes[40] ^= 0x80;
  uint8_t corruptBytes[sizeof(storage.bytes)];
  memcpy(corruptBytes, storage.bytes, sizeof(corruptBytes));

  Store corrupt(storage);
  CHECK(corrupt.begin() == Store::Status::CorruptData);
  CHECK(!corrupt.isLoaded());
  CHECK(corrupt.save("other-network", "other-pass-123") ==
        Store::Status::NotReady);
  CHECK(storage.length == originalLength);
  CHECK(memcmp(storage.bytes, corruptBytes, sizeof(corruptBytes)) == 0);

  memcpy(storage.bytes, originalBytes, sizeof(originalBytes));
  storage.bytes[4] = static_cast<uint8_t>(Store::CurrentSchemaVersion + 1);
  Store future(storage);
  CHECK(future.begin() == Store::Status::UnsupportedVersion);
  CHECK(!future.isLoaded());
  CHECK(future.save("other-network", "other-pass-123") ==
        Store::Status::NotReady);
}

void reportsReadFailuresWithoutEnablingWrites() {
  MemoryStorage storage;
  storage.failRead = true;
  Store store(storage);
  CHECK(store.begin() == Store::Status::StorageError);
  CHECK(!store.isLoaded());
  CHECK(store.save("ssid", "password-123") == Store::Status::NotReady);
}
}

int main() {
  startsEmptyAndPersistsProfilesAcrossInstances();
  replacesCredentialsWithoutDuplicatingTheSsid();
  supportsOpenNetworksAndEnforcesInputBounds();
  enforcesBoundedProfileCountButAllowsReplacementAtCapacity();
  forgetsProfilesAndMaintainsPreferredIndex();
  failedCredentialReplacementLeavesPreviousRecordUntouched();
  rejectsCorruptAndUnsupportedRecordsWithoutOverwritingThem();
  reportsReadFailuresWithoutEnablingWrites();

  if (failures != 0) {
    fprintf(stderr, "%d Wi-Fi profile store test(s) failed.\n", failures);
    return 1;
  }

  printf("WiFiProfileStore tests passed.\n");
  return 0;
}
