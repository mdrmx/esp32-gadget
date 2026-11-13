#pragma once

#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <string>

class String {
public:
  String() = default;
  String(const char* value) : value_(value != nullptr ? value : "") {}
  String(const std::string& value) : value_(value) {}

  unsigned int length() const {
    return static_cast<unsigned int>(value_.length());
  }

  char operator[](unsigned int index) const {
    return value_[index];
  }

  int indexOf(char value, int start = 0) const {
    if (start < 0) start = 0;
    const size_t found = value_.find(value, static_cast<size_t>(start));
    return found == std::string::npos ? -1 : static_cast<int>(found);
  }

  int lastIndexOf(char value) const {
    const size_t found = value_.rfind(value);
    return found == std::string::npos ? -1 : static_cast<int>(found);
  }

  String substring(unsigned int start) const {
    if (start >= value_.length()) return String();
    return String(value_.substr(start));
  }

  String substring(unsigned int start, unsigned int end) const {
    if (start >= value_.length() || end <= start) return String();
    return String(value_.substr(start, end - start));
  }

  const char* c_str() const {
    return value_.c_str();
  }

  friend String operator+(const String& left, const String& right) {
    return String(left.value_ + right.value_);
  }

  friend String operator+(const String& left, const char* right) {
    return String(left.value_ + (right != nullptr ? right : ""));
  }

  friend String operator+(const char* left, const String& right) {
    return String((left != nullptr ? left : "") + right.value_);
  }

  bool operator==(const String& other) const {
    return value_ == other.value_;
  }

private:
  std::string value_;
};

#ifndef OUTPUT
#define OUTPUT 1
#endif

#ifndef HIGH
#define HIGH 1
#endif

inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
