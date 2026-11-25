#include "src/services/WiFiAvailabilityRefreshPolicy.h"

#include <stdint.h>
#include <stdio.h>

namespace {
int failures = 0;
#define CHECK(condition) do { \
  if (!(condition)) { \
    fprintf(stderr, "FAIL %s:%d: %s\\n", __FILE__, __LINE__, #condition); \
    ++failures; \
  } \
} while (0)

void startsImmediatelyWhenProfilesOpens() {
  WiFiAvailabilityRefreshPolicy policy;
  CHECK(policy.shouldStart(100U, true, false, false));
  CHECK(!policy.shouldStart(100U, false, false, false));
  CHECK(!policy.shouldStart(100U, true, true, false));
}

void refreshesAtTheConfiguredInterval() {
  WiFiAvailabilityRefreshPolicy policy;
  policy.noteAttempt(1000U);

  CHECK(!policy.shouldStart(
      1000U + WiFiAvailabilityRefreshPolicy::RefreshIntervalMs - 1U,
      true, false, false));
  CHECK(policy.shouldStart(
      1000U + WiFiAvailabilityRefreshPolicy::RefreshIntervalMs,
      true, false, false));
}

void pausesWhenProfilesAreNotVisibleOrScanIsActive() {
  WiFiAvailabilityRefreshPolicy policy;
  policy.noteAttempt(0U);
  const uint32_t due = WiFiAvailabilityRefreshPolicy::RefreshIntervalMs;

  CHECK(!policy.shouldStart(due, false, false, false));
  CHECK(!policy.shouldStart(due, true, true, false));
  CHECK(policy.shouldStart(due, true, false, false));
}

void retriesFailedScansOnTheShorterInterval() {
  WiFiAvailabilityRefreshPolicy policy;
  policy.noteAttempt(500U);

  CHECK(!policy.shouldStart(
      500U + WiFiAvailabilityRefreshPolicy::FailedScanRetryIntervalMs - 1U,
      true, false, true));
  CHECK(policy.shouldStart(
      500U + WiFiAvailabilityRefreshPolicy::FailedScanRetryIntervalMs,
      true, false, true));
}

void elapsedTimeIsSafeAcrossMillisRollover() {
  WiFiAvailabilityRefreshPolicy policy;
  const uint32_t startedAt = UINT32_MAX - 1000U;
  const uint32_t dueAt =
      startedAt + WiFiAvailabilityRefreshPolicy::RefreshIntervalMs;
  policy.noteAttempt(startedAt);

  CHECK(policy.shouldStart(dueAt - 1U, true, false, false) == false);
  CHECK(policy.shouldStart(dueAt, true, false, false));
}
}

int main() {
  startsImmediatelyWhenProfilesOpens();
  refreshesAtTheConfiguredInterval();
  pausesWhenProfilesAreNotVisibleOrScanIsActive();
  retriesFailedScansOnTheShorterInterval();
  elapsedTimeIsSafeAcrossMillisRollover();

  if (failures != 0) {
    fprintf(stderr, "%d WiFiAvailabilityRefreshPolicy test(s) failed.\\n",
            failures);
    return 1;
  }

  printf("WiFiAvailabilityRefreshPolicy tests passed.\\n");
  return 0;
}
