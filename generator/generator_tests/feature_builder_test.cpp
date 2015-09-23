#include "../../testing/testing.hpp"

#include "../feature_builder.hpp"

#include "../../indexer/feature_visibility.hpp"
#include "../../indexer/classificator_loader.hpp"
#include "../../indexer/classificator.hpp"


namespace
{

template <size_t N, size_t M> void AddTypes(FeatureParams & params, char const * (&arr)[N][M])
{
  Classificator const & c = classif();

  for (size_t i = 0; i < N; ++i)
    params.AddType(c.GetTypeByPath(vector<string>(arr[i], arr[i] + M)));
}

}

UNIT_TEST(FBuilder_ManyTypes)
{
  classificator::Load();

  FeatureBuilder1 fb1;

  FeatureParams params;

  char const * arr1[][1] = {
    { "building" },
    { "oneway" }
  };
  AddTypes(params, arr1);

  char const * arr2[][2] = {
    { "place", "country" },
    { "place", "state" },
    { "place", "county" },
    { "place", "region" },
    { "place", "city" },
    { "place", "town" },
    { "railway", "rail" }
  };
  AddTypes(params, arr2);

  params.FinishAddingTypes();

  params.AddHouseNumber("75");
  params.AddHouseName("Best House");

  StringUtf8Multilang::Builder builder;
  params.name.MakeFrom(builder.AddFullString(0, "Name"));

  fb1.SetParams(params);
  fb1.SetCenter(m2::PointD(0, 0));

  TEST(fb1.RemoveInvalidTypes(), ());
  TEST(fb1.CheckValid(), ());

  FeatureBuilder1::buffer_t buffer;
  TEST(fb1.PreSerialize(), ());
  fb1.Serialize(buffer);

  FeatureBuilder1 fb2;
  fb2.Deserialize(buffer);

  TEST(fb2.CheckValid(), ());
  TEST_EQUAL(fb1, fb2, ());
}

UNIT_TEST(FVisibility_RemoveNoDrawableTypes)
{
  classificator::Load();
  Classificator const & c = classif();

  vector<uint32_t> types;
  types.push_back(c.GetTypeByPath(vector<string>(1, "building")));
  char const * arr[] = { "amenity", "theatre" };
  types.push_back(c.GetTypeByPath(vector<string>(arr, arr + 2)));

  TEST(feature::RemoveNoDrawableTypes(types, feature::FEATURE_TYPE_AREA), ());
  TEST_EQUAL(types.size(), 2, ());
}

UNIT_TEST(FBuilder_RemoveUselessNames)
{
  classificator::Load();

  FeatureParams params;

  char const * arr3[][3] = { { "boundary", "administrative", "2" } };
  AddTypes(params, arr3);
  char const * arr2[][2] = { { "barrier", "fence" } };
  AddTypes(params, arr2);
  params.FinishAddingTypes();

  StringUtf8Multilang::Builder builder;
  params.name.MakeFrom(builder.AddFullString(0, "Name").AddFullString(8, "Имя"));

  FeatureBuilder1 fb1;
  fb1.SetParams(params);

  fb1.AddPoint(m2::PointD(0, 0));
  fb1.AddPoint(m2::PointD(1, 1));
  fb1.SetLinear();

  TEST(!fb1.GetName(0).empty(), ());
  TEST(!fb1.GetName(8).empty(), ());

  fb1.RemoveUselessNames();

  TEST(fb1.GetName(0).empty(), ());
  TEST(fb1.GetName(8).empty(), ());

  TEST(fb1.CheckValid(), ());
}
