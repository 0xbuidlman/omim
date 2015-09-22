#include "../base/SRC_FIRST.hpp"

#include "../platform/platform.hpp"

#include "../std/bind.hpp"
#include "../std/set.hpp"
#include "../std/algorithm.hpp"

#include "../indexer/scales.hpp"

#include "../yg/screen.hpp"
#include "../yg/display_list.hpp"
#include "../yg/skin.hpp"
#include "../yg/base_texture.hpp"

#include "screen_coverage.hpp"
#include "tile_renderer.hpp"
#include "window_handle.hpp"
#include "coverage_generator.hpp"

ScreenCoverage::ScreenCoverage()
  : m_tiler(),
    m_overlay(new yg::Overlay()),
    m_isEmptyDrawingCoverage(false),
    m_isEmptyModelAtCoverageCenter(true),
    m_leafTilesToRender(0)
{
  m_overlay->setCouldOverlap(false);
}

ScreenCoverage::ScreenCoverage(TileRenderer * tileRenderer,
                               CoverageGenerator * coverageGenerator,
                               shared_ptr<yg::gl::Screen> const & cacheScreen)
  : m_tileRenderer(tileRenderer),
    m_coverageGenerator(coverageGenerator),
    m_overlay(new yg::Overlay()),
    m_isEmptyDrawingCoverage(false),
    m_isEmptyModelAtCoverageCenter(true),
    m_leafTilesToRender(0),
    m_cacheScreen(cacheScreen)
{
  m_overlay->setCouldOverlap(false);
}

void ScreenCoverage::CopyInto(ScreenCoverage & cvg)
{
  cvg.m_tileRenderer = m_tileRenderer;
  cvg.m_tiler = m_tiler;
  cvg.m_screen = m_screen;
  cvg.m_coverageGenerator = m_coverageGenerator;
  cvg.m_tileRects = m_tileRects;
  cvg.m_newTileRects = m_newTileRects;
  cvg.m_newLeafTileRects = m_newLeafTileRects;
  cvg.m_isEmptyDrawingCoverage = m_isEmptyDrawingCoverage;
  cvg.m_isEmptyModelAtCoverageCenter = m_isEmptyModelAtCoverageCenter;
  cvg.m_leafTilesToRender = m_leafTilesToRender;
  cvg.m_countryNameAtCoverageCenter = m_countryNameAtCoverageCenter;

  TileCache * tileCache = &m_tileRenderer->GetTileCache();

  tileCache->Lock();

  cvg.m_tiles = m_tiles;

  for (TTileSet::const_iterator it = cvg.m_tiles.begin(); it != cvg.m_tiles.end(); ++it)
  {
    Tiler::RectInfo const & ri = (*it)->m_rectInfo;
    tileCache->LockTile(ri);
  }

  tileCache->Unlock();

  cvg.m_overlay.reset(m_overlay->clone());
}

void ScreenCoverage::Clear()
{
  m_tileRects.clear();
  m_newTileRects.clear();
  m_newLeafTileRects.clear();
  m_overlay->clear();
  m_isEmptyDrawingCoverage = false;
  m_isEmptyModelAtCoverageCenter = true;
  m_leafTilesToRender = 0;

  TileCache * tileCache = &m_tileRenderer->GetTileCache();

  tileCache->Lock();

  for (TTileSet::const_iterator it = m_tiles.begin(); it != m_tiles.end(); ++it)
  {
    Tiler::RectInfo const & ri = (*it)->m_rectInfo;
    tileCache->UnlockTile(ri);
  }

  tileCache->Unlock();

  m_tiles.clear();
}

void ScreenCoverage::Merge(Tiler::RectInfo const & ri)
{
  ASSERT(m_tileRects.find(ri) != m_tileRects.end(), ());

  TileCache & tileCache = m_tileRenderer->GetTileCache();
  TileSet & tileSet = m_tileRenderer->GetTileSet();

  Tile const * tile = 0;
  bool hasTile = false;

  /// every code that works both with tileSet and tileCache
  /// should lock them in the same order to avoid deadlocks
  /// (unlocking should be done in reverse order)
  tileSet.Lock();
  tileCache.Lock();

  hasTile = tileSet.HasTile(ri);

  if (hasTile)
  {
    ASSERT(tileCache.HasTile(ri), ());

    tile = &tileCache.GetTile(ri);
    ASSERT(m_tiles.find(tile) == m_tiles.end(), ());

    /// while in the TileSet, the tile is assumed to be locked

    m_tiles.insert(tile);
    m_tileRects.erase(ri);
    m_newTileRects.erase(ri);
    m_newLeafTileRects.erase(ri);

    if (m_tiler.isLeaf(ri))
    {
      m_isEmptyDrawingCoverage &= tile->m_isEmptyDrawing;
      m_leafTilesToRender--;
    }

    tileSet.RemoveTile(ri);
  }

  tileCache.Unlock();
  tileSet.Unlock();

  if (hasTile)
  {
    if (m_tiler.isLeaf(ri))
    {
      yg::Overlay * tileOverlayCopy = tile->m_overlay->clone();
      m_overlay->merge(*tileOverlayCopy,
                        tile->m_tileScreen.PtoGMatrix() * m_screen.GtoPMatrix());

      delete tileOverlayCopy;
    }
  }
}

bool ScreenCoverage::Cache(core::CommandsQueue::Environment const & env)
{
  /// caching tiles blitting commands.

  m_displayList.reset();
  m_displayList.reset(m_cacheScreen->createDisplayList());

  m_cacheScreen->setEnvironment(&env);

  m_cacheScreen->beginFrame();
  m_cacheScreen->setDisplayList(m_displayList.get());

  vector<yg::gl::BlitInfo> infos;

  for (TTileSet::const_iterator it = m_tiles.begin(); it != m_tiles.end(); ++it)
  {
    Tile const * tile = *it;

    size_t tileWidth = tile->m_renderTarget->width();
    size_t tileHeight = tile->m_renderTarget->height();

    yg::gl::BlitInfo bi;

    bi.m_matrix = tile->m_tileScreen.PtoGMatrix() * m_screen.GtoPMatrix();
    bi.m_srcRect = m2::RectI(0, 0, tileWidth - 2, tileHeight - 2);
    bi.m_texRect = m2::RectU(1, 1, tileWidth - 1, tileHeight - 1);
    bi.m_srcSurface = tile->m_renderTarget;

    infos.push_back(bi);
  }

  if (!infos.empty())
    m_cacheScreen->blit(&infos[0], infos.size(), true);

  m_overlay->draw(m_cacheScreen.get(), math::Identity<double, 3>());

  m_cacheScreen->setDisplayList(0);
  m_cacheScreen->endFrame();

  /// completing commands that was immediately executed
  /// while recording of displayList(for example UnlockStorage)

  m_cacheScreen->completeCommands();

  bool isCancelled = m_cacheScreen->isCancelled();

  m_cacheScreen->setEnvironment(0);

  return !isCancelled;
}

void ScreenCoverage::SetScreen(ScreenBase const & screen)
{
  m_screen = screen;

  m_newTileRects.clear();

  m_tiler.seed(m_screen, m_screen.GlobalRect().GlobalCenter(), m_tileRenderer->TileSize());

  vector<Tiler::RectInfo> allRects;
  vector<Tiler::RectInfo> newRects;
  TTileSet tiles;

  m_tiler.tiles(allRects, GetPlatform().PreCachingDepth());

  TileCache * tileCache = &m_tileRenderer->GetTileCache();

  tileCache->Lock();

  m_isEmptyDrawingCoverage = true;
  m_isEmptyModelAtCoverageCenter = true;
  m_leafTilesToRender = 0;

  for (size_t i = 0; i < allRects.size(); ++i)
  {
    Tiler::RectInfo const & ri = allRects[i];
    m_tileRects.insert(ri);

    if (tileCache->HasTile(ri))
    {
      tileCache->TouchTile(ri);
      Tile const * tile = &tileCache->GetTile(ri);
      ASSERT(tiles.find(tile) == tiles.end(), ());

      if (m_tiler.isLeaf(ri))
        m_isEmptyDrawingCoverage &= tile->m_isEmptyDrawing;

      tiles.insert(tile);
    }
    else
    {
      newRects.push_back(ri);
      if (m_tiler.isLeaf(ri))
        ++m_leafTilesToRender;
    }
  }

  /// computing difference between current and previous coverage
  /// tiles, that aren't in current coverage are unlocked to allow their deletion from TileCache
  /// tiles, that are new to the current coverage are added into m_tiles and locked in TileCache

  TTileSet erasedTiles;
  TTileSet addedTiles;

  set_difference(m_tiles.begin(), m_tiles.end(), tiles.begin(), tiles.end(), inserter(erasedTiles, erasedTiles.end()), TTileSet::key_compare());
  set_difference(tiles.begin(), tiles.end(), m_tiles.begin(), m_tiles.end(), inserter(addedTiles, addedTiles.end()), TTileSet::key_compare());

  for (TTileSet::const_iterator it = erasedTiles.begin(); it != erasedTiles.end(); ++it)
  {
    tileCache->UnlockTile((*it)->m_rectInfo);
    /// here we should "unmerge" erasedTiles[i].m_overlay from m_overlay
  }

  for (TTileSet::const_iterator it = addedTiles.begin(); it != addedTiles.end(); ++it)
  {
    tileCache->LockTile((*it)->m_rectInfo);
  }

  tileCache->Unlock();

  m_tiles = tiles;

  MergeOverlay();

  set<Tiler::RectInfo> drawnTiles;

  for (TTileSet::const_iterator it = m_tiles.begin(); it != m_tiles.end(); ++it)
  {
    Tiler::RectInfo const & ri = (*it)->m_rectInfo;
    drawnTiles.insert(Tiler::RectInfo(ri.m_tileScale, ri.m_x, ri.m_y));
  }

  vector<Tiler::RectInfo> firstClassTiles;
  vector<Tiler::RectInfo> secondClassTiles;

  unsigned newRectsCount = newRects.size();

  for (unsigned i = 0; i < newRectsCount; ++i)
  {
    Tiler::RectInfo nr = newRects[i];

    Tiler::RectInfo cr[4] =
    {
      Tiler::RectInfo(nr.m_tileScale + 1, nr.m_x * 2,     nr.m_y * 2),
      Tiler::RectInfo(nr.m_tileScale + 1, nr.m_x * 2 + 1, nr.m_y * 2),
      Tiler::RectInfo(nr.m_tileScale + 1, nr.m_x * 2 + 1, nr.m_y * 2 + 1),
      Tiler::RectInfo(nr.m_tileScale + 1, nr.m_x * 2,     nr.m_y * 2 + 1)
    };

    int childTilesToDraw = 4;

    for (int i = 0; i < 4; ++i)
      if (drawnTiles.count(cr[i]) || !m_screen.GlobalRect().IsIntersect(m2::AnyRectD(cr[i].m_rect)))
        --childTilesToDraw;

//    if (m_tiler.isLeaf(nr) || (childTilesToDraw > 1))

    if ((nr.m_tileScale == m_tiler.tileScale() - 2)
      ||(nr.m_tileScale == m_tiler.tileScale() ))
      firstClassTiles.push_back(nr);
    else
      secondClassTiles.push_back(nr);
  }

  /// clearing all old commands
  m_tileRenderer->ClearCommands();
  /// setting new sequenceID
  m_tileRenderer->SetSequenceID(GetSequenceID());

  m_tileRenderer->CancelCommands();

  // filtering out rects that are fully covered by its descedants

  int curNewTile = 0;

  // adding commands for tiles which aren't in cache
  for (size_t i = 0; i < firstClassTiles.size(); ++i, ++curNewTile)
  {
    Tiler::RectInfo const & ri = firstClassTiles[i];

    core::CommandsQueue::Chain chain;

    chain.addCommand(bind(&CoverageGenerator::AddMergeTileTask,
                          m_coverageGenerator,
                          ri,
                          GetSequenceID()));

    if (curNewTile == newRectsCount - 1)
      chain.addCommand(bind(&CoverageGenerator::AddFinishSequenceTask,
                            m_coverageGenerator,
                            GetSequenceID()));

    m_tileRenderer->AddCommand(ri, GetSequenceID(),
                               chain);

    if (m_tiler.isLeaf(ri))
      m_newLeafTileRects.insert(ri);

    m_newTileRects.insert(ri);
  }

  for (size_t i = 0; i < secondClassTiles.size(); ++i, ++curNewTile)
  {
    Tiler::RectInfo const & ri = secondClassTiles[i];

    core::CommandsQueue::Chain chain;

    chain.addCommand(bind(&TileRenderer::RemoveActiveTile,
                          m_tileRenderer,
                          ri));

    if (curNewTile == newRectsCount - 1)
      chain.addCommand(bind(&CoverageGenerator::AddFinishSequenceTask,
                            m_coverageGenerator,
                            GetSequenceID()));

    m_tileRenderer->AddCommand(ri, GetSequenceID(), chain);
  }
}

ScreenCoverage::~ScreenCoverage()
{
  Clear();
}

void ScreenCoverage::Draw(yg::gl::Screen * s, ScreenBase const & screen)
{
  if (m_displayList)
    m_displayList->draw(m_screen.PtoGMatrix() * screen.GtoPMatrix());
}

shared_ptr<yg::Overlay> const & ScreenCoverage::GetOverlay() const
{
  return m_overlay;
}

int ScreenCoverage::GetDrawScale() const
{
  return min(m_tiler.tileScale(), scales::GetUpperScale());
}

bool ScreenCoverage::IsEmptyDrawingCoverage() const
{
  return (m_leafTilesToRender <= 0) && m_isEmptyDrawingCoverage;
}

bool ScreenCoverage::IsEmptyModelAtCoverageCenter() const
{
  return m_isEmptyModelAtCoverageCenter;
}

void ScreenCoverage::ResetEmptyModelAtCoverageCenter()
{
  m_isEmptyModelAtCoverageCenter = false;
}

string ScreenCoverage::GetCountryNameAtCoverageCenter() const
{
  return m_countryNameAtCoverageCenter;
}

void ScreenCoverage::CheckEmptyModelAtCoverageCenter()
{
  if (!IsPartialCoverage() && IsEmptyDrawingCoverage())
  {
    m2::PointD centerPt = m_screen.GlobalRect().GetGlobalRect().Center();
    m_countryNameAtCoverageCenter = m_coverageGenerator->GetCountryName(centerPt);
    m_isEmptyModelAtCoverageCenter = !m_countryNameAtCoverageCenter.empty();
  }
}

bool ScreenCoverage::IsPartialCoverage() const
{
  return !m_newTileRects.empty();
}

void ScreenCoverage::SetSequenceID(int id)
{
  m_sequenceID = id;
}

int ScreenCoverage::GetSequenceID() const
{
  return m_sequenceID;
}

void ScreenCoverage::RemoveTiles(m2::AnyRectD const & r, int startScale)
{
  vector<Tile const*> toRemove;

  for (TTileSet::const_iterator it = m_tiles.begin(); it != m_tiles.end(); ++it)
  {
    Tiler::RectInfo const & ri = (*it)->m_rectInfo;

    if (r.IsIntersect(m2::AnyRectD(ri.m_rect)) && (ri.m_tileScale >= startScale))
      toRemove.push_back(*it);
  }

  TileCache * tileCache = &m_tileRenderer->GetTileCache();

  for (vector<Tile const *>::const_iterator it = toRemove.begin(); it != toRemove.end(); ++it)
  {
    Tiler::RectInfo const & ri = (*it)->m_rectInfo;
    tileCache->UnlockTile(ri);
    m_tiles.erase(*it);
    m_tileRects.erase(ri);
  }

  MergeOverlay();
}

void ScreenCoverage::MergeOverlay()
{
  m_overlay->clear();

  for (TTileSet::const_iterator it = m_tiles.begin(); it != m_tiles.end(); ++it)
  {
    Tiler::RectInfo const & ri = (*it)->m_rectInfo;
    if (m_tiler.isLeaf(ri))
    {
      scoped_ptr<yg::Overlay> copy((*it)->m_overlay->clone());
      m_overlay->merge(*copy.get(), (*it)->m_tileScreen.PtoGMatrix() * m_screen.GtoPMatrix());
    }
  }
}
