#pragma once
#include "bookmark.hpp"
#include "events.hpp"

//special number for additional layer category
const int additionalLayerCategory = -2;

class Framework;

class BookmarkManager : private noncopyable
{
  vector<BookmarkCategory *> m_categories;
  string m_lastCategoryUrl;
  Framework & m_framework;
  BookmarkCategory * m_additionalPoiLayer;

  typedef vector<BookmarkCategory *>::iterator CategoryIter;

  void drawCategory(BookmarkCategory const * cat, shared_ptr<PaintEvent> const & e);
public:
  BookmarkManager(Framework& f);
  ~BookmarkManager();

  void ClearBookmarks();

  /// Scans and loads all kml files with bookmarks in WritableDir.
  void LoadBookmarks();
  void LoadBookmark(string const & filePath);

  /// Client should know where it adds bookmark
  size_t AddBookmark(size_t categoryIndex, Bookmark & bm);

  size_t LastEditedBMCategory();

  inline size_t GetBmCategoriesCount() const { return m_categories.size(); }

  /// @returns 0 if category is not found
  BookmarkCategory * GetBmCategory(size_t index) const;

  size_t CreateBmCategory(string const & name);
  void DrawBookmarks(shared_ptr<PaintEvent> const & e);

  /// @name Delete bookmarks category with all bookmarks.
  /// @return true if category was deleted
  void DeleteBmCategory(CategoryIter i);
  bool DeleteBmCategory(size_t index);

  /// Additional layer methods
  void AdditionalPoiLayerSetInvisible();
  void AdditionalPoiLayerSetVisible();
  void AdditionalPoiLayerAddPoi(Bookmark const & bm);
  Bookmark const * AdditionalPoiLayerGetBookmark(size_t index) const;
  void AdditionalPoiLayerDeleteBookmark(int index);
  void AdditionalPoiLayerClear();
  bool IsAdditionalLayerPoi(const BookmarkAndCategory & bm) const;
  bool AdditionalLayerIsVisible() const { return m_additionalPoiLayer->IsVisible(); }
  size_t AdditionalLayerNumberOfPoi() const { return m_additionalPoiLayer->GetBookmarksCount(); }
};
