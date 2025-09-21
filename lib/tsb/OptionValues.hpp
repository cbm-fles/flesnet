// Copyright 2025 Jan de Cuveland
#pragma once

#include <chrono>
#include <istream>
#include <ostream>

class Nanoseconds {
private:
  int64_t m_value;

public:
  Nanoseconds() : m_value(0) {}
  Nanoseconds(std::chrono::nanoseconds val) : m_value(val.count()) {}
  Nanoseconds(int64_t val) : m_value(val) {}

  [[nodiscard]] int64_t count() const { return m_value; }
  [[nodiscard]] std::string to_str() const;

  operator std::chrono::nanoseconds() const {
    return std::chrono::nanoseconds(m_value);
  }
  friend std::istream& operator>>(std::istream& in, Nanoseconds& time);
  friend std::ostream& operator<<(std::ostream& out, const Nanoseconds& time);
};

std::istream& operator>>(std::istream& in, Nanoseconds& time);
std::ostream& operator<<(std::ostream& out, const Nanoseconds& time);
