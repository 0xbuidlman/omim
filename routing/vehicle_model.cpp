#include "routing/vehicle_model.hpp"

#include "indexer/classificator.hpp"
#include "indexer/feature.hpp"
#include "indexer/ftypes_matcher.hpp"

#include "base/macros.hpp"

#include "std/limits.hpp"
#include "std/initializer_list.hpp"

namespace routing
{

VehicleModel::InitListT const s_carLimits =
{
  { {"highway", "motorway"},       90 },
  { {"highway", "trunk"},          85 },
  { {"highway", "motorway_link"},  75 },
  { {"highway", "trunk_link"},     70 },
  { {"highway", "primary"},        65 },
  { {"highway", "primary_link"},   60 },
  { {"highway", "secondary"},      55 },
  { {"highway", "secondary_link"}, 50 },
  { {"highway", "tertiary"},       40 },
  { {"highway", "tertiary_link"},  30 },
  { {"highway", "residential"},    25 },
  { {"highway", "pedestrian"},     25 },
  { {"highway", "unclassified"},   25 },
  { {"highway", "service"},        15 },
  { {"highway", "living_street"},  10 },
  { {"highway", "road"},           10 },
  { {"highway", "track"},          5  },
  /// @todo: Add to classificator
  //{ {"highway", "shuttle_train"},  10 },
  //{ {"highway", "ferry"},          5  },
  //{ {"highway", "default"},        10 },
  /// @todo: Check type
  //{ {"highway", "construction"},   40 },
};

VehicleModel::InitListT const s_pedestrianLimits =
{
  { {"highway", "primary"},        5 },
  { {"highway", "primary_link"},   5 },
  { {"highway", "secondary"},      5 },
  { {"highway", "secondary_link"}, 5 },
  { {"highway", "tertiary"},       5 },
  { {"highway", "tertiary_link"},  5 },
  { {"highway", "residential"},    5 },
  { {"highway", "pedestrian"},     5 },
  { {"highway", "unclassified"},   5 },
  { {"highway", "service"},        5 },
  { {"highway", "living_street"},  5 },
  { {"highway", "road"},           5 },
  { {"highway", "track"},          5 },
  { {"highway", "path"},           5 },
  { {"highway", "steps"},          5 },
  { {"highway", "pedestrian"},     5 },
  { {"highway", "footway"},        5 },
};


VehicleModel::VehicleModel(Classificator const & c, VehicleModel::InitListT const & speedLimits)
  : m_maxSpeed(0),
    m_onewayType(c.GetTypeByPath({ "hwtag", "oneway" }))
{
  for (auto const & v : speedLimits)
  {
    m_maxSpeed = max(m_maxSpeed, v.m_speed);
    m_types[c.GetTypeByPath(vector<string>(v.m_types, v.m_types + 2))] = v.m_speed;
  }
}

template <size_t N>
void VehicleModel::SetAdditionalRoadTypes(Classificator const & c, initializer_list<char const *> (&arr)[N])
{
  for (size_t i = 0; i < N; ++i)
    m_addRoadTypes.push_back(c.GetTypeByPath(arr[i]));
}

double VehicleModel::GetSpeed(FeatureType const & f) const
{
  return GetSpeed(feature::TypesHolder(f));
}

double VehicleModel::GetSpeed(feature::TypesHolder const & types) const
{
  double speed = m_maxSpeed * 2;
  for (uint32_t t : types)
  {
    uint32_t const type = ftypes::BaseChecker::PrepareToMatch(t, 2);
    auto it = m_types.find(type);
    if (it != m_types.end())
      speed = min(speed, it->second);
  }
  if (speed <= m_maxSpeed)
    return speed;

  return 0.0;
}

bool VehicleModel::IsOneWay(FeatureType const & f) const
{
  return IsOneWay(feature::TypesHolder(f));
}

bool VehicleModel::IsOneWay(feature::TypesHolder const & types) const
{
  return types.Has(m_onewayType);
}

bool VehicleModel::IsRoad(FeatureType const & f) const
{
  return IsRoad(feature::TypesHolder(f));
}

bool VehicleModel::IsRoad(uint32_t type) const
{
  return (find(m_addRoadTypes.begin(), m_addRoadTypes.end(), type) != m_addRoadTypes.end() ||
          m_types.find(ftypes::BaseChecker::PrepareToMatch(type, 2)) != m_types.end());
}


CarModel::CarModel()
  : VehicleModel(classif(), s_carLimits)
{
  initializer_list<char const *> arr[] =
  {
    { "route", "ferry", "motorcar" },
    { "route", "ferry", "motor_vehicle" },
    { "railway", "rail", "motor_vehicle" },
  };

  SetAdditionalRoadTypes(classif(), arr);
}


PedestrianModel::PedestrianModel()
  : VehicleModel(classif(), s_pedestrianLimits),
    m_noFootType(0)   /// @todo Add additional no-foot type
{
  initializer_list<char const *> arr[] =
  {
    { "route", "ferry" },
    { "man_made", "pier" },
  };

  SetAdditionalRoadTypes(classif(), arr);
}

bool PedestrianModel::IsFoot(feature::TypesHolder const & types) const
{
  return (find(types.begin(), types.end(), m_noFootType) == types.end());
}

double PedestrianModel::GetSpeed(FeatureType const & f) const
{
  feature::TypesHolder types(f);

  // Fixed speed: 5 km/h.
  if (IsFoot(types) && IsRoad(types))
    return m_maxSpeed;
  else
    return 0.0;
}

}
