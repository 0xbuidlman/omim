package com.mapswithme.maps.bookmarks;

import android.content.Intent;
import android.os.Bundle;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;

import com.mapswithme.maps.R;

public abstract class AbstractBookmarkCategoryActivity extends AbstractBookmarkListActivity
{
  private int mSelectedPosition;

  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
  }

  @Override
  public void onCreateContextMenu(ContextMenu menu, View v, ContextMenuInfo menuInfo)
  {
    if (menuInfo instanceof AdapterView.AdapterContextMenuInfo)
    {
      AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo) menuInfo;
      if (info.position < getAdapter().getCount())
      {
        mSelectedPosition = info.position;
        menu.setHeaderTitle(mManager.getCategoryById(mSelectedPosition).getName());
      }
    }
    super.onCreateContextMenu(menu, v, menuInfo);
  }

  @Override
  public boolean onContextItemSelected(MenuItem item)
  {
    int itemId = item.getItemId();
    if (itemId == R.id.set_edit)
    {
      startActivity(new Intent(this, BookmarkListActivity.class).
                    putExtra(BookmarkActivity.PIN_SET, mSelectedPosition).
                    putExtra(BookmarkListActivity.EDIT_CONTENT, enableEditing()));
    }
    else if (itemId == R.id.set_delete)
    {
      mManager.deleteCategory(mSelectedPosition);
      getAdapter().notifyDataSetChanged();
    }
    return super.onContextItemSelected(item);
  }

  protected abstract boolean enableEditing();

  protected AbstractBookmarkCategoryAdapter getAdapter()
  {
    return ((AbstractBookmarkCategoryAdapter) getListView().getAdapter());
  }

  @Override
  protected void onStart()
  {
    super.onStart();
    getAdapter().notifyDataSetChanged();
  }
}
