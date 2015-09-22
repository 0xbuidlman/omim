package com.mapswithme.maps.base;

import android.os.Bundle;
import android.support.v4.app.ListFragment;
import android.support.v7.widget.Toolbar;
import android.view.View;

import com.mapswithme.maps.R;
import com.mapswithme.util.UiUtils;

public abstract class BaseMwmListFragment extends ListFragment
{
  private Toolbar mToolbar;

  @Override
  public void onViewCreated(View view, Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);

    mToolbar = (Toolbar) view.findViewById(R.id.toolbar);
    if (mToolbar != null)
      UiUtils.showHomeUpButton(mToolbar);
  }

  public Toolbar getToolbar()
  {
    return mToolbar;
  }

  public void onResume()
  {
    super.onResume();
    org.alohalytics.Statistics.logEvent("$onResume", this.getClass().getSimpleName());
  }

  @Override
  public void onPause()
  {
    super.onPause();
    org.alohalytics.Statistics.logEvent("$onPause", this.getClass().getSimpleName());
  }
}
