// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include <string.h>

#include "asc_ctype.hpp"
#include "convert.hpp"
#include "fstream.hpp"
#include "getdata.hpp"
#include "config.hpp"
#include "errors.hpp"
#include "stack_ptr.hpp"

namespace acommon {

  typedef unsigned char  Uni8;
  typedef unsigned short Uni16;
  typedef unsigned int   Uni32;


  //////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////
  //
  // Lookups
  //
  //////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////

  //////////////////////////////////////////////////////////////////////
  //
  // ToUniLookup
  //

  class ToUniLookup 
  {
    Uni32 data[256];
    static const Uni32 npos = (Uni32)(-1);
  public:
    void reset();
    Uni32 operator[] (char key) const {return data[(unsigned char)key];}
    bool have(char key) const {return data[(unsigned char)key] != npos;}
    bool insert(char key, Uni32 value);
  };

  void ToUniLookup::reset() 
  {
    for (int i = 0; i != 256; ++i)
      data[i] = npos;
  }

  bool ToUniLookup::insert(char key, Uni32 value)
  {
    if (data[(unsigned char)key] != npos) 
      return false;
    data[(unsigned char)key] = value;
    return true;
  }

  //////////////////////////////////////////////////////////////////////
  //
  // FromUniLookup
  //

  // Assumes that the maxium number of items in the table is 256
  // Also assumes (unsigned char)i == i % 256

  // Based on the iso-8859-* character sets it is very fast, almsot all
  // lookups involving no more than 2 comparisons.
  // NO looks ups involded more than 3 comparssions.
  // Also, no division (or modules) is done whatsoever.


  struct UniItem {
    Uni32 key;
    char  value;
  };

  class FromUniLookup 
  {
  private:
    char unknown;
    static const Uni32 npos = (Uni32)(-1);
    UniItem * overflow_end;
  
    UniItem data[256*4];

    UniItem overflow[256]; // you can never be too careful;
  
  public:
    FromUniLookup(char u = '?') : unknown(u) {}
    void reset();
    inline char operator[] (Uni32 key) const;
    bool insert(Uni32 key, char value);
  };

  void FromUniLookup::reset()
  {
    for (unsigned int i = 0; i != 256*4; ++i)
      data[i].key = npos;
    overflow_end = overflow;
  }

  inline char FromUniLookup::operator[] (Uni32 k) const
  {
    const UniItem * i = data + (unsigned char)k * 4;

    if (i->key == k) return i->value;
    ++i;
    if (i->key == k) return i->value;
    ++i;
    if (i->key == k) return i->value;
    ++i;
    if (i->key == k) return i->value;
  
    if (i->key == npos) return unknown;
  
    for(i = overflow; i != overflow_end; ++i)
      if (i->key == k) return i->value;

    return unknown;
  }

  bool FromUniLookup::insert(Uni32 k, char v) 
  {
    UniItem * i = data + (unsigned char)k * 4;
    UniItem * e = i + 4;
    while (i != e && i->key != npos) {
      if (i->key == k)
	return false;
      ++i;
    }
    if (i == e) {
      for(i = overflow; i != overflow_end; ++i)
	if (i->key == k) return false;
    }
    i->key = k;
    i->value = v;
    return true;
  }

  //////////////////////////////////////////////////////////////////////
  //
  // CharLookup
  //

  class CharLookup 
  {
  private:
    int data[256];
  public:
    void reset();
    char operator[] (char key) const {return data[(unsigned char)key];}
    bool insert(char key, char value);
  };

  void CharLookup::reset() {
    for (int i = 0; i != 256; ++i) 
      data[i] = -1;
  }

  bool CharLookup::insert(char key, char value) 
  {
    if (data[(unsigned char)key] != -1)
      return false;
    data[(unsigned char)key] = value;
    return true;
  }


  //////////////////////////////////////////////////////////////////////
  //
  // read in char data
  //

  PosibErr<void> read_in_char_data (const Config & config,
				    ParmString encoding,
				    ToUniLookup & to,
				    FromUniLookup & from)
  {
    to.reset();
    from.reset();
    String file_name = config.retrieve("data-dir");
    file_name += '/';
    file_name += encoding;
    file_name += ".dat";
    FStream data;
    PosibErrBase err = data.open(file_name, "r");
    if (err.get_err()) { 
      char mesg[128];
      snprintf(mesg, 128, _("This could also mean that the file \"%s\" could not be opened for reading or does not exist."),
               file_name.c_str());
      return make_err(unknown_encoding, encoding, mesg);
    }
    unsigned int chr;
    Uni32 uni;
    int c;
    while (c = data.get(), c != EOF && c != '\n');
    while (c = data.get(), c != EOF && c != '\n');
    for (chr = 0; chr != 256; ++chr) {
      data >> uni;

      if (!data)
	return make_err(bad_file_format, file_name);

      while (c = data.get(), c != EOF && c != '\n');

      to.insert(chr, uni);
      from.insert(uni, chr);
    }
  
    return no_err;
  }

  //////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////
  //
  //  Convert
  //
  //////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////


  bool operator== (const Convert & rhs, const Convert & lhs)
  {
    return strcmp(rhs.in_code(), lhs.in_code()) == 0
      && strcmp(rhs.out_code(), lhs.out_code()) == 0;
  }

  //////////////////////////////////////////////////////////////////////
  //
  // Trivial Conversion
  //

  template <typename Chr>
  struct DecodeDirect : public Decode 
  {
    void decode(const char * in0, int size, FilterCharVector & out) const {
      const Chr * in = reinterpret_cast<const Chr *>(in0);
      if (size == -1) {
	for (;*in; ++in)
	  out.append(*in);
      } else {
	const Chr * stop = reinterpret_cast<const Chr *>(in0 +size);
	for (;in != stop; ++in)
	  out.append(*in);
      }
    }
  };

  template <typename Chr>
  struct EncodeDirect : public Encode
  {
    void encode(const FilterChar * in, const FilterChar * stop, 
		CharVector & out) const {
      for (; in != stop; ++in) {
	Chr c = in->chr;
	out.append(&c, sizeof(Chr));
      }
    }
    bool encode_direct(FilterChar *, FilterChar *) const {
      return true;
    }
  };

  template <typename Chr>
  struct ConvDirect : public DirectConv
  {
    void convert(const char * in0, int size, CharVector & out) const {
      if (size == -1) {
	const Chr * in = reinterpret_cast<const Chr *>(in0);
	for (;*in != 0; ++in)
	  out.append(in, sizeof(Chr));
      } else {
	out.append(in0, size);
      }
    }
  };

  //////////////////////////////////////////////////////////////////////
  //
  //  Lookup Conversion
  //

  struct DecodeLookup : public Decode 
  {
    ToUniLookup lookup;
    PosibErr<void> init(ParmString code, const Config & c) 
      {FromUniLookup unused;
      return read_in_char_data(c, code, lookup, unused);}
    void decode(const char * in, int size, FilterCharVector & out) const {
      if (size == -1) {
	for (;*in; ++in)
	  out.append(lookup[*in]);
      } else {
	const char * stop = in + size;
	for (;in != stop; ++in)
	  out.append(lookup[*in]);
      }
    }
  };

  struct EncodeLookup : public Encode 
  {
    FromUniLookup lookup;
    PosibErr<void> init(ParmString code, const Config & c) 
      {ToUniLookup unused;
      return read_in_char_data(c, code, unused, lookup);}
    void encode(const FilterChar * in, const FilterChar * stop, 
		CharVector & out) const {
      for (; in != stop; ++in) {
	out.append(lookup[*in]);
      }
    }
    bool encode_direct(FilterChar * in, FilterChar * stop) const {
      for (; in != stop; ++in)
	*in = lookup[*in];
      return true;
    }
  };

  //////////////////////////////////////////////////////////////////////
  //
  //  UTF8
  //
  
#define get_check_next \
  if (in == stop) goto error;          \
  c = *in;                             \
  if ((c & 0xC0) != 0x80) goto error;  \
  ++in;                                \
  u <<= 6;                             \
  u |= c & 0x3F;                       \
  ++w;

  static inline FilterChar from_utf8 (const char * & in, const char * stop)
  {
    Uni32 u = (Uni32)(-1);
    FilterChar::Width w = 1;

    // the first char is guaranteed not to be off the end
    char c = *in;
    ++in;

    while (in != stop && (c & 0xC0) == 0x80) {c = *in; ++in; ++w;}
    if ((c & 0x80) == 0x00) { // 1-byte wide
      u = c;
    } else if ((c & 0xE0) == 0xC0) { // 2-byte wide
      u  = c & 0x1F;
      get_check_next;
    } else if ((c & 0xF0) == 0xE0) { // 3-byte wide
      u  = c & 0x0F;
      get_check_next;
      get_check_next;
    } else if ((c & 0xF8) == 0xF0) { // 4-byte wide
      u  = c & 0x07;
      get_check_next;
      get_check_next;
      get_check_next;
    } else if ((c & 0xFC) == 0xF8) { // 5-byte wide
      u  = c & 0x03;
      get_check_next;
      get_check_next;
      get_check_next;
      get_check_next;
    } else if ((c & 0xFE) == 0xFC) { // 6-byte wide
      u  = c & 0x03;
      get_check_next;
      get_check_next;
      get_check_next;
      get_check_next;
      get_check_next;
    } else {
      goto error;
    }

    return FilterChar(u, w);
  error:
    return FilterChar('?', w);
  }
  
  static inline void to_utf8 (FilterChar in, CharVector & out)
  {
    FilterChar::Chr c = in;
    
    if (c < 0x80) {
      out.append(c);
    }
    else if (c < 0x800) {
      out.append(0xC0 | c>>6);
      out.append(0x80 | c & 0x3F);
    }
    else if (c < 0x10000) {
      out.append(0xE0 | c>>12);
      out.append(0x80 | c>>6 & 0x3F);
      out.append(0x80 | c & 0x3F);
    }
    else if (c < 0x200000) {
      out.append(0xF0 | c>>18);
      out.append(0x80 | c>>12 & 0x3F);
      out.append(0x80 | c>>6 & 0x3F);
      out.append(0x80 | c & 0x3F);
    }
    else if (c < 0x4000000) {
      out.append(0xF8 | c>>24);
      out.append(0x80 | c>>18 & 0x3F);
      out.append(0x80 | c>>12 & 0x3F);
      out.append(0x80 | c>>6 & 0x3F);
      out.append(0x80 | c & 0x3F);
    }
    else if (c < 0x80000000) {
      out.append(0xF8 | c>>30);
      out.append(0x80 | c>>24 & 0x3F);
      out.append(0x80 | c>>18 & 0x3F);
      out.append(0x80 | c>>12 & 0x3F);
      out.append(0x80 | c>>6 & 0x3F);
      out.append(0x80 | c & 0x3F);
    }
  }
  
  struct DecodeUtf8 : public Decode 
  {
    ToUniLookup lookup;
    void decode(const char * in, int size, FilterCharVector & out) const {
      const char * stop = in + size; // this is OK even if size == -1
      while (*in && in != stop) {
	out.append(from_utf8(in, stop));
      }
    }
  };

  struct EncodeUtf8 : public Encode 
  {
    FromUniLookup lookup;
    void encode(const FilterChar * in, const FilterChar * stop, 
		CharVector & out) const {
      for (; in != stop; ++in) {
	to_utf8(*in, out);
      }
    }
  };

  
  //////////////////////////////////////////////////////////////////////
  //
  // new_aspell_convert
  //

  void Convert::generic_convert(const char * in, int size, CharVector & out)
  {
    buf_.clear();
    decode_->decode(in, size, buf_);
    buf_.append(0);
    FilterChar * start = buf_.pbegin();
    FilterChar * stop = buf_.pend();
    if (!filter.empty()) {
      filter.decode(start, stop);
      filter.process(start, stop);
      filter.encode(start, stop);
    }
    encode_->encode(start, stop, out);
  }

  const char * fix_encoding_str(ParmString enc, String & buf)
  {
    buf.reserve(enc.size() + 1);
    for (size_t i = 0; i != enc.size(); ++i)
      buf.push_back(asc_tolower(enc[i]));

    if (strncmp(buf.c_str(), "iso8859", 7) == 0)
      buf.insert(3, 1, '-'); // For backwards compatibility
    
    if (buf == "ascii")
      return "iso-8859-1";
    else if (buf == "machine unsigned 16")
      return "utf-16";
    else if (buf == "machine unsigned 32")
      return "utf-32";
    else
      return buf.c_str();
  }

  bool is_ascii_enc(ParmString enc)
  {
    return (enc == "ASCII" || enc == "ascii" 
            || enc == "ANSI_X3.4-1968");
  }


  PosibErr<Convert *> internal_new_convert(const Config & c,
                                           ParmString in, 
                                           ParmString out,
                                           bool if_needed)
  {
    String in_s;
    in = fix_encoding_str(in, in_s);

    String out_s;
    out = fix_encoding_str(out, out_s); 

    if (if_needed && in == out) return 0;

    StackPtr<Convert> conv(new Convert);
    RET_ON_ERR(conv->init(c, in, out));
    return conv.release();
    
  }

  PosibErr<void> Convert::init(const Config & c, ParmString in, ParmString out)
  {
    in_code_ = in;
    out_code_ = out;
    
    if (in_code_ == "iso-8859-1")
      decode_ = new DecodeDirect<Uni8>;
    else if (in_code_ == "utf-16")
      decode_ = new DecodeDirect<Uni16>;
    else if (in_code_ == "utf-32")
      decode_ = new DecodeDirect<Uni32>;
    else if (in_code_ == "utf-8")
      decode_ = new DecodeUtf8;
    else
      decode_ = new DecodeLookup;
    RET_ON_ERR(decode_->init(in_code_, c));
    
    if (out_code_ == "iso-8859-1")
      encode_ = new EncodeDirect<Uni8>;
    else if (out_code_ == "utf-16")
      encode_ = new EncodeDirect<Uni16>;
    else if (out_code_ == "utf-32")
      encode_ = new EncodeDirect<Uni32>;
    else if (out_code_ == "utf-8")
      encode_ = new EncodeUtf8;
    else
      encode_ = new EncodeLookup;
    RET_ON_ERR(encode_->init(out_code_, c));

    if (in_code_ == out_code_) {
      if (in_code_ == "utf-16")
	conv_ = new ConvDirect<Uni16>;
      else if (in_code_ == "utf-32")
	conv_ = new ConvDirect<Uni32>;
      else
	conv_ = new ConvDirect<char>;
    }

    if (conv_)
      RET_ON_ERR(conv_->init(decode_, encode_, c));

    return no_err;
  }

}
