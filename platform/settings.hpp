#pragma once

#include "../std/string.hpp"
#include "../std/map.hpp"

namespace Settings
{
  template <class T> bool FromString(string const & str, T & outValue);
  template <class T> string ToString(T const & value);

  class StringStorage
  {
    typedef map<string, string> ContainerT;
    ContainerT m_values;

    StringStorage();
    void Save() const;

  public:
    static StringStorage & Instance();

    bool GetValue(string const & key, string & outValue);
    void SetValue(string const & key, string const & value);
  };

  /// Retrieve setting
  /// @return false if setting is absent
  template <class ValueT> bool Get(string const & key, ValueT & outValue)
  {
    string strVal;
    return StringStorage::Instance().GetValue(key, strVal)
        && FromString(strVal, outValue);
  }
  /// Automatically saves setting to external file
  template <class ValueT> void Set(string const & key, ValueT const & value)
  {
    StringStorage::Instance().SetValue(key, ToString(value));
  }

  enum Units { Metric = 0, Yard, Foot };

  /// Use this function for running some stuff once according to date.
  /// @param[in]  date  Current date in format yymmdd.
  bool IsFirstLaunchForDate(int date);
  /// @Returns true when user launched app first time
  /// Use to track new installs
  /// @warning Can be used only before IsFirstLaunchForDate(int) call
  bool IsFirstLaunch();
}
