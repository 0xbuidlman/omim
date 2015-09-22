#include "country_status_display.hpp"

#include "../gui/controller.hpp"

#include "../graphics/overlay_renderer.hpp"

#include "../platform/platform.hpp"

#include "../base/string_format.hpp"

#include "../std/bind.hpp"
#include "../std/sstream.hpp"


string const CountryStatusDisplay::displayName() const
{
  if (!m_mapGroupName.empty())
    return m_mapName + " (" + m_mapGroupName + ")";
  else
    return m_mapName;
}

template <class T1, class T2>
void CountryStatusDisplay::SetStatusMessage(string const & msgID, T1 const * t1, T2 const * t2)
{
  string msg = m_controller->GetStringsBundle()->GetString(msgID);
  if (t1)
  {
    if (t2)
      msg = strings::Format(msg, *t1, *t2);
    else
    {
      msg = strings::Format(msg, *t1);

      size_t const count = msg.size();
      if (count > 0)
      {
        if (msg[count-1] == '\n')
          msg.erase(count-1, 1);
      }
    }
  }

  m_statusMsg->setIsVisible(true);
  m_statusMsg->setText(msg);
}

void CountryStatusDisplay::cache()
{
  m_downloadButton->setIsVisible(false);
  m_statusMsg->setIsVisible(false);
  setIsVisible(false);
  setIsDirtyRect(true);

  string const dn = displayName();

  if (m_countryIdx != storage::TIndex())
  {
    switch (m_countryStatus)
    {
    case storage::EInQueue:
    {
      string const s = dn + "\n";
      SetStatusMessage<string, int>("country_status_added_to_queue", &s);
      break;
    }

    case storage::EDownloading:
    {
      int const percent = m_countryProgress.first * 100 / m_countryProgress.second;
      string const s = "\n" + dn + "\n";
      SetStatusMessage<string, int>("country_status_downloading", &s, &percent);
      break;
    }

    case storage::ENotDownloaded:
      if (m_notEnoughSpace)
        SetStatusMessage<int, int>("not_enough_free_space_on_sdcard");
      else
      {
        m_downloadButton->setIsVisible(true);
        string const msg = m_controller->GetStringsBundle()->GetString("country_status_download");
        m_downloadButton->setText(strings::Format(msg, "\n" + dn));
      }
      break;

    case storage::EDownloadFailed:
    {
      m_downloadButton->setIsVisible(true);
      m_downloadButton->setText(m_controller->GetStringsBundle()->GetString("try_again"));

      string const s = "\n" + dn + "\n";
      SetStatusMessage<string, int>("country_status_download_failed", &s);

      setPivot(pivot());
      break;
    }

    default:
      return;
    }
  }

  setIsVisible(m_statusMsg->isVisible() || m_downloadButton->isVisible());

  // Element bound rect is possibly changed,
  // however it's called in the beginning of the function.
  setIsDirtyRect(true);
}

void CountryStatusDisplay::CountryStatusChanged(storage::TIndex const & idx)
{
  if (idx == m_countryIdx)
  {
    UpdateStatusAndProgress();

    setIsDirtyDrawing(true);
    invalidate();
  }
}

void CountryStatusDisplay::CountryProgress(storage::TIndex const & idx, pair<int64_t, int64_t> const & progress)
{
  if ((m_countryIdx == idx) && (m_countryStatus == storage::EDownloading))
  {
    m_countryProgress = progress;
    setIsDirtyDrawing(true);
    invalidate();
  }
}

CountryStatusDisplay::CountryStatusDisplay(Params const & p)
  : gui::Element(p), m_storage(p.m_storage)
{
  m_slotID = m_storage->Subscribe(bind(&CountryStatusDisplay::CountryStatusChanged, this, _1),
                                  bind(&CountryStatusDisplay::CountryProgress, this, _1, _2));

  gui::Button::Params bp;

  bp.m_depth = graphics::maxDepth - 10;
  bp.m_minWidth = 200;
  bp.m_minHeight = 40;
  bp.m_pivot = m2::PointD(0, 0);
  bp.m_position = graphics::EPosCenter;
  bp.m_text = "Download";

  m_downloadButton.reset(new gui::Button(bp));
  m_downloadButton->setOnClickListener(bind(&CountryStatusDisplay::downloadCountry, this));
  m_downloadButton->setIsVisible(false);
  m_downloadButton->setPosition(graphics::EPosCenter);

  m_downloadButton->setFont(EActive, graphics::FontDesc(16, graphics::Color(255, 255, 255, 255)));
  m_downloadButton->setFont(EPressed, graphics::FontDesc(16, graphics::Color(255, 255, 255, 255)));

  m_downloadButton->setColor(EActive, graphics::Color(graphics::Color(0, 0, 0, 0.6 * 255)));
  m_downloadButton->setColor(EPressed, graphics::Color(graphics::Color(0, 0, 0, 0.4 * 255)));

  gui::TextView::Params tp;
  tp.m_depth = graphics::maxDepth - 10;
  tp.m_pivot = m2::PointD(0, 0);
  tp.m_text = "Downloading";

  m_statusMsg.reset(new gui::TextView(tp));

  m_statusMsg->setIsVisible(false);
  m_statusMsg->setPosition(graphics::EPosCenter);

  m_statusMsg->setFont(gui::Element::EActive, graphics::FontDesc(18));

  setIsVisible(false);

  m_countryIdx = storage::TIndex();
  m_countryStatus = storage::EUnknown;
  m_notEnoughSpace = false;
}

CountryStatusDisplay::~CountryStatusDisplay()
{
  m_storage->Unsubscribe(m_slotID);
}

void CountryStatusDisplay::downloadCountry()
{
  if (GetPlatform().GetWritableStorageStatus(m_countryProgress.second) != Platform::STORAGE_OK)
  {
    m_notEnoughSpace = true;

    setIsDirtyDrawing(true);
    invalidate();
  }
  else
    m_storage->DownloadCountry(m_countryIdx);
}

void CountryStatusDisplay::setDownloadListener(gui::Button::TOnClickListener const & l)
{
  m_downloadButton->setOnClickListener(l);
}

void CountryStatusDisplay::UpdateStatusAndProgress()
{
  // Actually right now actual status is getting from Framework.
  // But here it's enough to get it from Storage because of we need only
  // download progress statuses.

  using namespace storage;

  m_countryProgress = m_storage->CountrySizeInBytes(m_countryIdx);

  m_countryStatus = m_storage->CountryStatus(m_countryIdx);
  if (m_countryStatus == EUnknown)
  {
    if (m_countryProgress.first > 0)
      m_countryStatus = EOnDisk;
    else
      m_countryStatus = ENotDownloaded;
  }
}

void CountryStatusDisplay::setCountryName(string const & name)
{
  if (m_fullName != name)
  {
    storage::CountryInfo::FullName2GroupAndMap(name, m_mapGroupName, m_mapName);
    LOG(LDEBUG, (m_mapName, m_mapGroupName));

    m_countryIdx = m_storage->FindIndexByName(m_mapName);
    UpdateStatusAndProgress();

    m_fullName = name;
    m_notEnoughSpace = false;

    setIsDirtyDrawing(true);
    invalidate();
  }
}

void CountryStatusDisplay::draw(graphics::gl::OverlayRenderer *r,
                                math::Matrix<double, 3, 3> const & m) const
{
  if (!isVisible())
    return;

  checkDirtyDrawing();

  //r->drawRectangle(roughBoundRect(), graphics::Color(0, 0, 255, 64), graphics::maxDepth);

  if (m_downloadButton->isVisible())
    m_downloadButton->draw(r, m);
  if (m_statusMsg->isVisible())
    m_statusMsg->draw(r, m);
}

vector<m2::AnyRectD> const & CountryStatusDisplay::boundRects() const
{
  checkDirtyDrawing();

  if (isDirtyRect())
  {
    m_boundRects.clear();

    m2::RectD r(pivot(), pivot());

    if (m_downloadButton->isVisible())
      r.Add(m_downloadButton->roughBoundRect());

    m_boundRects.push_back(m2::AnyRectD(r));
    setIsDirtyRect(false);
  }

  return m_boundRects;
}

void CountryStatusDisplay::setController(gui::Controller *controller)
{
  Element::setController(controller);
  m_statusMsg->setController(controller);
  m_downloadButton->setController(controller);
}

void CountryStatusDisplay::setPivot(m2::PointD const & pv)
{
  if (m_countryStatus == storage::EDownloadFailed)
  {
    size_t buttonHeight = m_downloadButton->roughBoundRect().SizeY();
    size_t statusHeight = m_statusMsg->roughBoundRect().SizeY();

    size_t commonHeight = buttonHeight + statusHeight + 10 * visualScale();

    m_downloadButton->setPivot(m2::PointD(pv.x, pv.y + commonHeight / 2 - buttonHeight / 2));
    m_statusMsg->setPivot(m2::PointD(pv.x, pv.y - commonHeight / 2 + statusHeight / 2));
  }
  else
  {
    m_downloadButton->setPivot(pv);
    m_statusMsg->setPivot(pv);
  }

  gui::Element::setPivot(pv);
}

bool CountryStatusDisplay::onTapStarted(m2::PointD const & pt)
{
  if (m_downloadButton->isVisible())
    return m_downloadButton->onTapStarted(pt);
  return false;
}

bool CountryStatusDisplay::onTapMoved(m2::PointD const & pt)
{
  if (m_downloadButton->isVisible())
    return m_downloadButton->onTapMoved(pt);
  return false;
}

bool CountryStatusDisplay::onTapEnded(m2::PointD const & pt)
{
  if (m_downloadButton->isVisible())
    return m_downloadButton->onTapEnded(pt);
  return false;
}

bool CountryStatusDisplay::onTapCancelled(m2::PointD const & pt)
{
  if (m_downloadButton->isVisible())
    return m_downloadButton->onTapCancelled(pt);
  return false;
}
