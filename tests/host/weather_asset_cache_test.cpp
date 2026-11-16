#include <stdio.h>
#include <stdlib.h>
#include <type_traits>

#include "../../src/apps/weather/WeatherAssetCache.h"

static void check(bool condition, const char* expression, int line) {
  if (!condition) {
    fprintf(stderr, "FAIL: %s (line %d)\n", expression, line);
    exit(EXIT_FAILURE);
  }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

static uint8_t* makePng(size_t length) {
  uint8_t* data = static_cast<uint8_t*>(malloc(length));
  if (data == nullptr) {
    fprintf(stderr, "Test allocation failed\n");
    exit(EXIT_FAILURE);
  }

  for (size_t i = 0; i < length; ++i) {
    data[i] = 0;
  }

  if (length >= 4) {
    data[0] = 0x89;
    data[1] = 0x50;
    data[2] = 0x4E;
    data[3] = 0x47;
  }
  return data;
}

int main() {
  static_assert(
    !std::is_copy_constructible<WeatherAssetCache>::value,
    "WeatherAssetCache must not copy its owned buffer");
  static_assert(
    !std::is_move_constructible<WeatherAssetCache>::value,
    "WeatherAssetCache must not move its owned buffer implicitly");

  WeatherAssetCache cache;
  CHECK(!cache.contains("01d"));

  size_t length = 123;
  CHECK(cache.get("01d", &length) == nullptr);
  CHECK(length == 0);

  uint8_t* invalidCodeData = makePng(32);
  CHECK(!cache.store("1d", invalidCodeData, 32));
  free(invalidCodeData);

  uint8_t* invalidSignature = makePng(32);
  invalidSignature[0] = 0x00;
  CHECK(!cache.store("01d", invalidSignature, 32));
  free(invalidSignature);

  uint8_t* tooShort = makePng(8);
  CHECK(!cache.store("01d", tooShort, 8));
  free(tooShort);

  uint8_t* tooLarge = makePng(WeatherAssetCache::MaxPngBytes + 1);
  CHECK(!cache.store(
    "01d", tooLarge, WeatherAssetCache::MaxPngBytes + 1));
  free(tooLarge);

  CHECK(!cache.store("01d", nullptr, 32));

  // A failed replacement must leave the existing cached asset untouched.
  uint8_t* first = makePng(48);
  CHECK(cache.store("01d", first, 48));
  CHECK(cache.contains("01d"));
  CHECK(!cache.contains("02d"));

  uint8_t* retrieved = cache.get("01d", &length);
  CHECK(retrieved == first);
  CHECK(length == 48);

  length = 99;
  CHECK(cache.get("02d", &length) == nullptr);
  CHECK(length == 0);

  uint8_t* rejectedReplacement = makePng(24);
  rejectedReplacement[1] = 0;
  CHECK(!cache.store("02d", rejectedReplacement, 24));
  free(rejectedReplacement);
  CHECK(cache.get("01d", &length) == first);
  CHECK(length == 48);

  // Successful replacement changes the identity and takes ownership of the
  // new buffer, releasing the prior one.
  uint8_t* second = makePng(64);
  CHECK(cache.store("02d", second, 64));
  CHECK(!cache.contains("01d"));
  CHECK(cache.contains("02d"));
  CHECK(cache.get("02d", &length) == second);
  CHECK(length == 64);

  // Re-keying the same owned buffer must not free it before cache destruction.
  CHECK(cache.store("03d", second, 64));
  CHECK(!cache.contains("02d"));
  CHECK(cache.contains("03d"));
  CHECK(cache.get("03d", &length) == second);

  cache.clear();
  CHECK(!cache.contains("03d"));
  length = 99;
  CHECK(cache.get("03d", &length) == nullptr);
  CHECK(length == 0);

  printf("WeatherAssetCache tests passed.\n");
  return EXIT_SUCCESS;
}
