//
//    Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
// Typed binary buffer for general-purpose I/O (endianness-aware,
// seal/reserve, varints, spans).

#pragma once

#include <atomic>
#include <bit>
#include <bitset>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace urkern {

enum class Endianness { LittleEndian, BigEndian };

consteval Endianness GetSystemEndianness() {
  if constexpr (std::endian::native == std::endian::big) {
    return Endianness::BigEndian;
  } else {
    return Endianness::LittleEndian;
  }
}

template <typename T>
concept Arithmetic8Byte = std::is_arithmetic_v<T> && sizeof(T) <= 8;

template <typename T>
concept Arithmetic16Byte = std::is_arithmetic_v<T> && sizeof(T) <= 16;

template <typename T>
concept Integral16Byte = std::is_integral_v<T> && sizeof(T) <= 16;

enum class BufferError {
  None,
  WriteAfterSeal,
  CannotAllocate,
  ReadOutOfBounds,
  CorruptedFormat,
};

inline const char* GetBufferErrorString(BufferError error) {
  switch (error) {
    case BufferError::None:
      return "NoError";
    case BufferError::WriteAfterSeal:
      return "WriteAfterSeal";
    case BufferError::CannotAllocate:
      return "CannotAllocate";
    case BufferError::ReadOutOfBounds:
      return "ReadOutOfBounds";
    case BufferError::CorruptedFormat:
      return "CorruptedFormat";
  }
  return "Unknown";
}

class Buffer {
 public:
  explicit Buffer(Endianness endianness = Endianness::LittleEndian)
      : endianness_(endianness) {}

  Buffer(const char* data, size_t data_size,
         Endianness endianness = Endianness::LittleEndian)
      : endianness_(endianness),
        allocated_size_(data_size),
        write_cursor_(data_size) {
    data_ = new (std::nothrow) char[allocated_size_];
    if (!data_) [[unlikely]] {
      last_error_ = BufferError::CannotAllocate;
      allocated_size_ = 0;
      write_cursor_ = 0;
      return;
    }
    std::memcpy(data_, data, write_cursor_);
  }

  ~Buffer() { delete[] data_; }

  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;

  Buffer(Buffer&& other) noexcept
      : endianness_(other.endianness_),
        allocated_size_(other.allocated_size_),
        write_cursor_(other.write_cursor_),
        read_cursor_(other.read_cursor_),
        read_limit_(other.read_limit_),
        data_(other.data_),
        last_error_(other.last_error_) {
    other.data_ = nullptr;
    other.allocated_size_ = 0;
    other.write_cursor_ = 0;
    other.read_cursor_ = 0;
  }

  template <Arithmetic8Byte T>
  void Read(std::span<T> data) {
    size_t size = data.size();
    if (size > std::numeric_limits<size_t>::max() / sizeof(T)) [[unlikely]] {
      last_error_ = BufferError::CorruptedFormat;
      return;
    }
    size_t bytes = sizeof(T) * size;
    if (!CheckReadableBytes(bytes)) [[unlikely]] {
      last_error_ = BufferError::ReadOutOfBounds;
      return;
    }
    std::memcpy(reinterpret_cast<char*>(data.data()), data_ + read_cursor_,
                bytes);
    read_cursor_ += bytes;
  }

  template <Arithmetic8Byte T>
  void Read(T* arr, size_t size) {
    Read(std::span<T>(arr, size));
  }

  char ReadChar() { return ReadInt<char>(); }
  unsigned char ReadUnsignedChar() { return ReadInt<unsigned char>(); }
  bool ReadBool() { return ReadInt<bool>(); }
  float ReadFloat() { return ReadNumber<float>(); }
  double ReadDouble() { return ReadNumber<double>(); }

  template <Integral16Byte T>
  T ReadInt() {
    return ReadNumber<T>();
  }

  template <Arithmetic16Byte T>
  T ReadNumber() {
    constexpr size_t size = sizeof(T);
    char data[size];
    if (!CheckReadableBytes(size)) [[unlikely]] {
      last_error_ = BufferError::ReadOutOfBounds;
      return 0;
    }
    if (GetSystemEndianness() == endianness_) [[likely]] {
      std::memcpy(data, data_ + read_cursor_, size);
    } else {
      for (size_t i = 0; i < size; ++i) {
        data[size - 1 - i] = data_[read_cursor_ + i];
      }
    }
    read_cursor_ += size;
    T l;
    std::memcpy(&l, data, size);
    return l;
  }

  template <size_t N>
  std::bitset<N> ReadBitset() {
    constexpr size_t BYTES = (N + 7) / 8;
    uint8_t data[BYTES];
    Read(data, BYTES);
    std::bitset<N> bs;
    for (size_t i = 0; i < N; ++i) {
      bool bit = (data[i / 8] >> (i % 8)) & 1;
      bs.set(i, bit);
    }
    return bs;
  }

  template <Arithmetic8Byte T>
  T ReadVarInt() {
    constexpr size_t size = sizeof(T);
    if (!CheckReadableBytes(1)) [[unlikely]] {
      last_error_ = BufferError::ReadOutOfBounds;
      return 0;
    }
    unsigned char actual_size = ReadUnsignedChar();
    if (actual_size > size) [[unlikely]] {
      last_error_ = BufferError::CorruptedFormat;
      return 0;
    }
    if (!CheckReadableBytes(actual_size)) [[unlikely]] {
      last_error_ = BufferError::ReadOutOfBounds;
      return 0;
    }
    char data[size];
    std::memset(data, 0, size);
    if (GetSystemEndianness() == endianness_) [[likely]] {
      std::memcpy(data, data_ + read_cursor_, actual_size);
    } else {
      for (size_t i = 0; i < actual_size; ++i) {
        data[actual_size - 1 - i] = data_[read_cursor_ + i];
      }
    }
    read_cursor_ += actual_size;
    T l = 0;
    std::memcpy(&l, data, size);
    return l;
  }

  std::string ReadString() {
    size_t size = ReadVarInt<size_t>();
    if (!CheckReadableBytes(size)) [[unlikely]] {
      last_error_ = BufferError::ReadOutOfBounds;
      return "";
    }
    std::string s(size, '\0');
    std::memcpy(s.data(), data_ + read_cursor_, size);
    read_cursor_ += size;
    return s;
  }

  template <typename Map, typename KeyFunc, typename ValueFunc>
  Map ReadMap(KeyFunc key_func, ValueFunc value_func) {
    size_t size = ReadVarInt<size_t>();
    Map map;
    for (size_t i = 0; i < size; ++i) {
      auto key = (this->*key_func)();
      auto value = (this->*value_func)();
      map[key] = value;
    }
    return map;
  }

  template <typename T, typename ValueFunc>
  std::vector<T> ReadVector(ValueFunc value_func) {
    size_t size = ReadVarInt<size_t>();
    std::vector<T> v;
    v.reserve(size);
    for (size_t i = 0; i < size; ++i) {
      v.push_back((this->*value_func)());
    }
    return v;
  }

  void WriteString(const std::string& str) {
    if (!CheckSeal()) [[unlikely]] {
      return;
    }
    size_t size = str.size();
    ReserveIncremental(size + sizeof(size));
    WriteVarInt(size);
    if (size > 0) {
      std::memcpy(data_ + write_cursor_, str.data(), size);
      write_cursor_ += size;
    }
  }

  void WriteChar(char c) { WriteInt(c); }
  void WriteUnsignedChar(unsigned char c) { WriteInt(c); }
  void WriteBool(bool b) { WriteInt(b); }
  void WriteFloat(float f) { WriteNumber(f); }
  void WriteDouble(double f) { WriteNumber(f); }

  template <Integral16Byte T>
  void WriteInt(T c) {
    WriteNumber(c);
  }

  template <Arithmetic16Byte T>
  void WriteNumber(T c) {
    if (!CheckSeal()) [[unlikely]] {
      return;
    }
    constexpr size_t size = sizeof(T);
    auto* pt = reinterpret_cast<char*>(&c);
    ReserveIncremental(size);
    if (GetSystemEndianness() == endianness_) [[likely]] {
      std::memcpy(data_ + write_cursor_, pt, size);
    } else {
      for (size_t i = 0; i < size; ++i) {
        data_[write_cursor_ + i] = pt[size - 1 - i];
      }
    }
    write_cursor_ += size;
  }

  template <size_t N>
  void WriteBitset(const std::bitset<N>& bs) {
    if (!CheckSeal()) [[unlikely]] {
      return;
    }
    constexpr size_t BYTES = (N + 7) / 8;
    uint8_t data[BYTES] = {};
    for (size_t i = 0; i < N; ++i) {
      if (bs[i]) {
        data[i / 8] |= uint8_t(1u << (i % 8));
      }
    }
    Write(data, BYTES);
  }

  template <Arithmetic8Byte T>
  void Write(std::span<const T> data) {
    if (!CheckSeal()) [[unlikely]] {
      return;
    }
    size_t bytes = sizeof(T) * data.size();
    ReserveIncremental(bytes);
    std::memcpy(data_ + write_cursor_,
                reinterpret_cast<const char*>(data.data()), bytes);
    write_cursor_ += bytes;
  }

  template <Arithmetic8Byte T>
  void Write(const T* arr, size_t size) {
    Write(std::span<const T>(arr, size));
  }

  template <Arithmetic8Byte T>
  void WriteVarInt(T c) {
    if (!CheckSeal()) [[unlikely]] {
      return;
    }
    constexpr unsigned char size = sizeof(T);
    auto* raw_data = reinterpret_cast<const char*>(&c);
    unsigned char actual_size = 0;
    for (unsigned char i = 0; i < size; ++i) {
      if (raw_data[i] != 0) {
        actual_size = i + 1;
      }
    }
    ReserveIncremental(actual_size + 1);
    WriteUnsignedChar(actual_size);
    if (GetSystemEndianness() == endianness_) [[likely]] {
      std::memcpy(data_ + write_cursor_, raw_data, actual_size);
    } else {
      for (unsigned char i = 0; i < actual_size; ++i) {
        data_[write_cursor_ + i] = raw_data[actual_size - 1 - i];
      }
    }
    write_cursor_ += actual_size;
  }

  template <typename KeyFunc, typename ValueFunc, typename Map>
  void WriteMap(Map& map, KeyFunc key_func, ValueFunc value_func) {
    if (!CheckSeal()) [[unlikely]] {
      return;
    }
    WriteVarInt(map.size());
    for (auto& kv : map) {
      (this->*key_func)(kv.first);
      (this->*value_func)(kv.second);
    }
  }

  template <typename ValueFunc, typename T>
  void WriteVector(const std::vector<T>& v, ValueFunc value_func) {
    if (!CheckSeal()) [[unlikely]] {
      return;
    }
    WriteVarInt(v.size());
    for (auto& value : v) {
      (this->*value_func)(value);
    }
  }

  std::string Dump(int width = 2, size_t wrap = 8) const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < write_cursor_; ++i) {
      if (i != 0) {
        oss << ((i % wrap == 0) ? '\n' : ' ');
      }
      oss << "0x" << std::setw(width)
          << +static_cast<uint8_t>(data_[i]);
    }
    return oss.str();
  }

  void Trim() {
    if (write_cursor_ == allocated_size_) [[unlikely]] {
      return;
    }
    if (!CheckSeal()) [[unlikely]] {
      return;
    }
    char* new_data = new (std::nothrow) char[write_cursor_];
    if (!new_data) [[unlikely]] {
      last_error_ = BufferError::CannotAllocate;
      return;
    }
    std::memcpy(new_data, data_, write_cursor_);
    delete[] data_;
    data_ = new_data;
    allocated_size_ = write_cursor_;
  }

  void Reset(bool deallocate = false) {
    if (!CheckSeal()) [[unlikely]] {
      return;
    }
    write_cursor_ = 0;
    read_cursor_ = 0;
    last_error_ = BufferError::None;
    if (deallocate) {
      allocated_size_ = 0;
      delete[] data_;
      data_ = nullptr;
    }
  }

  void set_endianness(Endianness endianness) { endianness_ = endianness; }
  const char* data() const { return data_; }
  const char* read_cursor_data() const { return data_ + read_cursor_; }
  char* data_mutable() { return data_; }
  char* read_cursor_data_mutable() { return data_ + read_cursor_; }

  [[nodiscard]] size_t write_cursor() const { return write_cursor_; }
  void set_write_cursor(size_t cursor) { write_cursor_ = cursor; }
  void set_read_cursor(size_t cursor) { read_cursor_ = cursor; }
  [[nodiscard]] size_t read_cursor() const { return read_cursor_; }
  [[nodiscard]] size_t size() const { return write_cursor_; }
  [[nodiscard]] size_t capacity() const { return allocated_size_; }

  [[nodiscard]] size_t readable_bytes() const {
    auto min_cursor = std::min(write_cursor_, read_limit_);
    if (read_cursor_ > min_cursor) {
      return 0;
    }
    return min_cursor - read_cursor_;
  }

  [[nodiscard]] size_t writable_bytes() const {
    return allocated_size_ - write_cursor_;
  }

  void SkipRead(size_t size) { read_cursor_ += size; }

  void SetReadLimit(size_t limit) {
    if (limit == 0) {
      read_limit_ = std::numeric_limits<size_t>::max();
    } else {
      read_limit_ = limit;
    }
  }

  void SkipWrite(size_t size) {
    if (sealed_.load(std::memory_order_acquire)) [[unlikely]] {
      last_error_ = BufferError::WriteAfterSeal;
      return;
    }
    ReserveIncremental(size);
    write_cursor_ += size;
  }

  /// Returns the previous error and clears it.
  [[nodiscard]] BufferError GetAndClearLastError() {
    BufferError error = last_error_;
    last_error_ = BufferError::None;
    return error;
  }

  void ReserveIncremental(size_t additional_bytes) {
    Reserve(write_cursor_ + additional_bytes);
  }

  void ReserveExact(size_t size) { Reserve(size, true); }

  void Reserve(size_t size, bool exact = false) {
    if (!CheckSeal()) [[unlikely]] {
      return;
    }
    if (!data_) [[unlikely]] {
      size_t target_size = exact ? size : size * 2;
      data_ = new (std::nothrow) char[target_size];
      if (!data_) [[unlikely]] {
        last_error_ = BufferError::CannotAllocate;
        return;
      }
      allocated_size_ = target_size;
      return;
    }
    if (allocated_size_ >= size) [[likely]] {
      return;
    }
    size_t target_size = size * 2;
    char* tmp_data = new (std::nothrow) char[target_size];
    if (!tmp_data) [[unlikely]] {
      last_error_ = BufferError::CannotAllocate;
      return;
    }
    allocated_size_ = target_size;
    std::memcpy(tmp_data, data_, write_cursor_);
    delete[] data_;
    data_ = tmp_data;
  }

  void Seal() { sealed_.store(true, std::memory_order_release); }

 private:
  [[nodiscard]] bool CheckReadableBytes(size_t required) const {
    return std::min(write_cursor_, read_limit_) >= read_cursor_ + required;
  }

  [[nodiscard]] bool CheckSeal() {
    if (sealed_.load(std::memory_order_acquire)) [[unlikely]] {
      last_error_ = BufferError::WriteAfterSeal;
      return false;
    }
    return true;
  }

  Endianness endianness_;
  size_t allocated_size_ = 0;
  size_t write_cursor_ = 0;
  size_t read_cursor_ = 0;
  size_t read_limit_ = std::numeric_limits<size_t>::max();
  char* data_ = nullptr;
  BufferError last_error_ = BufferError::None;
  std::atomic_bool sealed_{false};
};

}  // namespace urkern
