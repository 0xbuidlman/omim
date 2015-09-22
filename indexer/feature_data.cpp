#include "feature_data.hpp"
#include "classificator.hpp"
#include "feature.hpp"

#include "../std/algorithm.hpp"


using namespace feature;

TypesHolder::TypesHolder(FeatureBase const & f)
: m_size(0), m_geoType(f.GetFeatureType())
{
  f.ForEachTypeRef(*this);
}

string TypesHolder::DebugPrint() const
{
  Classificator const & c = classif();

  string s;
  for (size_t i = 0; i < Size(); ++i)
    s += c.GetFullObjectName(m_types[i]) + "  ";
  return s;
}

void TypesHolder::Remove(uint32_t t)
{
  if (m_size > 0)
  {
    uint32_t * e = m_types + m_size;
    if (std::remove(m_types, e, t) != e)
      --m_size;
  }
}


void FeatureParamsBase::MakeZero()
{
  layer = 0;
  rank = 0;
  ref.clear();
  house.Clear();
  name.Clear();
}

bool FeatureParamsBase::operator == (FeatureParamsBase const & rhs) const
{
  return (name == rhs.name && house == rhs.house && ref == rhs.ref &&
          layer == rhs.layer && rank == rhs.rank);
}

bool FeatureParamsBase::CheckValid() const
{
   CHECK(layer > LAYER_LOW && layer < LAYER_HIGH, ());
   return true;
}

string FeatureParamsBase::DebugString() const
{
  string utf8name;
  name.GetString(0, utf8name);

  return ("'" + utf8name + "' Layer:" + DebugPrint(layer) +
          (rank != 0 ? " Rank:" + DebugPrint(rank) : "") +
          (!house.IsEmpty() ? " House:" + house.Get() : "") +
          (!ref.empty() ? " Ref:" + ref : "") + " ");
}

void FeatureParamsBase::AddHouseName(string const & s)
{
  house.Set(house.IsEmpty() ? s : house.Get() + " \"" + s + "\"");
}

void FeatureParamsBase::AddHouseNumber(string const & ss)
{
  // Remove trailing zero's from house numbers.
  // It's important for debug checks of serialized-deserialized feature.
  string s(ss);
  uint64_t n;
  if (strings::to_uint64(s, n))
    s = strings::to_string(n);

  house.Set(house.IsEmpty() ? s : s + " \"" + house.Get() + "\"");
}

feature::EGeomType FeatureParams::GetGeomType() const
{
  // Geometry types can be combined.
  // We define exact type for priority : starting from GEOM_AREA.

  if (m_geomTypes[GEOM_AREA]) return GEOM_AREA;
  if (m_geomTypes[GEOM_LINE]) return GEOM_LINE;
  if (m_geomTypes[GEOM_POINT]) return GEOM_POINT;
  return GEOM_UNDEFINED;
}

uint8_t FeatureParams::GetTypeMask() const
{
  uint8_t h = 0;
  if (m_geomTypes[GEOM_POINT]) h |= HEADER_GEOM_POINT;
  if (m_geomTypes[GEOM_LINE]) h |= HEADER_GEOM_LINE;
  if (m_geomTypes[GEOM_AREA]) h |= HEADER_GEOM_AREA;
  return h;
}

void FeatureParams::AddTypes(FeatureParams const & rhs, uint32_t skipType2)
{
  if (skipType2 == 0)
  {
    m_Types.insert(m_Types.end(), rhs.m_Types.begin(), rhs.m_Types.end());
  }
  else
  {
    for (size_t i = 0; i < rhs.m_Types.size(); ++i)
    {
      uint32_t t = rhs.m_Types[i];
      ftype::TruncValue(t, 2);
      if (t != skipType2)
        m_Types.push_back(rhs.m_Types[i]);
    }
  }
}

void FeatureParams::FinishAddingTypes()
{
  sort(m_Types.begin(), m_Types.end());
  m_Types.erase(unique(m_Types.begin(), m_Types.end()), m_Types.end());
  if (m_Types.size() > max_types_count)
    m_Types.resize(max_types_count);
}

void FeatureParams::SetType(uint32_t t)
{
  m_Types.clear();
  m_Types.push_back(t);
}

bool FeatureParams::PopAnyType(uint32_t & t)
{
  t = m_Types.back();
  m_Types.pop_back();
  return m_Types.empty();
}

bool FeatureParams::PopExactType(uint32_t t)
{
  m_Types.erase(remove(m_Types.begin(), m_Types.end(), t), m_Types.end());
  return m_Types.empty();
}

bool FeatureParams::IsTypeExist(uint32_t t) const
{
  return (find(m_Types.begin(), m_Types.end(), t) != m_Types.end());
}

bool FeatureParams::operator == (FeatureParams const & rhs) const
{
  return (FeatureParamsBase::operator ==(rhs) &&
          m_Types == rhs.m_Types &&
          GetGeomType() == rhs.GetGeomType());
}

bool FeatureParams::CheckValid() const
{
  CHECK(!m_Types.empty() && m_Types.size() <= max_types_count, ());
  CHECK(GetGeomType() != GEOM_UNDEFINED, ());

  return FeatureParamsBase::CheckValid();
}

uint8_t FeatureParams::GetHeader() const
{
  uint8_t header = static_cast<uint8_t>(m_Types.size() - 1);

  if (!name.IsEmpty())
    header |= HEADER_HAS_NAME;

  if (layer != 0)
    header |= HEADER_HAS_LAYER;

  header |= GetTypeMask();

  // Geometry type for additional info is only one.
  switch (GetGeomType())
  {
  case GEOM_POINT: if (rank != 0) header |= HEADER_HAS_ADDINFO; break;
  case GEOM_LINE: if (!ref.empty()) header |= HEADER_HAS_ADDINFO; break;
  case GEOM_AREA: if (!house.IsEmpty()) header |= HEADER_HAS_ADDINFO; break;
  default:
    ASSERT(false, ("Undefined geometry type"));
  }

  return header;
}

uint32_t FeatureParams::GetIndexForType(uint32_t t)
{
  return classif().GetIndexForType(t);
}

uint32_t FeatureParams::GetTypeForIndex(uint32_t i)
{
  return classif().GetTypeForIndex(i);
}

string DebugPrint(FeatureParams const & p)
{
  ostringstream out;

  out << "Types: ";
  for (size_t i = 0; i < p.m_Types.size(); ++i)
    out << p.m_Types[i] << "; ";

  return (p.DebugString() + out.str());
}
