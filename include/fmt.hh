
#pragma once
#include <concepts>
#include <array>
#include <algorithm>
#include <limits>
#include "stdint.h"
#include "stddef.h"

#include "to_string.hh"
#include "spec.hh"

namespace reisfmt {

template <typename T>
concept Writeable = requires(T t, const char *buf, size_t n) {
  { t.write(buf, n) } -> std::same_as<void>;
};

// Foward declaration.
template <Writeable T>
class Fmt;

template <Writeable T, typename U>
struct Formatter;

template <Writeable T, typename U>
  requires std::integral<U>
struct Formatter<T, U> {
  static void print(Fmt<T> &fmt, U obj) {
    size_t len = 0;
    switch (fmt.spec.radix_) {
      case Spec::Radix::Bin:
        len = to_bit_str(fmt.buf, obj);
        break;
      case Spec::Radix::Hex:
        len = to_hex_str(fmt.buf, obj);
        break;
      case Spec::Radix::Dec:
      default:
        len = to_str(fmt.buf, obj);
        break;
    };

    StrIterator it(fmt.buf.data(), len);
    Formatter<T, StrIterator>::print(fmt, it);
  }
};

template <Writeable T>
struct Formatter<T, StrIterator> {
  static inline void print(Fmt<T> &fmt, StrIterator &it) {
    if (fmt.spec.has_prefix_) {
      fmt.device.write(fmt.spec.prefix_, fmt.spec.prefix_size_);
      fmt.spec.width_ -= fmt.spec.prefix_size_;
    }
    if ((fmt.spec.align_ == Spec::Align::Center || fmt.spec.align_ == Spec::Align::Right) &&
        fmt.spec.width_ > it.size_) {
      int diff = fmt.spec.width_ - it.size_;
      diff     = fmt.spec.align_ == Spec::Align::Center ? diff / 2 : diff;
      fmt.spec.width_ -= diff;
      while (diff-- > 0) {
        fmt.device.write(&fmt.spec.filler_, sizeof(fmt.spec.filler_));
      }
    }

    fmt.device.write(it.head_, it.size_);

    // align_ == Spec::Align::Left || Spec::Align::Center
    if (fmt.spec.width_ > it.size_) {
      while (fmt.spec.width_-- > it.size_) {
        fmt.device.write(&fmt.spec.filler_, sizeof(fmt.spec.filler_));
      }
    }
  }
};

template <Writeable T>
struct Formatter<T, const char *> {
  static inline void print(Fmt<T> &fmt, const char *str) {
    StrIterator it(str);
    Formatter<T, StrIterator>::print(fmt, it);
  }
};

template <Writeable T>
struct Formatter<T, std::basic_string<char>> {
  static inline void print(Fmt<T> &fmt, const std::string &str) {
    StrIterator it(str.c_str(), str.length());
    Formatter<T, StrIterator>::print(fmt, it);
  }
};

template <Writeable T>
class Fmt {
 public:
  T &device;
  std::array<char, sizeof(uint64_t) * 8> buf;
  StrIterator *it_;
  Spec spec;

  Fmt(T &device) : device(device) {};

 private:
  // Base case to stop the recursion.
  void format() {
    if (it_ && it_->peek()) {
      device.write(it_->head_, it_->size_);
      it_ = nullptr;
    }
  }

  template <typename U, typename... Args>
  void format(U first, Args... rest) {
    if (it_ == nullptr || it_->size_ == 0) {
      return;
    }
    auto start = it_->head_;

    // Find format start guard
    auto end = it_->find('{');
    device.write(start, end - start - int(it_->size_ > 0));
    if (it_->size_ > 0) {
      spec.from_str(*it_);

      // The formatter can be extented for custom types, se the context is saved to allow the custom formatter to
      // recursively call this function and change the context.
      auto it = it_;
      Formatter<T, U>::print(*this, first);
      it_ = it;

      it_->find('}');
      format(rest...);
    }
  }

 public:
  template <typename... Args>
  void println(const char *fmt, Args... args) {
    print(fmt, args...);
    device.write("\n", 1);
  }

  template <typename... Args>
  void print(const char *fmt, Args... args) {
    if (fmt) {
      StrIterator it(fmt);
      it_ = &it;
      format(args...);
      it_ = nullptr;
    }
  }
};

};  // namespace reisfmt
