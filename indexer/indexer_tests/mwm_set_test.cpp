#include "../../testing/testing.hpp"
#include "../mwm_set.hpp"


namespace
{
  class MwmValue : public MwmSet::MwmValueBase
  {
  };

  class TestMwmSet : public MwmSet
  {
  protected:
    virtual int GetInfo(string const & path, MwmInfo & info) const
    {
      int n = path[0] - '0';
      info.m_maxScale = n;
      info.m_limitRect = m2::RectD(0, 0, 1, 1);
      return 1;
    }
    virtual MwmValue * CreateValue(string const &) const
    {
      return new MwmValue();
    }

  public:
    ~TestMwmSet()
    {
      Cleanup();
    }
  };
}  // unnamed namespace


UNIT_TEST(MwmSetSmokeTest)
{
  TestMwmSet mwmSet;
  vector<MwmInfo> info;

  mwmSet.Add("0");
  mwmSet.Add("1");
  mwmSet.Add("2");
  mwmSet.Remove("1");
  mwmSet.GetMwmInfo(info);
  TEST_EQUAL(info.size(), 3, ());
  TEST(info[0].IsActive(), ());
  TEST_EQUAL(info[0].m_maxScale, 0, ());
  TEST(!info[1].IsActive(), ());
  TEST(info[2].IsActive(), ());
  TEST_EQUAL(info[2].m_maxScale, 2, ());
  {
    MwmSet::MwmLock lock0(mwmSet, 0);
    MwmSet::MwmLock lock1(mwmSet, 1);
    TEST(lock0.GetValue() != NULL, ());
    TEST(lock1.GetValue() == NULL, ());
  }

  mwmSet.Add("3");
  mwmSet.GetMwmInfo(info);
  TEST_EQUAL(info.size(), 3, ());
  TEST(info[0].IsActive(), ());
  TEST_EQUAL(info[0].m_maxScale, 0, ());
  TEST(info[1].IsActive(), ());
  TEST_EQUAL(info[1].m_maxScale, 3, ());
  TEST(info[2].IsActive(), ());
  TEST_EQUAL(info[2].m_maxScale, 2, ());

  {
    MwmSet::MwmLock lock(mwmSet, 1);
    TEST(lock.GetValue() != NULL, ());
    mwmSet.Remove("3");
    mwmSet.Add("4");
  }
  mwmSet.GetMwmInfo(info);
  TEST_EQUAL(info.size(), 4, ());
  TEST(info[0].IsActive(), ());
  TEST_EQUAL(info[0].m_maxScale, 0, ());
  TEST(!info[1].IsActive(), ());
  TEST(info[2].IsActive(), ());
  TEST_EQUAL(info[2].m_maxScale, 2, ());
  TEST(info[3].IsActive(), ());
  TEST_EQUAL(info[3].m_maxScale, 4, ());

  mwmSet.Add("5");
  mwmSet.GetMwmInfo(info);
  TEST_EQUAL(info.size(), 4, ());
  TEST(info[0].IsActive(), ());
  TEST_EQUAL(info[0].m_maxScale, 0, ());
  TEST(info[1].IsActive(), ());
  TEST_EQUAL(info[1].m_maxScale, 5, ());
  TEST(info[2].IsActive(), ());
  TEST_EQUAL(info[2].m_maxScale, 2, ());
  TEST(info[3].IsActive(), ());
  TEST_EQUAL(info[3].m_maxScale, 4, ());
}
