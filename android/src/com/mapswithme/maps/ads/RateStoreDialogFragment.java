package com.mapswithme.maps.ads;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v4.app.DialogFragment;
import android.text.format.DateUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.RatingBar;
import android.widget.TextView;

import com.mapswithme.maps.BuildConfig;
import com.mapswithme.maps.MWMApplication;
import com.mapswithme.maps.R;
import com.mapswithme.util.Constants;
import com.mapswithme.util.UiUtils;
import com.mapswithme.util.Utils;
import com.mapswithme.util.statistics.Statistics;
import com.nineoldandroids.animation.Animator;
import com.nineoldandroids.animation.ObjectAnimator;

public class RateStoreDialogFragment extends DialogFragment implements View.OnClickListener
{
  private float mRating;

  @NonNull
  @Override
  public Dialog onCreateDialog(Bundle savedInstanceState)
  {
    final AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
    final LayoutInflater inflater = getActivity().getLayoutInflater();

    final View root = inflater.inflate(R.layout.fragment_google_play_dialog, null);
    builder.
        setView(root).
        setNegativeButton(getString(R.string.remind_me_later), new DialogInterface.OnClickListener()
        {
          @Override
          public void onClick(DialogInterface dialog, int which)
          {
            Statistics.INSTANCE.trackSimpleNamedEvent(Statistics.EventName.RATE_DIALOG_LATER);
          }
        });

    final RatingBar rateBar = (RatingBar) root.findViewById(R.id.rb__play_rate);

    rateBar.setOnRatingBarChangeListener(new RatingBar.OnRatingBarChangeListener()
    {
      @Override
      public void onRatingChanged(RatingBar ratingBar, float rating, boolean fromUser)
      {
        Statistics.INSTANCE.trackRatingDialog(rating);
        mRating = rating;
        if (rating == 5.0f)
        {
          dismiss();
          Utils.openAppInMarket(getActivity(), BuildConfig.REVIEW_URL);
        }
        else
        {
          ObjectAnimator animator = ObjectAnimator.ofFloat(rateBar, "alpha", 1.0f, 0.0f);
          animator.addListener(new UiUtils.SimpleNineoldAnimationListener()
          {
            @Override
            public void onAnimationEnd(Animator animation)
            {
              final Button button = (Button) root.findViewById(R.id.btn__explain_bad_rating);
              button.setVisibility(View.VISIBLE);
              button.setOnClickListener(RateStoreDialogFragment.this);
              ((TextView) root.findViewById(R.id.tv__title)).setText(getString(R.string.rating_thanks));
              ((TextView) root.findViewById(R.id.tv__subtitle)).setText(getString(R.string.rating_share_ideas));
              root.findViewById(R.id.v__divider).setVisibility(View.VISIBLE);
              rateBar.setVisibility(View.GONE);
              super.onAnimationEnd(animation);
            }
          });
          animator.start();
        }
      }
    });

    return builder.create();
  }

  @Override
  public void onCancel(DialogInterface dialog)
  {
    super.onCancel(dialog);
    Statistics.INSTANCE.trackSimpleNamedEvent(Statistics.EventName.RATE_DIALOG_LATER);
  }

  @Override
  public void onClick(View v)
  {
    switch (v.getId())
    {
    case R.id.btn__explain_bad_rating:
      dismiss();
      final Intent intent = new Intent(Intent.ACTION_SENDTO);
      final PackageInfo info;
      long installTime = 0;
      try
      {
        info = MWMApplication.get().getPackageManager().getPackageInfo(BuildConfig.APPLICATION_ID, 0);
        installTime = info.firstInstallTime;
      } catch (PackageManager.NameNotFoundException e)
      {
        e.printStackTrace();
      }
      intent.setData(Utils.buildMailUri(Constants.Url.MAIL_MAPSME_RATING, "Rating : " + mRating,
          "OS : " + Build.VERSION.SDK_INT + "\n" + "Version : " + BuildConfig.APPLICATION_ID + " " + BuildConfig.VERSION_NAME + "\n" +
          getString(R.string.rating_user_since) + " " + DateUtils.formatDateTime(getActivity(), installTime, 0) + "\n\n"));
      try
      {
        startActivity(intent);
      } catch (android.content.ActivityNotFoundException ex)
      {
        //
      }
      break;
    }
  }
}
