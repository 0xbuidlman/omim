package com.mapswithme.maps.editor;

import android.support.annotation.Size;
import android.support.v7.app.AlertDialog;

import com.mapswithme.maps.base.BaseMwmToolbarFragment;
import com.mapswithme.util.Utils;
import com.mapswithme.util.statistics.Statistics;

public abstract class BaseAuthFragment extends BaseMwmToolbarFragment
{
  public enum AuthType
  {
    OSM("OSM"),
    FACEBOOK("Facebook"),
    GOOGLE("Google");

    String name;

    AuthType(String name)
    {
      this.name = name;
    }
  }

  protected void processAuth(@Size(2) String[] auth, AuthType type)
  {
    if (auth == null)
    {
      if (isAdded())
      {
        // TODO set correct text
        new AlertDialog.Builder(getActivity()).setTitle("Auth error!")
                                              .setPositiveButton(android.R.string.ok, null).show();

        Statistics.INSTANCE.trackEvent(Statistics.EventName.EDITOR_AUTH_REQUEST_RESULT,
                                       Statistics.params().add(Statistics.EventParam.IS_SUCCESS, false).add(Statistics.EventParam.TYPE, type.name));
      }
      return;
    }

    OsmOAuth.setAuthorization(auth[0], auth[1]);
    Utils.navigateToParent(getActivity());
    Statistics.INSTANCE.trackEvent(Statistics.EventName.EDITOR_AUTH_REQUEST_RESULT,
                                   Statistics.params().add(Statistics.EventParam.IS_SUCCESS, true).add(Statistics.EventParam.TYPE, type.name));
  }
}
