#ifndef ESP32_GADGET_APPLICATION_H
#define ESP32_GADGET_APPLICATION_H

// Platform-neutral lifecycle contract for one application.
// Implementations must retain their own app state and borrow platform services;
// they must not initialise or destroy shared hardware when entering/leaving.
class Application {
 public:
  virtual ~Application() {}

  // Stable, non-empty identifier used by ApplicationManager.
  virtual const char* id() const = 0;

  // Starts or resumes the application. A failed begin must leave it inactive.
  virtual bool begin() = 0;

  // Advances only the active application's work.
  virtual void update() = 0;

  // Stops app-owned work without releasing platform-owned resources.
  virtual void end() = 0;
};

#endif  // ESP32_GADGET_APPLICATION_H
