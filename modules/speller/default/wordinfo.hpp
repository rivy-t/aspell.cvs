// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#ifndef __aspeller_wordinfo__
#define __aspeller_wordinfo__

#include <assert.h>
#include "string.hpp"

namespace acommon {
  class OStream;
  class Convert;
}

namespace aspeller {

  // WordInfo

  typedef unsigned int WordInfo; // 4 bits

  enum CasePattern {Other, FirstUpper, AllLower, AllUpper};
  //   Other      00
  //   FirstUpper 01
  //   AllLower   10
  //   AllUpper   11
  // First bit : is upper
  // Second bit: uniform case

  static const WordInfo CASE_PATTERN = 3;
  static const WordInfo ALL_PLAIN    = (1 << 2);
  static const WordInfo ALL_CLEAN    = (1 << 3);

  using namespace acommon;

  class Language;
  struct ConvertWord;

  // WordEntry is an entry in the dictionary.  Both word and aff
  // should point to a string that will stay in memory as long as the
  // dictionary does unless under very special circumstances.  Thus
  // free_ should generally be null.
  struct WordEntry
  {
    const char * word;
    const char * aff;
    const char * catg;
    void (* adv_)(WordEntry *);
    void (* free_)(WordEntry *);
    void * intr[2];
    unsigned word_size;
    enum What {Other, Word, Soundslike, Clean, Misspelled} what;
    WordInfo word_info;
    int frequency; // 0 .. 255
    // if type is Word than aff will be defined, otherwise it won't
    bool at_end() {return !word;}
    bool adv() {if (adv_) {adv_(this); return true;} word = 0; return false;}
    operator bool () const {return word != 0;}
    OStream & write(OStream & o, const Language & l,
		    const ConvertWord &, Convert * c = 0) const;
    WordEntry() {memset(this, 0, sizeof(WordEntry));}
    void clear() {if (free_) free_(this); memset(this, 0, sizeof(WordEntry));}
    ~WordEntry() {if (free_) free_(this);}
  };
}

#endif
