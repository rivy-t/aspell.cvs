// Aspell main C++ include file
// Copyright 1998-2000 by Kevin Atkinson under the terms of the LGPL.

#ifndef __aspeller_speller__
#define __aspeller_speller__

#include <vector>

#include "clone_ptr.hpp"
#include "copy_ptr.hpp"
#include "data.hpp"
#include "enumeration.hpp"
#include "speller.hpp"
#include "check_list.hpp"

using namespace acommon;

namespace acommon {
  class StringMap;
  class Config;
  class WordList;
}
// The speller class is responsible for keeping track of the
// dictionaries coming up with suggestions and the like. Its methods
// are NOT meant to be used my multiple threads and/or documents.

namespace aspeller {

  class Language;
  class SensitiveCompare;
  class Suggest;

  class SpellerImpl : public Speller
  {
  public:
    SpellerImpl(); // does not set anything up. 
    ~SpellerImpl();

    PosibErr<void> setup(Config *);

    void setup_tokenizer(Tokenizer *);

    //
    // Low level Word List Management methods
    //

  public:

    enum SpecialId {main_id, personal_id, session_id, 
		    personal_repl_id, none_id};

    typedef Enumeration<VirEnumeration<DataSet *> > WordLists;

    WordLists wordlists() const;
    int num_wordlists() const;

    bool have(const DataSet::Id &) const;
    bool have(SpecialId) const;
    LocalWordSet locate (const DataSet::Id &);
    bool attach(DataSet *, const LocalWordSetInfo * li = 0);
    bool steal(DataSet *, const LocalWordSetInfo * li = 0);
    bool detach(const DataSet::Id &);
    bool destroy(const DataSet::Id &);
  
    SpecialId check_id(const DataSet::Id &) const;
    void change_id(const DataSet::Id &, SpecialId);
  
    bool use_to_check(const DataSet::Id &) const;
    void use_to_check(const DataSet::Id &, bool);
    bool use_to_suggest(const DataSet::Id &) const;
    void use_to_suggest(const DataSet::Id &, bool);
    bool save_on_saveall(const DataSet::Id &) const;
    void save_on_saveall(const DataSet::Id &, bool);
    bool own(const DataSet::Id &) const;
    void own(const DataSet::Id &, bool);

    PosibErr<const WordList *> personal_word_list  () const;
    PosibErr<const WordList *> session_word_list   () const;
    PosibErr<const WordList *> main_word_list      () const;

    //
    // Language methods
    //
    
    char * to_lower(char *);

    const char * lang_name() const;

    const Language & lang() const {return *lang_;}

    //
    // Spelling methods
    //
  
    PosibErr<bool> check(char * word, char * word_end, /* it WILL modify word */
			 unsigned int run_together_limit,
			 CheckInfo *, GuessInfo *);

    PosibErr<bool> check(MutableString word) {
      guess_info.reset(guesses);
      return check(word.begin(), 
		   word.end(), 
		   unconditional_run_together_ ? run_together_limit_ : 0,
		   check_inf, &guess_info);
    }
    PosibErr<bool> check(ParmString word)
    {
      std::vector<char> w(word.size()+1);
      strncpy(&*w.begin(), word, w.size());
      return check(MutableString(&w.front(), w.size() - 1));
    }

    PosibErr<bool> check(const char * word) {return check(ParmString(word));}

    bool check_affix(ParmString word, CheckInfo & ci, GuessInfo * gi);

    bool check_simple(ParmString, WordEntry &);

    const CheckInfo * check_info() {
      if (check_inf[0].word)
        return check_inf;
      else if (guess_info.num > 0)
        return guesses + 1;
      else
        return 0;
    }
    
    //
    // High level Word List management methods
    //

    PosibErr<void> add_to_personal(MutableString word);
    PosibErr<void> add_to_session(MutableString word);

    PosibErr<void> save_all_word_lists();

    PosibErr<void> clear_session();

    PosibErr<const WordList *> suggest(MutableString word);
    // the suggestion list and the elements in it are only 
    // valid until the next call to suggest.

    PosibErr<void> store_replacement(MutableString mis, 
				     MutableString cor);

    PosibErr<void> store_replacement(const String & mis, const String & cor,
				     bool memory);


    

    //
    // Private Stuff (from here to the end of the class)
    //

    class DataSetCollection;
    class ConfigNotifier;

  private:
    friend class ConfigNotifier;

    CachePtr<const Language>   lang_;
    CopyPtr<SensitiveCompare>  sensitive_compare_;
    CopyPtr<DataSetCollection> wls_;
    ClonePtr<Suggest>       suggest_;
    ClonePtr<Suggest>       intr_suggest_;
    unsigned int            ignore_count;
    bool                    ignore_repl;
    bool                    unconditional_run_together_;
    bool                    run_together_specified_;
    unsigned int            run_together_limit_;
    const char *            run_together_middle_;
    unsigned int            run_together_min_;
    unsigned int            run_together_start_len_;
    String                  prev_mis_repl_;
    String                  prev_cor_repl_;

    void operator= (const SpellerImpl &other);
    SpellerImpl(const SpellerImpl &other);

  public:
    // these are public so that other classes and functions can use them, 
    // DO NOT USE

    const SensitiveCompare & sensitive_compare() const {return *sensitive_compare_;}

    const DataSetCollection & data_set_collection() const {return *wls_;}

    PosibErr<void> set_check_lang(ParmString lang, ParmString lang_dir);
  
    double distance (const char *, const char *, 
		     const char *, const char *) const;

    CheckInfo check_inf[8];
    CheckInfo guesses[8];
    GuessInfo guess_info;

    struct WSInfo {
      const BasicWordSet * ws;
      SensitiveCompare cmp;
      ConvertWord convert;
    };

    typedef Vector<WSInfo> WS;
    WS check_ws, affix_ws, suggest_ws, suggest_affix_ws;

    bool affix_info, affix_compress;

    bool use_soundslike, fast_scan, fast_lookup;

  };

  struct LookupInfo {
    SpellerImpl * sp;
    enum Mode {Word, Guess, Soundslike, AlwaysTrue} mode;
    SpellerImpl::WS::const_iterator begin;
    SpellerImpl::WS::const_iterator end;
    inline LookupInfo(SpellerImpl * s, Mode m);
    inline bool lookup (ParmString word, WordEntry & o) const;
  };

  inline LookupInfo::LookupInfo(SpellerImpl * s, Mode m) : sp(s), mode(m) {
    switch (m) { 
    case Word: 
      begin = sp->affix_ws.begin(); 
      end = sp->affix_ws.end();
      return;
    case Guess: 
      begin = sp->check_ws.begin(); 
      end = sp->check_ws.end(); 
      mode = Word; 
      return;
    case Soundslike: 
      begin = sp->suggest_affix_ws.begin(); 
      end = sp->suggest_affix_ws.end(); 
      return;
    case AlwaysTrue: 
      return; 
    }
  }
  
}

#endif
