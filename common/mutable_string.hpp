// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_MUTABLE_STRING__HPP
#define ASPELL_MUTABLE_STRING__HPP

#include <string.h>

#include "parm_string.hpp"

namespace acommon {

  class MutableString {
  public:
    MutableString() : str_(0), size_(0) {}
    MutableString(char * str) : str_(str), size_(strlen(str)) {}
    MutableString(char * str, unsigned int sz) : str_(str), size_(sz) {}

    bool empty() const {
      return size_ == 0;
    }
    unsigned int size() const {
      return size_;
    }
    operator char * () const {
      return str_;
    }
    operator ParmString () const {
      return ParmString(str_, size_);
    }
    char * str () const {
      return str_;
    }
    char * begin() const {
      return str_;
    }
    char * end() const {
      return str_ + size_;
    }
  public: // but only use if really necessary
    char * str_;
    unsigned int size_;
  };

  static inline bool operator==(MutableString s1, MutableString s2)
  {
    if (s1.size() != s2.size())
      return false;
    else
      return strncmp(s1,s2, s1.size()) == 0;
  }
  static inline bool operator==(const char * s1, MutableString s2)
  {
    return strcmp(s1,s2) == 0;
  }
  static inline bool operator==(MutableString s1, const char * s2)
  {
    return strcmp(s1,s2) == 0;
  }

  static inline bool operator!=(MutableString s1, MutableString s2)
  {
    if (s1.size() != s2.size())
      return true;
    else
      return strncmp(s1,s2, s1.size()) != 0;
  }
  static inline bool operator!=(const char * s1, MutableString s2)
  {
    return strcmp(s1,s2) != 0;
  }
  static inline bool operator!=(MutableString s1, const char * s2)
  {
    return strcmp(s1,s2) != 0;
  }
}

#endif
