#pragma once
#include "base/buffer_vector.hpp"

#include "std/cstdint.hpp"
#include "std/initializer_list.hpp"
#include "std/shared_ptr.hpp"
#include "std/unordered_map.hpp"
#include "std/utility.hpp"
#include "std/vector.hpp"

class Classificator;
class FeatureType;

namespace feature { class TypesHolder; }

namespace routing
{

class IVehicleModel
{
public:
  virtual ~IVehicleModel() {}

  /// @return Allowed speed in KMpH.
  /// 0 means that it's forbidden to move on this feature or it's not a road at all.
  virtual double GetSpeed(FeatureType const & f) const = 0;

  /// @returns Max speed in KMpH for this model
  virtual double GetMaxSpeed() const = 0;

  virtual bool IsOneWay(FeatureType const & f) const = 0;
};

class IVehicleModelFactory
{
public:
  virtual ~IVehicleModelFactory() {}

  /// @return Default vehicle model which corresponds for all countrines,
  /// but it may be non optimal for some countries
  virtual shared_ptr<IVehicleModel> GetVehicleModel() const = 0;

  /// @return The most optimal vehicle model for specified country
  virtual shared_ptr<IVehicleModel> GetVehicleModelForCountry(string const & country) const = 0;
};

class VehicleModel : public IVehicleModel
{
public:
  struct SpeedForType
  {
    char const * m_types[2];  /// 2-arity road type
    double m_speedKMpH;       /// max allowed speed on this road type
  };
  typedef initializer_list<SpeedForType> InitListT;

  VehicleModel(Classificator const & c, InitListT const & speedLimits);

  /// @name Overrides from IVehicleModel.
  //@{
  double GetSpeed(FeatureType const & f) const override;
  double GetMaxSpeed() const override { return m_maxSpeedKMpH; }
  bool IsOneWay(FeatureType const & f) const override;
  //@}

  double GetSpeed(feature::TypesHolder const & types) const;
  bool IsOneWay(feature::TypesHolder const & types) const;

  bool IsRoad(FeatureType const & f) const;
  template <class TList> bool IsRoad(TList const & types) const
  {
    for (uint32_t t : types)
      if (IsRoad(t))
        return true;
    return false;
  }
  bool IsRoad(uint32_t type) const;

protected:
  /// Used in derived class constructors only. Not for public use.
  void SetAdditionalRoadTypes(Classificator const & c,
                              initializer_list<char const *> const * arr, size_t sz);

  double m_maxSpeedKMpH;

private:
  unordered_map<uint32_t, double> m_types;

  buffer_vector<uint32_t, 4> m_addRoadTypes;
  uint32_t m_onewayType;
};

}  // namespace routing
