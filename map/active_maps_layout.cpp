#include "active_maps_layout.hpp"
#include "framework.hpp"

#include "../std/algorithm.hpp"


namespace storage
{

ActiveMapsLayout::ActiveMapsLayout(Framework & framework)
  : m_framework(framework)
{
  m_subscribeSlotID = GetStorage().Subscribe(bind(&ActiveMapsLayout::StatusChangedCallback, this, _1),
                                             bind(&ActiveMapsLayout::ProgressChangedCallback, this, _1, _2));
}

ActiveMapsLayout::~ActiveMapsLayout()
{
  GetStorage().Unsubscribe(m_subscribeSlotID);
}

void ActiveMapsLayout::Init(vector<string> const & maps)
{
  Clear();

  Storage & storage = GetStorage();
  for (auto const & file : maps)
  {
    TIndex const index = storage.FindIndexByFile(Storage::MapWithoutExt(file));
    if (index.IsValid())
    {
      TStatus status;
      TMapOptions options;
      storage.CountryStatusEx(index, status, options);
      if (status == TStatus::EOnDisk || status == TStatus::EOnDiskOutOfDate)
        m_items.push_back({ index, status, options, options });
    }
    else
      LOG(LWARNING, ("Can't find map index for", file));
  }

  auto const comparatorFn = [&storage] (Item const & lhs, Item const & rhs)
  {
    if (lhs.m_status != rhs.m_status)
      return lhs.m_status > rhs.m_status;
    else
      return storage.CountryName(lhs.m_index) < storage.CountryName(rhs.m_index);
  };

  sort(m_items.begin(), m_items.end(), comparatorFn);

  m_split = { 0, m_items.size() };
  for (size_t i = 0; i < m_items.size(); ++i)
  {
    if (m_items[i].m_status == TStatus::EOnDisk)
    {
      m_split.second = i;
      break;
    }
  }
}

void ActiveMapsLayout::Clear()
{
  m_items.clear();
  m_split = { 0, 0 };
}

size_t ActiveMapsLayout::GetSizeToUpdateAllInBytes() const
{
  size_t result = 0;
  for (int i = m_split.first; i < m_split.second; ++i)
  {
    Item const & item = m_items[i];
    if (item.m_status != TStatus::EInQueue && item.m_status != TStatus::EDownloading)
      result += GetStorage().CountryByIndex(item.m_index).GetFile().GetRemoteSize(item.m_options);
  }

  return result;
}

void ActiveMapsLayout::UpdateAll()
{
  vector<Item> toDownload;
  toDownload.reserve(m_items.size());
  for (int i = m_split.first; i < m_split.second; ++i)
  {
    Item const & item = m_items[i];
    if (item.m_status != TStatus::EInQueue && item.m_status != TStatus::EDownloading)
      toDownload.push_back(item);
  }

  for (Item const & item : toDownload)
    DownloadMap(item.m_index, item.m_options);
}

void ActiveMapsLayout::CancelAll()
{
  vector<TIndex> downloading;
  downloading.reserve(m_items.size());
  for (Item const & item : m_items)
  {
    if (item.m_status == TStatus::EInQueue || item.m_status == TStatus::EDownloading)
      downloading.push_back(item.m_index);
  }

  Storage & st = m_framework.Storage();
  for (TIndex const & index : downloading)
    st.DeleteFromDownloader(index);
}

int ActiveMapsLayout::GetOutOfDateCount() const
{
  int result = 0;
  for (size_t i = m_split.first; i < m_split.second; ++i)
  {
    Item const & item = m_items[i];
    if (item.m_status != TStatus::ENotDownloaded)
      ++result;
  }

  return result;
}

int ActiveMapsLayout::GetCountInGroup(TGroup const & group) const
{
  int result = 0;
  switch (group)
  {
  case TGroup::ENewMap:
    result = m_split.first;
    break;
  case TGroup::EOutOfDate:
    result = m_split.second - m_split.first;
    break;
  case TGroup::EUpToDate:
    result = m_items.size() - m_split.second;
    break;
  default:
    ASSERT(false, ());
  }

  return result;
}

bool ActiveMapsLayout::IsEmpty() const
{
  return GetCountInGroup(TGroup::ENewMap) == 0 && GetCountInGroup(TGroup::EOutOfDate) == 0 && GetCountInGroup(TGroup::EUpToDate) == 0;
}

string const & ActiveMapsLayout::GetCountryName(TGroup const & group, int position) const
{
  return GetCountryName(GetItemInGroup(group, position).m_index);
}

string const & ActiveMapsLayout::GetCountryName(TIndex const & index) const
{
  return GetStorage().CountryName(index);
}

TStatus ActiveMapsLayout::GetCountryStatus(TGroup const & group, int position) const
{
  return GetItemInGroup(group, position).m_status;
}

TStatus ActiveMapsLayout::GetCountryStatus(TIndex const & index) const
{
  Item const * item = FindItem(index);
  if (item != nullptr)
    return item->m_status;

  return GetStorage().CountryStatusEx(index);
}

TMapOptions ActiveMapsLayout::GetCountryOptions(TGroup const & group, int position) const
{
  return GetItemInGroup(group, position).m_options;
}

TMapOptions ActiveMapsLayout::GetCountryOptions(TIndex const & index) const
{
  Item const * item = FindItem(index);
  if (item)
    return item->m_options;

  TStatus status;
  TMapOptions options;
  GetStorage().CountryStatusEx(index, status, options);
  return options;
}

LocalAndRemoteSizeT const ActiveMapsLayout::GetDownloadableCountrySize(TGroup const & group, int position) const
{
  Item const & item = GetItemInGroup(group, position);
  return GetStorage().CountrySizeInBytes(item.m_index, item.m_downloadRequest);
}

LocalAndRemoteSizeT const ActiveMapsLayout::GetDownloadableCountrySize(TIndex const & index) const
{
  Item const * item = FindItem(index);
  ASSERT(item, ());
  return GetStorage().CountrySizeInBytes(index, item->m_downloadRequest);
}

LocalAndRemoteSizeT const ActiveMapsLayout::GetCountrySize(TGroup const & group, int position,
                                                           TMapOptions const & options) const
{
  return GetCountrySize(GetItemInGroup(group, position).m_index, options);
}

LocalAndRemoteSizeT const ActiveMapsLayout::GetCountrySize(TIndex const & index, TMapOptions const & options) const
{
  return GetStorage().CountrySizeInBytes(index, options);
}

LocalAndRemoteSizeT const ActiveMapsLayout::GetRemoteCountrySizes(TGroup const & group, int position) const
{
  return GetRemoteCountrySizes(GetItemInGroup(group, position).m_index);
}

LocalAndRemoteSizeT const ActiveMapsLayout::GetRemoteCountrySizes(TIndex const & index) const
{
  CountryFile const & c = GetStorage().CountryByIndex(index).GetFile();
  size_t const mapSize = c.GetRemoteSize(TMapOptions::EMapOnly);
  return { mapSize, c.GetRemoteSize(TMapOptions::ECarRouting) };
}

int ActiveMapsLayout::AddListener(ActiveMapsListener * listener)
{
  m_listeners[m_currentSlotID] = listener;
  return m_currentSlotID++;
}

void ActiveMapsLayout::RemoveListener(int slotID)
{
  m_listeners.erase(slotID);
}

void ActiveMapsLayout::DownloadMap(TIndex const & index, TMapOptions const & options)
{
  TMapOptions validOptions = ValidOptionsForDownload(options);
  Item * item = FindItem(index);
  if (item)
  {
    ASSERT(item != nullptr, ());
    item->m_downloadRequest = validOptions;
  }
  else
  {
    int position = InsertInGroup(TGroup::ENewMap, { index, TStatus::ENotDownloaded, validOptions, validOptions });
    NotifyInsertion(TGroup::ENewMap, position);
  }

  m_framework.DownloadCountry(index, validOptions);
}

void ActiveMapsLayout::DownloadMap(TGroup const & group, int position, TMapOptions const & options)
{
  Item & item = GetItemInGroup(group, position);
  item.m_downloadRequest = ValidOptionsForDownload(options);
  m_framework.DownloadCountry(item.m_index, item.m_downloadRequest);
}

void ActiveMapsLayout::DeleteMap(TIndex const & index, const TMapOptions & options)
{
  TGroup group;
  int position;
  VERIFY(GetGroupAndPositionByIndex(index, group, position), ());
  DeleteMap(group, position, options);
}

void ActiveMapsLayout::DeleteMap(TGroup const & group, int position, TMapOptions const & options)
{
  TIndex indexCopy = GetItemInGroup(group, position).m_index;
  m_framework.DeleteCountry(indexCopy, ValidOptionsForDelete(options));
}

void ActiveMapsLayout::RetryDownloading(TGroup const & group, int position)
{
  Item const & item = GetItemInGroup(group, position);
  ASSERT(item.m_options != item.m_downloadRequest, ());
  m_framework.DownloadCountry(item.m_index, item.m_downloadRequest);
}

void ActiveMapsLayout::RetryDownloading(TIndex const & index)
{
  Item * item = FindItem(index);
  ASSERT(item != nullptr, ());
  m_framework.DownloadCountry(item->m_index, item->m_downloadRequest);
}

TIndex const & ActiveMapsLayout::GetCoreIndex(TGroup const & group, int position) const
{
  return GetItemInGroup(group, position).m_index;
}

string const ActiveMapsLayout::GetFormatedCountryName(TIndex const & index)
{
  string group, country;
  GetStorage().GetGroupAndCountry(index, group, country);
  if (!group.empty())
    return country + " (" + group + ")";
  else
    return country;
}

bool ActiveMapsLayout::IsDownloadingActive() const
{
  auto iter = find_if(m_items.begin(), m_items.end(), [] (Item const & item)
  {
    return item.m_status == TStatus::EDownloading || item.m_status == TStatus::EInQueue;
  });

  return iter != m_items.end();
}

void ActiveMapsLayout::CancelDownloading(TGroup const & group, int position)
{
  Item & item = GetItemInGroup(group, position);
  GetStorage().DeleteFromDownloader(item.m_index);
  item.m_downloadRequest = item.m_options;
}

void ActiveMapsLayout::ShowMap(TGroup const & group, int position)
{
  ShowMap(GetItemInGroup(group, position).m_index);
}

void ActiveMapsLayout::ShowMap(TIndex const & index)
{
  m_framework.ShowCountry(index);
}

Storage const & ActiveMapsLayout::GetStorage() const
{
  return m_framework.Storage();
}

Storage & ActiveMapsLayout::GetStorage()
{
  return m_framework.Storage();
}

void ActiveMapsLayout::StatusChangedCallback(TIndex const & index)
{
  TStatus newStatus = TStatus::EUnknown;
  TMapOptions options = TMapOptions::EMapOnly;
  GetStorage().CountryStatusEx(index, newStatus, options);

  TGroup group = TGroup::ENewMap;
  int position = 0;
  VERIFY(GetGroupAndPositionByIndex(index, group, position), (index));
  Item & item = GetItemInGroup(group, position);

  TStatus oldStatus = item.m_status;
  item.m_status = newStatus;

  if (newStatus == TStatus::EOnDisk)
  {
    if (group != TGroup::EUpToDate)
    {
      // Here we handle
      // "NewMap" -> "Actual Map without routing"
      // "NewMap" -> "Actual Map with routing"
      // "OutOfDate without routing" -> "Actual map without routing"
      // "OutOfDate without routing" -> "Actual map with routing"
      // "OutOfDate with Routing" -> "Actual map with routing"
      //   For "NewMaps" always true - m_options == m_m_downloadRequest == options
      //   but we must notify that options changed because for "NewMaps" m_options is virtual state
      if (item.m_options != options || group == TGroup::ENewMap)
      {
        TMapOptions requestOptions = options;
        item.m_downloadRequest = item.m_options = options;
        NotifyOptionsChanged(group, position, item.m_options, requestOptions);
      }

      int newPosition = MoveItemToGroup(group, position, TGroup::EUpToDate);
      NotifyMove(group, position, TGroup::EUpToDate, newPosition);
      group = TGroup::EUpToDate;
      position = newPosition;
    }
    else if (item.m_options != options)
    {
      // Here we handle
      // "Actual map without routing" -> "Actual map with routing"
      // "Actual map with routing" -> "Actual map without routing"
      TMapOptions requestOpt = item.m_downloadRequest;
      item.m_options = item.m_downloadRequest = options;
      NotifyOptionsChanged(group, position, item.m_options, requestOpt);
    }
    NotifyStatusChanged(group, position, oldStatus, item.m_status);
  }
  else if (newStatus == TStatus::ENotDownloaded)
  {
    if (group == TGroup::ENewMap)
    {
      // "NewMap downloading" -> "Cancel downloading"
      // We handle here only status change for "New maps"
      // because if new status ENotDownloaded than item.m_options is invalid.
      // Map have no options and gui not show routing icon
      NotifyStatusChanged(group, position, oldStatus, item.m_status);
    }
    else
    {
      // "Actual of not map been deleted"
      // We not notify about options changed!
      NotifyStatusChanged(group, position, oldStatus, item.m_status);
      DeleteFromGroup(group, position);
      NotifyDeletion(group, position);
    }
  }
  else if (newStatus == TStatus::EOnDiskOutOfDate)
  {
    // We can drop here only if user start update some map and cancel it
    NotifyStatusChanged(group, position, oldStatus, item.m_status);

    ASSERT(item.m_options == options, ());
    item.m_downloadRequest = item.m_options = options;
  }
  else
  {
    // EDownloading
    // EInQueue
    // downloadig faild for some reason
    NotifyStatusChanged(group, position, oldStatus, item.m_status);
  }
}

void ActiveMapsLayout::ProgressChangedCallback(TIndex const & index, LocalAndRemoteSizeT const & sizes)
{
  if (m_listeners.empty())
    return;

  TGroup group = TGroup::ENewMap;
  int position = 0;
  VERIFY(GetGroupAndPositionByIndex(index, group, position), ());
  for (TListenerNode listener : m_listeners)
    listener.second->DownloadingProgressUpdate(group, position, sizes);
}

ActiveMapsLayout::Item const & ActiveMapsLayout::GetItemInGroup(TGroup const & group, int position) const
{
  int index = GetStartIndexInGroup(group) + position;
  ASSERT(index < m_items.size(), ());
  return m_items[index];
}

ActiveMapsLayout::Item & ActiveMapsLayout::GetItemInGroup(TGroup const & group, int position)
{
  int index = GetStartIndexInGroup(group) + position;
  ASSERT(index < m_items.size(), ());
  return m_items[index];
}

int ActiveMapsLayout::GetStartIndexInGroup(TGroup const & group) const
{
  switch (group)
  {
    case TGroup::ENewMap: return 0;
    case TGroup::EOutOfDate: return m_split.first;
    case TGroup::EUpToDate: return m_split.second;
    default:
      ASSERT(false, ());
  }

  return m_items.size();
}

ActiveMapsLayout::Item * ActiveMapsLayout::FindItem(TIndex const & index)
{
  vector<Item>::iterator iter = find_if(m_items.begin(), m_items.end(), [&index] (Item const & item)
  {
    return item.m_index == index;
  });

  if (iter == m_items.end())
    return nullptr;

  return &(*iter);
}

ActiveMapsLayout::Item const * ActiveMapsLayout::FindItem(TIndex const & index) const
{
  vector<Item>::const_iterator iter = find_if(m_items.begin(), m_items.end(), [&index] (Item const & item)
  {
    return item.m_index == index;
  });

  if (iter == m_items.end())
    return nullptr;

  return &(*iter);
}

bool ActiveMapsLayout::GetGroupAndPositionByIndex(TIndex const & index, TGroup & group, int & position)
{
  auto it = find_if(m_items.begin(), m_items.end(), [&index] (Item const & item)
  {
    return item.m_index == index;
  });

  if (it == m_items.end())
    return false;

  int vectorIndex = distance(m_items.begin(), it);
  if (vectorIndex >= m_split.second)
  {
    group = TGroup::EUpToDate;
    position = vectorIndex - m_split.second;
  }
  else if (vectorIndex >= m_split.first)
  {
    group = TGroup::EOutOfDate;
    position = vectorIndex - m_split.first;
  }
  else
  {
    group = TGroup::ENewMap;
    position = vectorIndex;
  }

  return true;
}

int ActiveMapsLayout::InsertInGroup(TGroup const & group, Item const & item)
{
  typedef vector<Item>::iterator TItemIter;
  TItemIter startSort;
  TItemIter endSort;

  if (group == TGroup::ENewMap)
  {
    m_items.insert(m_items.begin(), item);
    ++m_split.first;
    ++m_split.second;
    startSort = m_items.begin();
    endSort = m_items.begin() + m_split.first;
  }
  else if (group == TGroup::EOutOfDate)
  {
    m_items.insert(m_items.begin() + m_split.first, item);
    ++m_split.second;
    startSort = m_items.begin() + m_split.first;
    endSort = m_items.begin() + m_split.second;
  }
  else
  {
    m_items.insert(m_items.begin() + m_split.second, item);
    startSort = m_items.begin() + m_split.second;
    endSort = m_items.end();
  }

  Storage & st = m_framework.Storage();
  sort(startSort, endSort, [&] (Item const & lhs, Item const & rhs)
  {
    return st.CountryName(lhs.m_index) < st.CountryName(rhs.m_index);
  });

  TItemIter newPosIter = find_if(startSort, endSort, [&item] (Item const & it)
  {
    return it.m_index == item.m_index;
  });

  ASSERT(m_split.first <= m_items.size(), ());
  ASSERT(m_split.second <= m_items.size(), ());

  ASSERT(newPosIter != m_items.end(), ());
  return distance(startSort, newPosIter);
}

void ActiveMapsLayout::DeleteFromGroup(TGroup const & group, int position)
{
  if (group == TGroup::ENewMap)
  {
    --m_split.first;
    --m_split.second;
  }
  else if (group == TGroup::EOutOfDate)
  {
    --m_split.second;
  }

  ASSERT(m_split.first >= 0, ());
  ASSERT(m_split.second >= 0, ());

  m_items.erase(m_items.begin() + GetStartIndexInGroup(group) + position);
}

int ActiveMapsLayout::MoveItemToGroup(TGroup const & group, int position, TGroup const & newGroup)
{
  Item item = GetItemInGroup(group, position);
  DeleteFromGroup(group, position);
  return InsertInGroup(newGroup, item);
}

void ActiveMapsLayout::NotifyInsertion(TGroup const & group, int position)
{
  for (TListenerNode listener : m_listeners)
    listener.second->CountryGroupChanged(group, -1, group, position);
}

void ActiveMapsLayout::NotifyDeletion(TGroup const & group, int position)
{
  for (TListenerNode listener : m_listeners)
    listener.second->CountryGroupChanged(group, position, group, -1);
}

void ActiveMapsLayout::NotifyMove(TGroup const & oldGroup, int oldPosition,
                                  TGroup const & newGroup, int newPosition)
{
  for (TListenerNode listener : m_listeners)
    listener.second->CountryGroupChanged(oldGroup, oldPosition, newGroup, newPosition);
}

void ActiveMapsLayout::NotifyStatusChanged(TGroup const & group, int position,
                                           TStatus const & oldStatus, TStatus const & newStatus)
{
  for (TListenerNode listener : m_listeners)
    listener.second->CountryStatusChanged(group, position, oldStatus, newStatus);
}

void ActiveMapsLayout::NotifyOptionsChanged(TGroup const & group, int position,
                                            TMapOptions const & oldOpt, TMapOptions const & newOpt)
{
  for (TListenerNode listener : m_listeners)
    listener.second->CountryOptionsChanged(group, position, oldOpt, newOpt);
}

TMapOptions ActiveMapsLayout::ValidOptionsForDownload(TMapOptions const & options)
{
  return options | TMapOptions::EMapOnly;
}

TMapOptions ActiveMapsLayout::ValidOptionsForDelete(TMapOptions const & options)
{
  if (options & TMapOptions::EMapOnly)
    return options | TMapOptions::ECarRouting;
  return options;
}

}
