#include "string.hpp"
#include "string_enumeration.hpp"
#include "string_list.hpp"

namespace pcommon {

  struct StringListNode {
    // private data structure
    // default copy & destructor unsafe
    String           data;
    StringListNode * next;
    StringListNode(ParmString str,  StringListNode * n = 0)
      : data(str), next(n) {}
  };

  class StringListEnumeration : public StringEnumeration {
    // default copy and destructor safe
  private:
    StringListNode * n_;
  public:
    StringEnumeration * clone() const;
    void assign(const StringEnumeration *);

    StringListEnumeration(StringListNode * n) : n_(n) {}
    const char * next() {
      const char * temp;
      if (n_ == 0) {
	temp = 0;
      } else {
	temp = n_->data.c_str();
	n_ = n_->next;
      }
      return temp;
    }
    bool at_end() const {
      return n_ == 0;
    }
  };


  class StringListImpl : public StringList {
    // copy and destructor provided
  private:
    StringListNode * first;

    StringListNode * * find (ParmString str);
    void copy(const StringListImpl &);
    void destroy();
  public:
    friend bool operator==(const StringListImpl &, const StringListImpl &);
    StringListImpl() : first(0) {}
    StringListImpl(const StringListImpl & other) 
    {
      copy(other);
    }
    StringListImpl & operator= (const StringListImpl & other)
    {
      destroy();
      copy(other);
      return *this;
    }
    virtual ~StringListImpl() 
    {
      destroy();
    }

    StringList * clone() const;
    void assign(const StringList *);

    bool add(ParmString);
    bool remove(ParmString);
    void clear();

    StringEnumeration * elements() const;
    StringListEnumeration elements_obj() const 
    {
      return StringListEnumeration(first);
    }

    bool empty() const { return first == 0; }
    unsigned int size() const { abort(); }

  };

}
