#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Bounded, versioned Wi-Fi profile storage. Storage is injected so the profile
// rules and binary format can be tested without Arduino or ESP32 dependencies.
// The ESP32 composition root supplies PreferencesWiFiProfileStorage.
template <typename Storage>
class WiFiProfileStore {
 public:
  static const uint8_t MaxProfiles = 5;
  static const size_t MaxSsidLength = 32;
  static const size_t MaxPasswordLength = 63;
  static const uint8_t NoPreferredProfile = 0xFF;

  static const uint32_t StorageMagic = 0x57494649UL; // "WIFI"
  static const uint16_t CurrentSchemaVersion = 1;

  static const size_t EncodedHeaderSize = 10;
  static const size_t EncodedProfileSize =
      (MaxSsidLength + 1) + (MaxPasswordLength + 1);
  static const size_t EncodedChecksumSize = 4;
  static const size_t MaxEncodedSize =
      EncodedHeaderSize +
      (MaxProfiles * EncodedProfileSize) +
      EncodedChecksumSize;

  enum class Status : uint8_t {
    Success = 0,
    NotFound,
    NotReady,
    InvalidArgument,
    Full,
    ProfileNotFound,
    StorageError,
    CorruptData,
    UnsupportedVersion
  };

  struct Profile {
    char ssid[MaxSsidLength + 1];
    char password[MaxPasswordLength + 1];
  };

  explicit WiFiProfileStore(Storage& storage)
      : storage_(storage), count_(0), preferredIndex_(NoPreferredProfile),
        loaded_(false) {
    memset(profiles_, 0, sizeof(profiles_));
  }

  // Loads the current schema. NotFound is a normal first-boot result and leaves
  // an empty, writable store. Corrupt/unknown data is never silently replaced.
  Status begin() {
    loaded_ = false;
    clearProfiles();

    uint8_t encoded[MaxEncodedSize];
    size_t encodedLength = 0;
    if (!storage_.read(encoded, sizeof(encoded), encodedLength)) {
      return Status::StorageError;
    }

    if (encodedLength == 0) {
      loaded_ = true;
      return Status::NotFound;
    }

    if (encodedLength != MaxEncodedSize) {
      return Status::CorruptData;
    }

    if (readU32(encoded) != StorageMagic) {
      return Status::CorruptData;
    }

    const uint16_t version = readU16(encoded + 4);
    if (version != CurrentSchemaVersion) {
      return Status::UnsupportedVersion;
    }

    if (readU16(encoded + 6) != MaxEncodedSize) {
      return Status::CorruptData;
    }

    const uint32_t storedChecksum =
        readU32(encoded + MaxEncodedSize - EncodedChecksumSize);
    const uint32_t calculatedChecksum =
        crc32(encoded, MaxEncodedSize - EncodedChecksumSize);
    if (storedChecksum != calculatedChecksum) {
      return Status::CorruptData;
    }

    const uint8_t storedCount = encoded[8];
    const uint8_t storedPreferredIndex = encoded[9];
    if (storedCount > MaxProfiles ||
        (storedCount == 0 &&
         storedPreferredIndex != NoPreferredProfile) ||
        (storedCount > 0 &&
         storedPreferredIndex != NoPreferredProfile &&
         storedPreferredIndex >= storedCount)) {
      return Status::CorruptData;
    }

    Profile decoded[MaxProfiles];
    memset(decoded, 0, sizeof(decoded));

    size_t offset = EncodedHeaderSize;
    for (uint8_t i = 0; i < MaxProfiles; ++i) {
      memcpy(decoded[i].ssid, encoded + offset, sizeof(decoded[i].ssid));
      offset += sizeof(decoded[i].ssid);
      memcpy(
          decoded[i].password,
          encoded + offset,
          sizeof(decoded[i].password));
      offset += sizeof(decoded[i].password);

      if (i < storedCount) {
        if (!isValidStoredProfile(decoded[i])) {
          return Status::CorruptData;
        }
        for (uint8_t j = 0; j < i; ++j) {
          if (strcmp(decoded[i].ssid, decoded[j].ssid) == 0) {
            return Status::CorruptData;
          }
        }
      } else if (!isZeroProfile(decoded[i])) {
        // Unused slots must be cleared; deleted passwords must not linger in
        // the serialized record after a profile is forgotten.
        return Status::CorruptData;
      }
    }

    memcpy(profiles_, decoded, sizeof(profiles_));
    count_ = storedCount;
    preferredIndex_ = storedPreferredIndex;
    loaded_ = true;
    return Status::Success;
  }

  // Add a profile or replace the credentials for an existing SSID. A
  // replacement is built as a complete candidate record and becomes visible
  // in RAM only after the storage backend accepts the whole new blob.
  Status save(
      const char* ssid,
      const char* password,
      bool makePreferred = false) {
    if (!loaded_) {
      return Status::NotReady;
    }

    Profile replacement;
    if (!makeProfile(ssid, password, replacement)) {
      return Status::InvalidArgument;
    }

    Profile candidateProfiles[MaxProfiles];
    memcpy(candidateProfiles, profiles_, sizeof(candidateProfiles));
    uint8_t candidateCount = count_;
    uint8_t candidatePreferred = preferredIndex_;

    const int8_t existingIndex = findProfileIndex(ssid);
    uint8_t targetIndex = 0;

    if (existingIndex >= 0) {
      targetIndex = static_cast<uint8_t>(existingIndex);
    } else {
      if (candidateCount >= MaxProfiles) {
        return Status::Full;
      }
      targetIndex = candidateCount;
      ++candidateCount;
    }

    candidateProfiles[targetIndex] = replacement;

    if (makePreferred || candidatePreferred == NoPreferredProfile) {
      candidatePreferred = targetIndex;
    }

    return commit(
        candidateProfiles, candidateCount, candidatePreferred);
  }

  Status setPreferred(const char* ssid) {
    if (!loaded_) {
      return Status::NotReady;
    }
    if (ssid == nullptr || ssid[0] == '\0') {
      return Status::InvalidArgument;
    }

    const int8_t index = findProfileIndex(ssid);
    if (index < 0) {
      return Status::ProfileNotFound;
    }

    if (static_cast<uint8_t>(index) == preferredIndex_) {
      return Status::Success;
    }

    return commit(profiles_, count_, static_cast<uint8_t>(index));
  }

  Status remove(const char* ssid) {
    if (!loaded_) {
      return Status::NotReady;
    }
    if (ssid == nullptr || ssid[0] == '\0') {
      return Status::InvalidArgument;
    }

    const int8_t foundIndex = findProfileIndex(ssid);
    if (foundIndex < 0) {
      return Status::ProfileNotFound;
    }

    const uint8_t removeIndex = static_cast<uint8_t>(foundIndex);
    Profile candidateProfiles[MaxProfiles];
    memset(candidateProfiles, 0, sizeof(candidateProfiles));

    uint8_t destination = 0;
    for (uint8_t source = 0; source < count_; ++source) {
      if (source == removeIndex) {
        continue;
      }
      candidateProfiles[destination++] = profiles_[source];
    }

    const uint8_t candidateCount = destination;
    uint8_t candidatePreferred = NoPreferredProfile;
    if (candidateCount > 0) {
      if (preferredIndex_ == removeIndex) {
        candidatePreferred = 0;
      } else if (preferredIndex_ != NoPreferredProfile) {
        candidatePreferred = preferredIndex_ > removeIndex
            ? static_cast<uint8_t>(preferredIndex_ - 1)
            : preferredIndex_;
      }
    }

    return commit(
        candidateProfiles, candidateCount, candidatePreferred);
  }

  bool getPreferred(Profile& output) const {
    if (!loaded_ || preferredIndex_ == NoPreferredProfile ||
        preferredIndex_ >= count_) {
      return false;
    }
    output = profiles_[preferredIndex_];
    return true;
  }

  bool getAt(uint8_t index, Profile& output) const {
    if (!loaded_ || index >= count_) {
      return false;
    }
    output = profiles_[index];
    return true;
  }

  bool getBySsid(const char* ssid, Profile& output) const {
    if (!loaded_ || ssid == nullptr || ssid[0] == '\0') {
      return false;
    }
    const int8_t index = findProfileIndex(ssid);
    if (index < 0) {
      return false;
    }
    output = profiles_[static_cast<uint8_t>(index)];
    return true;
  }

  uint8_t count() const {
    return loaded_ ? count_ : 0;
  }

  bool hasPreferred() const {
    return loaded_ &&
        preferredIndex_ != NoPreferredProfile &&
        preferredIndex_ < count_;
  }

  bool isLoaded() const {
    return loaded_;
  }

 private:
  static uint16_t readU16(const uint8_t* bytes) {
    return static_cast<uint16_t>(
        static_cast<uint16_t>(bytes[0]) |
        (static_cast<uint16_t>(bytes[1]) << 8));
  }

  static uint32_t readU32(const uint8_t* bytes) {
    return static_cast<uint32_t>(bytes[0]) |
        (static_cast<uint32_t>(bytes[1]) << 8) |
        (static_cast<uint32_t>(bytes[2]) << 16) |
        (static_cast<uint32_t>(bytes[3]) << 24);
  }

  static void writeU16(uint8_t* bytes, uint16_t value) {
    bytes[0] = static_cast<uint8_t>(value & 0xFF);
    bytes[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
  }

  static void writeU32(uint8_t* bytes, uint32_t value) {
    bytes[0] = static_cast<uint8_t>(value & 0xFF);
    bytes[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    bytes[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    bytes[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
  }

  static uint32_t crc32(const uint8_t* bytes, size_t length) {
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0; i < length; ++i) {
      crc ^= bytes[i];
      for (uint8_t bit = 0; bit < 8; ++bit) {
        crc = (crc & 1UL) != 0
            ? (crc >> 1) ^ 0xEDB88320UL
            : crc >> 1;
      }
    }
    return crc ^ 0xFFFFFFFFUL;
  }

  static size_t boundedLength(const char* value, size_t maximum) {
    if (value == nullptr) {
      return maximum + 1;
    }
    size_t length = 0;
    while (length <= maximum && value[length] != '\0') {
      ++length;
    }
    return length;
  }

  static bool makeProfile(
      const char* ssid,
      const char* password,
      Profile& output) {
    const size_t ssidLength = boundedLength(ssid, MaxSsidLength);
    const size_t passwordLength = password == nullptr
        ? 0
        : boundedLength(password, MaxPasswordLength);

    if (ssidLength == 0 || ssidLength > MaxSsidLength ||
        passwordLength > MaxPasswordLength ||
        (passwordLength > 0 && passwordLength < 8)) {
      return false;
    }

    memset(&output, 0, sizeof(output));
    memcpy(output.ssid, ssid, ssidLength);
    if (passwordLength > 0) {
      memcpy(output.password, password, passwordLength);
    }
    return true;
  }

  static bool hasTerminator(const char* value, size_t capacity) {
    return memchr(value, '\0', capacity) != nullptr;
  }

  static bool isZeroTail(const char* value, size_t capacity) {
    bool foundTerminator = false;
    for (size_t i = 0; i < capacity; ++i) {
      if (!foundTerminator && value[i] == '\0') {
        foundTerminator = true;
      } else if (foundTerminator && value[i] != '\0') {
        return false;
      }
    }
    return foundTerminator;
  }

  static bool isZeroProfile(const Profile& profile) {
    const uint8_t* bytes =
        reinterpret_cast<const uint8_t*>(&profile);
    for (size_t i = 0; i < sizeof(profile); ++i) {
      if (bytes[i] != 0) {
        return false;
      }
    }
    return true;
  }

  static bool isValidStoredProfile(const Profile& profile) {
    if (!hasTerminator(profile.ssid, sizeof(profile.ssid)) ||
        !hasTerminator(profile.password, sizeof(profile.password)) ||
        !isZeroTail(profile.ssid, sizeof(profile.ssid)) ||
        !isZeroTail(profile.password, sizeof(profile.password))) {
      return false;
    }

    const size_t ssidLength = strlen(profile.ssid);
    const size_t passwordLength = strlen(profile.password);
    return ssidLength > 0 &&
        ssidLength <= MaxSsidLength &&
        passwordLength <= MaxPasswordLength &&
        (passwordLength == 0 || passwordLength >= 8);
  }

  static void encode(
      const Profile* profiles,
      uint8_t count,
      uint8_t preferredIndex,
      uint8_t output[MaxEncodedSize]) {
    memset(output, 0, MaxEncodedSize);
    writeU32(output, StorageMagic);
    writeU16(output + 4, CurrentSchemaVersion);
    writeU16(output + 6, static_cast<uint16_t>(MaxEncodedSize));
    output[8] = count;
    output[9] = preferredIndex;

    size_t offset = EncodedHeaderSize;
    for (uint8_t i = 0; i < MaxProfiles; ++i) {
      memcpy(output + offset, profiles[i].ssid, sizeof(profiles[i].ssid));
      offset += sizeof(profiles[i].ssid);
      memcpy(
          output + offset,
          profiles[i].password,
          sizeof(profiles[i].password));
      offset += sizeof(profiles[i].password);
    }

    writeU32(
        output + MaxEncodedSize - EncodedChecksumSize,
        crc32(output, MaxEncodedSize - EncodedChecksumSize));
  }

  int8_t findProfileIndex(const char* ssid) const {
    if (ssid == nullptr) {
      return -1;
    }
    for (uint8_t i = 0; i < count_; ++i) {
      if (strcmp(profiles_[i].ssid, ssid) == 0) {
        return static_cast<int8_t>(i);
      }
    }
    return -1;
  }

  void clearProfiles() {
    memset(profiles_, 0, sizeof(profiles_));
    count_ = 0;
    preferredIndex_ = NoPreferredProfile;
  }

  Status commit(
      const Profile candidateProfiles[MaxProfiles],
      uint8_t candidateCount,
      uint8_t candidatePreferred) {
    uint8_t encoded[MaxEncodedSize];
    encode(
        candidateProfiles,
        candidateCount,
        candidatePreferred,
        encoded);

    if (!storage_.write(encoded, sizeof(encoded))) {
      return Status::StorageError;
    }

    memcpy(profiles_, candidateProfiles, sizeof(profiles_));
    count_ = candidateCount;
    preferredIndex_ = candidatePreferred;
    return Status::Success;
  }

  Storage& storage_;
  Profile profiles_[MaxProfiles];
  uint8_t count_;
  uint8_t preferredIndex_;
  bool loaded_;
};
