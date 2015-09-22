#include "geourl_process.hpp"

#include "../indexer/scales.hpp"
#include "../indexer/mercator.hpp"

#include "../base/string_utils.hpp"


namespace url_scheme
{
  void Info::Reset()
  {
    m_lat = m_lon = EmptyValue();
    m_zoom = scales::GetUpperScale();
  }

  m2::RectD Info::GetViewport() const
  {
    ASSERT ( IsValid(), () );

    return scales::GetRectForLevel(m_zoom,
                                   m2::PointD(MercatorBounds::LonToX(m_lon),
                                              MercatorBounds::LatToY(m_lat)),
                                   1.0);
  }


  class DoGeoParse
  {
    Info & m_info;

    enum TMode { START, LAT, LON, ZOOM, FINISH };
    TMode m_mode;

    static void ToDouble(string const & token, double & d)
    {
      double temp;
      if (strings::to_double(token, temp))
        d = temp;
    }

    bool CheckKeyword(string const & token)
    {
      if (token == "lat" || token == "point")
        m_mode = LAT;
      else if (token == "lon")
        m_mode = LON;
      else if (token == "zoom")
        m_mode = ZOOM;
      else
        return false;

      return true;
    }

  public:
    DoGeoParse(Info & info) : m_info(info), m_mode(START)
    {
    }

    void operator()(string const & token)
    {
      switch (m_mode)
      {
      case START:
        // Only geo scheme is supported by this parser
        if (token != "geo")
          m_mode = FINISH;
        else
          m_mode = LAT;
        break;

      case LAT:
        if (!CheckKeyword(token))
        {
          ToDouble(token, m_info.m_lat);
          m_mode = LON;
        }
        break;

      case LON:
        if (!CheckKeyword(token))
        {
          ToDouble(token, m_info.m_lon);
          m_mode = ZOOM;
        }
        break;

      case ZOOM:
        if (!CheckKeyword(token))
        {
          ToDouble(token, m_info.m_zoom);
          m_mode = FINISH;
        }
        break;

      default:
        break;
      }
    }
  };

  void ParseGeoURL(string const & s, Info & info)
  {
    strings::Tokenize(s, ":/?&=,", DoGeoParse(info));
  }
}
