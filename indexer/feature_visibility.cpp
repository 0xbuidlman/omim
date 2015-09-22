#include "feature_visibility.hpp"
#include "classificator.hpp"
#include "feature.hpp"
#include "scales.hpp"

#include "../base/assert.hpp"

#include "../std/array.hpp"


namespace
{
  bool NeedProcessParent(ClassifObject const * p)
  {
    string const & n = p->GetName();
    // same as is_mark_key (@see osm2type.cpp)
    return (n == "bridge" || n == "junction" || n == "oneway" || n == "fee");
  }
}

template <class ToDo> typename ToDo::ResultType
Classificator::ProcessObjects(uint32_t type, ToDo & toDo) const
{
  typedef typename ToDo::ResultType ResultType;
  ResultType res = ResultType(); // default initialization

  ClassifObject const * p = &m_root;
  uint8_t i = 0;
  uint8_t v;

  // it's enough for now with our 3-level classificator
  array<ClassifObject const *, 8> path;

  // get objects route in hierarchy for type
  while (ftype::GetValue(type, i, v))
  {
    p = p->GetObject(v);
    path[i++] = p;
    toDo(p);
  }

  if (path.empty())
    return res;
  else
  {
    // process objects from child to root
    for (; i > 0; --i)
    {
      // process and stop find if needed
      if (toDo(path[i-1], res)) break;

      // no need to process parents
      if (!NeedProcessParent(path[i-1])) break;
    }
    return res;
  }
}

ClassifObject const * Classificator::GetObject(uint32_t type) const
{
  ClassifObject const * p = &m_root;
  uint8_t i = 0;

  // get the final ClassifObject
  uint8_t v;
  while (ftype::GetValue(type, i, v))
  {
    ++i;
    p = p->GetObject(v);
  }

  return p;
}

string Classificator::GetFullObjectName(uint32_t type) const
{
  ClassifObject const * p = &m_root;
  uint8_t i = 0;
  string s;

  // get the final ClassifObject
  uint8_t v;
  while (ftype::GetValue(type, i, v))
  {
    ++i;
    p = p->GetObject(v);
    s = s + p->GetName() + '|';
  }

  return s;
}

namespace feature
{

namespace
{
  class DrawRuleGetter
  {
    int m_scale;
    ClassifObject::FeatureGeoType m_ft;
    vector<drule::Key> & m_keys;
    string & m_name;

  public:
    DrawRuleGetter(int scale, feature::EGeomType ft,
                  vector<drule::Key> & keys, string & name)
      : m_scale(scale), m_ft(ClassifObject::FeatureGeoType(ft)), m_keys(keys), m_name(name)
    {
    }

    typedef bool ResultType;

    void operator() (ClassifObject const * p)
    {
#ifdef DEBUG
      if (!m_name.empty()) m_name += '-';
      m_name += p->GetName();
#endif
    }
    bool operator() (ClassifObject const * p, bool & res)
    {
      res = true;
      p->GetSuitable(m_scale, m_ft, m_keys);
      return false;
    }
  };
}

pair<int, bool> GetDrawRule(FeatureBase const & f, int level,
                            vector<drule::Key> & keys, string & names)
{
  feature::TypesHolder types(f);

  ASSERT ( keys.empty(), () );
  Classificator const & c = classif();

  DrawRuleGetter doRules(level, types.GetGeoType(), keys, names);
  for (size_t i = 0; i < types.Size(); ++i)
    (void)c.ProcessObjects(types[i], doRules);

  return make_pair(types.GetGeoType(), types.Has(c.GetCoastType()));
}

namespace
{
  class IsDrawableChecker
  {
    int m_scale;

  public:
    IsDrawableChecker(int scale) : m_scale(scale) {}

    typedef bool ResultType;

    void operator() (ClassifObject const *) {}
    bool operator() (ClassifObject const * p, bool & res)
    {
      if (p->IsDrawable(m_scale))
      {
        res = true;
        return true;
      }
      return false;
    }
  };

  class IsDrawableLikeChecker
  {
    ClassifObject::FeatureGeoType m_type;

  public:
    IsDrawableLikeChecker(FeatureGeoType type)
      : m_type(ClassifObject::FeatureGeoType(type))
    {
    }

    typedef bool ResultType;

    void operator() (ClassifObject const *) {}
    bool operator() (ClassifObject const * p, bool & res)
    {
      if (p->IsDrawableLike(m_type))
      {
        res = true;
        return true;
      }
      return false;
    }
  };

  class TextRulesChecker
  {
    int m_scale;
    ClassifObject::FeatureGeoType m_ft;

  public:
    TextRulesChecker(int scale, feature::EGeomType ft)
      : m_scale(scale), m_ft(ClassifObject::FeatureGeoType(ft))
    {
    }

    typedef bool ResultType;

    void operator() (ClassifObject const *) {}
    bool operator() (ClassifObject const * p, bool & res)
    {
      vector<drule::Key> keys;
      p->GetSuitable(m_scale, m_ft, keys);

      for (size_t i = 0; i < keys.size(); ++i)
        if (keys[i].m_type == drule::caption || keys[i].m_type == drule::pathtext)
        {
          res = true;
          return true;
        }

      return false;
    }
  };
}

bool IsDrawableAny(uint32_t type)
{
  return classif().GetObject(type)->IsDrawableAny();
}

bool IsDrawableLike(vector<uint32_t> const & types, FeatureGeoType ft)
{
  Classificator const & c = classif();

  IsDrawableLikeChecker doCheck(ft);
  for (size_t i = 0; i < types.size(); ++i)
    if (c.ProcessObjects(types[i], doCheck))
      return true;
  return false;
}

bool IsDrawableForIndex(FeatureBase const & f, int level)
{
  Classificator const & c = classif();

  feature::TypesHolder types(f);

  if (types.GetGeoType() == feature::GEOM_AREA && !types.Has(c.GetCoastType()))
    if (!scales::IsGoodForLevel(level, f.GetLimitRect()))
      return false;

  IsDrawableChecker doCheck(level);
  for (size_t i = 0; i < types.Size(); ++i)
    if (c.ProcessObjects(types[i], doCheck))
      return true;

  return false;
}

int GetMinDrawableScale(FeatureBase const & f)
{
  int const upBound = scales::GetUpperScale();

  for (int level = 0; level <= upBound; ++level)
    if (feature::IsDrawableForIndex(f, level))
      return level;

  return -1;
}

namespace
{
  void AddRange(pair<int, int> & dest, pair<int, int> const & src)
  {
    if (src.first != -1)
    {
      ASSERT_GREATER(src.first, -1, ());
      ASSERT_GREATER(src.second, -1, ());

      dest.first = min(dest.first, src.first);
      dest.second = max(dest.second, src.second);

      ASSERT_GREATER(dest.first, -1, ());
      ASSERT_GREATER(dest.second, -1, ());
    }
  }

  class DoGetScalesRange
  {
    pair<int, int> m_scales;
  public:
    DoGetScalesRange() : m_scales(1000, -1000) {}
    typedef bool ResultType;

    void operator() (ClassifObject const *) {}
    bool operator() (ClassifObject const * p, bool & res)
    {
      res = true;
      AddRange(m_scales, p->GetDrawScaleRange());
      return false;
    }

    pair<int, int> GetScale() const
    {
      return (m_scales.first > m_scales.second ? make_pair(-1, -1) : m_scales);
    }
  };
}

pair<int, int> GetDrawableScaleRange(uint32_t type)
{
  DoGetScalesRange doGet;
  (void)classif().ProcessObjects(type, doGet);
  return doGet.GetScale();
}

pair<int, int> GetDrawableScaleRange(TypesHolder const & types)
{
  pair<int, int> res(1000, -1000);

  for (size_t i = 0; i < types.Size(); ++i)
    AddRange(res, GetDrawableScaleRange(types[i]));

  return (res.first > res.second ? make_pair(-1, -1) : res);
}

namespace
{
  bool IsDrawableText(feature::TypesHolder const & types, int level)
  {
    Classificator const & c = classif();

    TextRulesChecker doCheck(level, types.GetGeoType());
    for (size_t i = 0; i < types.Size(); ++i)
      if (c.ProcessObjects(types[i], doCheck))
        return true;

    return false;
  }
}

pair<int, int> GetDrawableScaleRangeForText(feature::TypesHolder const & types)
{
  int const upBound = scales::GetUpperScale();
  int lowL = -1;
  for (int level = 0; level <= upBound; ++level)
  {
    if (IsDrawableText(types, level))
    {
      lowL = level;
      break;
    }
  }

  if (lowL == -1)
    return make_pair(-1, -1);

  int highL = lowL;
  for (int level = upBound; level > lowL; --level)
  {
    if (IsDrawableText(types, level))
    {
      highL = level;
      break;
    }
  }

  return make_pair(lowL, highL);
}

pair<int, int> GetDrawableScaleRangeForText(FeatureBase const & f)
{
  return GetDrawableScaleRangeForText(TypesHolder(f));
}

bool IsHighway(vector<uint32_t> const & types)
{
  ClassifObject const * pRoot = classif().GetRoot();

  for (size_t i = 0; i < types.size(); ++i)
  {
    uint8_t v;
    CHECK(ftype::GetValue(types[i], 0, v), (types[i]));
    {
      if (pRoot->GetObject(v)->GetName() == "highway")
        return true;
    }
  }

  return false;
}

/*
bool IsJunction(vector<uint32_t> const & types)
{
  char const * arr[] = { "highway", "motorway_junction" };
  static const uint32_t type = classif().GetTypeByPath(vector<string>(arr, arr + 2));

  for (size_t i = 0; i < types.size(); ++i)
    if (types[i] == type)
      return true;

  return false;
}
*/

bool UsePopulationRank(uint32_t type)
{
  class CheckerT
  {
     uint32_t m_types[3];

  public:
    CheckerT()
    {
      Classificator & c = classif();

      vector<string> vec;
      vec.push_back("place");
      vec.push_back("city");
      m_types[0] = c.GetTypeByPath(vec);

      vec.push_back("capital");
      m_types[1] = c.GetTypeByPath(vec);

      vec.clear();
      vec.push_back("place");
      vec.push_back("town");
      m_types[2] = c.GetTypeByPath(vec);
    }

    bool IsMyType(uint32_t t) const
    {
      uint32_t const * e = m_types + ARRAY_SIZE(m_types);
      return (find(m_types, e, t) != e);
    }
  };

  static CheckerT checker;
  return (checker.IsMyType(type));
}

}
