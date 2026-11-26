#ifndef ESP32_GADGET_APPLICATION_LAUNCHER_CONTROLLER_H
#define ESP32_GADGET_APPLICATION_LAUNCHER_CONTROLLER_H

#include <stdint.h>

#include "ApplicationManager.h"

// Coordinates the launcher lifecycle without owning applications or UI.
// Opening the launcher suspends the active app; selecting or returning starts
// the chosen app through ApplicationManager while shared platform services live.
class ApplicationLauncherController {
 public:
  explicit ApplicationLauncherController(ApplicationManager& manager)
      : manager_(manager),
        returnApplicationId_(nullptr),
        open_(false) {}

  bool open() {
    if (open_) {
      return false;
    }

    const char* activeId = manager_.activeId();
    if (manager_.activeApplication() != nullptr && !manager_.stopActive()) {
      return false;
    }

    returnApplicationId_ = activeId;
    open_ = true;
    return true;
  }

  bool select(uint8_t applicationIndex) {
    if (!open_) {
      return false;
    }

    const Application* application = manager_.applicationAt(applicationIndex);
    if (application == nullptr) {
      return false;
    }

    const char* applicationId = application->id();
    if (applicationId == nullptr || applicationId[0] == '\0') {
      return false;
    }

    if (!manager_.switchTo(applicationId)) {
      return false;
    }

    close();
    return true;
  }

  // Return to the app that was active when the launcher opened. If there was
  // no active app, BACK simply closes the launcher and leaves the platform idle.
  bool back() {
    if (!open_) {
      return false;
    }

    if (returnApplicationId_ != nullptr &&
        !manager_.switchTo(returnApplicationId_)) {
      return false;
    }

    close();
    return true;
  }

  bool isOpen() const {
    return open_;
  }

  const char* returnApplicationId() const {
    return returnApplicationId_;
  }

 private:
  ApplicationManager& manager_;
  const char* returnApplicationId_;
  bool open_;

  void close() {
    open_ = false;
    returnApplicationId_ = nullptr;
  }
};

#endif  // ESP32_GADGET_APPLICATION_LAUNCHER_CONTROLLER_H
