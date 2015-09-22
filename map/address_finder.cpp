#include "framework.hpp"

#include "../indexer/classificator.hpp"
#include "../indexer/feature_visibility.hpp"

#include "../platform/preferred_languages.hpp"


namespace
{
  class FeatureInfoT
  {
  public:
    FeatureInfoT(double d, feature::TypesHolder & types,
                 string & name, string & house, m2::PointD const & pt)
      : m_types(types), m_pt(pt), m_dist(d)
    {
      m_name.swap(name);
      m_house.swap(house);
    }

    bool operator<(FeatureInfoT const & rhs) const
    {
      return (m_dist < rhs.m_dist);
    }

    void Swap(FeatureInfoT & rhs)
    {
      swap(m_dist, rhs.m_dist);
      swap(m_pt, rhs.m_pt);
      m_name.swap(rhs.m_name);
      m_house.swap(rhs.m_house);
      swap(m_types, rhs.m_types);
    }

    string m_name, m_house;
    feature::TypesHolder m_types;
    m2::PointD m_pt;
    double m_dist;
  };

  void swap(FeatureInfoT & i1, FeatureInfoT & i2)
  {
    i1.Swap(i2);
  }

//  string DebugPrint(FeatureInfoT const & info)
//  {
//    return ("Name = " + info.m_name +
//            " House = " + info.m_house +
//            " Distance = " + strings::to_string(info.m_dist));
//  }

  class DoGetFeatureInfoBase
  {
  protected:
    virtual bool IsInclude(double dist, feature::TypesHolder const & types) const = 0;
    virtual double GetResultDistance(double d, feature::TypesHolder const & types) const = 0;
    virtual double NeedProcess(feature::TypesHolder const & types) const
    {
      // feature should be visible in needed scale
      pair<int, int> const r = feature::GetDrawableScaleRange(types);
      return my::between_s(r.first, r.second, m_scale);
    }

    /// @return epsilon value for distance compare according to priority:
    /// point feature is better than linear, that is better than area.
    static double GetCompareEpsilonImpl(feature::EGeomType type, double eps)
    {
      using namespace feature;
      switch (type)
      {
      case GEOM_POINT: return 0.0 * eps;
      case GEOM_LINE: return 1.0 * eps;
      case GEOM_AREA: return 2.0 * eps;
      default:
        ASSERT ( false, () );
        return numeric_limits<double>::max();
      }
    }

  public:
    DoGetFeatureInfoBase(m2::PointD const & pt, int scale)
      : m_pt(pt), m_scale(scale)
    {
      m_coastType = classif().GetCoastType();
    }

    void operator() (FeatureType const & f)
    {
      feature::TypesHolder types(f);
      if (!types.Has(m_coastType) && NeedProcess(types))
      {
        double const d = f.GetDistance(m_pt, m_scale);
        ASSERT_GREATER_OR_EQUAL(d, 0.0, ());

        if (IsInclude(d, types))
        {
          string name, house;
          f.GetPrefferedNames(name, house /*dummy parameter*/);
          house = f.GetHouseNumber();

          // if geom type is not GEOM_POINT, result center point doesn't matter in future use
          m2::PointD const pt = (types.GetGeoType() == feature::GEOM_POINT) ?
                f.GetCenter() : m2::PointD();

          // name, house are assigned like move semantics
          m_cont.push_back(FeatureInfoT(GetResultDistance(d, types), types, name, house, pt));
        }
      }
    }

    void SortResults()
    {
      sort(m_cont.begin(), m_cont.end());
    }

  private:
    m2::PointD m_pt;
    uint32_t m_coastType;

  protected:
    int m_scale;
    vector<FeatureInfoT> m_cont;
  };

  class DoGetFeatureTypes : public DoGetFeatureInfoBase
  {
  protected:
    virtual bool IsInclude(double dist, feature::TypesHolder const & types) const
    {
      return (dist <= m_eps);
    }
    virtual double GetResultDistance(double d, feature::TypesHolder const & types) const
    {
      return (d + GetCompareEpsilonImpl(types.GetGeoType(), m_eps));
    }

  public:
    DoGetFeatureTypes(m2::PointD const & pt, double eps, int scale)
      : DoGetFeatureInfoBase(pt, scale), m_eps(eps)
    {
    }

    void GetFeatureTypes(size_t count, vector<string> & types)
    {
      SortResults();

      Classificator const & c = classif();

      for (size_t i = 0; i < min(count, m_cont.size()); ++i)
        for (size_t j = 0; j < m_cont[i].m_types.Size(); ++j)
          types.push_back(c.GetReadableObjectName(m_cont[i].m_types[j]));
    }

  private:
    double m_eps;
  };
}

void Framework::GetFeatureTypes(m2::PointD pt, vector<string> & types) const
{
  pt = m_navigator.ShiftPoint(pt);

  int const sm = 20;
  m2::RectD pixR(m2::PointD(pt.x - sm, pt.y - sm), m2::PointD(pt.x + sm, pt.y + sm));

  m2::RectD glbR;
  m_navigator.Screen().PtoG(pixR, glbR);

  int const scale = GetDrawScale();
  DoGetFeatureTypes getTypes(m_navigator.Screen().PtoG(pt),
                             max(glbR.SizeX(), glbR.SizeY()) / 2.0,
                             scale);

  m_model.ForEachFeature(glbR, getTypes, scale);

  getTypes.GetFeatureTypes(5, types);
}


namespace
{
  class DoGetAddressBase : public DoGetFeatureInfoBase
  {
  public:
    class TypeChecker
    {
      vector<uint32_t> m_localities, m_streets, m_buildings;
      int m_localityScale;

      template <size_t count, size_t ind>
      void FillMatch(char const * (& arr)[count][ind], vector<uint32_t> & vec)
      {
        STATIC_ASSERT ( count > 0 );
        STATIC_ASSERT ( ind > 0 );

        Classificator const & c = classif();

        vec.reserve(count);
        for (size_t i = 0; i < count; ++i)
        {
          vector<string> v(arr[i], arr[i] + ind);
          vec.push_back(c.GetTypeByPath(v));
        }
      }

      static bool IsMatchImpl(vector<uint32_t> const & vec, feature::TypesHolder const & types)
      {
        for (size_t i = 0; i < types.Size(); ++i)
        {
          uint32_t type = types[i];
          ftype::TruncValue(type, 2);

          if (find(vec.begin(), vec.end(), type) != vec.end())
            return true;
        }

        return false;
      }

    public:
      TypeChecker()
      {
        char const * arrLocalities[][2] = {
          { "place", "city" },
          { "place", "town" },
          { "place", "village" },
          { "place", "hamlet" }
        };

        char const * arrStreet[][2] = {
          { "highway", "primary" },
          { "highway", "secondary" },
          { "highway", "residential" },
          { "highway", "tertiary" },
          { "highway", "living_street" },
          { "highway", "service" }
        };

        char const * arrBuilding[][1] = {
          { "building" }
        };

        FillMatch(arrLocalities, m_localities);
        m_localityScale = 0;
        for (size_t i = 0; i < m_localities.size(); ++i)
        {
          m_localityScale = max(m_localityScale,
                                feature::GetDrawableScaleRange(m_localities[i]).first);
        }

        FillMatch(arrStreet, m_streets);
        FillMatch(arrBuilding, m_buildings);
      }

      int GetLocalitySearchScale() const { return m_localityScale; }

      bool IsLocality(feature::TypesHolder const & types) const
      {
        return IsMatchImpl(m_localities, types);
      }
      bool IsStreet(feature::TypesHolder const & types) const
      {
        return IsMatchImpl(m_streets, types);
      }
      /*
      bool IsBuilding(feature::TypesHolder const & types) const
      {
        return IsMatchImpl(m_buildings, types);
      }
      */

      double GetLocalityDivideFactor(feature::TypesHolder const & types) const
      {
        double arrF[] = { 10.0, 10.0, 1.0, 1.0 };
        ASSERT_EQUAL ( ARRAY_SIZE(arrF), m_localities.size(), () );

        for (size_t i = 0; i < types.Size(); ++i)
        {
          uint32_t type = types[i];
          ftype::TruncValue(type, 2);

          vector<uint32_t>::const_iterator j = find(m_localities.begin(), m_localities.end(), type);
          if (j != m_localities.end())
            return arrF[distance(m_localities.begin(), j)];
        }

        return 1.0;
      }
    };

  protected:
    TypeChecker const & m_checker;

  public:
    DoGetAddressBase(m2::PointD const & pt, int scale, TypeChecker const & checker)
      : DoGetFeatureInfoBase(pt, scale), m_checker(checker)
    {
    }
  };

  class DoGetAddressInfo : public DoGetAddressBase
  {
  protected:
    virtual bool IsInclude(double dist, feature::TypesHolder const & types) const
    {
      // 0 - point, 1 - linear, 2 - area;
      return (dist <= m_arrEps[types.GetGeoType()]);
    }

    virtual double GetResultDistance(double d, feature::TypesHolder const & types) const
    {
      return (d + GetCompareEpsilonImpl(types.GetGeoType(), 5.0 * MercatorBounds::degreeInMetres));
    }

    virtual double NeedProcess(feature::TypesHolder const & types) const
    {
      using namespace feature;
      // we need features with texts for address lookup
      pair<int, int> const r = GetDrawableScaleRangeForRules(types, RULE_TEXT | RULE_SYMBOL);
      return my::between_s(r.first, r.second, m_scale);
    }

    static void GetReadableTypes(search::Engine const * eng, int8_t lang,
                                 feature::TypesHolder const & types,
                                 Framework::AddressInfo & info)
    {
      // Try to add types from categories.
      size_t const count = types.Size();
      for (size_t i = 0; i < count; ++i)
      {
        uint32_t type = types[i];

        string s;
        if (!eng->GetNameByType(type, lang, s))
        {
          // Try to use common type truncation if no match found.
          ftype::TruncValue(type, 2);
          (void)eng->GetNameByType(type, lang, s);
        }

        if (!s.empty())
          info.m_types.push_back(s);
      }

      // If nothing added - return raw classificator types.
      if (info.m_types.empty())
      {
        Classificator const & c = classif();
        for (size_t i = 0; i < count; ++i)
          info.m_types.push_back(c.GetReadableObjectName(types[i]));
      }
    }

  public:
    DoGetAddressInfo(m2::PointD const & pt, int scale, TypeChecker const & checker,
                     double (&arrRadius) [3])
      : DoGetAddressBase(pt, scale, checker)
    {
      for (size_t i = 0; i < 3; ++i)
      {
        // use average value to convert meters to degrees
        m2::RectD const r = MercatorBounds::RectByCenterXYAndSizeInMeters(pt, arrRadius[i]);
        m_arrEps[i] = (r.SizeX() + r.SizeY()) / 2.0;
      }
    }

    void FillAddress(search::Engine const * eng, Framework::AddressInfo & info)
    {
      int8_t const lang = eng->GetCurrentLanguage();

      SortResults();

      for (size_t i = 0; i < m_cont.size(); ++i)
      {
        bool const isStreet = m_checker.IsStreet(m_cont[i].m_types);

        if (info.m_street.empty() && isStreet)
          info.m_street = m_cont[i].m_name;

        if (info.m_house.empty())
          info.m_house = m_cont[i].m_house;

        if (info.m_name.empty())
        {
          if (m_cont[i].m_types.GetGeoType() != feature::GEOM_LINE)
          {
            info.m_name = m_cont[i].m_name;

            GetReadableTypes(eng, lang, m_cont[i].m_types, info);
          }
        }

        if (!(info.m_street.empty() || info.m_name.empty()))
          break;
      }
    }

  private:
    double m_arrEps[3];
  };

  class DoGetLocality : public DoGetAddressBase
  {
  protected:
    virtual bool IsInclude(double dist, feature::TypesHolder const & types) const
    {
      return (dist <= m_eps);
    }

    virtual double GetResultDistance(double d, feature::TypesHolder const & types) const
    {
      // This routine is needed for quality of locality prediction.
      // Hamlet may be the nearest point, but it's a part of a City. So use the divide factor
      // for distance, according to feature type.
      return (d / m_checker.GetLocalityDivideFactor(types));
    }

    virtual double NeedProcess(feature::TypesHolder const & types) const
    {
      return (types.GetGeoType() == feature::GEOM_POINT && m_checker.IsLocality(types));
    }

  public:
    DoGetLocality(m2::PointD const & pt, int scale, TypeChecker const & checker,
                  m2::RectD const & rect)
      : DoGetAddressBase(pt, scale, checker)
    {
      // use maximum value to convert meters to degrees
      m_eps = max(rect.SizeX(), rect.SizeY());
    }

    void FillLocality(Framework::AddressInfo & info, Framework const & fm)
    {
      SortResults();
      //LOG(LDEBUG, (m_cont));

      for (size_t i = 0; i < m_cont.size(); ++i)
      {
        if (!m_cont[i].m_name.empty() && fm.GetCountryName(m_cont[i].m_pt) == info.m_country)
        {
          info.m_city = m_cont[i].m_name;
          break;
        }
      }
    }

  private:
    double m_eps;
  };
}

namespace
{
  /// Global instance for type checker.
  /// @todo Possible need to add synhronization.
  typedef DoGetAddressBase::TypeChecker CheckerT;
  CheckerT * g_checker = 0;

  CheckerT & GetChecker()
  {
    if (g_checker == 0)
      g_checker = new CheckerT();
    return *g_checker;
  }
}

void Framework::GetAddressInfo(m2::PointD const & pt, AddressInfo & info) const
{
  info.Clear();

  info.m_country = GetCountryName(pt);
  if (info.m_country.empty())
  {
    LOG(LINFO, ("Can't find region for point ", pt));
    return;
  }

  int const scale = scales::GetUpperScale();
  double addressR[] = {
    15.0,   // radius to search point POI's
    100.0,  // radius to search street names
    5.0     // radius to search building numbers (POI's)
  };

  // pass maximum value for all addressR
  m2::RectD const rect = MercatorBounds::RectByCenterXYAndSizeInMeters(pt, addressR[1]);
  DoGetAddressInfo getAddress(pt, scale, GetChecker(), addressR);

  m_model.ForEachFeature(rect, getAddress, scale);

  getAddress.FillAddress(GetSearchEngine(), info);

  GetLocality(pt, info);
}

void Framework::GetAddressInfo(FeatureType const & ft, m2::PointD const & pt, AddressInfo & info) const
{
  info.Clear();

  info.m_country = GetCountryName(pt);
  if (info.m_country.empty())
  {
    LOG(LINFO, ("Can't find region for point ", pt));
    return;
  }

  double const inf = numeric_limits<double>::max();
  double addressR[] = { inf, inf, inf };

  DoGetAddressInfo getAddress(pt, scales::GetUpperScale(), GetChecker(), addressR);
  getAddress(ft);
  getAddress.FillAddress(GetSearchEngine(), info);

  GetLocality(pt, info);
}

void Framework::GetLocality(m2::PointD const & pt, AddressInfo & info) const
{
  CheckerT & checker = GetChecker();

  int const scale = checker.GetLocalitySearchScale();
  LOG(LDEBUG, ("Locality scale = ", scale));

  // radius to search localities
  m2::RectD const rect = MercatorBounds::RectByCenterXYAndSizeInMeters(pt, 20000.0);
  DoGetLocality getLocality(pt, scale, checker, rect);

  m_model.ForEachFeature(rect, getLocality, scale);

  getLocality.FillLocality(info, *this);
}

string Framework::AddressInfo::FormatAddress() const
{
  string result = m_house;
  if (!m_street.empty())
  {
    if (!result.empty())
      result += ' ';
    result += m_street;
  }
  if (!m_city.empty())
  {
    if (!result.empty())
      result += ", ";
    result += m_city;
  }
  if (!m_country.empty())
  {
    if (!result.empty())
      result += ", ";
    result += m_country;
  }
  return result;
}

string Framework::AddressInfo::FormatTypes() const
{
  string result;
  for (size_t i = 0; i < m_types.size(); ++i)
  {
    ASSERT ( !m_types.empty(), () );
    if (!result.empty())
      result += ' ';
    result += m_types[i];
  }
  return result;
}

void Framework::AddressInfo::Clear()
{
  m_country.clear();
  m_city.clear();
  m_street.clear();
  m_house.clear();
  m_name.clear();
  m_types.clear();
}
