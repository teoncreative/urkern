//
//    Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include <cctype>
#include <string>

namespace urkern {

// Natural sort comparator: treats embedded numbers as integers.
// "ship_6_2" < "ship_6_10", "frame_1" < "frame_2" < "frame_10"
inline bool NaturalLess(const std::string& a, const std::string& b) {
  size_t i = 0;
  size_t j = 0;

  while (i < a.size() && j < b.size()) {
    if (std::isdigit(static_cast<unsigned char>(a[i])) &&
        std::isdigit(static_cast<unsigned char>(b[j]))) {
      size_t a_start = i;
      size_t b_start = j;
      while (i < a.size() && std::isdigit(static_cast<unsigned char>(a[i]))) {
        i++;
      }
      while (j < b.size() && std::isdigit(static_cast<unsigned char>(b[j]))) {
        j++;
      }

      size_t a_len = i - a_start;
      size_t b_len = j - b_start;

      if (a_len != b_len) {
        return a_len < b_len;
      }
      int cmp = a.compare(a_start, a_len, b, b_start, b_len);
      if (cmp != 0) {
        return cmp < 0;
      }
    } else {
      char ca =
          static_cast<char>(std::tolower(static_cast<unsigned char>(a[i])));
      char cb =
          static_cast<char>(std::tolower(static_cast<unsigned char>(b[j])));
      if (ca != cb) {
        return ca < cb;
      }
      i++;
      j++;
    }
  }

  return a.size() < b.size();
}

struct NaturalSortCompare {
  bool operator()(const std::string& a, const std::string& b) const {
    return NaturalLess(a, b);
  }
};

}  // namespace urkern
