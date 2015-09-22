#include "bookmark.hpp"

#include "../indexer/mercator.hpp"

#include "../coding/file_reader.hpp"
#include "../coding/parse_xml.hpp"  // LoadFromKML
#include "../coding/internal/file_data.hpp"

#include "../platform/platform.hpp"

#include "../base/stl_add.hpp"
#include "../base/string_utils.hpp"

#include "../std/fstream.hpp"
#include "../std/algorithm.hpp"


void BookmarkCategory::AddBookmarkImpl(Bookmark const & bm, double scale)
{
  Bookmark * p = new Bookmark(bm);
  p->SetScale(scale);

  m_bookmarks.push_back(p);
}

void BookmarkCategory::AddBookmark(Bookmark const & bm, double scale)
{
  AddBookmarkImpl(bm, scale);
}

void BookmarkCategory::ReplaceBookmark(size_t index, Bookmark const & bm, double scale)
{
  ASSERT_LESS ( index, m_bookmarks.size(), () );
  if (index < m_bookmarks.size())
  {
    Bookmark * p = new Bookmark(bm);
    p->SetScale(scale);

    Bookmark const * old = m_bookmarks[index];
    // Save creation time stamp
    p->SetTimeStamp(old->GetTimeStamp());
    m_bookmarks[index] = p;

    delete old;
  }
  else
    LOG(LWARNING, ("Trying to replace non-existing bookmark"));
}

BookmarkCategory::~BookmarkCategory()
{
  ClearBookmarks();
}

void BookmarkCategory::ClearBookmarks()
{
  for_each(m_bookmarks.begin(), m_bookmarks.end(), DeleteFunctor());
  m_bookmarks.clear();
}

void BookmarkCategory::DeleteBookmark(size_t index)
{
  if (index < m_bookmarks.size())
  {
    delete m_bookmarks[index];
    m_bookmarks.erase(m_bookmarks.begin() + index);
  }
  else
  {
    LOG(LWARNING, ("Trying to delete non-existing bookmark in category", GetName(), "at index", index));
  }
}

Bookmark const * BookmarkCategory::GetBookmark(size_t index) const
{
  return (index < m_bookmarks.size() ? m_bookmarks[index] : 0);
}

int BookmarkCategory::GetBookmark(m2::PointD const org, double const squareDistance) const
{
  for (size_t i = 0; i < m_bookmarks.size(); ++i)
  {
    if (squareDistance >= org.SquareLength(m_bookmarks[i]->GetOrg()))
      return static_cast<int>(i);
  }
  return -1;
}

namespace
{
  string PointToString(m2::PointD const & org)
  {
    double const lon = MercatorBounds::XToLon(org.x);
    double const lat = MercatorBounds::YToLat(org.y);

    ostringstream ss;
    ss.precision(8);

    ss << lon << "," << lat;
    return ss.str();
  }
}

namespace bookmark_impl
{
  class KMLParser
  {
    // Fixes icons which are not supported by MapsWithMe
    string GetSupportedBMType(string const & s) const
    {
      static char const * icons[] = {
          "placemark-red", "placemark-blue", "placemark-purple",
          "placemark-pink", "placemark-brown", "placemark-green", "placemark-orange"
      };

      // Remove leading '#' symbol
      string const result = s.substr(1);
      for (size_t i = 0; i < ARRAY_SIZE(icons); ++i)
        if (result == icons[i])
          return result;

      // Not recognized symbols are replaced with default one
      LOG(LWARNING, ("Icon", result, "for bookmark", m_name, "is not supported"));
      return icons[0];
    }

    BookmarkCategory & m_category;

    vector<string> m_tags;

    string m_name;
    string m_type;
    string m_description;
    time_t m_timeStamp;

    m2::PointD m_org;
    double m_scale;

    void Reset()
    {
      m_name.clear();
      m_description.clear();
      m_org = m2::PointD(-1000, -1000);
      m_type.clear();
      m_scale = -1.0;
      m_timeStamp = my::INVALID_TIME_STAMP;
    }

    void SetOrigin(string const & s)
    {
      // order in string is: lon, lat, z

      strings::SimpleTokenizer iter(s, ", ");
      if (iter)
      {
        double lon;
        if (strings::to_double(*iter, lon) && MercatorBounds::ValidLon(lon) && ++iter)
        {
          double lat;
          if (strings::to_double(*iter, lat) && MercatorBounds::ValidLat(lat))
            m_org = m2::PointD(MercatorBounds::LonToX(lon), MercatorBounds::LatToY(lat));
          else
            LOG(LWARNING, ("Invalid coordinates", s, "while loading bookmark", m_name));
        }
      }
    }

    bool MakeValid()
    { 
      if (MercatorBounds::ValidX(m_org.x) && MercatorBounds::ValidY(m_org.y))
      {
        // set default name
        if (m_name.empty())
          m_name = PointToString(m_org);

        // set default pin
        if (m_type.empty())
          m_type = "placemark-red";

        return true;
      }

      return false;
    }

  public:
    KMLParser(BookmarkCategory & cat) : m_category(cat)
    {
      Reset();
    }

    bool Push(string const & name)
    {
      m_tags.push_back(name);
      return true;
    }

    void AddAttr(string const &, string const &) {}

    void Pop(string const & tag)
    {
      ASSERT_EQUAL(m_tags.back(), tag, ());

      if (tag == "Placemark" && MakeValid())
      {
        Bookmark bm(m_org, m_name, m_type);
        bm.SetTimeStamp(m_timeStamp);
        bm.SetDescription(m_description);
        m_category.AddBookmarkImpl(bm, m_scale);
        Reset();
      }
      m_tags.pop_back();
    }

    void CharData(string value)
    {
      strings::Trim(value);

      size_t const count = m_tags.size();
      if (count > 1 && !value.empty())
      {
        string const & currTag = m_tags[count - 1];
        string const & prevTag = m_tags[count - 2];

        if (prevTag == "Document")
        {
          if (currTag == "name")
            m_category.SetName(value);
          else if (currTag == "visibility")
            m_category.SetVisible(value == "0" ? false : true);
        }
        else if (prevTag == "Placemark")
        {
          if (currTag == "name")
            m_name = value;
          else if (currTag == "styleUrl")
            m_type = GetSupportedBMType(value);
          else if (currTag == "description")
            m_description = value;
        }
        else if (count > 3 && m_tags[count-3] == "Placemark")
        {
          if (prevTag == "Point")
          {
            if (currTag == "coordinates")
              SetOrigin(value);
          }
          else if (prevTag == "ExtendedData")
          {
            if (currTag == "mwm:scale")
            {
              if (!strings::to_double(value, m_scale))
                m_scale = -1.0;
            }
          }
          else if (prevTag == "TimeStamp")
          {
            if (currTag == "when")
            {
              m_timeStamp = my::StringToTimestamp(value);
              if (m_timeStamp == my::INVALID_TIME_STAMP)
                LOG(LWARNING, ("Invalid timestamp in Placemark:", value));
            }
          }
        }
      }
    }
  };
}

void BookmarkCategory::LoadFromKML(ReaderPtr<Reader> const & reader)
{
  ReaderSource<ReaderPtr<Reader> > src(reader);
  bookmark_impl::KMLParser parser(*this);
  if (!ParseXML(src, parser, true))
    LOG(LERROR, ("XML read error. Probably, incorrect file encoding."));
}

BookmarkCategory * BookmarkCategory::CreateFromKMLFile(string const & file)
{
  BookmarkCategory * cat = new BookmarkCategory("");
  try
  {
    cat->LoadFromKML(new FileReader(file));
    cat->m_file = file;
  }
  catch (std::exception const & e)
  {
    LOG(LWARNING, ("Error while loading bookmarks from", file, e.what()));
    delete cat;
    cat = 0;
  }
  return cat;
}

namespace
{
char const * kmlHeader =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<kml xmlns=\"http://earth.google.com/kml/2.2\">\n"
    "<Document>\n"
    "  <Style id=\"placemark-blue\">\n"
    "    <IconStyle>\n"
    "      <Icon>\n"
    "        <href>http://mapswith.me/placemarks/placemark-blue.png</href>\n"
    "      </Icon>\n"
    "    </IconStyle>\n"
    "  </Style>\n"
    "  <Style id=\"placemark-brown\">\n"
    "    <IconStyle>\n"
    "      <Icon>\n"
    "        <href>http://mapswith.me/placemarks/placemark-brown.png</href>\n"
    "      </Icon>\n"
    "    </IconStyle>\n"
    "  </Style>\n"
    "  <Style id=\"placemark-green\">\n"
    "    <IconStyle>\n"
    "      <Icon>\n"
    "        <href>http://mapswith.me/placemarks/placemark-green.png</href>\n"
    "      </Icon>\n"
    "    </IconStyle>\n"
    "  </Style>\n"
    "  <Style id=\"placemark-orange\">\n"
    "    <IconStyle>\n"
    "      <Icon>\n"
    "        <href>http://mapswith.me/placemarks/placemark-orange.png</href>\n"
    "      </Icon>\n"
    "    </IconStyle>\n"
    "  </Style>\n"
    "  <Style id=\"placemark-pink\">\n"
    "    <IconStyle>\n"
    "      <Icon>\n"
    "        <href>http://mapswith.me/placemarks/placemark-pink.png</href>\n"
    "      </Icon>\n"
    "    </IconStyle>\n"
    "  </Style>\n"
    "  <Style id=\"placemark-purple\">\n"
    "    <IconStyle>\n"
    "      <Icon>\n"
    "        <href>http://mapswith.me/placemarks/placemark-purple.png</href>\n"
    "      </Icon>\n"
    "    </IconStyle>\n"
    "  </Style>\n"
    "  <Style id=\"placemark-red\">\n"
    "    <IconStyle>\n"
    "      <Icon>\n"
    "        <href>http://mapswith.me/placemarks/placemark-red.png</href>\n"
    "      </Icon>\n"
    "    </IconStyle>\n"
    "  </Style>\n"
;

char const * kmlFooter =
    "</Document>\n"
    "</kml>\n";
}

namespace
{
  inline void SaveStringWithCDATA(ostream & stream, string const & s)
  {
    // According to kml/xml spec, we need to escape special symbols with CDATA
    if (s.find_first_of("<&") != string::npos)
      stream << "<![CDATA[" << s << "]]>";
    else
      stream << s;
  }
}

void BookmarkCategory::SaveToKML(ostream & s)
{
  s << kmlHeader;

  // Use CDATA if we have special symbols in the name
  s << "  <name>";
  SaveStringWithCDATA(s, GetName());
  s << "</name>\n";

  s << "  <visibility>" << (IsVisible() ? "1" : "0") <<"</visibility>\n";

  for (size_t i = 0; i < m_bookmarks.size(); ++i)
  {
    Bookmark const * bm = m_bookmarks[i];
    s << "  <Placemark>\n";
    s << "    <name>";
    SaveStringWithCDATA(s, bm->GetName());
    s << "</name>\n";

    if (!bm->GetDescription().empty())
    {
      s << "    <description>";
      SaveStringWithCDATA(s, bm->GetDescription());
      s << "</description>\n";
    }

    time_t const timeStamp = bm->GetTimeStamp();
    if (timeStamp != my::INVALID_TIME_STAMP)
    {
      string const strTimeStamp = my::TimestampToString(timeStamp);
      ASSERT_EQUAL(strTimeStamp.size(), 20, ("We always generate fixed length UTC-format timestamp"));
      s << "    <TimeStamp><when>" << strTimeStamp << "</when></TimeStamp>\n";
    }

    s << "    <styleUrl>#" << bm->GetType() << "</styleUrl>\n"
      << "    <Point><coordinates>" << PointToString(bm->GetOrg()) << "</coordinates></Point>\n";

    double const scale = bm->GetScale();
    if (scale != -1.0)
    {
      /// @todo Factor out to separate function to use for other custom params.
      s << "    <ExtendedData xmlns:mwm=\"http://mapswith.me\">\n"
        << "      <mwm:scale>" << bm->GetScale() << "</mwm:scale>\n"
        << "    </ExtendedData>\n";
    }

    s << "  </Placemark>\n";
  }

  s << kmlFooter;
}

namespace
{
  bool IsBadCharForPath(strings::UniChar const & c)
  {
    static strings::UniChar const illegalChars[] = {':', '/', '\\', '<', '>', '\"', '|', '?', '*'};

    for (size_t i = 0; i < ARRAY_SIZE(illegalChars); ++i)
      if (c < ' ' || illegalChars[i] == c)
        return true;

    return false;
  }
}

string BookmarkCategory::RemoveInvalidSymbols(string const & name)
{
  // Remove not allowed symbols
  strings::UniString uniName = strings::MakeUniString(name);
  strings::UniString::iterator iEnd = remove_if(uniName.begin(), uniName.end(), &IsBadCharForPath);
  if (iEnd != uniName.end())
  {
    // buffer_vector doesn't have erase function - call resize here (it's even better in this case).
    uniName.resize(distance(uniName.begin(), iEnd));
  }

  return (uniName.empty() ? "Bookmarks" : strings::ToUtf8(uniName));
}

string BookmarkCategory::GenerateUniqueFileName(const string & path, string name)
{
  string const kmlExt(".kml");
  // check if file name already contains .kml extension
  size_t const extPos = name.rfind(kmlExt);
  if (extPos == name.size() - kmlExt.size())
    name.resize(name.size() - kmlExt.size());

  size_t counter = 1;
  string suffix;
  while (Platform::IsFileExistsByFullPath(path + name + suffix + ".kml"))
    suffix = strings::to_string(counter++);
  return (path + name + suffix + ".kml");
}

bool BookmarkCategory::SaveToKMLFile()
{
  string oldFile;

  // Get valid file name from category name
  string const name = RemoveInvalidSymbols(m_name);

  if (!m_file.empty())
  {
    size_t i2 = m_file.find_last_of('.');
    if (i2 == string::npos)
      i2 = m_file.size();
    size_t i1 = m_file.find_last_of("\\/");
    if (i1 == string::npos)
      i1 = 0;
    else
      ++i1;

    // If m_file doesn't match name, assign new m_file for this category and save old file name.
    if (m_file.substr(i1, i2 - i1).find(name) != 0)
    {
      oldFile = GenerateUniqueFileName(GetPlatform().WritableDir(), name);
      m_file.swap(oldFile);
    }
  }
  else
    m_file = GenerateUniqueFileName(GetPlatform().WritableDir(), name);

  try
  {
    /// @todo On Windows UTF-8 file names are not supported.
    ofstream of(m_file.c_str());
    SaveToKML(of);
    of.flush();

    if (!of.fail())
    {
      // delete old file
      if (!oldFile.empty())
        VERIFY ( my::DeleteFileX(oldFile), (oldFile, m_file));

      return true;
    }
  }
  catch (std::exception const & e)
  {
    LOG(LWARNING, ("Exception while saving bookmarks:", e.what()));
  }

  LOG(LWARNING, ("Can't save bookmarks category", m_name, "to file", m_file));

  // return old file name in case of error
  if (!oldFile.empty())
    m_file.swap(oldFile);

  return false;
}
