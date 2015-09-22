#include "xml_element.hpp"

#include "../coding/parse_xml.hpp"
#include "../base/string_utils.hpp"

#include "../std/cstdio.hpp"
#include "../std/algorithm.hpp"


void XMLElement::AddKV(string const & k, string const & v)
{
  childs.push_back(XMLElement());
  XMLElement & e = childs.back();

  e.tagKey = ET_TAG;
  e.k = k;
  e.v = v;
  e.parent = this;
}

void BaseOSMParser::AddAttr(string const & key, string const & value)
{
  if (m_current)
  {
    if (key == "id")
      CHECK ( strings::to_uint64(value, m_current->id), ("Unknown element with invalid id : ", value) );
    else if (key == "lon")
      CHECK ( strings::to_double(value, m_current->lng), ("Bad node lon : ", value) );
    else if (key == "lat")
      CHECK ( strings::to_double(value, m_current->lat), ("Bad node lat : ", value) );
    else if (key == "ref")
      CHECK ( strings::to_uint64(value, m_current->ref), ("Bad node ref in way : ", value) );
    else if (key == "k")
      m_current->k = value;
    else if (key == "v")
      m_current->v = value;
    else if (key == "type")
      m_current->type = value;
    else if (key == "role")
      m_current->role = value;
  }
}

bool BaseOSMParser::MatchTag(string const & tagName, XMLElement::ETag & tagKey)
{
  /// as tagKey we use first two char of tag name
  tagKey = XMLElement::ETag(*reinterpret_cast<uint16_t const *>(tagName.data()));
  switch (tagKey)
  {
    /// this tags will ignored in Push function
    case XMLElement::ET_MEMBER:
    case XMLElement::ET_TAG:
    case XMLElement::ET_ND:
      return false;
    default:
      return true;
  }
  return true;
}

bool BaseOSMParser::Push(string const & tagName)
{
  XMLElement::ETag tagKey;

  if (!MatchTag(tagName, tagKey) && (m_depth != 2))
    return false;

  ++m_depth;

  if (m_depth == 1)
  {
    m_current = 0;
  }
  else if (m_depth == 2)
  {
    m_current = &m_element;
    m_current->parent = 0;
  }
  else
  {
    m_current->childs.push_back(XMLElement());
    m_current->childs.back().parent = m_current;
    m_current = &m_current->childs.back();
  }

  if (m_depth >= 2)
    m_current->tagKey = tagKey;
  return true;
}

void BaseOSMParser::Pop(string const &)
{
  --m_depth;

  if (m_depth >= 2)
    m_current = m_current->parent;

  else if (m_depth == 1)
  {
    EmitElement(m_current);
    (*m_current) = XMLElement();
  }
}

namespace
{
  struct StdinReader
  {
    uint64_t Read(char * buffer, uint64_t bufferSize)
    {
      return fread(buffer, sizeof(char), bufferSize, stdin);
    }
  };

  struct FileReader
  {
    FILE * m_file;

    FileReader(string const & filename)
    {
      m_file = fopen(filename.c_str(), "rb");
    }

    ~FileReader()
    {
      fclose(m_file);
    }

    uint64_t Read(char * buffer, uint64_t bufferSize)
    {
      return fread(buffer, sizeof(char), bufferSize, m_file);
    }
  };
}

void ParseXMLFromStdIn(BaseOSMParser & parser)
{
  StdinReader reader;
  (void)ParseXMLSequence(reader, parser);
}

void ParseXMLFromFile(BaseOSMParser & parser, string const & osmFileName)
{
  FileReader reader(osmFileName);
  (void)ParseXMLSequence(reader, parser);
}
