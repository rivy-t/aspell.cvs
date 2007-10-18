/* This file is part of The New Aspell
 * Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#include "speller.hpp"
#include "convert.hpp"
#include "clone_ptr.hpp"
#include "config.hpp"

namespace aspell {

  void IntrCheckInfo::get_suf(char * buf) const {
    if (inner_suf_add_len - outer_suf_strip_len > 0) {
      memcpy(buf, inner_suf_add, inner_suf_add_len);
      if (outer_suf_add_len > 0) {
        buf += inner_suf_add_len - outer_suf_strip_len;
        memcpy(buf, outer_suf_add, outer_suf_add_len);
      }
    } else if (outer_suf_add_len > 0) {
      memcpy(buf, outer_suf_add, outer_suf_add_len);
    }
  }

  Speller::Speller(SpellerLtHandle h) : lt_handle_(h) {}

  Speller::~Speller() {}
}

