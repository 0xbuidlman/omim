#include "bookmark_manager.hpp"
#include "framework.hpp"
#include "user_mark.hpp"

#include "../graphics/depth_constants.hpp"

#include "../platform/platform.hpp"
#include "../platform/settings.hpp"

#include "../indexer/scales.hpp"

#include "../geometry/transformations.hpp"

#include "../base/stl_add.hpp"

#include "../std/algorithm.hpp"
#include "../std/target_os.hpp"
#include "../std/vector.hpp"


BookmarkManager::BookmarkManager(Framework & f)
  : m_framework(f)
  , m_bmScreen(0)
  , m_lastScale(1.0)
  , m_cache(NULL)
  , m_selection(f)
{
  m_userMarkLayers.reserve(2);
  m_userMarkLayers.push_back(new SearchUserMarkContainer(graphics::activePinDepth, m_framework));
  m_userMarkLayers.push_back(new ApiUserMarkContainer(graphics::activePinDepth, m_framework));
  UserMarkContainer::InitStaticMarks(FindUserMarksContainer(UserMarkContainer::SEARCH_MARK));
}

BookmarkManager::~BookmarkManager()
{
  for_each(m_userMarkLayers.begin(), m_userMarkLayers.end(), DeleteFunctor());
  m_userMarkLayers.clear();

  ClearItems();
  ResetScreen();
}

namespace
{
char const * BOOKMARK_CATEGORY = "LastBookmarkCategory";
char const * BOOKMARK_TYPE = "LastBookmarkType";
}

void BookmarkManager::SaveState() const
{
  Settings::Set(BOOKMARK_CATEGORY, m_lastCategoryUrl);
  Settings::Set(BOOKMARK_TYPE, m_lastType);
}

void BookmarkManager::LoadState()
{
  Settings::Get(BOOKMARK_CATEGORY, m_lastCategoryUrl);
  Settings::Get(BOOKMARK_TYPE, m_lastType);
}

namespace
{

class LazyMatrixCalc
{
  ScreenBase const & m_screen;
  double & m_lastScale;

  typedef ScreenBase::MatrixT MatrixT;
  MatrixT m_matrix;
  double m_resScale;
  bool m_scaleChanged;

  void CalcScaleG2P()
  {
    // Check whether viewport scale changed since the last drawing.
    m_scaleChanged = false;
    double const d = m_lastScale / m_screen.GetScale();
    if (d >= 2.0 || d <= 0.5)
    {
      m_scaleChanged = true;
      m_lastScale = m_screen.GetScale();
    }

    // GtoP matrix for scaling only (with change Y-axis direction).
    m_resScale = 1.0 / m_lastScale;
    m_matrix = math::Scale(math::Identity<double, 3>(), m_resScale, -m_resScale);
  }

public:
  LazyMatrixCalc(ScreenBase const & screen, double & lastScale)
    : m_screen(screen), m_lastScale(lastScale), m_resScale(0.0)
  {
  }

  bool IsScaleChanged()
  {
    if (m_resScale == 0.0)
      CalcScaleG2P();
    return m_scaleChanged;
  }

  MatrixT const & GetScaleG2P()
  {
    if (m_resScale == 0.0)
      CalcScaleG2P();
    return m_matrix;
  }

  MatrixT const & GetFinalG2P()
  {
    if (m_resScale == 0.0)
    {
      // Final GtoP matrix for drawing track's display lists.
      m_resScale = m_lastScale / m_screen.GetScale();
      m_matrix =
          math::Shift(
            math::Scale(
              math::Rotate(math::Identity<double, 3>(), m_screen.GetAngle()),
            m_resScale, m_resScale),
          m_screen.GtoP(m2::PointD(0.0, 0.0)));
    }
    return m_matrix;
  }
};

}

void BookmarkManager::DrawCategory(BookmarkCategory const * cat, PaintOverlayEvent const & e) const
{
#ifndef USE_DRAPE
  /// TODO cutomize draw in UserMarkContainer for user Draw method
  Navigator const & navigator = m_framework.GetNavigator();
  ScreenBase const & screen = navigator.Screen();

  Drawer * pDrawer = e.GetDrawer();
  graphics::Screen * pScreen = pDrawer->screen();

  LazyMatrixCalc matrix(screen, m_lastScale);

  // Draw tracks.
  for (size_t i = 0; i < cat->GetTracksCount(); ++i)
  {
    Track const * track = cat->GetTrack(i);
    if (track->HasDisplayList())
      track->Draw(pScreen, matrix.GetFinalG2P());
  }

  cat->Draw(e, m_cache);
#endif // USE_DRAPE
}

void BookmarkManager::ClearItems()
{
  ResetRouteTrack();
  for_each(m_categories.begin(), m_categories.end(), DeleteFunctor());
  m_categories.clear();
}

void BookmarkManager::LoadBookmarks()
{
  ClearItems();

  string const dir = GetPlatform().SettingsDir();

  Platform::FilesList files;
  Platform::GetFilesByExt(dir, BOOKMARKS_FILE_EXTENSION, files);
  for (size_t i = 0; i < files.size(); ++i)
    LoadBookmark(dir + files[i]);

  LoadState();
}

void BookmarkManager::LoadBookmark(string const & filePath)
{
  BookmarkCategory * cat = BookmarkCategory::CreateFromKMLFile(filePath, m_framework);
  if (cat)
    m_categories.push_back(cat);
}

size_t BookmarkManager::AddBookmark(size_t categoryIndex, const m2::PointD & ptOrg, BookmarkData & bm)
{
  bm.SetTimeStamp(time(0));
  bm.SetScale(m_framework.GetDrawScale());

  BookmarkCategory * pCat = m_categories[categoryIndex];
  pCat->AddBookmark(ptOrg, bm);
  pCat->SetVisible(true);
  pCat->SaveToKMLFile();

  m_lastCategoryUrl = pCat->GetFileName();
  m_lastType = bm.GetType();
  SaveState();

  return (pCat->GetBookmarksCount() - 1);
}

size_t BookmarkManager::MoveBookmark(size_t bmIndex, size_t curCatIndex, size_t newCatIndex)
{
  BookmarkCategory * cat = m_framework.GetBmCategory(curCatIndex);

  Bookmark * bm = cat->GetBookmark(bmIndex);
  BookmarkData data = bm->GetData();
  m2::PointD ptOrg = bm->GetOrg();

  cat->DeleteBookmark(bmIndex);
  cat->SaveToKMLFile();

  return m_framework.AddBookmark(newCatIndex, ptOrg, data);
}

void BookmarkManager::ReplaceBookmark(size_t catIndex, size_t bmIndex, BookmarkData const & bm)
{
  BookmarkCategory * pCat = m_categories[catIndex];
  pCat->ReplaceBookmark(bmIndex, bm);
  pCat->SaveToKMLFile();

  m_lastType = bm.GetType();
  SaveState();
}

size_t BookmarkManager::LastEditedBMCategory()
{
  for (size_t i = 0; i < m_categories.size(); ++i)
  {
    if (m_categories[i]->GetFileName() == m_lastCategoryUrl)
      return i;
  }

  if (m_categories.empty())
    m_categories.push_back(new BookmarkCategory(m_framework.GetStringsBundle().GetString("my_places"), m_framework));

  return 0;
}

string BookmarkManager::LastEditedBMType() const
{
  return (m_lastType.empty() ? BookmarkCategory::GetDefaultType() : m_lastType);
}

BookmarkCategory * BookmarkManager::GetBmCategory(size_t index) const
{
  return (index < m_categories.size() ? m_categories[index] : 0);
}

size_t BookmarkManager::CreateBmCategory(string const & name)
{
  m_categories.push_back(new BookmarkCategory(name, m_framework));
  return (m_categories.size()-1);
}

void BookmarkManager::DrawItems(shared_ptr<PaintEvent> const & e) const
{
#ifndef USE_DRAPE
  ASSERT(m_cache != NULL, ());
  ScreenBase const & screen = m_framework.GetNavigator().Screen();
  m2::RectD const limitRect = screen.ClipRect();

  LazyMatrixCalc matrix(screen, m_lastScale);

  double const drawScale = m_framework.GetDrawScale();
  double const visualScale = m_framework.GetVisualScale();
  auto trackUpdateFn = [&matrix, &limitRect, this, drawScale, visualScale](Track const * track)
  {
    if (limitRect.IsIntersect(track->GetLimitRect()))
    {
      if (!track->HasDisplayList() || matrix.IsScaleChanged())
        track->CreateDisplayList(m_bmScreen, matrix.GetScaleG2P(), drawScale, visualScale);
    }
    else
      track->DeleteDisplayList();
  };

  auto dlUpdateFn = [&trackUpdateFn] (BookmarkCategory const * cat)
  {
    if (cat->IsVisible())
    {
      for (size_t j = 0; j < cat->GetTracksCount(); ++j)
        trackUpdateFn(cat->GetTrack(j));
    }
  };

  // Update track's display lists.
  for (size_t i = 0; i < m_categories.size(); ++i)
  {
    BookmarkCategory const * cat = m_categories[i];
    dlUpdateFn(cat);
  }

  graphics::Screen * pScreen = e->drawer()->screen();
  pScreen->beginFrame();

  PaintOverlayEvent event(e->drawer(), screen);
  for_each(m_userMarkLayers.begin(), m_userMarkLayers.end(), bind(&UserMarkContainer::Draw, _1, event, m_cache));
  for_each(m_categories.begin(), m_categories.end(), bind(&BookmarkManager::DrawCategory, this, _1, event));
  if (m_routeTrack != nullptr)
  {
    trackUpdateFn(m_routeTrack.get());
    if (m_routeTrack->HasDisplayList())
      m_routeTrack->Draw(pScreen, LazyMatrixCalc(screen, m_lastScale).GetFinalG2P());
  }
  m_selection.Draw(event, m_cache);

  pScreen->endFrame();
#endif // USE_DRAPE
}

void BookmarkManager::DeleteBmCategory(CategoryIter i)
{
  BookmarkCategory * cat = *i;
  if (m_selection.m_container == cat)
  {
    PinClickManager & clikManager = m_framework.GetBalloonManager();
    clikManager.RemovePin();
    clikManager.Dismiss();
  }

  FileWriter::DeleteFileX(cat->GetFileName());

  delete cat;

  m_categories.erase(i);
}

bool BookmarkManager::DeleteBmCategory(size_t index)
{
  if (index < m_categories.size())
  {
    DeleteBmCategory(m_categories.begin() + index);
    return true;
  }
  else
    return false;
}

void BookmarkManager::ActivateMark(UserMark const * mark, bool needAnim)
{
  m_selection.ActivateMark(mark, needAnim);
}

bool BookmarkManager::UserMarkHasActive() const
{
  return m_selection.IsActive();
}

bool BookmarkManager::IsUserMarkActive(UserMark const * mark) const
{
  if (mark == nullptr)
    return false;

  return (m_selection.m_container == mark->GetContainer() &&
          m_selection.m_ptOrg.EqualDxDy(mark->GetOrg(), 1.0E-4));
}

namespace
{
  class BestUserMarkFinder
  {
  public:
    BestUserMarkFinder(BookmarkManager::TouchRectHolder const & rectHolder)
      : m_rectHolder(rectHolder)
      , m_d(numeric_limits<double>::max())
      , m_mark(NULL) {}

    void operator()(UserMarkContainer const * container)
    {
      m2::AnyRectD const & rect = m_rectHolder.GetTouchArea(container->GetType());
      UserMark const * findedMark = container->FindMarkInRect(rect, m_d);
      if (findedMark != NULL)
        m_mark = findedMark;
    }

    UserMark const * GetFindedMark() const { return m_mark; }

  private:
    BookmarkManager::TouchRectHolder const & m_rectHolder;
    double m_d;
    UserMark const * m_mark;
  };

  class SimpleTouchRectHolder : public BookmarkManager::TouchRectHolder
  {
  public:
    SimpleTouchRectHolder(m2::AnyRectD const & rect)
      : m_rect(rect)
    {
    }

    m2::AnyRectD const & GetTouchArea(UserMarkContainer::Type type) const
    {
      UNUSED_VALUE(type);
      return m_rect;
    }

  private:
    m2::AnyRectD const & m_rect;
  };
}

UserMark const * BookmarkManager::FindNearestUserMark(m2::AnyRectD const & rect) const
{
  SimpleTouchRectHolder holder(rect);
  return FindNearestUserMark(holder);
}

UserMark const * BookmarkManager::FindNearestUserMark(TouchRectHolder & holder) const
{
  BestUserMarkFinder finder(holder);
  for_each(m_categories.begin(), m_categories.end(), bind(&BestUserMarkFinder::operator(), &finder, _1));
  finder(FindUserMarksContainer(UserMarkContainer::API_MARK));
  finder(FindUserMarksContainer(UserMarkContainer::SEARCH_MARK));

  return finder.GetFindedMark();
}

void BookmarkManager::UserMarksSetVisible(UserMarkContainer::Type type, bool isVisible)
{
  FindUserMarksContainer(type)->SetVisible(isVisible);
}

bool BookmarkManager::UserMarksIsVisible(UserMarkContainer::Type type) const
{
  return FindUserMarksContainer(type)->IsVisible();
}

void BookmarkManager::UserMarksSetDrawable(UserMarkContainer::Type type, bool isDrawable)
{
  FindUserMarksContainer(type)->SetIsDrawable(isDrawable);
}

void BookmarkManager::UserMarksIsDrawable(UserMarkContainer::Type type)
{
  FindUserMarksContainer(type)->IsDrawable();
}

UserMark * BookmarkManager::UserMarksAddMark(UserMarkContainer::Type type, const m2::PointD & ptOrg)
{
  return FindUserMarksContainer(type)->GetController().CreateUserMark(ptOrg);
}

void BookmarkManager::UserMarksClear(UserMarkContainer::Type type, size_t skipCount/* = 0*/)
{
  FindUserMarksContainer(type)->Clear(skipCount);
}

UserMarkContainer::Controller & BookmarkManager::UserMarksGetController(UserMarkContainer::Type type)
{
  return FindUserMarksContainer(type)->GetController();
}

void BookmarkManager::SetScreen(graphics::Screen * screen)
{
  ResetScreen();
  m_bmScreen = screen;
  m_cache = new UserMarkDLCache(m_bmScreen);
}

void BookmarkManager::ResetScreen()
{
  delete m_cache;
  m_cache = NULL;

  auto dlDeleteFn = [] (BookmarkCategory const * cat)
  {
    for (size_t j = 0; j < cat->GetTracksCount(); ++j)
      cat->GetTrack(j)->DeleteDisplayList();
  };

  if (m_bmScreen)
  {
    // Delete display lists for all tracks
    for_each(m_categories.begin(), m_categories.end(), dlDeleteFn);
    if (m_routeTrack != nullptr)
      m_routeTrack->DeleteDisplayList();

    m_bmScreen = 0;
  }
}

void BookmarkManager::SetRouteTrack(Track & track)
{
  m_routeTrack.reset(track.CreatePersistent());
}

void BookmarkManager::ResetRouteTrack()
{
  m_routeTrack.reset();
}

UserMarkContainer const * BookmarkManager::FindUserMarksContainer(UserMarkContainer::Type type) const
{
  ASSERT(type >= 0 && type < m_userMarkLayers.size(), ());
  ASSERT(m_userMarkLayers[(size_t)type]->GetType() == type, ());
  return m_userMarkLayers[(size_t)type];
}

UserMarkContainer * BookmarkManager::FindUserMarksContainer(UserMarkContainer::Type type)
{
  ASSERT(type >= 0 && type < m_userMarkLayers.size(), ());
  ASSERT(m_userMarkLayers[(size_t)type]->GetType() == type, ());
  return m_userMarkLayers[(size_t)type];
}
