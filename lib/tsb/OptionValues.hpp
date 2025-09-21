// Copyright 2025 Jan de Cuveland
#pragma once

#include <chrono>
#include <concepts>
#include <iostream>
#include <string>

class Nanoseconds {
public:
  Nanoseconds() : m_value(0) {}
  Nanoseconds(std::chrono::nanoseconds value) : m_value(value.count()) {}
  Nanoseconds(int64_t value) : m_value(value) {}

  [[nodiscard]] int64_t count() const { return m_value; }
  [[nodiscard]] std::string to_string() const;

  operator std::chrono::nanoseconds() const {
    return std::chrono::nanoseconds(m_value);
  }
  static Nanoseconds parse(const std::string& str);

private:
  int64_t m_value;
};

class SizeValue {
public:
  SizeValue() : m_value(0) {}
  SizeValue(size_t value) : m_value(value) {}

  [[nodiscard]] size_t value() const { return m_value; }
  [[nodiscard]] std::string to_string() const;

  operator size_t() const { return m_value; }
  static SizeValue parse(const std::string& str);

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
