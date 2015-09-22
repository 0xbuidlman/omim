#include "../../testing/testing.hpp"

#include "../country_info.hpp"
#include "../country.hpp"

#include "../../indexer/mercator.hpp"

#include "../../platform/platform.hpp"

#include "../../base/logging.hpp"


using namespace storage;

namespace
{
  typedef storage::CountryInfoGetter CountryInfoT;
  CountryInfoT * GetCountryInfo()
  {
    Platform & pl = GetPlatform();
    return new CountryInfoT(pl.GetReader(PACKED_POLYGONS_FILE),
                            pl.GetReader(COUNTRIES_FILE));
  }
}

UNIT_TEST(CountryInfo_GetByPoint_Smoke)
{
  scoped_ptr<CountryInfoT> getter(GetCountryInfo());

  // Minsk
  CountryInfo info;
  getter->GetRegionInfo(m2::PointD(MercatorBounds::LonToX(27.5618818),
                                   MercatorBounds::LatToY(53.9022651)),
                        info);

  TEST_EQUAL(info.m_name, "Belarus", ());
  TEST_EQUAL(info.m_flag, "by", ());
}

namespace
{
  bool IsEmptyName(map<string, CountryInfo> const & id2info, string const & id)
  {
    map<string, CountryInfo>::const_iterator i = id2info.find(id);
    TEST(i != id2info.end(), ());
    return i->second.m_name.empty();
  }
}

UNIT_TEST(CountryInfo_ValidName_Smoke)
{
  string buffer;
  ReaderPtr<Reader>(GetPlatform().GetReader(COUNTRIES_FILE)).ReadAsString(buffer);

  map<string, CountryInfo> id2info;
  storage::LoadCountryFile2CountryInfo(buffer, id2info);

  TEST(!IsEmptyName(id2info, "Germany_Baden-Wurttemberg"), ());
  TEST(!IsEmptyName(id2info, "France_Paris & Ile-de-France"), ());

  TEST(IsEmptyName(id2info, "Russia_Far Eastern"), ());
  TEST(IsEmptyName(id2info, "UK_Northern Ireland"), ());
}

UNIT_TEST(CountryInfo_USARect)
{
  scoped_ptr<CountryInfoT> getter(GetCountryInfo());

  m2::RectD rects[3];
  getter->CalcUSALimitRect(rects);

  LOG(LINFO, ("Continental: ", rects[0]));
  LOG(LINFO, ("Alaska: ", rects[1]));
  LOG(LINFO, ("Hawaii: ", rects[2]));
}
