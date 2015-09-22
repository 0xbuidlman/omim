#include "categories_holder.hpp"
#include "search_delimiters.hpp"
#include "search_string_utils.hpp"
#include "classificator.hpp"

#include "../coding/reader.hpp"
#include "../coding/reader_streambuf.hpp"
#include "../coding/multilang_utf8_string.hpp"

#include "../base/logging.hpp"
#include "../base/stl_add.hpp"


namespace
{

enum State
{
  EParseTypes,
  EParseLanguages
};

}  // unnamed namespace


CategoriesHolder::CategoriesHolder(Reader * reader)
{
  ReaderStreamBuf buffer(reader);
  istream s(&buffer);
  LoadFromStream(s);
}

void CategoriesHolder::AddCategory(Category & cat, vector<uint32_t> & types)
{
  if (!cat.m_synonyms.empty() && !types.empty())
  {
    shared_ptr<Category> p(new Category());
    p->Swap(cat);

    for (size_t i = 0; i < types.size(); ++i)
      m_type2cat.insert(make_pair(types[i], p));

    for (size_t i = 0; i < p->m_synonyms.size(); ++i)
    {
      StringT const uniName = search::NormalizeAndSimplifyString(p->m_synonyms[i].m_name);

      vector<StringT> tokens;
      SplitUniString(uniName, MakeBackInsertFunctor(tokens), search::CategoryDelimiters());

      for (size_t j = 0; j < tokens.size(); ++j)
        for (size_t k = 0; k < types.size(); ++k)
          m_name2type.insert(make_pair(tokens[j], types[k]));
    }
  }

  cat.m_synonyms.clear();
  types.clear();
}

void CategoriesHolder::LoadFromStream(istream & s)
{
  m_type2cat.clear();
  m_name2type.clear();

  State state = EParseTypes;
  string line;

  Category cat;
  vector<uint32_t> types;

  Classificator const & c = classif();

  while (s.good())
  {
    getline(s, line);
    strings::SimpleTokenizer iter(line, ":|");

    switch (state)
    {
    case EParseTypes:
      {
        AddCategory(cat, types);

        while (iter)
        {
          // split category to sub categories for classificator
          vector<string> v;
          strings::Tokenize(*iter, "-", MakeBackInsertFunctor(v));

          // get classificator type
          types.push_back(c.GetTypeByPath(v));
          ++iter;
        }

        if (!types.empty())
          state = EParseLanguages;
      }
      break;

    case EParseLanguages:
      {
        if (!iter)
        {
          state = EParseTypes;
          continue;
        }
        int8_t const langCode = StringUtf8Multilang::GetLangIndex(*iter);
        if (langCode == StringUtf8Multilang::UNSUPPORTED_LANGUAGE_CODE)
        {
          LOG(LWARNING, ("Invalid language code:", *iter));
          continue;
        }

        while (++iter)
        {
          Category::Name name;
          name.m_lang = langCode;
          name.m_name = *iter;

          if (name.m_name.empty())
          {
            LOG(LWARNING, ("Empty category name"));
            continue;
          }

          if (name.m_name[0] >= '0' && name.m_name[0] <= '9')
          {
            name.m_prefixLengthToSuggest = name.m_name[0] - '0';
            name.m_name = name.m_name.substr(1);
          }
          else
            name.m_prefixLengthToSuggest = 10;

          cat.m_synonyms.push_back(name);
        }
      }
      break;
    }
  }

  // add last category
  AddCategory(cat, types);
}

bool CategoriesHolder::GetNameByType(uint32_t type, int8_t lang, string & name) const
{
  pair<IteratorT, IteratorT> const range = m_type2cat.equal_range(type);

  for (IteratorT i = range.first; i != range.second; ++i)
  {
    Category const & cat = *i->second;
    for (size_t j = 0; j < cat.m_synonyms.size(); ++j)
      if (cat.m_synonyms[j].m_lang == lang)
      {
        name = cat.m_synonyms[j].m_name;
        return true;
      }
  }

  if (range.first != range.second)
  {
    name = range.first->second->m_synonyms[0].m_name;
    return true;
  }

  return false;
}
