// This file is part of The New Aspell
// Copyright (C) 2001-2003 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_CHAR_VECTOR__HPP
#define ASPELL_CHAR_VECTOR__HPP

#include "vector.hpp"
#include "ostream.hpp"

namespace acommon
{
  class CharVector : public Vector<char>, public OStream
  {
  public:
    void append (char c) {Vector<char>::append(c);}
    void append (const void * d, unsigned int size) {
      Vector<char>::append(static_cast<const char *>(d), size);}
    void append (const void * d, const void * e) {
      Vector<char>::append(static_cast<const char *>(d), 
                           static_cast<const char *>(e));}
    

    void write (char c) {CharVector::append(c);}
    void write (ParmString str) {CharVector::append(str, str.size());}
    void write (const void * str, unsigned int size) {CharVector::append(str, size);}

    unsigned short & at16(unsigned int pos) 
      {return reinterpret_cast<unsigned short &>(operator[](pos));}
    unsigned int   & at32(unsigned int pos) 
      {return reinterpret_cast<unsigned int &>(operator[](pos));}

    //FIXME: Make this more efficent by rewriting the implemenation
    //       to work with raw memory rather than using vector<char>
    template <typename Itr>
    void replace(iterator start, iterator stop, Itr rstart, Itr rstop) {
      iterator i = erase(start,stop);
      insert(i, rstart, rstop);
    }

    CharVector & operator << (ParmString str) {
      append(str, str.size());
      return *this;
    }

    CharVector & operator << (char c) {
      append(c);
      return *this;
    }
    
  };
}

#endif
