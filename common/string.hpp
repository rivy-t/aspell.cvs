// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_STRING__HPP
#define ASPELL_STRING__HPP

#include <string.h>
#include <string>

#include "hash_fun.hpp"
#include "parm_string.hpp"
#include "mutable_string.hpp"
#include "istream.hpp"
#include "ostream.hpp"

namespace acommon {

  template <typename Ret> class PosibErr;
  
  class String : public std::string, public OStream
  {
  public:
    String() : std::string() {}
    String(const char * s) : std::string(s) {}
    String(const char * s, unsigned int size) : std::string(s, size) {}
    String(ParmString s) : std::string(s) {}
    String(MutableString s) : std::string(s.str, s.size) {}
    String(const std::string & s) : std::string(s) {}
    String(const String & other) : std::string(other) {}
#ifndef __SUNPRO_CC
    // This causes a conflict with the copy constructor on Suns comp
    inline String(const PosibErr<String> & other);
#endif
    String & operator= (const char * s) {
      std::string::operator= (s);
      return *this;
    }
    inline String & operator= (const PosibErr<const char *> & s);
    String & operator= (ParmString s) {
      std::string::operator= (s);
      return *this;
    }
    String & operator= (MutableString s) {
      std::string::assign(s.str, s.size);
      return *this;
    }
    String & operator= (const std::string & s) {
      std::string::operator= (s);
      return *this;
    }
    String & operator= (const String & other) {
      std::string::operator= (other);
      return *this;
    }
    /*inline*/ String & operator= (const PosibErr<String> & s);

    ~String() {}

    void clear() {*this = "";}

    void write (char c);
    void write (ParmString);
    void write (const void *, unsigned int);
  };

  inline String operator+ (ParmString rhs, ParmString lhs)
  {
    String tmp;
    tmp.reserve(rhs.size() + lhs.size());
    tmp += rhs;
    tmp += lhs;
    return tmp;
  }

  inline ParmString::ParmString(const String & s) : str_(s.c_str()), size_(s.size()) {}


  class StringIStream : public IStream {
    const char * in_str;
    char         delem;
  public:
    StringIStream(ParmString s, char d = ';')
      : IStream(d), in_str(s) {}
    bool getline(String & str, char c);
    char * getline(char *, size_t, char c);
    bool read(void * data, unsigned int size);
  };

  template <> struct hash<String> : public HashString<String> {};


}
#endif
