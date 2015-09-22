package com.mapswithme.maps.api;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;

import com.mapswithme.maps.Framework;

public class ParsedMmwRequest
{

  private static volatile ParsedMmwRequest sCurrentRequest;

  // caller info
  private ApplicationInfo mCallerInfo;
  // title
  private String mTitle;
  // pending intent to call back
  private PendingIntent mPendingIntent;
  // return on ballon click
  private boolean mReturnOnBalloonClick;
  // pick point mode
  private boolean mPickPoint;
  // custom button name
  private String mCustomButtonName;

  // response data
  private boolean mHasPoint;
  private double mLat;
  private double mLon;
  private double mZoomLevel;


  public double getLat() { return mLat; }

  public double getLon() { return mLon; }

  public String getName() { return mName;}

  public boolean isPickPointMode() { return mPickPoint; }

  public boolean hasCustomButtonName() { return !TextUtils.isEmpty(mCustomButtonName); }

  public String getCustomButtonName() { return mCustomButtonName; }

  private String mName;
  private String mId;

  public static ParsedMmwRequest getCurrentRequest() { return sCurrentRequest; }

  public static boolean hasRequest() { return sCurrentRequest != null; }

  public static void setCurrentRequest(ParsedMmwRequest request) { sCurrentRequest = request; }

  public static ParsedMmwRequest extractFromIntent(Intent data, Context context)
  {
    final ParsedMmwRequest request = new ParsedMmwRequest();

    request.mCallerInfo = data.getParcelableExtra(Const.EXTRA_CALLER_APP_INFO);
    request.mTitle = data.getStringExtra(Const.EXTRA_TITLE);
    request.mReturnOnBalloonClick = data.getBooleanExtra(Const.EXTRA_RETURN_ON_BALLOON_CLICK, false);
    request.mPickPoint = data.getBooleanExtra(Const.EXTRA_PICK_POINT, false);
    request.mCustomButtonName = data.getStringExtra(Const.EXTRA_CUSTOM_BUTTON_NAME);

    if (data.getBooleanExtra(Const.EXTRA_HAS_PENDING_INTENT, false))
    {
      request.mPendingIntent = data.getParcelableExtra(Const.EXTRA_CALLER_PENDING_INTENT);
    }

    return request;
  }

  // Response data
  public ApplicationInfo getCallerInfo()
  {
    return mCallerInfo;
  }

  public boolean hasTitle() { return mTitle != null; }

  public String getTitle() { return mTitle; }

  // Request data
  public boolean hasPoint()
  {
    return mHasPoint;
  }

  public void setHasPoint(boolean hasPoint) { mHasPoint = hasPoint; }

  public boolean hasPendingIntent() { return mPendingIntent != null; }

  public boolean doReturnOnBalloonClick() { return mReturnOnBalloonClick; }

  public void setPointData(double lat, double lon, String name, String id)
  {
    mLat = lat;
    mLon = lon;
    mName = name;
    mId = id;
  }

  public Drawable getIcon(Context context)
  {
    return context.getPackageManager().getApplicationIcon(mCallerInfo);
  }

  public CharSequence getCallerName(Context context)
  {
    return context.getPackageManager().getApplicationLabel(mCallerInfo);
  }

  public boolean sendResponse(Context context, boolean success)
  {
    if (hasPendingIntent())
    {
      // set zoom level from framework
      mZoomLevel = Framework.getDrawScale();
      final Intent i = new Intent();
      if (success)
      {
        i.putExtra(Const.EXTRA_MWM_RESPONSE_POINT_LAT, mLat)
            .putExtra(Const.EXTRA_MWM_RESPONSE_POINT_LON, mLon)
            .putExtra(Const.EXTRA_MWM_RESPONSE_POINT_NAME, mName)
            .putExtra(Const.EXTRA_MWM_RESPONSE_POINT_ID, mId)
            .putExtra(Const.EXTRA_MWM_RESPONSE_ZOOM, mZoomLevel);
      }
      try
      {
        mPendingIntent.send(context, success ? Activity.RESULT_OK : Activity.RESULT_CANCELED, i);
        return true;
      } catch (final Exception e)
      {
        e.printStackTrace();
      }
    }
    return false;
  }

  public void sendResponseAndFinish(final Activity activity, final boolean success)
  {
    activity.runOnUiThread(new Runnable()
    {
      @Override
      public void run()
      {
        sendResponse(activity, success);
        activity.finish();
      }
    });
  }

  /**
   * Do not use constructor externally. Use {@link ParsedMmwRequest#extractFromIntent(Intent, Context)} instead.
   */
  private ParsedMmwRequest()
  {}
}
