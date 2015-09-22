package com.mapswithme.maps.settings;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.ListActivity;
import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Environment;
import android.os.StatFs;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.CheckedTextView;
import android.widget.ListView;
import android.widget.TextView;

import com.mapswithme.maps.R;
import com.mapswithme.util.Statistics;
import com.mapswithme.util.Utils;


public class StoragePathActivity extends ListActivity
{
  private static String TAG = "SettingsActivity";

  /// ListView adapter
  private static class StoragePathAdapter extends BaseAdapter
  {
    private static String TAG = "SettingsAdapter";
    private static String MWM_DIR_POSTFIX = "/MapsWithMe/";

    /// @name Different row types.
    //@{
    private static final int TYPE_HEADER = 0;
    private static final int TYPE_ITEM = 1;
    private static final int TYPES_COUNT = 2;
    //@}

    private LayoutInflater m_inflater;
    private Activity m_context;

    private String m_currPath;
    private String m_defPath;
    private long m_sizeNeeded;

    private int m_listItemHeight;

    public StoragePathAdapter(Activity context, String currPath, String defPath)
    {
      m_context = context;
      m_inflater = m_context.getLayoutInflater();

      m_currPath = currPath;
      m_defPath = defPath;

      m_listItemHeight = (int)Utils.getAttributeDimension(context, android.R.attr.listPreferredItemHeight);

      updateList();
    }

    private String getSizeString(long size)
    {
      String arrS[] = { "Kb", "Mb", "Gb" };

      long current = 1024;
      int i = 0;
      for (; i < arrS.length; ++i)
      {
        final long bound = 1024 * current;
        if (size < bound)
          break;
        else
          current = bound;
      }

      // left 1 digit after the comma and add postfix string
      return String.format("%.1f %s", (double)size / (double)current, arrS[i]);
    }

    private static class StorageItem
    {
      String m_path;
      long m_size;
    }

    private List<StorageItem> m_items = new ArrayList<StorageItem>();
    private int m_current = -1;

    private boolean isAvailable(int index)
    {
      assert(index >= 0 && index < m_items.size());
      return ((m_current != index) && (m_items.get(index).m_size >= m_sizeNeeded));
    }

    private int findItemByPath(String path)
    {
      for (int i = 0; i < m_items.size(); ++i)
        if (m_items.get(i).m_path.equals(path))
          return i;
      return -1;
    }

    private void updateList()
    {
      m_sizeNeeded = getDirSize(m_currPath + MWM_DIR_POSTFIX);
      Log.i(TAG, "Needed size for maps: " + m_sizeNeeded);

      m_items.clear();

      //parseMountFile("/etc/vold.conf", VOLD_MODE, currPath, defPath, sizeNeeded);
      //parseMountFile("/etc/vold.fstab", VOLD_MODE, currPath, defPath, sizeNeeded);
      parseMountFile("/proc/mounts", MOUNTS_MODE);

      m_current = findItemByPath(m_currPath);
      assert(m_current != -1);
    }

    private boolean addStorage(String path)
    {
      try
      {
        final File f = new File(path);
        if (f.exists() && f.isDirectory() && f.canWrite())
        {
          if (findItemByPath(path) != -1)
            return false;

          StatFs stat = new StatFs(path);
          final long size = (long)stat.getAvailableBlocks() * (long)stat.getBlockSize();
          Log.i(TAG, "Available size = " + size);

          StorageItem item = new StorageItem();
          item.m_path = path;
          item.m_size = size;

          m_items.add(item);
          return true;
        }
        else
          Log.i(TAG, "File error for storage: " + path);
      }
      catch (IllegalArgumentException ex)
      {
        // Suppress exceptions for unavailable storages.
        Log.i(TAG, "StatFs error for storage: " + path);
      }

      return false;
    }

    private static int CURRENT_PATH = 1;
    private static int DEFAULT_PATH = 2;

    /// @return Flag that current path or default path is added successfully.
    private int callAddStorage(String path, int flag)
    {
      if (addStorage(path))
      {
        if (path.equals(m_currPath))
          flag |= CURRENT_PATH;
        if (path.equals(m_defPath))
          flag |= DEFAULT_PATH;
      }

      return flag;
    }

    private static int VOLD_MODE = 1;
    private static int MOUNTS_MODE = 2;

    private void parseMountFile(String file, int mode)
    {
      BufferedReader reader = null;
      int addFlag = 0;

      try
      {
        reader = new BufferedReader(new FileReader(file));

        while (true)
        {
          String line = reader.readLine();
          if (line == null) break;

          String[] arr = line.split(" ");
          if (arr.length >= 3)
          {
            if (arr[0].charAt(0) == '#')
              continue;

            if (mode == VOLD_MODE)
            {
              Log.i(TAG, "Label = " + arr[1] + "; Path = " + arr[2]);

              if (arr[0].startsWith("dev_mount"))
                addFlag = callAddStorage(arr[2], addFlag);
            }
            else
            {
              assert(mode == MOUNTS_MODE);
              Log.i(TAG, "Label = " + arr[0] + "; Path = " + arr[1]);

              String prefixes[] = { "tmpfs", "/dev/block/vold", "/dev/fuse" };
              for (String s : prefixes)
                if (arr[0].startsWith(s))
                  addFlag = callAddStorage(arr[1], addFlag);
            }
          }
        }
      }
      catch (IOException e)
      {
        Log.w(TAG, "Can't read file: " + file);
      }
      finally
      {
        Utils.closeStream(reader);
      }

      // Check that current and default paths are added to the list.
      if ((addFlag & CURRENT_PATH) == 0)
        addFlag = callAddStorage(m_currPath, addFlag);
      if ((addFlag & DEFAULT_PATH) == 0)
        addFlag = callAddStorage(m_defPath, addFlag);

      assert(addFlag == (CURRENT_PATH | DEFAULT_PATH));
    }

    /// @name Assume that MapsWithMe folder doesn't have inner folders and symbolic links.
    //@{
    private long getDirSize(String name)
    {
      File dir = new File(name);
      assert(dir.exists());
      assert(dir.isDirectory());

      long size = 0;
      for (File f : dir.listFiles())
      {
        assert(f.isFile());
        size += f.length();
      }

      return (size + 1024*1024);
    }

    // delete all files (except settings.ini) in directory
    private void deleteFiles(File dir)
    {
      assert(dir.exists());
      assert(dir.isDirectory());

      for (File file : dir.listFiles())
      {
        assert(file.isFile());

        // skip settings.ini - this file should be always in one place
        if (file.getName().equalsIgnoreCase("settings.ini"))
          continue;

        if (!file.delete())
          Log.w(TAG, "Can't delete file: " + file.getName());
      }
    }
    //@}

    private static int HEADERS_COUNT = 1;

    @Override
    public int getItemViewType(int position)
    {
      return (position == 0 ? TYPE_HEADER : TYPE_ITEM);
    }

    @Override
    public int getViewTypeCount()
    {
      return TYPES_COUNT;
    }

    @Override
    public int getCount()
    {
      return (m_items != null ? m_items.size() + HEADERS_COUNT : HEADERS_COUNT);
    }

    @Override
    public StorageItem getItem(int position)
    {
      return (position == 0 ? null : m_items.get(getIndexFromPos(position)));
    }

    @Override
    public long getItemId(int position)
    {
      return position;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent)
    {
      // 1. It's a strange thing, but when I tried to use setClickable,
      // all the views become nonclickable.
      // 2. I call setMinimumHeight(listPreferredItemHeight)
      // because standard item's height is unknown.

      switch (getItemViewType(position))
      {
      case TYPE_HEADER:
      {
        if (convertView == null)
        {
          convertView = m_inflater.inflate(android.R.layout.simple_list_item_1, null);
          convertView.setMinimumHeight(m_listItemHeight);
        }

        TextView v = (TextView) convertView;
        v.setText(m_context.getString(R.string.maps) + ": " + getSizeString(m_sizeNeeded));
        break;
      }

      case TYPE_ITEM:
      {
        final int index = getIndexFromPos(position);
        final StorageItem item = m_items.get(index);

        if (convertView == null)
        {
          convertView = m_inflater.inflate(android.R.layout.simple_list_item_single_choice, null);
          convertView.setMinimumHeight(m_listItemHeight);
        }

        CheckedTextView v = (CheckedTextView) convertView;
        v.setText(item.m_path + ": " + getSizeString(item.m_size));
        v.setChecked(index == m_current);
        v.setEnabled((index == m_current) || isAvailable(index));
        break;
      }
      }

      return convertView;
    }

    private int getIndexFromPos(int position)
    {
      final int index = position - HEADERS_COUNT;
      assert(index >= 0 && index < m_items.size());
      return index;
    }

    private String getFullPath(int index)
    {
      assert(index >= 0 && index < m_items.size());
      return (m_items.get(index).m_path + MWM_DIR_POSTFIX);
    }

    private boolean doMoveMaps(String path)
    {
      if (StoragePathActivity.nativeSetStoragePath(path))
      {
        if (m_current != -1)
          deleteFiles(new File(getFullPath(m_current)));

        return true;
      }

      return false;
    }

    private void doUpdateAfterMove(String path)
    {
      m_currPath = path;

      updateList();
      notifyDataSetChanged();
    }

    private class MoveFilesTask extends AsyncTask<String, Void, Boolean>
    {
      private ProgressDialog m_dlg;
      private String m_resPath;

      public MoveFilesTask(Activity context, String path)
      {
        m_resPath = path;

        m_dlg = new ProgressDialog(context);
        m_dlg.setMessage(context.getString(R.string.wait_several_minutes));
        m_dlg.setProgressStyle(ProgressDialog.STYLE_SPINNER);
        m_dlg.setIndeterminate(true);
        m_dlg.setCancelable(false);
      }

      @Override
      protected void onPreExecute()
      {
        m_dlg.show();
      }

      @Override
      protected Boolean doInBackground(String... params)
      {
        return doMoveMaps(params[0]);
      }

      @Override
      protected void onPostExecute(Boolean result)
      {
        // Using dummy try-catch because of the following:
        // http://stackoverflow.com/questions/2745061/java-lang-illegalargumentexception-view-not-attached-to-window-manager
        try
        {
          m_dlg.dismiss();
        }
        catch (Exception e)
        {
        }

        if (result)
          doUpdateAfterMove(m_resPath);
      }
    }

    public void onListItemClick(final int position)
    {
      final int index = getIndexFromPos(position);
      if (isAvailable(index))
      {
        final String path = getFullPath(index);

        File f = new File(path);
        if (!f.exists() && !f.mkdirs())
        {
          Log.e(TAG, "Can't create directory: " + path);
          return;
        }

        new AlertDialog.Builder(m_context)
        .setCancelable(false)
        .setTitle(R.string.move_maps)
        .setPositiveButton(R.string.ok, new DialogInterface.OnClickListener()
        {
          @Override
          public void onClick(DialogInterface dlg, int which)
          {
            Log.i(TAG, "Transfer data to storage: " + path);

            MoveFilesTask task = new MoveFilesTask(m_context, m_items.get(index).m_path);
            task.execute(path);

            dlg.dismiss();
          }
        })
        .setNegativeButton(R.string.cancel, new DialogInterface.OnClickListener()
        {
          @Override
          public void onClick(DialogInterface dlg, int which)
          {
            dlg.dismiss();
          }
        })
        .create()
        .show();
      }
    }
  }

  @Override
  public void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);

    final String currPath = nativeGetStoragePath();
    final String defPath = Environment.getExternalStorageDirectory().getAbsolutePath();
    Log.i(TAG, "Current and Default maps pathes: " + currPath + "; " + defPath);

    setListAdapter(new StoragePathAdapter(this, currPath, defPath));
  }

  @Override
  protected void onStart()
  {
    super.onStart();
    
    Statistics.INSTANCE.startActivity(this);
  }
  
  @Override
  protected void onStop()
  {
    super.onStop();
    
    Statistics.INSTANCE.stopActivity(this);
  }

  @Override
  protected void onListItemClick(final ListView l, View v, final int position, long id)
  {
    // Do not process clicks on header items.
    if (position != 0)
      ((StoragePathAdapter) getListView().getAdapter()).onListItemClick(position);
  }

  private native String nativeGetStoragePath();
  private static native boolean nativeSetStoragePath(String newPath);
}
