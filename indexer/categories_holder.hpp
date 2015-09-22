#pragma once
#include "../base/string_utils.hpp"

#include "../std/vector.hpp"
#include "../std/map.hpp"
#include "../std/string.hpp"
#include "../std/iostream.hpp"
#include "../std/shared_ptr.hpp"


class Reader;

class CategoriesHolder
{
public:
  struct Category
  {
    static const uint8_t EMPTY_PREFIX_LENGTH = 10;

    struct Name
    {
      string m_name;
      int8_t m_lang;
      uint8_t m_prefixLengthToSuggest;
    };

    vector<Name> m_synonyms;

    inline void Swap(Category & r)
    {
      m_synonyms.swap(r.m_synonyms);
    }
  };

private:
  typedef strings::UniString StringT;
  typedef multimap<uint32_t, shared_ptr<Category> > Type2CategoryContT;
  typedef multimap<StringT, uint32_t> Name2CatContT;
  typedef Type2CategoryContT::const_iterator IteratorT;

  Type2CategoryContT m_type2cat;
  Name2CatContT m_name2type;

public:
  CategoriesHolder() {}
  /// Takes ownership of reader.
  explicit CategoriesHolder(Reader * reader);

  void LoadFromStream(istream & s);

  template <class ToDo>
  void ForEachCategory(ToDo toDo) const
  {
    for (IteratorT i = m_type2cat.begin(); i != m_type2cat.end(); ++i)
      toDo(*i->second);
  }

  template <class ToDo>
  void ForEachName(ToDo toDo) const
  {
    for (IteratorT i = m_type2cat.begin(); i != m_type2cat.end(); ++i)
      for (size_t j = 0; j < i->second->m_synonyms.size(); ++j)
        toDo(i->second->m_synonyms[j]);
  }

  template <class ToDo>
  void ForEachTypeByName(StringT const & name, ToDo toDo) const
  {
    typedef typename Name2CatContT::const_iterator IterT;

    pair<IterT, IterT> range = m_name2type.equal_range(name);
    while (range.first != range.second)
    {
      toDo(range.first->second);
      ++range.first;
    }
  }

  /// Search name for type with preffered language.
  /// If no name for this language, return first (en) name.
  /// @return false if no categories for type.
  bool GetNameByType(uint32_t type, int8_t lang, string & name) const;

  inline bool IsTypeExist(uint32_t type) const
  {
    // pass any language
    string dummy;
    return GetNameByType(type, 0, dummy);
  }

  inline void Swap(CategoriesHolder & r)
  {
    m_type2cat.swap(r.m_type2cat);
    m_name2type.swap(r.m_name2type);
  }

private:
  void AddCategory(Category & cat, vector<uint32_t> & types);
};

inline void swap(CategoriesHolder & a, CategoriesHolder & b)
{
  return a.Swap(b);
}
