#include "editor/config_loader.hpp"
#include "editor/editor_config.hpp"

#include "platform/platform.hpp"

#include "coding/internal/file_data.hpp"
#include "coding/reader.hpp"

#include "std/fstream.hpp"
#include "std/iterator.hpp"

#include "3party/Alohalytics/src/http_client.h"
#include "3party/pugixml/src/pugixml.hpp"

namespace
{
using alohalytics::HTTPClientPlatformWrapper;

auto const kConfigFileName = "editor.config";
auto const kHashFileName = "editor.config.hash";

auto const kSynchroTimeout = hours(4);
auto const kRemoteHashUrl = "http://osmz.ru/mwm/editor.config.date";
auto const kRemoteConfigUrl = "http://osmz.ru/mwm/editor.config";

string GetConfigFilePath() { return GetPlatform().WritablePathForFile(kConfigFileName); }
string GetHashFilePath() { return GetPlatform().WritablePathForFile(kHashFileName); }

string RunSimpleHttpRequest(string const & url)
{
  HTTPClientPlatformWrapper request(url);
  auto const result = request.RunHTTPRequest();

  if (result && !request.was_redirected() && request.error_code() == 200)  // 200 - http status OK
  {
    return request.server_response();
  }

  return {};
}
}  // namespace

namespace editor
{
void Waiter::Interrupt()
{
  {
    lock_guard<mutex> lock(m_mutex);
    m_interrupted = true;
  }

  m_event.notify_all();
}

ConfigLoader::ConfigLoader(EditorConfigWrapper & config) : m_config(config)
{
  pugi::xml_document doc;
  LoadFromLocal(doc);
  ResetConfig(doc);
  m_loaderThread = thread(&ConfigLoader::LoadFromServer, this);
}

ConfigLoader::~ConfigLoader()
{
  m_waiter.Interrupt();
  m_loaderThread.join();
}

void ConfigLoader::LoadFromServer()
{
  auto const hash = LoadHash(GetHashFilePath());

  try
  {
    do
    {
      auto const remoteHash = GetRemoteHash();
      if (remoteHash.empty() || hash == remoteHash)
        continue;

      pugi::xml_document doc;
      GetRemoteConfig(doc);

      if (SaveAndReload(doc))
        SaveHash(remoteHash, GetHashFilePath());

    } while (m_waiter.Wait(kSynchroTimeout));
  }
  catch (RootException const & ex)
  {
    LOG(LERROR, (ex.Msg()));
  }
}

bool ConfigLoader::SaveAndReload(pugi::xml_document const & doc)
{
  if (doc.empty())
    return false;

  auto const filePath = GetConfigFilePath();
  auto const result =
    my::WriteToTempAndRenameToFile(filePath, [&doc](string const & fileName)
    {
      return doc.save_file(fileName.c_str(), "  ");
    });

  if (!result)
    return false;

  ResetConfig(doc);
  return true;
}

void ConfigLoader::ResetConfig(pugi::xml_document const & doc)
{
  auto config = make_shared<EditorConfig>();
  config->SetConfig(doc);
  m_config.Set(config);
}

// static
void ConfigLoader::LoadFromLocal(pugi::xml_document & doc)
{
  string content;
  auto const reader = GetPlatform().GetReader(kConfigFileName);
  reader->ReadAsString(content);

  if (!doc.load_buffer(content.data(), content.size()))
    MYTHROW(ConfigLoadError, ("Can't parse config"));
}

// static
string ConfigLoader::GetRemoteHash()
{
  return RunSimpleHttpRequest(kRemoteHashUrl);
}

// static
void ConfigLoader::GetRemoteConfig(pugi::xml_document & doc)
{
  auto const result = RunSimpleHttpRequest(kRemoteConfigUrl);
  if (result.empty())
    return;

  if (!doc.load_string(result.c_str(), pugi::parse_default | pugi::parse_comments))
    doc.reset();
}

// static
bool ConfigLoader::SaveHash(string const & hash, string const & filePath)
{
  auto const result =
    my::WriteToTempAndRenameToFile(filePath, [&hash](string const & fileName)
    {
      ofstream ofs(fileName, ofstream::out);
      if (!ofs.is_open())
        return false;
      
      ofs.write(hash.data(), hash.size());
      return true;
    });

  return result;
}

// static
string ConfigLoader::LoadHash(string const & filePath)
{
  ifstream ifs(filePath, ifstream::in);
  if (!ifs.is_open())
    return {};

  return {istreambuf_iterator<char>(ifs), istreambuf_iterator<char>()};
}
}  // namespace editor
