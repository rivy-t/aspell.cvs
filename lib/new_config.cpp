// This file is part of The New Aspell
// Copyright (C) 2002 by Kevin Atkinson under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.

#include <string.h>

#include "config.hpp"
#include "errors.hpp"
#include "filter.hpp"
#include <stdio.h>

#define DEBUG {fprintf(stderr,"File: %s(%i)\n",__FILE__,__LINE__);}

namespace acommon {
  
  extern void activate_dynamic_filteroptions(Config *config);
  extern void activate_filter_modes(Config *config);


  Config * new_config() {

    Config * config = new_basic_config();

    activate_filter_modes(config);
    activate_dynamic_filteroptions(config);
    return config;
  }

}
