#pragma once

#include "../base/mutex.hpp"

#include "../geometry/screenbase.hpp"

#include "../yg/overlay.hpp"

#include "tile.hpp"
#include "tiler.hpp"
#include "render_policy.hpp"

class TileRenderer;

namespace yg
{
  namespace gl
  {
    class Screen;
  }
}

class CoverageGenerator;

class ScreenCoverage
{
private:
  /// Unique ID of this screen coverage
  int m_sequenceID;
  /// Queue to put new rendering tasks in
  TileRenderer * m_tileRenderer;
  /// Coverage generator
  CoverageGenerator * m_coverageGenerator;
  /// Tiler to compute visible and predicted tiles
  Tiler m_tiler;
  /// Last covered screen
  ScreenBase  m_screen;
  /// Container for a rects, that forms a set of tiles in the m_screen
  typedef set<Tiler::RectInfo> TTileRectSet;
  /// This set contains all rects that was returned by Tiler.
  /// This includes already drawn rects, and rects which is not drawn yet.
  TTileRectSet m_tileRects;
  /// This set contains rects, which was send to TileRenderer to draw
  TTileRectSet m_newTileRects;
  /// Subset of newTileRects, only leaf rects that should be drawn
  /// For quick check for Partial/NonPartial coverage
  TTileRectSet m_newLeafTileRects;
  /// Typedef for a set of tiles, that are visible for the m_screen
  typedef set<Tile const *, LessRectInfo> TTileSet;
  /// This set contains all tiles that are found in the TileCache.
  /// Tiles in this set are locked to prevent their deletion
  /// from TileCache while drawing them
  TTileSet m_tiles;
  /// Overlay composed of overlays for visible tiles
  scoped_ptr<yg::Overlay> m_overlay;

  /// State flags

  /// Does the all leaf tiles in this coverage are empty?
  bool m_isEmptyDrawingCoverage;
  /// If the map model empty at the screen center?
  bool m_isEmptyModelAtCoverageCenter;
  /// How many "leaf" tiles we should render to cover the screen.
  /// This is efficiently the size of newLeafTileRects and is cached for
  /// quick check.
  int m_leafTilesToRender;
  /// Screen, which is used for caching of this ScreenCoverage into DisplayList
  shared_ptr<yg::gl::Screen> m_cacheScreen;
  /// DisplayList which holds cached ScreenCoverage
  shared_ptr<yg::gl::DisplayList> m_displayList;

  /// Direct copying is prohibited.
  ScreenCoverage(ScreenCoverage const & src);
  ScreenCoverage const & operator=(ScreenCoverage const & src);

  /// For each tile in m_tiles merge it's overlay into the big one.
  void MergeOverlay();

public:

  /// Default Constructor
  ScreenCoverage();
  /// Constructor
  ScreenCoverage(TileRenderer * tileRenderer,
                 CoverageGenerator * coverageGenerator,
                 shared_ptr<yg::gl::Screen> const & cacheScreen);
  /// Destructor
  ~ScreenCoverage();
  /// Copy all needed information into specified ScreenCoverage
  void CopyInto(ScreenCoverage & cvg);
  /// Make screen coverage empty
  void Clear();
  /// set unique ID for all actions, used to compute this coverage
  void SetSequenceID(int id);
  /// get unique ID for all actions, used to compute this coverage
  int GetSequenceID() const;
  /// Is this screen coverage partial, which means that it contains non-drawn rects
  bool IsPartialCoverage() const;
  /// Is this screen coverage contains only empty tiles
  bool IsEmptyDrawingCoverage() const;
  /// Is the model empty at the screen center
  bool IsEmptyModelAtCoverageCenter() const;
  /// Check, whether the model is empty at the center of the coverage.
  void CheckEmptyModelAtCoverageCenter();
  /// Getter for Overlay
  yg::Overlay * GetOverlay() const;
  /// Cache coverage in display list
  void Cache();
  /// add rendered tile to coverage. Tile is locked, so make sure to unlock it in case it's not needed.
  void Merge(Tiler::RectInfo const & ri);
  /// recalculate screen coverage, using as much info from prev coverage as possible
  void SetScreen(ScreenBase const & screen);
  /// draw screen coverage
  void Draw(yg::gl::Screen * s, ScreenBase const & currentScreen);
  /// get draw scale for the tiles in the current coverage
  /// Not all tiles in coverage could correspond to this value,
  /// as there could be tiles from lower and higher level in the
  /// coverage to provide a smooth scale transition experience
  int GetDrawScale() const;
  /// Unlock and remove tiles which intersect the specified rect
  /// and deeper or equal than specified scale
  void RemoveTiles(m2::AnyRectD const & r, int startScale);
};
