package com.mapswithme.maps;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.AssetManager;
import android.os.Build;
import android.os.Environment;
import android.util.Log;
import android.widget.Toast;

import com.mapswithme.maps.MapStorage.Index;
import com.mapswithme.maps.bookmarks.data.BookmarkManager;
import com.mapswithme.maps.location.LocationService;
import com.mapswithme.util.Utils;


public class MWMApplication extends android.app.Application implements MapStorage.Listener
{
  private final static String TAG = "MWMApplication";

  private LocationService m_location = null;
  private LocationState m_locationState = null;
  private MapStorage m_storage = null;
  private int m_slotID = 0;

  private boolean m_isProVersion = false;

  // Set default string to Google Play page.
  private String m_proVersionURL =
      "https://play.google.com/store/apps/details?id=com.mapswithme.maps.pro";

  private void showDownloadToast(int resID, Index idx)
  {
    final String msg = String.format(getString(resID), m_storage.countryName(idx));
    Toast.makeText(this, msg, Toast.LENGTH_LONG).show();
  }

  @Override
  public void onCountryStatusChanged(Index idx)
  {
    switch (m_storage.countryStatus(idx))
    {
    case MapStorage.ON_DISK:
      showDownloadToast(R.string.download_country_success, idx);
      break;

    case MapStorage.DOWNLOAD_FAILED:
      showDownloadToast(R.string.download_country_failed, idx);
      break;
    }
  }

  @Override
  public void onCountryProgress(Index idx, long current, long total)
  {
  }

  @Override
  public void onCreate()
  {
    super.onCreate();

    m_isProVersion = getPackageName().endsWith(".pro");

    // http://stackoverflow.com/questions/1440957/httpurlconnection-getresponsecode-returns-1-on-second-invocation
    if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.ECLAIR_MR1)
      System.setProperty("http.keepAlive", "false");

    // get url for PRO version
    if (!m_isProVersion)
    {
      AssetManager assets = getAssets();
      InputStream stream = null;
      try
      {
        stream = assets.open("app_info.txt");
        BufferedReader reader = new BufferedReader(new InputStreamReader(stream));

        final String s = reader.readLine();
        if (s.length() > 0)
          m_proVersionURL = s;

        Log.i(TAG, "Pro version url: " + m_proVersionURL);
      }
      catch (IOException ex)
      {
        // suppress exceptions - pro version doesn't need app_info.txt
      }
      Utils.closeStream(stream);
    }

    final String extStoragePath = getDataStoragePath();
    final String extTmpPath = getExtAppDirectoryPath("caches");

    // Create folders if they don't exist
    new File(extStoragePath).mkdirs();
    new File(extTmpPath).mkdirs();

    // init native framework
    nativeInit(getApkPath(),
               extStoragePath,
               getTmpPath(),
               extTmpPath,
               m_isProVersion);

    m_slotID = getMapStorage().subscribe(this);

    // init cross-platform strings bundle
    nativeSetString("country_status_added_to_queue", getString(R.string.country_status_added_to_queue));
    nativeSetString("country_status_downloading", getString(R.string.country_status_downloading));
    nativeSetString("country_status_download", getString(R.string.country_status_download));
    nativeSetString("country_status_download_failed", getString(R.string.country_status_download_failed));
    nativeSetString("try_again", getString(R.string.try_again));
    nativeSetString("not_enough_free_space_on_sdcard", getString(R.string.not_enough_free_space_on_sdcard));
    nativeSetString("dropped_pin", getString(R.string.dropped_pin));
    nativeSetString("my_places", getString(R.string.my_places));

    // init BookmarkManager (automatically loads bookmarks)
    if (m_isProVersion)
      BookmarkManager.getBookmarkManager(getApplicationContext());
  }

  public LocationService getLocationService()
  {
    if (m_location == null)
      m_location = new LocationService(this);

    return m_location;
  }

  public LocationState getLocationState()
  {
    if (m_locationState == null)
      m_locationState = new LocationState();

    return m_locationState;
  }

  public MapStorage getMapStorage()
  {
    if (m_storage == null)
      m_storage = new MapStorage();

    return m_storage;
  }

  public String getApkPath()
  {
    try
    {
      return getPackageManager().getApplicationInfo(getPackageName(), 0).sourceDir;
    }
    catch (NameNotFoundException e)
    {
      Log.e(TAG, "Can't get apk path from PackageManager");
      return "";
    }
  }

  public String getDataStoragePath()
  {
    return Environment.getExternalStorageDirectory().getAbsolutePath() + "/MapsWithMe/";
  }

  public String getExtAppDirectoryPath(String folder)
  {
    final String storagePath = Environment.getExternalStorageDirectory().getAbsolutePath();
    return storagePath.concat(String.format("/Android/data/%s/%s/", getPackageName(), folder));
  }

  /// Check if we have free space on storage (writable path).
  public native boolean hasFreeSpace(long size);

  public boolean isProVersion()
  {
    return m_isProVersion;
  }

  public String getProVersionURL()
  {
    return m_proVersionURL;
  }

  private String getTmpPath()
  {
    return getCacheDir().getAbsolutePath() + "/";
  }

  /*
  private String getSettingsPath()
  {
    return getFilesDir().getAbsolutePath() + "/";
  }
   */

  static
  {
    System.loadLibrary("mapswithme");
  }

  private native void nativeInit(String apkPath,
                                 String storagePath,
                                 String tmpPath,
                                 String extTmpPath,
                                 boolean isPro);

  public native boolean nativeIsBenchmarking();

  /// @name Dealing with dialogs.
  /// @note Constants should be equal with map/dialog_settings.hpp
  /// @{
  static public final int FACEBOOK = 0;
  static public final int BUYPRO = 1;
  public native boolean shouldShowDialog(int dlg);

  static public final int OK = 0;
  static public final int LATER = 1;
  static public final int NEVER = 2;
  public native void submitDialogResult(int dlg, int res);
  /// @}

  private native void nativeSetString(String name, String value);

  /// Dealing with Settings
  public native boolean nativeGetBoolean(String name, boolean defaultVal);
  public native void nativeSetBoolean(String name, boolean val);
}
