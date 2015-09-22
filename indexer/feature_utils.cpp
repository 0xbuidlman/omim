#include "feature_utils.hpp"
#include "feature_visibility.hpp"
#include "classificator.hpp"
#include "feature_data.hpp"
#include "scales.hpp"

#include "../geometry/point2d.hpp"

#include "../base/base.hpp"

#include "../std/vector.hpp"


namespace feature
{

namespace impl
{

class FeatureEstimator
{
  template <size_t N>
  static bool IsEqual(uint32_t t, uint32_t const (&arr)[N])
  {
    for (size_t i = 0; i < N; ++i)
      if (arr[i] == t)
        return true;
    return false;
  }

public:

  FeatureEstimator()
  {
    m_TypeContinent   = GetType("place", "continent");
    m_TypeCountry     = GetType("place", "country");

    m_TypeState       = GetType("place", "state");
    m_TypeCounty[0]   = GetType("place", "region");
    m_TypeCounty[1]   = GetType("place", "county");

    m_TypeCity        = GetType("place", "city");
    m_TypeCityCapital = GetType("place", "city", "capital");
    m_TypeTown        = GetType("place", "town");

    m_TypeVillage[0]  = GetType("place", "village");
    m_TypeVillage[1]  = GetType("place", "suburb");

    m_TypeSmallVillage[0]  = GetType("place", "hamlet");
    m_TypeSmallVillage[1]  = GetType("place", "locality");
    m_TypeSmallVillage[2]  = GetType("place", "farm");
  }

  void CorrectScaleForVisibility(TypesHolder const & types, int & scale) const
  {
    pair<int, int> const scaleR = feature::DrawableScaleRangeForText(types);
    if (scale < scaleR.first || scale > scaleR.second)
      scale = scaleR.first;
  }

  m2::RectD CorrectRectForScales(TypesHolder const & types, m2::RectD const & rect) const
  {
    int const scale = scales::GetScaleLevel(rect);
    int scaleNew = scale;
    CorrectScaleForVisibility(types, scaleNew);

    return ((scale != scaleNew) ? scales::GetRectForLevel(scaleNew, rect.Center(), 1.0) : rect);
  }

  m2::RectD GetViewport(TypesHolder const & types, m2::RectD const & limitRect) const
  {
    if (types.GetGeoType() != feature::GEOM_POINT)
      return CorrectRectForScales(types, limitRect);

    int const upperScale = scales::GetUpperScale();
    int scale = upperScale;
    for (size_t i = 0; i < types.Size(); ++i)
      scale = min(scale, GetScaleForType(types[i]));

    CorrectScaleForVisibility(types, scale);

    m2::PointD const centerXY = limitRect.Center();
    return scales::GetRectForLevel(scale, centerXY, 1.0);
  }

  uint8_t GetSearchRank(TypesHolder const & types, m2::PointD const & pt, uint32_t population) const
  {
    for (size_t i = 0; i < types.Size(); ++i)
    {
      if (IsEqual(types[i], m_TypeSmallVillage))
        population = max(population, static_cast<uint32_t>(100));
      else if (IsEqual(types[i], m_TypeVillage))
        population = max(population, static_cast<uint32_t>(1000));
      else if (types[i] == m_TypeTown || IsEqual(types[i], m_TypeCounty))
        population = max(population, static_cast<uint32_t>(10000));
      else if (types[i] == m_TypeState)
      {
        m2::RectD usaRects[] = {
          // Continental part of USA
          m2::RectD(-125.73195962769162293, 25.168771674082393019,
                    -66.925073086214325713, 56.956377399113392812),
          // Alaska
          m2::RectD(-151.0, 63.0, -148.0, 66.0),
          // Hawaii
          m2::RectD(-179.3665041396082529, 17.740790096801504205,
                    -153.92127500280855656, 31.043358939740215874),
          // Canada
          m2::RectD(-141.00315086636985029, 45.927730040557435132,
                    -48.663019303849921471, 162.92387487639103938)
        };

        bool isUSA = false;
        for (size_t i = 0; i < ARRAY_SIZE(usaRects); ++i)
          if (usaRects[i].IsPointInside(pt))
          {
            isUSA = true;
            break;
          }

        if (isUSA)
        {
          //LOG(LINFO, ("USA state population = ", population));
          // Get rank equal to city for USA's states.
          population = max(population, static_cast<uint32_t>(500000));
        }
        else
        {
          //LOG(LINFO, ("Other state population = ", population));
          // Reduce rank for other states.
          population = population / 10;
          population = max(population, static_cast<uint32_t>(10000));
          population = min(population, static_cast<uint32_t>(500000));
        }
      }
      else if (types[i] == m_TypeCity)
        population = max(population, static_cast<uint32_t>(500000));
      else if (types[i] == m_TypeCityCapital)
        population = max(population, static_cast<uint32_t>(1000000));
      else if (types[i] == m_TypeCountry)
        population = max(population, static_cast<uint32_t>(2000000));
      else if (types[i] == m_TypeContinent)
        population = max(population, static_cast<uint32_t>(20000000));
    }

    return static_cast<uint8_t>(log(double(population)) / log(1.1));
  }

private:

  // Returns width and height (lon and lat) for a given type.
  int GetScaleForType(uint32_t const type) const
  {
    if (type == m_TypeContinent)
      return 2;

    /// @todo Load countries bounding rects.
    if (type == m_TypeCountry)
      return 4;

    if (type == m_TypeState)
      return 6;

    if (IsEqual(type, m_TypeCounty))
      return 7;

    if (type == m_TypeCity || type == m_TypeCityCapital)
      return 9;

    if (type == m_TypeTown)
      return 9;

    if (IsEqual(type, m_TypeVillage))
      return 12;

    if (IsEqual(type, m_TypeSmallVillage))
      return 14;

    return scales::GetUpperScale();
  }

  static uint32_t GetType(string const & s1,
                          string const & s2 = string(),
                          string const & s3 = string())
  {
    vector<string> path;
    path.push_back(s1);
    if (!s2.empty()) path.push_back(s2);
    if (!s3.empty()) path.push_back(s3);
    return classif().GetTypeByPath(path);
  }

  uint32_t m_TypeContinent;
  uint32_t m_TypeCountry;
  uint32_t m_TypeState;
  uint32_t m_TypeCounty[2];
  uint32_t m_TypeCity;
  uint32_t m_TypeCityCapital;
  uint32_t m_TypeTown;
  uint32_t m_TypeVillage[2];
  uint32_t m_TypeSmallVillage[3];
};

FeatureEstimator const & GetFeatureEstimator()
{
  static FeatureEstimator const featureEstimator;
  return featureEstimator;
}

}  // namespace feature::impl

m2::RectD GetFeatureViewport(TypesHolder const & types, m2::RectD const & limitRect)
{
  return impl::GetFeatureEstimator().GetViewport(types, limitRect);
}

uint8_t GetSearchRank(TypesHolder const & types, m2::PointD const & pt, uint32_t population)
{
  return impl::GetFeatureEstimator().GetSearchRank(types, pt, population);
}

} // namespace feature
