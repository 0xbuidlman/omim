package com.mapswithme.util;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.Activity;
import android.content.ClipData;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.text.TextUtils;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.Window;
import android.widget.EditText;
import android.widget.Toast;

import com.mapswithme.maps.BuildConfig;
import com.mapswithme.maps.MWMApplication;

import java.io.Closeable;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.NoSuchElementException;

public class Utils
{
  private static final String TAG = "Utils";

  public static String firstNotEmpty(String... args)
  {
    for (int i = 0; i < args.length; i++)
      if (!TextUtils.isEmpty(args[i]))
        return args[i];

    throw new NoSuchElementException("All argument are empty");
  }

  public static void closeStream(Closeable stream)
  {
    if (stream != null)
    {
      try
      {
        stream.close();
      } catch (final IOException e)
      {
        Log.e(TAG, "Can't close stream", e);
      }
    }
  }

  public static boolean isAmazonDevice()
  {
    return "Amazon".equalsIgnoreCase(Build.MANUFACTURER);
  }


  public static boolean hasAnyGoogleStoreInstalled()
  {
    final String GooglePlayStorePackageNameOld = "com.google.market";
    final String GooglePlayStorePackageNameNew = "com.android.vending";
    final PackageManager pm = MWMApplication.get().getPackageManager();
    final List<PackageInfo> packages = pm.getInstalledPackages(0);
    for (final PackageInfo packageInfo : packages)
    {
      if (packageInfo.packageName.equals(GooglePlayStorePackageNameOld)
          || packageInfo.packageName.equals(GooglePlayStorePackageNameNew))
        return true;
    }
    return false;
  }

  // if enabled, screen will be turned off automatically by the system
  // if disabled, screen will be always turn on
  public static void automaticIdleScreen(boolean enable, Window w)
  {
    if (enable)
      w.clearFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    else
      w.addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
  }

  public static float getAttributeDimension(Activity activity, int attr)
  {
    final android.util.TypedValue value = new android.util.TypedValue();
    final boolean b = activity.getTheme().resolveAttribute(attr, value, true);
    assert (b);
    final android.util.DisplayMetrics metrics = new android.util.DisplayMetrics();
    activity.getWindowManager().getDefaultDisplay().getMetrics(metrics);
    return value.getDimension(metrics);
  }

  public static void setTextAndCursorToEnd(EditText edit, String s)
  {
    edit.setText(s);
    edit.setSelection(s.length());
  }

  public static void toastShortcut(Context context, String message)
  {
    Toast.makeText(context, message, Toast.LENGTH_LONG).show();
  }

  public static void toastShortcut(Context context, int messageResId)
  {
    final String message = context.getString(messageResId);
    toastShortcut(context, message);
  }

  public static boolean isIntentSupported(Context context, Intent intent)
  {
    return context.getPackageManager().resolveActivity(intent, 0) != null;
  }

  public static boolean apiEqualOrGreaterThan(int api)
  {
    return Build.VERSION.SDK_INT >= api;
  }

  public static boolean apiLowerThan(int api)
  {
    return Build.VERSION.SDK_INT < api;
  }

  public static void checkNotNull(Object object)
  {
    if (null == object) throw new NullPointerException("Argument here must not be NULL");
  }

  @SuppressLint("NewApi")
  @SuppressWarnings("deprecation")
  public static void copyTextToClipboard(Context context, String text)
  {
    if (apiLowerThan(11))
    {
      final android.text.ClipboardManager clipbord =
          (android.text.ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
      clipbord.setText(text);
    }
    else
    {
      // This is different classes in different packages
      final android.content.ClipboardManager clipboard =
          (android.content.ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
      final ClipData clip = ClipData.newPlainText("maps.me: " + text, text);
      clipboard.setPrimaryClip(clip);
    }
  }

  public static <K, V> String mapPrettyPrint(Map<K, V> map)
  {
    if (map == null)
      return "[null]";
    if (map.isEmpty())
      return "[]";


    String joined = "";
    for (final K key : map.keySet())
    {
      final String keyVal = key + "=" + map.get(key);
      if (joined.length() > 0)
        joined = TextUtils.join(",", new Object[]{joined, keyVal});
      else
        joined = keyVal;
    }

    return "[" + joined + "]";
  }

  @TargetApi(Build.VERSION_CODES.HONEYCOMB)
  public static MenuItem addMenuCompat(Menu menu, int id, int order, int titleResId, int iconResId)
  {
    final MenuItem mItem = menu.add(Menu.NONE, id, order, titleResId);
    mItem.setIcon(iconResId);
    if (apiEqualOrGreaterThan(11))
      mItem.setShowAsAction(MenuItem.SHOW_AS_ACTION_IF_ROOM);

    return mItem;
  }

  public static Object[] asObjectArray(Object... args)
  {
    return args;
  }

  public static boolean isPackageInstalled(String packageUri)
  {
    PackageManager pm = MWMApplication.get().getPackageManager();
    boolean installed;
    try
    {
      pm.getPackageInfo(packageUri, PackageManager.GET_ACTIVITIES);
      installed = true;
    } catch (PackageManager.NameNotFoundException e)
    {
      installed = false;
    }
    return installed;
  }

  public static void launchPackage(Activity activity, String appPackage)
  {
    final Intent intent = activity.getPackageManager().getLaunchIntentForPackage(appPackage);
    if (intent != null)
      activity.startActivity(intent);
  }


  public static boolean isIntentAvailable(Intent intent)
  {
    PackageManager mgr = MWMApplication.get().getPackageManager();
    return mgr.queryIntentActivities(intent, PackageManager.MATCH_DEFAULT_ONLY).size() > 0;
  }

  public static Uri buildMailUri(String to, String subject, String body)
  {
    String uriString = Constants.Url.MAILTO_SCHEME + Uri.encode(to) +
        Constants.Url.MAIL_SUBJECT + Uri.encode(subject) +
        Constants.Url.MAIL_BODY + Uri.encode(body);

    return Uri.parse(uriString);
  }

  /**
   * Stores logcat output of the application to a file on primary external storage.
   *
   * @return name of the logfile. May be null in case of error.
   */
  public static String saveLogToFile()
  {
    String fullName = MWMApplication.get().getDataStoragePath() + "log.txt";
    File file = new File(fullName);
    InputStreamReader reader = null;
    FileWriter writer = null;
    try
    {
      writer = new FileWriter(file);
      writer.write("Android version: " + Build.VERSION.SDK_INT + "\n");
      writer.write("Device: " + getDeviceModel() + "\n");
      writer.write("App version: " + BuildConfig.APPLICATION_ID + " " + BuildConfig.VERSION_CODE + "\n");
      writer.write("Locale : " + Locale.getDefault() + "\n\n");

      String cmd = "logcat -d -v time";
      Process process = Runtime.getRuntime().exec(cmd);
      reader = new InputStreamReader(process.getInputStream());
      char[] buffer = new char[10000];
      do
      {
        int n = reader.read(buffer, 0, buffer.length);
        if (n == -1)
          break;
        writer.write(buffer, 0, n);
      } while (true);

      reader.close();
      writer.close();
    } catch (IOException e)
    {
      closeStream(writer);
      closeStream(reader);

      return null;
    }

    return fullName;
  }

  private static String getDeviceModel()
  {
    String model = Build.MODEL;
    if (!model.startsWith(Build.MANUFACTURER))
      model = Build.MANUFACTURER + " " + model;

    return model;
  }


  private Utils() {}
}
