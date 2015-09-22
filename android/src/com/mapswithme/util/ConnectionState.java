package com.mapswithme.util;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;

public class ConnectionState
{
  public static final int NOT_CONNECTED = 0;
  public static final int CONNECTED_BY_3G = 1;
  public static final int CONNECTED_BY_WIFI = 2;
  public static final int CONNECTED_BY_WIFI_AND_3G = CONNECTED_BY_3G & CONNECTED_BY_WIFI;

  public static int getState(Context c)
  {
    boolean haveConnectedWifi = false;
    boolean haveConnectedMobile = false;

    ConnectivityManager cm = (ConnectivityManager) c.getSystemService(Context.CONNECTIVITY_SERVICE);
    NetworkInfo[] netInfo = cm.getAllNetworkInfo();
    for (NetworkInfo ni : netInfo)
    {
      if (ni.getTypeName().equalsIgnoreCase("WIFI"))
        if (ni.isConnected())
          haveConnectedWifi = true;
      if (ni.getTypeName().equalsIgnoreCase("MOBILE"))
        if (ni.isConnected())
          haveConnectedMobile = true;
    }
    if (haveConnectedWifi && haveConnectedMobile)
      return CONNECTED_BY_WIFI_AND_3G;
    else if (haveConnectedMobile)
      return CONNECTED_BY_3G;
    else if (haveConnectedWifi)
      return CONNECTED_BY_WIFI;

    return NOT_CONNECTED;
  }

  public static boolean is3GConnected(Context c)
  {
    return (getState(c) & CONNECTED_BY_3G) == CONNECTED_BY_3G;
  }

  public static boolean isWifiConnected(Context c)
  {
    return (getState(c) & CONNECTED_BY_WIFI) == CONNECTED_BY_WIFI;
  }

  public static boolean isConnected(Context c)
  {
    return !(getState(c) == NOT_CONNECTED);
  }
}
