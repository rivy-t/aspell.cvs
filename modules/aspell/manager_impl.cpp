// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#include <stdlib.h>

#include "amanager.hpp"
#include "clone_ptr-t.hpp"
#include "config.hpp"
#include "copy_ptr-t.hpp"
#include "data.hpp"
#include "data_id.hpp"
#include "errors.hpp"
#include "language.hpp"
#include "manager_impl.hpp"
#include "string_list.hpp"
#include "suggest.hpp"

namespace aspell {
  //
  // data_access functions
  //

  const char * ManagerImpl::lang_name() const {
    return lang_->name();
  }

  //////////////////////////////////////////////////////////////////////
  //
  // Spell check methods
  //

  PosibErr<void> ManagerImpl::add_to_personal(MutableString word) {
    DataSetCollection::Iterator i = wls_->locate(personal_id);
    if (i == wls_->end()) return no_err;
    return static_cast<WritableWordSet *>(i->data_set)->add(word.str());
  }
  
  PosibErr<void> ManagerImpl::add_to_session(MutableString word) {
    DataSetCollection::Iterator i = wls_->locate(session_id);
    if (i == wls_->end()) return no_err;
    return static_cast<WritableWordSet *>(i->data_set)->add(word.str());
  }

  PosibErr<void> ManagerImpl::clear_session() {
    DataSetCollection::Iterator i = wls_->locate(session_id);
    if (i == wls_->end()) return no_err;
    return static_cast<WritableWordSet *>(i->data_set)->clear();
  }

  PosibErr<void> ManagerImpl::store_replacement(MutableString mis, 
						MutableString cor)
  {
    return store_replacement(mis.str(),cor.str(), true);
  }


  PosibErr<void> ManagerImpl::store_replacement(const String & mis, 
						const String & cor, 
						bool memory) 
  {
    if (ignore_repl) return no_err;
    DataSetCollection::Iterator i = wls_->locate(personal_repl_id);
    if (i == wls_->end()) return no_err;
    String::size_type pos;
    Enumeration<StringEnumeration> sugels 
      = intr_suggest_->suggest(mis.c_str()).elements();
    const char * first_word = sugels.next();
    const char * w1;
    const char * w2 = 0;
    if (pos = cor.find(' '), pos == String::npos 
	? (w1 =check_simple(cor).word) != 0
	: ((w1 = check_simple((String)cor.substr(0,pos)).word) != 0
	   && (w2 = check_simple((String)cor.substr(pos+1)).word) != 0) ) { 
      // cor is a correct spelling
      String cor_orignal_casing(w1);
      if (w2 != 0) {
	cor_orignal_casing += cor[pos];
	cor_orignal_casing += w2;
      }
      if (first_word == 0 || cor != first_word) {
	static_cast<WritableReplacementSet *>(i->data_set)
	  ->add(to_lower(lang(), mis), 
		cor_orignal_casing);
      }
      
      if (memory && prev_cor_repl_ == mis) 
	store_replacement(prev_mis_repl_, cor, false);
      
    } else { // cor is not a correct spelling
      
      if (memory) {
	if (prev_cor_repl_ != mis)
	  prev_mis_repl_ = mis;
	prev_cor_repl_ = cor;
      }
    }
    return no_err;
  }

  //
  // simple functions
  //

  PosibErr<const WordList *> ManagerImpl::suggest(MutableString word) 
  {
    return &suggest_->suggest(word.str());
  }
  
  ManagerImpl::SpecialId ManagerImpl::check_id(const DataSet::Id & wl) const {
    return wls_->locate(wl)->special_id;
  }

  bool ManagerImpl::use_to_check(const DataSet::Id & wl) const 
  {
    return wls_->locate(wl)->use_to_check;
  }

  void ManagerImpl::use_to_check(const DataSet::Id & wl, bool v) {
    wls_->locate(wl)->use_to_check = v;
  }

  bool ManagerImpl::use_to_suggest(const DataSet::Id & wl) const {
    return wls_->locate(wl)->use_to_suggest;
  }

  void ManagerImpl::use_to_suggest(const DataSet::Id & wl, bool v) {
    wls_->locate(wl)->use_to_suggest = v;
  }

  bool ManagerImpl::save_on_saveall(const DataSet::Id & wl) const {
    return wls_->locate(wl)->save_on_saveall;
  }

  void ManagerImpl::save_on_saveall(const DataSet::Id & wl, bool v) {
    wls_->locate(wl)->save_on_saveall = v;
  }

  bool ManagerImpl::own(const DataSet::Id & wl) const {
    return wls_->locate(wl)->own;
  }

  void ManagerImpl::own(const DataSet::Id & wl, bool v) {
    wls_->locate(wl)->own = v;
  }

  BasicWordInfo ManagerImpl::check_simple (ParmString w) {
    const char * x = w;
    BasicWordInfo w0;
    while (*x != '\0' && (x-w) < static_cast<int>(ignore_count)) ++x;
    if (*x == '\0') return w.str();
    DataSetCollection::ConstIterator i   = wls_->begin();
    DataSetCollection::ConstIterator end = wls_->end();
    for (; i != end; ++i) {
      if  (i->use_to_check && 
	   i->data_set->basic_type == DataSet::basic_word_set &&
	   (w0 = static_cast<const BasicWordSet *>(i->data_set)
	    ->lookup(w,i->local_info.compare))
	   )
	return w0;
    }
    return 0;
  };

  PosibErr<bool> ManagerImpl::check(char * word, char * word_end, /* it WILL modify word */
				    unsigned int run_together_limit,
				    CompoundInfo::Position pos,
				    SingleWordInfo * words)
  {
    assert(run_together_limit <= 8); // otherwise it will go above the 
                                     // bounds of the word array
    words[0].clear();
    BasicWordInfo w = check_simple(word);
    if (w) {
      if (pos == CompoundInfo::Orig) {
	words[0] = w.word;
	words[1].clear();
	return true;
      }
      bool check_if_valid = !(unconditional_run_together_ 
			      && strlen(word) >= run_together_min_);
      if (!check_if_valid || w.compound.compatible(pos)) { 
	words[0] = w.word;
	words[1].clear();
	return true;
      } else {
	return false;
      }
    }
    
    if (run_together_limit <= 1 
	|| (!unconditional_run_together_ && !run_together_specified_))
      return false;
    for (char * i = word + run_together_start_len_; 
	 i <= word_end - run_together_start_len_;
	 ++i) 
      {
	char t = *i;
	*i = '\0';
	BasicWordInfo s = check_simple(word);
	*i = t;
	if (!s) continue;
	CompoundInfo c = s.compound;
	CompoundInfo::Position end_pos = new_position(pos, CompoundInfo::End);
	char m = run_together_middle_[c.mid_char()];
	//
	// FIXME: Deal with casing of the middle character properly
	//        if case insentate than it can be anything
	//        otherwise it should match the case of previous
	//        letter
	//
	bool check_if_valid = !(unconditional_run_together_ 
				&& i - word >= static_cast<int>(run_together_min_));
	if (check_if_valid) {
	  CompoundInfo::Position beg_pos = new_position(pos, CompoundInfo::Beg);
	  if (!c.compatible(beg_pos)) 
	    continue;
	  if (c.mid_required() && *i != m)
	    continue;
	}
	words[0].set(s.word, *i == m ? m : '\0');
	words[1].clear();
	if ((!check_if_valid || !c.mid_required()) /* if check then !s.mid_required() */
	    && check(i, word_end, run_together_limit - 1, end_pos, words + 1))
	  return true;
	if ((check_if_valid ? *i == m : strchr(run_together_middle_, *i) != 0) 
	    && word_end - (i + 1) >= static_cast<int>(run_together_min_)) {
	  if (check(i+1, word_end, run_together_limit - 1, end_pos, words + 1))
	    return true;
	  else // already checked word (i+1) so no need to check it again
	    ++i;
	}
      }
    words[0].clear();
    return false;
  }
  

  //////////////////////////////////////////////////////////////////////
  //
  // Word list managment methods
  //
  
  PosibErr<void> ManagerImpl::save_all_word_lists() {
    DataSetCollection::Iterator i   = wls_->begin();
    DataSetCollection::Iterator end = wls_->end();
    WritableDataSet * wl;
    for (; i != end; ++i) {
      if  (i->save_on_saveall && 
	   (wl = dynamic_cast<WritableDataSet *>(i->data_set)))
	RET_ON_ERR(wl->synchronize());
    }
    return no_err;
  }

  int ManagerImpl::num_wordlists() const {
    return wls_->wordlists_.size();
  }

  ManagerImpl::WordLists ManagerImpl::wordlists() const {
    return WordLists(MakeVirEnumeration<DataSetCollection::Parms>
		     (wls_->begin(), DataSetCollection::Parms(wls_->end())));
  }

  bool ManagerImpl::have(const DataSet::Id &to_find) const {
    return wls_->locate(to_find) != wls_->end();
  }

  LocalWordSet ManagerImpl::locate(const DataSet::Id &to_find) {
    DataSetCollection::Iterator i = wls_->locate(to_find);
    LocalWordSet ws;
    if (i == wls_->end()) {
      return LocalWordSet();
    } else {
      return LocalWordSet(static_cast<LoadableDataSet *>(i->data_set), 
			  i->local_info);
    }
    return ws;
  }

  bool ManagerImpl::have(ManagerImpl::SpecialId to_find) const {
    return wls_->locate(to_find) != wls_->end();
  }

  PosibErr<const WordList *> ManagerImpl::personal_word_list() const {
    return 
      static_cast<const WordList *>
      (static_cast<const BasicWordSet *>
       (wls_->locate(personal_id)->data_set));
  }

  PosibErr<const WordList *> ManagerImpl::session_word_list() const {
    return 
      static_cast<const WordList *>
      (static_cast<const BasicWordSet *>
       (wls_->locate(session_id)->data_set));
  }

  bool ManagerImpl::attach(DataSet * w, const LocalWordSetInfo * li) {
    DataSetCollection::Iterator i = wls_->locate(w);
    if (i != wls_->end()) {
      return false;
    } else {
      if (!lang_) 
	{
	  lang_.reset(new Language(*w->lang()));
	}
      w->attach(*lang_);
      DataSetCollection::Item wc(w);
      wc.set_sensible_defaults();
      if (li == 0) {
	wc.local_info.set(lang_, config_);
      } else {
	wc.local_info = *li;
	wc.local_info.set_language(lang_);
      }
      wls_->wordlists_.push_back(wc);
      return true;
    }
  }

  bool ManagerImpl::steal(DataSet * w, const LocalWordSetInfo * li) {
    bool ret = attach(w,li);
    own(w, true);
    return ret;
  }

  bool ManagerImpl::detach(const DataSet::Id &w) {
    DataSetCollection::Iterator to_del = wls_->locate(w);
    if (to_del == wls_->wordlists_.end()) return false;
    to_del->data_set->detach();
    wls_->wordlists_.erase(to_del);
    return true;
  }  

  bool ManagerImpl::destroy(const DataSet::Id & w) {
    DataSetCollection::Iterator to_del = wls_->locate(w);
    if (to_del == wls_->wordlists_.end()) return false;
    assert(to_del->own);
    delete to_del->data_set;
    wls_->wordlists_.erase(to_del);
    return true;
  }

  void ManagerImpl::change_id(const DataSet::Id & w , SpecialId id) {
    DataSetCollection::Iterator to_change = wls_->locate(w);

    assert(to_change != wls_->end());

    assert (id == none_id || !have(id));
    
    switch (id) {
    case main_id:
      if (dynamic_cast<BasicWordSet *>(to_change->data_set)) {

	to_change->use_to_check    = true;
	to_change->use_to_suggest  = true;
	to_change->save_on_saveall = false;

      } else if (dynamic_cast<BasicMultiSet *>(to_change->data_set)) {
	
	to_change->use_to_check    = false;
	to_change->use_to_suggest  = false;
	to_change->save_on_saveall = false;
	
      } else {
	
	abort();
	
      }
      break;
    case personal_id:
      assert(dynamic_cast<WritableWordSet *>(to_change->data_set));
      to_change->use_to_check = true;
      to_change->use_to_suggest = true;
      to_change->save_on_saveall = true;
      break;
    case session_id:
      assert(dynamic_cast<WritableWordSet *>(to_change->data_set));
      to_change->use_to_check = true;
      to_change->use_to_suggest = true;
      to_change->save_on_saveall = false;
      break;
    case personal_repl_id:
      assert (dynamic_cast<BasicReplacementSet *>(to_change->data_set));
      to_change->use_to_check = false;
      to_change->use_to_suggest = true;
      to_change->save_on_saveall = config_->retrieve_bool("save-repl");
      break;
    case none_id:
      break;
    }
    to_change->special_id = id;
  }

  //////////////////////////////////////////////////////////////////////
  //
  // Config Notifier
  //

  struct UpdateMember {
    const char * name;
    enum Type {String, Int, Bool, Add, Rem, RemAll};
    Type type;
    union Fun {
      typedef void (*WithStr )(ManagerImpl *, const char *);
      typedef void (*WithInt )(ManagerImpl *, int);
      typedef void (*WithBool)(ManagerImpl *, bool);
      WithStr  with_str;
      WithInt  with_int;
      WithBool with_bool;
      Fun() {}
      Fun(WithStr  m) : with_str (m) {}
      Fun(WithInt  m) : with_int (m) {}
      Fun(WithBool m) : with_bool(m) {}
      void call(ManagerImpl * m, const char * val) const {(*with_str) (m,val);}
      void call(ManagerImpl * m, int val)          const {(*with_int) (m,val);}
      void call(ManagerImpl * m, bool val)         const {(*with_bool)(m,val);}
    } fun;
    typedef ManagerImpl::ConfigNotifier CN;
  };

  template <typename T>
  void callback(ManagerImpl * m, const KeyInfo * ki, T value, 
		UpdateMember::Type t);
  
  class ManagerImpl::ConfigNotifier : public Notifier {
  private:
    ManagerImpl * manager_;
  public:
    ConfigNotifier(ManagerImpl * m) 
      : manager_(m) 
    {}

    void item_updated(const KeyInfo * ki, int value) {
      callback(manager_, ki, value, UpdateMember::Int);
    }
    void item_updated(const KeyInfo * ki, bool value) {
      callback(manager_, ki, value, UpdateMember::Bool);
    }
    void item_updated(const KeyInfo * ki, const char * value) {
      callback(manager_, ki, value, UpdateMember::String);
    }

    static void ignore(ManagerImpl * m, int value) {
      m->ignore_count = value;
    }
    static void ignore_accents(ManagerImpl * m, bool value) {
      abort();
    }
    static void ignore_case(ManagerImpl * m, bool value) {
      abort();
    }
    static void ignore_repl(ManagerImpl * m, bool value) {
      m->ignore_repl = value;
    }
    static void save_repl(ManagerImpl * m, bool value) {
      // FIXME
      // m->save_on_saveall(DataSet::Id(&m->personal_repl()), value);
    }
    static void sug_mode(ManagerImpl * m, const char * mode) {
      m->suggest_->set_mode(mode);
      m->intr_suggest_->set_mode(mode);
    }
    static void run_together(ManagerImpl * m, bool value) {
      m->unconditional_run_together_ = value;
    }
    static void run_together_limit(ManagerImpl * m, int value) {
      if (value > 8) {
	m->config()->replace("run-together-limit", "8");
	// will loop back
      } else {
	m->run_together_limit_ = value;
      }
    }
    static void run_together_min(ManagerImpl * m, int value) {
      m->run_together_min_ = value;
      if (m->unconditional_run_together_ 
	  && m->run_together_min_ < m->run_together_start_len_)
	m->run_together_start_len_ = m->run_together_min_;
    }
    
  };

  static UpdateMember update_members[] = 
  {
    {"ignore",         UpdateMember::Int,     UpdateMember::CN::ignore}
    ,{"ignore-accents",UpdateMember::Bool,    UpdateMember::CN::ignore_accents}
    ,{"ignore-case",   UpdateMember::Bool,    UpdateMember::CN::ignore_case}
    ,{"ignore-repl",   UpdateMember::Bool,    UpdateMember::CN::ignore_repl}
    ,{"save-repl",     UpdateMember::Bool,    UpdateMember::CN::save_repl}
    ,{"sug-mode",      UpdateMember::String,  UpdateMember::CN::sug_mode}
    ,{"run-together",  
	UpdateMember::Bool,    
	UpdateMember::CN::run_together}
    ,{"run-together-limit",  
	UpdateMember::Int,    
	UpdateMember::CN::run_together_limit}
    ,{"run-together-min",  
	UpdateMember::Int,    
	UpdateMember::CN::run_together_min}
  };

  template <typename T>
  void callback(ManagerImpl * m, const KeyInfo * ki, T value, 
		UpdateMember::Type t) 
  {
    const UpdateMember * i
      = update_members;
    const UpdateMember * end   
      = i + sizeof(update_members)/sizeof(UpdateMember);
    while (i != end) {
      if (strcmp(ki->name, i->name) == 0) {
	if (i->type == t) {
	  i->fun.call(m, value);
	  break;
	}
      }
      ++i;
    }
  }

  //////////////////////////////////////////////////////////////////////
  //
  // ManagerImpl inititization members
  //

  ManagerImpl::ManagerImpl() 
    : Manager(0) /* FIXME */, ignore_repl(true)
  {}

  PosibErr<void> ManagerImpl::setup(Config * c) {
    assert (config_ == 0);
    config_.reset(c);
    //config_->read_in(&override); // FIXME

    ignore_repl = config_->retrieve_bool("ignore-repl");
    ignore_count = config_->retrieve_int("ignore");

    wls_.reset(new DataSetCollection());

    RET_ON_ERR_SET(add_data_set(config_->retrieve("master-path"), *config_, this),
		   LoadableDataSet *, ltemp);
    
    change_id(ltemp, main_id);

    ClonePtr<StringList> extra_dicts(new_string_list()); // fixme Make ClonePtr StackPtr
    config_->retrieve_list("extra-dicts", extra_dicts);
    Enumeration<StringEnumeration> els = extra_dicts->elements();
    const char * dict_name;
    while ( (dict_name = els.next()) != 0)
      RET_ON_ERR(add_data_set(dict_name,*config_, this));
    
    {
      BasicWordSet * temp;
      temp = new_default_writable_word_set();
      PosibErrBase pe = temp->load(config_->retrieve("personal-path"),config_);
      if (pe.has_err(cant_read_file))
	temp->set_check_lang(lang_name(), config_);
      else if (pe.has_err())
	return pe;
      steal(temp);
      change_id(temp, personal_id);
    }
    
    {
      BasicWordSet * temp;
      temp = new_default_writable_word_set();
      temp->set_check_lang(lang_name(), config_);
      steal(temp);
      change_id(temp, session_id);
    }
     
    {
      BasicReplacementSet * temp = new_default_writable_replacement_set();
      PosibErrBase pe = temp->load(config_->retrieve("repl-path"),config_);
      if (pe.has_err(cant_read_file))
	temp->set_check_lang(lang_name(), config_);
      else if (pe.has_err())
	return pe;
      steal(temp);
      change_id(temp, personal_repl_id);
    }

    unconditional_run_together_ = config_->retrieve_bool("run-together");
    run_together_specified_     = config_->retrieve_bool("run-together-specified");
    run_together_middle_        = lang().mid_chars();

    run_together_limit_  = config_->retrieve_int("run-together-limit");
    if (run_together_limit_ > 8) {
      config_->replace("run-together-limit", "8");
      run_together_limit_ = 8;
    }
    run_together_min_    = config_->retrieve_int("run-together-min");

    run_together_start_len_ = config_->retrieve_int("run-together-specified");
    if (unconditional_run_together_ 
	&& run_together_min_ < run_together_start_len_)
      run_together_start_len_ = run_together_min_;
      
    suggest_.reset(new_default_suggest(this));
    intr_suggest_.reset(new_default_suggest(this));

    config_notifier_.reset(new ConfigNotifier(this));
    config_->add_notifier(config_notifier_);

    config_->set_attached(true);
    return no_err;
  }

  //////////////////////////////////////////////////////////////////////
  //
  // ManagerImpl destrution members
  //

  ManagerImpl::~ManagerImpl() {
    DataSetCollection::Iterator i   = wls_->begin();
    DataSetCollection::Iterator end = wls_->end();
    for (; i != end; ++i) {
      if (i->own && i->data_set)
	delete i->data_set;
    }
  }

  //////////////////////////////////////////////////////////////////////
  //
  //
  //

  void ManagerImpl::DataSetCollection::Item::set_sensible_defaults()
  {
    switch (data_set->basic_type) {
    case DataSet::basic_word_set:
      use_to_check = true;
      use_to_suggest = true;
      break;
    case DataSet::basic_replacement_set:
      use_to_check = false;
      use_to_suggest = true;
    case DataSet::basic_multi_set:
      break;
    default:
      abort();
    }
  }

  extern "C"
  Manager * libpspell_aspell_LTX_new_manager_class(ManagerLtHandle)
  {
    return new ManagerImpl();
  }
}

namespace pcommon {
  template class CopyPtr<aspell::Language>;
}

