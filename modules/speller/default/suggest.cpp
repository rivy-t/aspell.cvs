// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

// suggest.cc Suggestion code for Aspell

// The magic behind my spell checker comes from merging Lawrence
// Philips excellent metaphone algorithm and Ispell's near miss
// strategy which is inserting a space or hyphen, interchanging two
// adjacent letters, changing one letter, deleting a letter, or adding
// a letter.
// 
// The process goes something like this.
// 
// 1.     Convert the misspelled word to its soundslike equivalent (its
//        metaphone for English words).
// 
// 2.     Find words that have the same soundslike pattern.
//
// 3.     Find words that have similar soundslike patterns. A similar
//        soundlike pattern is a pattern that is obtained by
//        interchanging two adjacent letters, changing one letter,
//        deleting a letter, or adding a letter.
//
// 4.     Score the result list and return the words with the lowest
//        score. The score is roughly the weighed average of the edit
//        distance of the word to the misspelled word, the soundslike
//        equivalent of the two words, and the phoneme of the two words.
//        The edit distance is the weighed total of the number of
//        deletions, insertions, exchanges, or adjacent swaps needed to
//        make one string equivalent to the other.
//
// Please note that the soundlike equivalent is a rough approximation
// of how the words sounds. It is not the phoneme of the word by any
// means.  For more information on the metaphone algorithm please see
// the file metaphone.cc which included a detailed description of it.

#include "getdata.hpp"

#include "fstream.hpp"

#include "aspeller.hpp"
#include "asuggest.hpp"
#include "basic_list.hpp"
#include "clone_ptr-t.hpp"
#include "config.hpp"
#include "data.hpp"
#include "editdist.hpp"
#include "editdist2.hpp"
#include "errors.hpp"
#include "file_data_util.hpp"
#include "hash-t.hpp"
#include "language.hpp"
#include "leditdist.hpp"
#include "speller_impl.hpp"
#include "stack_ptr.hpp"
#include "suggest.hpp"


//#include "iostream.hpp"
//#define DEBUG_SUGGEST

using namespace aspeller;
using namespace acommon;
using namespace std;

namespace aspeller_default_suggest {

  typedef vector<String> NearMissesFinal;

  template <class Iterator>
  inline Iterator preview_next (Iterator i) {
    return ++i;
  }
  
  //
  // OriginalWord stores infomation about the original misspelled word
  //   for convince and speed.
  //
  struct OriginalWord {
    String   word;
    String   word_stripped;
    String   soundslike;
    String   phoneme;
    CasePattern  case_pattern;
    OriginalWord() {}
    OriginalWord (const String &w, const String &sl, const String &p)
      : word(w), soundslike(sl), phoneme(p) {}
    OriginalWord (const String &w, const String &sl, const String &p,
		 const String &l, CasePattern cp)
      : word(w), word_stripped(l), soundslike(sl), phoneme(p), case_pattern(cp)
    {}
  };

  //
  // struct ScoreWordSound - used for storing the possible words while
  //   they are being processed.
  //

  struct ScoreWordSound {
    const char *  word;
    const char *  word_stripped;
    int           score;
    int           soundslike_score;
    bool          count;
    ReplacementList::VirEmul * repl_list;
    ScoreWordSound() {repl_list = 0;}
    ~ScoreWordSound() {delete repl_list;}
  };

  inline int compare (const ScoreWordSound &lhs, 
		      const ScoreWordSound &rhs) 
  {
    int temp = lhs.score - rhs.score;
    if (temp) return temp;
    return strcmp(lhs.word,rhs.word);
  }

  inline bool operator < (const ScoreWordSound & lhs, 
			  const ScoreWordSound & rhs) {
    return compare(lhs, rhs) < 0;
  }

  inline bool operator <= (const ScoreWordSound & lhs, 
			   const ScoreWordSound & rhs) {
    return compare(lhs, rhs) <= 0;
  }

  inline bool operator == (const ScoreWordSound & lhs, 
			   const ScoreWordSound & rhs) {
    return compare(lhs, rhs) == 0;
  }

  typedef BasicList<ScoreWordSound> NearMisses;
 
  class Score {
  protected:
    const Language * lang;
    OriginalWord      original_word;
    SuggestParms     parms;

  public:
    Score(const Language *l, const String &w, const SuggestParms & p)
      : lang(l), original_word(w, l->to_soundslike(w.c_str()), 
			      l->to_phoneme(w.c_str()),
			      to_stripped(*l, w),
			      case_pattern(*l, w)),
      parms(p)
    {}
    String fix_case(const String & word) {
      return aspeller::fix_case(*lang,original_word.case_pattern,word);
    }
  };

  class Working : public Score {
   
    int threshold;

    unsigned int max_word_length;

    SpellerImpl  *     speller;
    NearMisses         scored_near_misses;
    NearMisses         near_misses;
    NearMissesFinal  * near_misses_final;

    BasicList<String>      strings;

    static const bool do_count = true;
    static const bool dont_count = false;
    static const bool do_need_alloc = true;
    static const bool dont_need_alloc = false;

    void try_sound(const char *, int ms);
    void add_nearmiss(const char * word, int ms, bool count, 
		      bool need_alloc, ReplacementList::VirEmul * rl = 0) {
      near_misses.push_front(ScoreWordSound());
      ScoreWordSound & d = near_misses.front();
      if (need_alloc) {
	strings.push_front(word);
	d.word = strings.front().c_str();
      } else {
	d.word = word;
      }

      if (parms.use_typo_analysis) {

	unsigned int l = strlen(word);
	if (l > max_word_length) max_word_length = l;
	
      }

      if (!is_stripped(*lang,word)) {
	strings.push_front(to_stripped(*lang,word));
	d.word_stripped = strings.front().c_str();
      } else {
	d.word_stripped = d.word;
      }

      d.soundslike_score = ms;
      d.count = count;
      d.repl_list = rl;
    }
    int needed_level(int want, int soundslike_score) {
      int n = (100*want - parms.soundslike_weight*soundslike_score)
	/(parms.word_weight*parms.edit_distance_weights.min);
      return n > 0 ? n : 0;
    }
    int weighted_average(int soundslike_score, int word_score) {
      return (parms.word_weight*word_score 
	      + parms.soundslike_weight*soundslike_score)/100;
    }
    int skip_first_couple(NearMisses::iterator & i) {
      int k = 0;
      while (preview_next(i) != scored_near_misses.end()) 
	// skip over the first couple of items as they should
	// not be counted in the threshold score.
      {
	if (!i->count) {
	  ++i;
	} else if (k == parms.skip) {
	  break;
	} else {
	  ++k;
	  ++i;
	}
      }
      return k;
    }

    void try_others();
    void score_list();
    void transfer();
  public:
    Working(SpellerImpl * m, const Language *l,
	    const String & w, const SuggestParms & p)
      : Score(l,w,p), threshold(1), max_word_length(0), speller(m) {}
    void get_suggestions(NearMissesFinal &sug);
    void get_suggestions_ultra(NearMissesFinal &sug);
  };

  //
  // try_sound - tries the soundslike string if there is a match add 
  //    the possable words to near_misses
  //

  void Working::try_sound (const char * m, int ms)  
  {
    // sound is the object in the list which is a lot smaller than m

    for (SpellerImpl::DataSetCollection::const_iterator i 
	   = speller->data_set_collection().begin();
	 i != speller->data_set_collection().end();
	 ++i) {
      
      if (!i->use_to_suggest) continue;

      if (i->data_set->basic_type == DataSet::basic_word_set) {

	BasicWordSet::Emul e = static_cast<const BasicWordSet *>
	  (i->data_set)->words_w_soundslike(m);
	BasicWordInfo w;
	String word;
	while ((w = e.next())) {
	  w.get_word(word, i->local_info.convert);
	  add_nearmiss(word.c_str(), ms, do_count, do_need_alloc);
	}
	
      } else {

	BasicReplacementSet::Emul e = static_cast<const BasicReplacementSet *>(i->data_set)->repls_w_soundslike(m);
	ReplacementList repl;
	while (! (repl = e.next()).empty() )
	  add_nearmiss(repl.misspelled_word, ms, 
		       dont_count, dont_need_alloc, repl.elements);	  
      }
    }
  }

  //
  // try_others - tries to come up with possible suggestions
  //
  
  void Working::try_others () {

    const String & word       = original_word.word;
    const String & soundslike = original_word.soundslike;
    
    String::size_type i;
    
    String new_soundslike;
    new_soundslike.reserve(soundslike.size() + 1);

    // Insert a space or hyphone

    if (word.size() >= 4) {

      char * new_word = new char[word.size() + 2];
      strncpy(new_word, word.data(), word.size());
      new_word[word.size() + 1] = '\0';
      new_word[word.size() + 0] = new_word[word.size() - 1];

      for (i = word.size() - 2; i >= 2; --i) {
	new_word[i+1] = new_word[i];
	new_word[i] = '\0';
	
	if (speller->check(new_word) && speller->check(new_word + i + 1)) {
	  new_word[i] = ' ';
	  add_nearmiss(new_word, parms.edit_distance_weights.del2,
		       dont_count, do_need_alloc);

	  new_word[i] = '-';
	  add_nearmiss(new_word, parms.edit_distance_weights.del2,
		       dont_count, do_need_alloc);
	}
      }
      
      delete[] new_word;
    }
    const char * original_soundslike = original_word.soundslike.c_str();
    //unsigned int original_soundslike_len = strlen(original_soundslike);
      
    EditDist (* edit_dist_fun)(const char *, const char *, 
			       const EditDistanceWeights &);

    if (parms.soundslike_level == 1)
      edit_dist_fun = limit1_edit_distance;
    else
      edit_dist_fun = limit2_edit_distance;

    for (SpellerImpl::DataSetCollection::const_iterator i 
	   = speller->data_set_collection().begin();
	 i != speller->data_set_collection().end();
	 ++i) {

      if (!i->use_to_suggest) continue;
      
      if (i->data_set->basic_type == DataSet::basic_word_set) {

	const BasicWordSet * data_set 
	  = static_cast<const BasicWordSet *>(i->data_set);

	StackPtr<SoundslikeEnumeration> els(data_set->soundslike_elements());
    
	SoundslikeWord sw;	
	EditDist score;
        unsigned int stopped_at = LARGE_NUM;
        //CERR << "\\" << original_soundslike << '\n';
	while ( (sw = els->next(stopped_at)) == true) 
	{
	  score = edit_dist_fun(sw.soundslike,
				original_soundslike, 
				parms.edit_distance_weights);
	  stopped_at = score.stopped_at - sw.soundslike;
	  if (score < LARGE_NUM) {
            //CERR << "//" << sw.soundslike << ' ' << score << '\n';
	    stopped_at = LARGE_NUM;
	    BasicWordSet::Emul e = data_set->words_w_soundslike(sw);
	    BasicWordInfo bw;
	    String word;
	    while ((bw = e.next())) {
	      bw.get_word(word, i->local_info.convert);
	      add_nearmiss(word.c_str(), score, do_count, do_need_alloc);
	    }
	  }
	}

      } else {
	
	const BasicReplacementSet * repl_set
	  = static_cast<const BasicReplacementSet *>(i->data_set);

	StackPtr<SoundslikeEnumeration> els(repl_set->soundslike_elements());
    
	SoundslikeWord w;
	EditDist score;
        unsigned int stopped_at = LARGE_NUM;
	while ( (w = els->next(stopped_at)) == true) 
	{
	  score = edit_dist_fun(w.soundslike,
				original_soundslike, 
				parms.edit_distance_weights);
	  
	  stopped_at = score.stopped_at - w.soundslike;
	  if (score < LARGE_NUM) {
	    stopped_at = LARGE_NUM;
	    BasicReplacementSet::Emul e = repl_set->repls_w_soundslike(w);
	    ReplacementList repl;
	    while (! (repl = e.next()).empty() )
	      add_nearmiss(repl.misspelled_word, score, 
			   dont_count, dont_need_alloc, repl.elements);
	  }
	}
      }
    }
  }

  void Working::score_list() {
    if (near_misses.empty()) return;

    bool no_soundslike = strcmp(speller->lang().soundslike_name(), "none") == 0;
    parms.set_original_word_size(original_word.word.size());
      
    NearMisses::iterator i;
    NearMisses::iterator prev;
    int word_score;
      
    near_misses.push_front(ScoreWordSound());
    // the first item will NEVER be looked at.
    scored_near_misses.push_front(ScoreWordSound());
    scored_near_misses.front().score = -1;
    // this item will only be looked at when sorting so 
    // make it a small value to keep it at the front.

    int try_for = (parms.word_weight*parms.edit_distance_weights.max)/100;
    while (true) {
      try_for += (parms.word_weight*parms.edit_distance_weights.max)/100;
	
      // put all pairs whose score <= initial_limit*max_weight
      // into the scored list

      prev = near_misses.begin();
      i = prev;
      ++i;
      while (i != near_misses.end()) {

	int level = needed_level(try_for, i->soundslike_score);
	
	if (no_soundslike)
	  word_score = i->soundslike_score;
	else if (level >= int(i->soundslike_score/parms.edit_distance_weights.min))
	  word_score = edit_distance(original_word.word_stripped.c_str(),
				     i->word_stripped,
				     level, level,
				     parms.edit_distance_weights);
	else
	  word_score = LARGE_NUM;
	  
	if (word_score < LARGE_NUM) {
	  i->score = weighted_average(i->soundslike_score, word_score);
	    
	  scored_near_misses.splice_into(near_misses,prev,i);
	    
	  i = prev; // Yes this is right due to the slice
	  ++i;
	    
	} else {
	    
	  prev = i;
	  ++i;
	    
	}
      }
	
      scored_near_misses.sort();
	
      i = scored_near_misses.begin();
      ++i;
	
      if (i == scored_near_misses.end()) continue;
	
      int k = skip_first_couple(i);
	
      if ((k == parms.skip && i->score <= try_for) 
	  || prev == near_misses.begin() ) // or no more left in near_misses
	break;
    }
      
    threshold = i->score + parms.span;
    if (threshold < parms.edit_distance_weights.max)
      threshold = parms.edit_distance_weights.max;

#  ifdef DEBUG_SUGGEST
    COUT << "Threshold is: " << threshold << "\n";
    COUT << "try_for: " << try_for << "\n";
    COUT << "Size of scored: " << scored_near_misses.size() << "\n";
    COUT << "Size of ! scored: " << near_misses.size() << "\n";
#  endif
      
    //if (threshold - try_for <=  parms.edit_distance_weights.max/2) return;
      
    prev = near_misses.begin();
    i = prev;
    ++i;
    while (i != near_misses.end()) {
	
      int initial_level = needed_level(try_for, i->soundslike_score);
      int max_level = needed_level(threshold, i->soundslike_score);
	
      if (no_soundslike)
	word_score = i->soundslike_score;
      else if (initial_level < max_level)
	word_score = edit_distance(original_word.word_stripped.c_str(),
				   i->word_stripped,
				   initial_level+1,max_level,
				   parms.edit_distance_weights);
      else
	word_score = LARGE_NUM;
	
      if (word_score < LARGE_NUM) {
	i->score = weighted_average(i->soundslike_score, word_score);
	  
	scored_near_misses.splice_into(near_misses,prev,i);
	  
	i = prev; // Yes this is right due to the slice
	++i;
	  
      } else {
	  
	prev = i;
	++i;

      }
    }

    scored_near_misses.sort();
    scored_near_misses.pop_front();
    
    if (parms.use_typo_analysis) {
      int max = 0;
      unsigned int j;

      CharVector original, word;
      original.resize(original_word.word.size() + 1);
      for (j = 0; j != original_word.word.size(); ++j)
        original[j] = lang->to_normalized(original_word.word[j]);
      original[j] = 0;
      word.resize(max_word_length + 1);
      
      for (i = scored_near_misses.begin();
	   i != scored_near_misses.end() && i->score <= threshold;
	   ++i)
      {
	for (j = 0; (i->word)[j] != 0; ++j)
	  word[j] = lang->to_normalized((i->word)[j]);
	word[j] = 0;
	int word_score 
	  = typo_edit_distance(word.data(), original.data(),
			       parms.typo_edit_distance_weights);
	i->score = weighted_average(i->soundslike_score, word_score);
	if (max < i->score) max = i->score;
      }
      threshold = max;
      for (;i != scored_near_misses.end() && i->score <= threshold; ++i)
	i->score = threshold + 1;

      scored_near_misses.sort();
    }
  }

  void Working::transfer() {

#  ifdef DEBUG_SUGGEST
    COUT << "\n" << "\n" 
	 << original_word.word << '\t' 
	 << original_word.soundslike << '\t'
	 << "\n";
#  endif
    int c = 1;
    hash_set<String,HashString<String> > duplicates_check;
    String final_word;
    pair<hash_set<String,HashString<String> >::iterator, bool> dup_pair;
    for (NearMisses::const_iterator i = scored_near_misses.begin();
	 i != scored_near_misses.end() && c <= parms.limit
	   && ( i->score <= threshold || c <= 3 );
	 ++i, ++c) {
#    ifdef DEBUG_SUGGEST
      COUT << i->word << '\t' << i->score 
           << '\t' << lang->to_soundslike(i->word) << "\n";
#    endif
      if (i->repl_list != 0) {
	const char * word;
	string::size_type pos;
	while((word = i->repl_list->next()) != 0) {
	  dup_pair = duplicates_check.insert(fix_case(word));
	  if (dup_pair.second && 
	      ((pos = dup_pair.first->find(' '), pos == String::npos)
	       ? (bool)speller->check(*dup_pair.first)
	       : (speller->check((String)dup_pair.first->substr(0,pos)) 
		  && speller->check((String)dup_pair.first->substr(pos+1))) ))
	    near_misses_final->push_back(*dup_pair.first);
	}
      } else {
	dup_pair = duplicates_check.insert(fix_case(i->word));
	if (dup_pair.second )
	  near_misses_final->push_back(*dup_pair.first);
      }
    }
  }
  
  void Working::get_suggestions(NearMissesFinal & sug) {
    near_misses_final = & sug;
    if (original_word.soundslike.empty()) return;
    try_others();
    score_list();
    transfer();
  }
  
  class SuggestionListImpl : public SuggestionList {
    struct Parms {
      typedef const char *                    Value;
      typedef NearMissesFinal::const_iterator Iterator;
      Iterator end;
      Parms(Iterator e) : end(e) {}
      bool endf(Iterator e) const {return e == end;}
      Value end_state() const {return 0;}
      Value deref(Iterator i) const {return i->c_str();}
    };
  public:
    NearMissesFinal suggestions;

    SuggestionList * clone() const {return new SuggestionListImpl(*this);}
    void assign(const SuggestionList * other) {
      *this = *static_cast<const SuggestionListImpl *>(other);
    }

    bool empty() const { return suggestions.empty(); }
    Size size() const { return suggestions.size(); }
    VirEmul * elements() const {
      return new MakeVirEnumeration<Parms, StringEnumeration>
	(suggestions.begin(), Parms(suggestions.end()));
    }
  };

  class SuggestImpl : public Suggest {
    SpellerImpl * speller_;
    SuggestionListImpl  suggestion_list;
    SuggestParms parms_;
  public:
    SuggestImpl(SpellerImpl * m) 
      : speller_(m), parms_(m->config()->retrieve("sug-mode")) 
    {parms_.fill_distance_lookup(m->config(), m->lang());}
    SuggestImpl(SpellerImpl * m, const SuggestParms & p) 
      : speller_(m), parms_(p) 
    {parms_.fill_distance_lookup(m->config(), m->lang());}
    PosibErr<void> set_mode(ParmString mode) {
      return parms_.set(mode);
    }
    double score(const char *base, const char *other) {
      //parms_.set_original_word_size(strlen(base));
      //Score s(&speller_->lang(),base,parms_);
      //string sl = speller_->lang().to_soundslike(other);
      //ScoreWordSound sws(other, sl.c_str());
      //s.score(sws);
      //return sws.score;
      return -1;
    }
    SuggestionList & suggest(const char * word);
  };

  SuggestionList & SuggestImpl::suggest(const char * word) { 
#   ifdef DEBUG_SUGGEST
    COUT << "=========== begin suggest " << word << " ===========\n";
#   endif
    parms_.set_original_word_size(strlen(word));
    suggestion_list.suggestions.resize(0);
    Working sug(speller_, &speller_->lang(),word,parms_);
    sug.get_suggestions(suggestion_list.suggestions);
#   ifdef DEBUG_SUGGEST
    COUT << "^^^^^^^^^^^  end suggest " << word << "  ^^^^^^^^^^^\n";
#   endif
    return suggestion_list;
  }
  
}

namespace aspeller {
  Suggest * new_default_suggest(SpellerImpl * m) {
    return new aspeller_default_suggest::SuggestImpl(m);
  }

  Suggest * new_default_suggest(SpellerImpl * m, const SuggestParms & p) {
    return new aspeller_default_suggest::SuggestImpl(m,p);
  }

  PosibErr<void> SuggestParms::set(ParmString mode) {

    if (mode != "normal" && mode != "fast" && mode != "ultra" && mode != "bad-spellers")
      return make_err(bad_value, "sug-mode", mode, "one of ultra, fast, normal, or bad-spellers");

    edit_distance_weights.del1 =  95;
    edit_distance_weights.del2 =  95;
    edit_distance_weights.swap =  90;
    edit_distance_weights.sub =  100;
    edit_distance_weights.similar = 10;
    edit_distance_weights.max = 100;
    edit_distance_weights.min =  90;

    normal_soundslike_weight = 50;
    small_word_soundslike_weight = 15;
    small_word_threshold = 4;

    soundslike_weight = normal_soundslike_weight;
    word_weight       = 100 - normal_soundslike_weight;
      
    skip = 2;
    limit = 100;
    if (mode == "normal") {
      use_typo_analysis = true;
      soundslike_level = 2; // either one or two
      span = 50;
    } else if (mode == "fast") {
      use_typo_analysis = true;
      soundslike_level = 1; // either one or two
      span = 50;
    } else if (mode == "ultra") {
      use_typo_analysis = false;
      soundslike_level = 1; // either one or two
      span = 50;
    } else if (mode == "bad-spellers") {
      use_typo_analysis = false;
      normal_soundslike_weight = 55;
      small_word_threshold = 0;
      soundslike_level = 2; // either one or two
      span = 125;
      limit = 1000;
    } else {
      abort(); // this should NEVER happen.
    }

    return no_err;
  }

  PosibErr<void> SuggestParms::fill_distance_lookup(const Config * c, const Language & l) {
    
    TypoEditDistanceWeights & w = typo_edit_distance_weights;

    String keyboard = c->retrieve("keyboard");

    if (keyboard == "none") {
      
      use_typo_analysis = false;
      
    } else {

      FStream in;
      String file, dir1, dir2;
      fill_data_dir(c, dir1, dir2);
      find_file(file, dir1, dir2, keyboard, ".kbd");
      RET_ON_ERR(in.open(file.c_str(), "r"));

      int c = l.max_normalized() + 1;
      w.repl .init(c);
      w.extra.init(c);

      for (int i = 0; i != c; ++i) {
	for (int j = 0; j != c; ++j) {
	  w.repl (i,j) = w.repl_dis2;
	  w.extra(i,j) = w.extra_dis2;
	}
      }

      String key, data;
      while (getdata_pair(in, key, data)) {
	if (key.size() != 2) 
	  return make_err(bad_file_format, file);
	w.repl (l.to_normalized(key[0]),
		l.to_normalized(key[1])) = w.repl_dis1;
	w.repl (l.to_normalized(key[1]),
		l.to_normalized(key[0])) = w.repl_dis1;
	w.extra(l.to_normalized(key[0]),
		l.to_normalized(key[1])) = w.extra_dis1;
	w.extra(l.to_normalized(key[1]),
		l.to_normalized(key[0])) = w.extra_dis1;
      }

      for (int i = 0; i != c; ++i) {
	w.repl(i,i) = 0;
	w.extra(i,i) = w.extra_dis1;
      }
      
    }
    return no_err;
  }
  
    
  SuggestParms * SuggestParms::clone() const {
    return new SuggestParms(*this);
  }

  void SuggestParms::set_original_word_size(int size) {
    if (size <= small_word_threshold) {
      soundslike_weight = small_word_soundslike_weight;
    } else {
      soundslike_weight = normal_soundslike_weight;
    }
    word_weight = 100 - soundslike_weight;
  }
}
