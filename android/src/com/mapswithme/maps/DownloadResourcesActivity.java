package com.mapswithme.maps;

import java.io.File;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.graphics.Color;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import com.mapswithme.maps.location.LocationService;
import com.mapswithme.util.ConnectionState;

public class DownloadResourcesActivity extends Activity implements LocationService.Listener, MapStorage.Listener
{
  private static final String TAG = "DownloadResourcesActivity";

  // Error codes, should match the same codes in JNI

  private static final int ERR_DOWNLOAD_SUCCESS = 0;
  private static final int ERR_NOT_ENOUGH_MEMORY = -1;
  private static final int ERR_NOT_ENOUGH_FREE_SPACE = -2;
  private static final int ERR_STORAGE_DISCONNECTED = -3;
  private static final int ERR_DOWNLOAD_ERROR = -4;
  private static final int ERR_NO_MORE_FILES = -5;
  private static final int ERR_FILE_IN_PROGRESS = -6;

  private MWMApplication mApplication = null;
  private MapStorage mMapStorage = null;
  private int mSlotId = 0;
  private TextView mMsgView = null;
  private TextView mLocationMsgView = null;
  private ProgressBar mProgress = null;
  private Button mButton = null;
  private CheckBox mDownloadCountryCheckBox = null;
  private LocationService mLocationService = null;
  private String mCountryName = null;

  private void setDownloadMessage(int bytesToDownload)
  {
    Log.d(TAG, "prepareFilesDownload, bytesToDownload:" + bytesToDownload);

    if (bytesToDownload < 1024 * 1024)
      mMsgView.setText(String.format(getString(R.string.download_resources),
                                     (float)bytesToDownload / 1024,
                                     getString(R.string.kb)));
    else
      mMsgView.setText(String.format(getString(R.string.download_resources,
                                               (float)bytesToDownload / 1024 / 1024,
                                               getString(R.string.mb))));

    // set normal text color
    mMsgView.setTextColor(Color.WHITE);
  }

  private boolean prepareFilesDownload()
  {
    final int bytes = getBytesToDownload();

    // Show map if no any downloading needed.
    if (bytes == 0)
    {
      showMapView();
      return false;
    }

    // Do initialization once.
    if (mMapStorage == null)
      initDownloading();

    if (bytes > 0)
    {
      setDownloadMessage(bytes);

      mProgress.setMax(bytes);
      mProgress.setProgress(0);
    }
    else
    {
      finishFilesDownload(bytes);
    }

    return true;
  }

  private static final int DOWNLOAD = 0;
  private static final int PAUSE = 1;
  private static final int RESUME = 2;
  private static final int TRY_AGAIN = 3;
  private static final int PROCEED_TO_MAP = 4;
  private static final int BTN_COUNT = 5;

  private View.OnClickListener m_btnListeners[] = null;
  private String m_btnNames[] = null;

  private void initDownloading()
  {
    // Get GUI elements and subscribe to map storage (for country downloading).
    mMapStorage = mApplication.getMapStorage();
    mSlotId = mMapStorage.subscribe(this);

    mMsgView = (TextView)findViewById(R.id.download_resources_message);
    mProgress = (ProgressBar)findViewById(R.id.download_resources_progress);
    mButton = (Button)findViewById(R.id.download_resources_button);
    mDownloadCountryCheckBox = (CheckBox)findViewById(R.id.download_country_checkbox);
    mLocationMsgView = (TextView)findViewById(R.id.download_resources_location_message);

    // Initialize button states.
    m_btnListeners = new View.OnClickListener[BTN_COUNT];
    m_btnNames = new String[BTN_COUNT];

    m_btnListeners[DOWNLOAD] = new View.OnClickListener()
    {
      @Override
      public void onClick(View v) { onDownloadClicked(v); }
    };
    m_btnNames[DOWNLOAD] = getString(R.string.download);

    m_btnListeners[PAUSE] = new View.OnClickListener()
    {
      @Override
      public void onClick(View v) { onPauseClicked(v); }
    };
    m_btnNames[PAUSE] = getString(R.string.pause);

    m_btnListeners[RESUME] = new View.OnClickListener()
    {
      @Override
      public void onClick(View v) { onResumeClicked(v); }
    };
    m_btnNames[RESUME] = getString(R.string.continue_download);

    m_btnListeners[TRY_AGAIN] = new View.OnClickListener()
    {
      @Override
      public void onClick(View v) { onTryAgainClicked(v); }
    };
    m_btnNames[TRY_AGAIN] = getString(R.string.try_again);

    m_btnListeners[PROCEED_TO_MAP] = new View.OnClickListener()
    {
      @Override
      public void onClick(View v) { onProceedToMapClicked(v); }
    };
    m_btnNames[PROCEED_TO_MAP] = getString(R.string.download_resources_continue);

    // Start listening the location.
    mLocationService = mApplication.getLocationService();
    mLocationService.startUpdate(this);
  }

  private void setAction(int action)
  {
    mButton.setOnClickListener(m_btnListeners[action]);
    mButton.setText(m_btnNames[action]);
  }

  private void doDownload()
  {
    if (startNextFileDownload(this) == ERR_NO_MORE_FILES)
      finishFilesDownload(ERR_NO_MORE_FILES);
  }

  private void onDownloadClicked(View v)
  {
    setAction(PAUSE);
    doDownload();
  }

  private void onPauseClicked(View v)
  {
    setAction(RESUME);
    cancelCurrentFile();
  }

  private void onResumeClicked(View v)
  {
    setAction(PAUSE);
    doDownload();
  }

  private void onTryAgainClicked(View v)
  {
    // Initialize downloading from the beginning.
    if (prepareFilesDownload())
    {
      setAction(PAUSE);
      doDownload();
    }
  }

  private void onProceedToMapClicked(View v)
  {
    showMapView();
  }

  public String getErrorMessage(int res)
  {
    int id;
    switch (res)
    {
    case ERR_NOT_ENOUGH_FREE_SPACE: id = R.string.not_enough_free_space_on_sdcard; break;
    case ERR_STORAGE_DISCONNECTED: id = R.string.disconnect_usb_cable; break;

    case ERR_DOWNLOAD_ERROR:
      if (ConnectionState.isConnected(this))
        id = R.string.download_has_failed;
      else
        id = R.string.no_internet_connection_detected;
      break;

    default: id = R.string.not_enough_memory;
    }

    return getString(id);
  }

  public void showMapView()
  {
    // Continue with Main UI initialization (MWMActivity)
    Intent mwmActivityIntent = new Intent(this, MWMActivity.class);

    // Disable animation because MWMActivity should appear exactly over this one
    mwmActivityIntent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION | Intent.FLAG_ACTIVITY_REORDER_TO_FRONT);
    startActivity(mwmActivityIntent);

    finish();
  }

  public void finishFilesDownload(int result)
  {
    if (result == ERR_NO_MORE_FILES)
    {
      if (mCountryName != null && mDownloadCountryCheckBox.isChecked())
      {
        mDownloadCountryCheckBox.setVisibility(View.GONE);
        mLocationMsgView.setVisibility(View.GONE);
        mMsgView.setText(String.format(getString(R.string.downloading_country_can_proceed),
                                       mCountryName));

        MapStorage.Index idx = mMapStorage.findIndexByName(mCountryName);

        if (idx.isValid())
        {
          mProgress.setMax((int)mMapStorage.countryRemoteSizeInBytes(idx));
          mProgress.setProgress(0);

          mMapStorage.downloadCountry(idx);

          setAction(PROCEED_TO_MAP);
        }
        else
          showMapView();
      }
      else
        showMapView();
    }
    else
    {
      mMsgView.setText(getErrorMessage(result));
      mMsgView.setTextColor(Color.RED);

      setAction(TRY_AGAIN);
    }
  }

  @Override
  public void onCountryStatusChanged(MapStorage.Index idx)
  {
    final int status = mMapStorage.countryStatus(idx);

    if (status == MapStorage.ON_DISK)
      showMapView();
  }

  @Override
  public void onCountryProgress(MapStorage.Index idx, long current, long total)
  {
    // Important check - activity can be destroyed
    // but notifications from downloading thread are coming.
    if (mProgress != null)
      mProgress.setProgress((int)current);
  }

  private Intent getPackageIntent(String s)
  {
    return getPackageManager().getLaunchIntentForPackage(s);
  }

  private boolean checkLiteProPackages(boolean isPro)
  {
    try
    {
      if (!isPro)
      {
        final Intent intent = getPackageIntent("com.mapswithme.maps.pro");
        if (intent != null)
        {
          Log.i(TAG, "Trying to launch pro version");

          startActivity(intent);
          finish();
          return true;
        }
      }
      else
      {
        if (getPackageIntent("com.mapswithme.maps") != null)
        {
          Toast.makeText(this, R.string.suggest_uninstall_lite, Toast.LENGTH_LONG).show();
        }
      }
    }
    catch (ActivityNotFoundException ex)
    {
      Log.d(TAG, "Intent not found", ex);
    }

    return false;
  }

  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    // Do not turn off the screen while downloading needed resources
    getWindow().addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

    super.onCreate(savedInstanceState);

    mApplication = (MWMApplication)getApplication();

    final boolean isPro = mApplication.isProVersion();
    if (checkLiteProPackages(isPro))
      return;

    setContentView(R.layout.download_resources);

    // Create sdcard folder if it doesn't exist
    new File(mApplication.getDataStoragePath()).mkdirs();
    // Used to migrate from v2.0.0 to 2.0.1
    moveMaps(mApplication.getExtAppDirectoryPath("files"),
             mApplication.getDataStoragePath());

    if (prepareFilesDownload())
    {
      setAction(DOWNLOAD);

      if (ConnectionState.getState(this) == ConnectionState.CONNECTED_BY_WIFI)
        onDownloadClicked(mButton);
    }
  }

  @Override
  protected void onDestroy()
  {
    super.onDestroy();

    if (mLocationService != null)
    {
      mLocationService.stopUpdate(this);
      mLocationService = null;
    }

    if (mMapStorage != null)
      mMapStorage.unsubscribe(mSlotId);
  }

  @Override
  protected void onPause()
  {
    super.onPause();

    if (mLocationService != null)
      mLocationService.stopUpdate(this);
  }

  @Override
  protected void onResume()
  {
    super.onResume();

    if (mLocationService != null)
      mLocationService.startUpdate(this);
  }

  public void onDownloadProgress(int currentTotal, int currentProgress, int globalTotal, int globalProgress)
  {
    if (mProgress != null)
      mProgress.setProgress(globalProgress);
  }

  public void onDownloadFinished(int errorCode)
  {
    if (errorCode == ERR_DOWNLOAD_SUCCESS)
    {
      int res = startNextFileDownload(this);
      if (res == ERR_NO_MORE_FILES)
        finishFilesDownload(res);
    }
    else
      finishFilesDownload(errorCode);
  }

  @Override
  public void onLocationUpdated(long time, double lat, double lon, float accuracy)
  {
    if (mCountryName == null)
    {
      Log.i(TAG, "Searching for country name at location lat=" + lat + ", lon=" + lon);

      mCountryName = findCountryByPos(lat, lon);
      if (mCountryName != null)
      {
        mLocationMsgView.setVisibility(View.VISIBLE);

        int countryStatus = mMapStorage.countryStatus(mMapStorage.findIndexByName(mCountryName));
        if (countryStatus == MapStorage.ON_DISK)
          mLocationMsgView.setText(String.format(getString(R.string.download_location_map_up_to_date), mCountryName));
        else
        {
          CheckBox checkBox = (CheckBox)findViewById(R.id.download_country_checkbox);
          checkBox.setVisibility(View.VISIBLE);

          String msgViewText;
          String checkBoxText;

          if (countryStatus == MapStorage.ON_DISK_OUT_OF_DATE)
          {
            msgViewText = getString(R.string.download_location_update_map_proposal);
            checkBoxText = String.format(getString(R.string.update_country_ask), mCountryName);
          }
          else
          {
            msgViewText = getString(R.string.download_location_map_proposal);
            checkBoxText = String.format(getString(R.string.download_country_ask), mCountryName);
          }

          mLocationMsgView.setText(msgViewText);
          checkBox.setText(checkBoxText);
        }

        mLocationService.stopUpdate(this);
        mLocationService = null;
      }
    }
  }

  @Override
  public void onCompassUpdated(long time, double magneticNorth, double trueNorth, double accuracy)
  {
  }

  @Override
  public void onLocationError(int errorCode)
  {
  }

  private native void moveMaps(String fromFolder, String toFolder);
  private native int getBytesToDownload();
  private native boolean isWorldExists(String path);
  private native int startNextFileDownload(Object observer);
  private native String findCountryByPos(double lat, double lon);
  private native void cancelCurrentFile();
}
