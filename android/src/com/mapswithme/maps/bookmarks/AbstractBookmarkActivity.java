package com.mapswithme.maps.bookmarks;

import android.annotation.SuppressLint;
import android.app.ActionBar;
import android.app.Activity;
import android.os.Build;
import android.os.Bundle;
import android.view.MenuItem;
import android.view.inputmethod.InputMethodManager;

import com.mapswithme.maps.bookmarks.data.BookmarkManager;
import com.mapswithme.util.Statistics;

public abstract class AbstractBookmarkActivity extends Activity
{
  protected BookmarkManager mManager;

  @SuppressLint("NewApi")
  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
    mManager = BookmarkManager.getBookmarkManager(getApplicationContext());

    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB)
    {
      // http://stackoverflow.com/questions/6867076/getactionbar-returns-null
      ActionBar bar = getActionBar();
      if (bar != null)
        bar.setDisplayHomeAsUpEnabled(true);
    }
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
  public boolean onOptionsItemSelected(MenuItem item)
  {
    if (item.getItemId() == android.R.id.home)
    {
      InputMethodManager imm = (InputMethodManager) getSystemService(Activity.INPUT_METHOD_SERVICE);
      imm.toggleSoftInput(InputMethodManager.HIDE_IMPLICIT_ONLY, 0);
      onBackPressed();
      return true;
    }
    else
      return super.onOptionsItemSelected(item);
  }
}
