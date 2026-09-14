/* Copyright (C) 2025 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Author: Jan de Cuveland */
#pragma once

#include <chrono>
#include <concepts>
#include <iostream>
#include <string>

class Nanoseconds {
public:
  constexpr Nanoseconds() : m_value(0) {}
  // Allow implicit conversion from std::chrono::nanoseconds and int64_t
  constexpr Nanoseconds(std::chrono::nanoseconds value)
      : m_value(value.count()) {}
  constexpr Nanoseconds(int64_t value) : m_value(value) {}

  [[nodiscard]] constexpr int64_t count() const noexcept { return m_value; }
  [[nodiscard]] std::string to_string() const;

  constexpr operator std::chrono::nanoseconds() const {
    return std::chrono::nanoseconds(m_value);
  }
  static Nanoseconds parse(const std::string& str);

  // Comparison operators
  bool operator==(const Nanoseconds& other) const = default;
  auto operator<=>(const Nanoseconds& other) const = default;

  // Arithmetic operators
  constexpr Nanoseconds operator+(const Nanoseconds& other) const noexcept {
    return {m_value + other.m_value};
  }
  constexpr Nanoseconds operator-(const Nanoseconds& other) const noexcept {
    return {m_value - other.m_value};
  }
  constexpr Nanoseconds operator*(int64_t factor) const noexcept {
    return {m_value * factor};
  }
  constexpr Nanoseconds operator/(int64_t divisor) const noexcept {
    return {m_value / divisor};
  }

  constexpr Nanoseconds& operator+=(const Nanoseconds& other) noexcept {
    m_value += other.m_value;
    return *this;
  }
  constexpr Nanoseconds& operator-=(const Nanoseconds& other) noexcept {
    m_value -= other.m_value;
    return *this;
  }

private:
  // Internal representation in nanoseconds, value may be negative
  int64_t m_value;
};

class SizeValue {
public:
  constexpr SizeValue() : m_value(0) {}
  // Allow implicit conversion from size_t
  constexpr SizeValue(size_t value) : m_value(value) {}

  [[nodiscard]] constexpr size_t value() const noexcept { return m_value; }
  [[nodiscard]] std::string to_string() const;

  constexpr operator size_t() const { return m_value; }
  static SizeValue parse(const std::string& str);

  // Comparison operators
  bool operator==(const SizeValue& other) const = default;
  auto operator<=>(const SizeValue& other) const = default;

  // Arithmetic operators
  constexpr SizeValue operator+(const SizeValue& other) const noexcept {
    return {m_value + other.m_value};
  }
  constexpr SizeValue operator-(const SizeValue& other) const noexcept {
    return {m_value - other.m_value};
  }
  constexpr SizeValue operator*(size_t factor) const noexcept {
    return {m_value * factor};
  }
  constexpr SizeValue operator/(size_t divisor) const noexcept {
    return {m_value / divisor};
  }

  constexpr SizeValue& operator+=(const SizeValue& other) noexcept {
    m_value += other.m_value;
    return *this;
  }
  constexpr SizeValue& operator-=(const SizeValue& other) noexcept {
    m_value -= other.m_value;
    return *this;
  }

private:
  size_t m_value;
};

template <typename T>
concept OptionValueType =
    std::same_as<T, Nanoseconds> || std::same_as<T, SizeValue>;

template <OptionValueType T>
std::istream& operator>>(std::istream& in, T& value) {
  std::string str;
  in >> str;
  if (!in) {
    return in;
  }
  try {
    value = T::parse(str);
  } catch (const std::exception&) {
    in.setstate(std::ios::failbit);
  }
  return in;
}

template <OptionValueType T>
std::ostream& operator<<(std::ostream& out, const T& value) {
  return out << value.to_string();
}

namespace option_value_literals {

namespace time_literals {
constexpr Nanoseconds operator""_ns(unsigned long long value) {
  return {static_cast<int64_t>(value)};
}
constexpr Nanoseconds operator""_us(unsigned long long value) {
  return {std::chrono::microseconds(static_cast<long long>(value))};
}
constexpr Nanoseconds operator""_ms(unsigned long long value) {
  return {std::chrono::milliseconds(static_cast<long long>(value))};
}
constexpr Nanoseconds operator""_s(unsigned long long value) {
  return {std::chrono::seconds(static_cast<long long>(value))};
}
} // namespace time_literals

namespace size_literals {
constexpr SizeValue operator""_B(unsigned long long value) {
  return {static_cast<size_t>(value)};
}
constexpr SizeValue operator""_KiB(unsigned long long value) {
  return {static_cast<size_t>(value) * 1024ULL};
}
constexpr SizeValue operator""_MiB(unsigned long long value) {
  return {static_cast<size_t>(value) * 1024ULL * 1024};
}
constexpr SizeValue operator""_GiB(unsigned long long value) {
  return {static_cast<size_t>(value) * 1024ULL * 1024 * 1024};
}
constexpr SizeValue operator""_TiB(unsigned long long value) {
  return {static_cast<size_t>(value) * 1024ULL * 1024 * 1024 * 1024};
}
constexpr SizeValue operator""_PiB(unsigned long long value) {
  return {static_cast<size_t>(value) * 1024ULL * 1024 * 1024 * 1024 * 1024};
}
constexpr SizeValue operator""_kB(unsigned long long value) {
  return {static_cast<size_t>(value) * 1000ULL};
}
constexpr SizeValue operator""_MB(unsigned long long value) {
  return {static_cast<size_t>(value) * 1000ULL * 1000};
}
constexpr SizeValue operator""_GB(unsigned long long value) {
  return {static_cast<size_t>(value) * 1000ULL * 1000 * 1000};
}
constexpr SizeValue operator""_TB(unsigned long long value) {
  return {static_cast<size_t>(value) * 1000ULL * 1000 * 1000 * 1000};
}
constexpr SizeValue operator""_PB(unsigned long long value) {
  return {static_cast<size_t>(value) * 1000ULL * 1000 * 1000 * 1000 * 1000};
}
} // namespace size_literals

// Convenience aliases to bring all literals into scope
using namespace time_literals;
using namespace size_literals;

} // namespace option_value_literals
