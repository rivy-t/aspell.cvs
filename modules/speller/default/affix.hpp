// This file is part of The New Aspell
// Copyright (C) 2002 by Kevin Atkinson under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.
//
// Copyright 2002 Kevin B. Hendricks, Stratford, Ontario, Canada And
// Contributors.  All rights reserved. See the file affix.license for
// details.

#ifndef ASPELL_AFFIX__HPP
#define ASPELL_AFFIX__HPP

#include "posib_err.hpp"
#include "wordinfo.hpp"
#include "fstream.hpp"
#include "parm_string.hpp"
#include "char_vector.hpp"
#include "objstack.hpp"

#define SETSIZE         256
#define MAXAFFIXES      256
#define MAXWORDLEN      100
#define XPRODUCT        (1 << 0)

#define MAXLNLEN        1024

#define TESTAFF( a , b) strchr(a, b)

namespace acommon {
  class Config;
  struct CheckInfo;
}

namespace aspeller {

  using namespace acommon;

  class Language;

  class SpellerImpl;
  using acommon::CheckInfo;
  struct GuessInfo;

  struct CheckList;
  CheckList * new_check_list();
  void delete_check_list(CheckList *);
  CheckInfo * check_list_data(CheckList *);

  struct LookupInfo;
  struct AffEntry;
  struct PfxEntry;
  struct SfxEntry;

  struct WordAff
  {
    String word;
    String af;
  };

  class AffixMgr
  {
    const Language * lang;

    PfxEntry *          pStart[SETSIZE];
    SfxEntry *          sStart[SETSIZE];
    PfxEntry *          pFlag[SETSIZE];
    SfxEntry *          sFlag[SETSIZE];

    const char *        encoding;
    //const char *        compound;
    //int                 cpdmin;

    ObjStack strings;
    void *   data_;

    const char * affix_file;

  public:
 
    AffixMgr(const Language * l) : lang(l), data_(0) {}
    ~AffixMgr();

    PosibErr<void> setup(ParmString affpath);

    bool affix_check(const LookupInfo &, ParmString, CheckInfo &, GuessInfo *) const;
    bool prefix_check(const LookupInfo &, ParmString, CheckInfo &, GuessInfo *) const;
    bool suffix_check(const LookupInfo &, ParmString, CheckInfo &, GuessInfo *,
                      int sfxopts, AffEntry* ppfx) const;

    void get_word(String & word, const CheckInfo &) const;

    void  munch(ParmString word, CheckList *) const;
    void  expand(ParmString word, ParmString affixes, CheckList *) const;
    // expand enough so the affixes does not effect the first limit
    // characters
    int  expand(ParmString word, ParmString af, int limit, WordAff * l) const;

  private:
    PosibErr<void> parse_file(const char * affpath);

    void encodeit(AffEntry * ptr, char * cs);
    PosibErr<void> build_pfxlist(PfxEntry* pfxptr);
    PosibErr<void> build_sfxlist(SfxEntry* sfxptr);
    PosibErr<void> process_pfx_order();
    PosibErr<void> process_sfx_order();
  };

  PosibErr<AffixMgr *> new_affix_mgr(ParmString name, 
                                     const Language * lang);

}

#endif

