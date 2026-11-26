#ifndef ESP32_GADGET_APPLICATION_MANAGER_H
#define ESP32_GADGET_APPLICATION_MANAGER_H

#include <stdint.h>
#include <string.h>

#include "Application.h"

// Fixed-capacity lifecycle coordinator. Registration stores non-owning
// pointers; the composition root owns application instances and platform
// services. No heap allocation or Arduino dependencies are introduced.
class ApplicationManager {
 public:
  static const uint8_t MaxApplications = 4;

  ApplicationManager()
      : applications_(), count_(0), active_(nullptr) {}

  // Registration is bounded and rejects null/empty or duplicate identifiers.
  // The manager does not take ownership of the supplied application.
  bool registerApplication(Application& application) {
    const char* applicationId = application.id();
    if (!isValidId(applicationId) || count_ >= MaxApplications) {
      return false;
    }

    for (uint8_t i = 0; i < count_; ++i) {
      if (&application == applications_[i] ||
          strcmp(applicationId, applications_[i]->id()) == 0) {
        return false;
      }
    }

    applications_[count_++] = &application;
    return true;
  }

  // Starts the first app. Re-requesting the active id is idempotent; starting
  // another id while one is active requires switchTo().
  bool start(const char* applicationId) {
    Application* target = find(applicationId);
    if (target == nullptr) {
      return false;
    }
    if (active_ != nullptr) {
      return active_ == target;
    }
    if (!target->begin()) {
      return false;
    }

    active_ = target;
    return true;
  }

  // Ends the current app before beginning the target. If target startup fails,
  // the manager attempts to restart the previous app and reports failure.
  // A failed restart leaves the manager with no active application.
  bool switchTo(const char* applicationId) {
    Application* target = find(applicationId);
    if (target == nullptr) {
      return false;
    }
    if (target == active_) {
      return true;
    }

    Application* previous = active_;
    if (previous != nullptr) {
      previous->end();
      active_ = nullptr;
    }

    if (target->begin()) {
      active_ = target;
      return true;
    }

    if (previous != nullptr && previous->begin()) {
      active_ = previous;
    }
    return false;
  }

  // Stops active app-owned work. Shared display, touch, connectivity, location,
  // time and storage services remain alive in the platform composition root.
  bool stopActive() {
    if (active_ == nullptr) {
      return false;
    }

    active_->end();
    active_ = nullptr;
    return true;
  }

  void update() {
    if (active_ != nullptr) {
      active_->update();
    }
  }

  bool hasApplication(const char* applicationId) const {
    return find(applicationId) != nullptr;
  }

  const char* activeId() const {
    return active_ != nullptr ? active_->id() : nullptr;
  }

  Application* activeApplication() const {
    return active_;
  }

  uint8_t registeredCount() const {
    return count_;
  }

 private:
  Application* applications_[MaxApplications];
  uint8_t count_;
  Application* active_;

  static bool isValidId(const char* applicationId) {
    return applicationId != nullptr && applicationId[0] != '\0';
  }

  Application* find(const char* applicationId) const {
    if (!isValidId(applicationId)) {
      return nullptr;
    }

    for (uint8_t i = 0; i < count_; ++i) {
      if (strcmp(applicationId, applications_[i]->id()) == 0) {
        return applications_[i];
      }
    }
    return nullptr;
  }
};

#endif  // ESP32_GADGET_APPLICATION_MANAGER_H
