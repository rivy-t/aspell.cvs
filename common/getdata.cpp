// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include <string.h>

#include "istream.hpp"
#include "getdata.hpp"
#include "asc_ctype.hpp"

namespace acommon {

  bool getdata_pair(IStream & in, DataPair & d, const Buffer & buf)
  {
    buf.data[0] = '\0'; // to avoid some special cases
    char * p;
    char * end;

    // get first non blank line
    do {
        p = buf.data + 1;
        end = in.getline(p, buf.size-1);
        if (end == 0) return false;
        while (*p == ' ' || *p == '\t') ++p;
    } while (*p == '#' || *p == '\0');

    // get key
    d.key.str_ = p;
    while (*p != '\0' &&
           ((*p != ' ' && *p != '\t' && *p != '#') || *(p-1) == '\\')) ++p;
    d.key.size_ = p - d.key.str_;

    // figure out if there is a value and add terminate key
    d.value.str_ = p; // in case there is no value
    d.value.size_ = 0;
    if (*p == '#' || *p == '\0') {*p = '\0'; return true;}
    *p = '\0';

    // skip any whitspace
    ++p;
    while (*p == ' ' || *p == '\t') ++p;
    if (*p == '\0' || *p == '#') {return true;}

    // get value
    d.value.str_ = p;
    while (*p != '\0' && (*p != '#' || *(p-1) == '\\')) ++p;
    
    // remove trailing white space and terminate value
    --p;
    while (*p == ' ' || *p == '\t') --p;
    if (*p == '\\' && *(p + 1) != '\0') ++p;
    ++p;
    d.value.size_ = p - d.value.str_;
    *p = '\0';

    return true;
  }

  void unescape(char * dest, const char * src)
  {
    while (*src) {
      if (*src == '\\') {
	++src;
	switch (*src) {
	case 'n': *dest = '\n'; break;
	case 'r': *dest = '\r'; break;
	case 't': *dest = '\t'; break;
	case 'f': *dest = '\f'; break;
	case 'v': *dest = '\v'; break;
	default: *dest = *src;
	}
      } else {
	*dest = *src;
      }
      ++src;
      ++dest;
    }
    *dest = '\0';
  }

  bool escape(char * dest, const char * src, size_t limit, const char * others)
  {
    const char * end = dest + limit;
    while (*src) {
      if (dest == end) return false;
      switch (*src) {
      case '\n': *dest++ = '\\'; *dest = 'n'; break;
      case '\r': *dest++ = '\\'; *dest = 'r'; break;
      case '\t': *dest++ = '\\'; *dest = 't'; break;
      case '\f': *dest++ = '\\'; *dest = 'f'; break;
      case '\v': *dest++ = '\\'; *dest = 'v'; break;
      case '\\': *dest++ = '\\'; *dest = '\\'; break;
      case '#' : *dest++ = '\\'; *dest = '#'; break;
      default:
	if (others && strchr(others, *src)) *dest++ = '\\';
	*dest = *src;
      }
      ++src;
      ++dest;
    }
    *dest = '\0';
    return true;
  }

  void to_lower(char * str)
  {
    for (; *str; str++) *str = asc_tolower(*str);
  }

  bool split(DataPair & d)
  {
    char * p   = d.value;
    char * end = p + d.value.size();
    d.key.str_ = p;
    while (p != end) {
      ++p;
      if ((*p == ' ' || *p == '\t') && *(p-1) != '\\') break;
    }
    d.key.size_ = p - d.key.str_;
    *p = 0;
    if (p != end) {
      ++p;
      while (p != end && (*p == ' ' || *p == '\t')) ++p;
    }
    d.value.str_ = p;
    d.value.size_ = end - p;
    return d.key.size_ != 0;
  }

  void init(ParmString str, DataPair & d, const Buffer & buf)
  {
    const char * s = str;
    while (*s == ' ' || *s == '\t') ++s;
    size_t l = str.size() - (s - str);
    if (l > buf.size - 1) l = buf.size - 1;
    memcpy(buf.data, s, l);
    buf.data[l] = '\0';
    d.value.str_  = buf.data;
    d.value.size_ = l;
  }


}
