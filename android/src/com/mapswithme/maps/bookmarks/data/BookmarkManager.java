package com.mapswithme.maps.bookmarks.data;

import android.util.Pair;

import com.mapswithme.maps.R;
import com.mapswithme.util.statistics.Statistics;

import java.util.Arrays;
import java.util.List;

public enum BookmarkManager
{
  INSTANCE;

  private static Icon[] sIcons = {
      new Icon("placemark-red", "placemark-red", R.drawable.color_picker_red_off, R.drawable.color_picker_red_on),
      new Icon("placemark-blue", "placemark-blue", R.drawable.color_picker_blue_off, R.drawable.color_picker_blue_on),
      new Icon("placemark-purple", "placemark-purple", R.drawable.color_picker_purple_off, R.drawable.color_picker_purple_on),
      new Icon("placemark-yellow", "placemark-yellow", R.drawable.color_picker_yellow_off, R.drawable.color_picker_yellow_on),
      new Icon("placemark-pink", "placemark-pink", R.drawable.color_picker_pink_off, R.drawable.color_picker_pink_on),
      new Icon("placemark-brown", "placemark-brown", R.drawable.color_picker_brown_off, R.drawable.color_picker_brown_on),
      new Icon("placemark-green", "placemark-green", R.drawable.color_picker_green_off, R.drawable.color_picker_green_on),
      new Icon("placemark-orange", "placemark-orange", R.drawable.color_picker_orange_off, R.drawable.color_picker_orange_on)
  };

  BookmarkManager()
  {
    loadBookmarks();
  }

  private native void loadBookmarks();

  public void deleteBookmark(Bookmark bmk)
  {
    deleteBookmark(bmk.getCategoryId(), bmk.getBookmarkId());
  }

  public void deleteTrack(Track track)
  {
    nativeDeleteTrack(track.getCategoryId(), track.getTrackId());
  }

  private native void nativeDeleteTrack(int cat, int trk);

  private native void deleteBookmark(int c, int b);

  public BookmarkCategory getCategoryById(int id)
  {
    if (id < getCategoriesCount())
      return new BookmarkCategory(id);
    else
      return null;
  }

  public native int getCategoriesCount();

  public native boolean deleteCategory(int index);

  public Icon getIconByType(String type)
  {
    for (Icon icon : sIcons)
    {
      if (icon.getType().equals(type))
        return icon;
    }
    // return default icon
    return sIcons[0];
  }

  public List<Icon> getIcons()
  {
    return Arrays.asList(sIcons);
  }

  public Bookmark getBookmark(Pair<Integer, Integer> catAndBmk)
  {
    return getBookmark(catAndBmk.first, catAndBmk.second);
  }

  public Bookmark getBookmark(int cat, int bmk)
  {
    return getCategoryById(cat).getBookmark(bmk);
  }

  public void setCategoryName(BookmarkCategory cat, String newName)
  {
    cat.setName(newName);
  }

  public Pair<Integer, Integer> addNewBookmark(String name, double lat, double lon)
  {
    final int cat = getLastEditedCategory();
    final int bmk = addBookmarkToLastEditedCategory(name, lat, lon);
    Statistics.INSTANCE.trackBookmarkCreated();

    return new Pair<>(cat, bmk);
  }

  public native int createCategory(String name);

  public native void showBookmarkOnMap(int c, int b);

  public native String saveToKmzFile(int catID, String tmpPath);

  public native int addBookmarkToLastEditedCategory(String name, double lat, double lon);

  public native int getLastEditedCategory();
}
