#include "testing/testing.hpp"

#include "generator/generator_tests_support/test_feature.hpp"

#include "search/downloader_search_callback.hpp"
#include "search/mode.hpp"
#include "search/result.hpp"
#include "search/search_integration_tests/helpers.hpp"
#include "search/search_tests_support/test_results_matching.hpp"
#include "search/search_tests_support/test_search_request.hpp"

#include "storage/downloader_search_params.hpp"
#include "storage/map_files_downloader.hpp"
#include "storage/storage.hpp"

#include "geometry/rect2d.hpp"

#include "base/logging.hpp"
#include "base/macros.hpp"

#include "std/algorithm.hpp"
#include "std/string.hpp"

using namespace generator::tests_support;
using namespace search::tests_support;

namespace search
{
namespace
{
string const kCountriesTxt = R"({
  "id": "Countries",
  "v": )" + strings::to_string(0 /* version */) +
                             R"(,
  "g": [
      {
       "id": "Flatland",
       "g": [
        {
         "id": "Square One",
         "s": 123,
         "old": [
          "Flatland"
         ]
        },
        {
         "id": "Square Two",
         "s": 456,
         "old": [
          "Flatland"
         ]
        }
       ]
      },
      {
       "id": "Wonderland",
       "g": [
        {
         "id": "Shortpondville",
         "s": 789,
         "old": [
          "Wonderland"
         ]
        },
        {
         "id": "Longpondville",
         "s": 100,
         "old": [
          "Wonderland"
         ]
        }
       ]
      }
   ]})";

class TestMapFilesDownloader : public storage::MapFilesDownloader
{
public:
  // MapFilesDownloader overrides:
  void GetServersList(int64_t const mapVersion, string const & mapFileName,
                      TServersListCallback const & callback) override
  {
  }

  void DownloadMapFile(vector<string> const & urls, string const & path, int64_t size,
                       TFileDownloadedCallback const & onDownloaded,
                       TDownloadingProgressCallback const & onProgress) override
  {
  }

  TProgress GetDownloadingProgress() override { return TProgress{}; }
  
  bool IsIdle() override { return false; }
  
  void Reset() override {}
};

class TestDelegate : public DownloaderSearchCallback::Delegate
{
public:
  // DownloaderSearchCallback::Delegate overrides:
  void RunUITask(function<void()> fn) override { fn(); }
};

class DownloaderSearchRequest : public TestSearchRequest, public TestDelegate
{
public:
  DownloaderSearchRequest(TestSearchEngine & engine, string const & query)
    : TestSearchRequest(engine, MakeSearchParams(query), m2::RectD(0, 0, 1, 1) /* viewport */)
    , m_storage(kCountriesTxt, make_unique<TestMapFilesDownloader>())
    , m_downloaderCallback(static_cast<DownloaderSearchCallback::Delegate &>(*this),
                           m_engine /* index */, m_engine.GetCountryInfoGetter(), m_storage,
                           MakeDownloaderParams(query))
  {
    SetCustomOnResults(bind(&DownloaderSearchRequest::OnResultsDownloader, this, _1));
  }

  void OnResultsDownloader(search::Results const & results)
  {
    m_downloaderCallback(results);
    OnResults(results);
  }

  vector<storage::DownloaderSearchResult> const & GetResults() const { return m_downloaderResults; }

private:
  search::SearchParams MakeSearchParams(string const & query)
  {
    search::SearchParams p;
    p.m_query = query;
    p.m_inputLocale = "en";
    p.SetMode(search::Mode::Downloader);
    p.SetSuggestsEnabled(false);
    p.SetForceSearch(true);
    return p;
  }

  storage::DownloaderSearchParams MakeDownloaderParams(string const & query)
  {
    storage::DownloaderSearchParams p;
    p.m_query = query;
    p.m_inputLocale = "en";
    p.m_onResults = [this](storage::DownloaderSearchResults const & r)
    {
      CHECK(!m_endMarker, ());
      if (r.m_endMarker)
      {
        m_endMarker = true;
        return;
      }

      m_downloaderResults.insert(m_downloaderResults.end(), r.m_results.begin(), r.m_results.end());
    };
    return p;
  }

  vector<storage::DownloaderSearchResult> m_downloaderResults;
  bool m_endMarker = false;

  storage::Storage m_storage;

  DownloaderSearchCallback m_downloaderCallback;
};

class DownloaderSearchTest : public SearchTest
{
public:
  void AddRegion(string const & countryName, string const & regionName, m2::PointD const & p1,
                 m2::PointD const & p2)
  {
    TestPOI cornerPost1(p1, regionName + " corner post 1", "en");
    TestPOI cornerPost2(p2, regionName + " corner post 2", "en");
    TestCity capital((p1 + p2) * 0.5, regionName + " capital", "en", 0 /* rank */);
    TestCountry country(p1 * 0.3 + p2 * 0.7, countryName, "en");
    BuildCountry(regionName, [&](TestMwmBuilder & builder)
    {
      builder.Add(cornerPost1);
      builder.Add(cornerPost2);
      builder.Add(capital);
      if (!countryName.empty())
      {
        // Add the country feature to one region only.
        builder.Add(country);
        m_worldCountries.push_back(country);
      }
    });
    m_worldCities.push_back(capital);
  };

  void BuildWorld()
  {
    SearchTest::BuildWorld([&](TestMwmBuilder & builder)
    {
      for (auto const & ft : m_worldCountries)
        builder.Add(ft);
      for (auto const & ft : m_worldCities)
        builder.Add(ft);
    });
  }

private:
  vector<TestCountry> m_worldCountries;
  vector<TestCity> m_worldCities;
};

template <typename T>
void TestResults(vector<T> received, vector<T> expected)
{
  sort(received.begin(), received.end());
  sort(expected.begin(), expected.end());
  TEST_EQUAL(expected, received, ());
}

UNIT_CLASS_TEST(DownloaderSearchTest, Smoke)
{
  AddRegion("Flatland", "Square One", m2::PointD(0.0, 0.0), m2::PointD(1.0, 1.0));
  AddRegion("", "Square Two", m2::PointD(1.0, 1.0), m2::PointD(3.0, 3.0));
  AddRegion("Wonderland", "Shortpondland", m2::PointD(-1.0, -1.0), m2::PointD(0.0, 0.0));
  AddRegion("", "Longpondland", m2::PointD(-3.0, -3.0), m2::PointD(-1.0, -1.0));
  BuildWorld();

  {
    DownloaderSearchRequest request(m_engine, "square one");
    request.Run();

    TestResults(request.GetResults(),
                {storage::DownloaderSearchResult("Square One", "Square One capital")});
  }

  {
    DownloaderSearchRequest request(m_engine, "shortpondland");
    request.Run();

    TestResults(request.GetResults(),
                {storage::DownloaderSearchResult("Shortpondland", "Shortpondland capital")});
  }

  {
    DownloaderSearchRequest request(m_engine, "flatland");
    request.Run();

    TestResults(request.GetResults(), {storage::DownloaderSearchResult("Flatland", "Flatland")});
  }

  {
    DownloaderSearchRequest request(m_engine, "square");
    request.Run();

    TestResults(request.GetResults(),
                {storage::DownloaderSearchResult("Square One", "Square One capital"),
                 storage::DownloaderSearchResult("Square Two", "Square Two capital")});
  }
}
}  // namespace
}  // namespace search
