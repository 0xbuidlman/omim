package com.mapswithme.maps.widget.placepage;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.Color;
import android.location.Location;
import android.net.Uri;
import android.os.Build;
import android.support.annotation.ColorInt;
import android.support.annotation.NonNull;
import android.support.v4.app.Fragment;
import android.support.v7.widget.Toolbar;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.TextUtils;
import android.text.style.ForegroundColorSpan;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.PopupMenu;
import android.widget.RelativeLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.text.NumberFormat;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Currency;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

import com.mapswithme.maps.Framework;
import com.mapswithme.maps.MwmActivity;
import com.mapswithme.maps.MwmApplication;
import com.mapswithme.maps.R;
import com.mapswithme.maps.api.ParsedMwmRequest;
import com.mapswithme.maps.bookmarks.data.Bookmark;
import com.mapswithme.maps.bookmarks.data.BookmarkManager;
import com.mapswithme.maps.bookmarks.data.DistanceAndAzimut;
import com.mapswithme.maps.bookmarks.data.MapObject;
import com.mapswithme.maps.bookmarks.data.Metadata;
import com.mapswithme.maps.downloader.CountryItem;
import com.mapswithme.maps.downloader.DownloaderStatusIcon;
import com.mapswithme.maps.downloader.MapManager;
import com.mapswithme.maps.editor.Editor;
import com.mapswithme.maps.editor.OpeningHours;
import com.mapswithme.maps.editor.data.TimeFormatUtils;
import com.mapswithme.maps.editor.data.Timetable;
import com.mapswithme.maps.location.LocationHelper;
import com.mapswithme.maps.routing.RoutingController;
import com.mapswithme.maps.widget.ArrowView;
import com.mapswithme.maps.widget.BaseShadowController;
import com.mapswithme.maps.widget.ObservableScrollView;
import com.mapswithme.maps.widget.ScrollViewShadowController;
import com.mapswithme.util.Graphics;
import com.mapswithme.util.StringUtils;
import com.mapswithme.util.ThemeUtils;
import com.mapswithme.util.UiUtils;
import com.mapswithme.util.Utils;
import com.mapswithme.util.concurrency.UiThread;
import com.mapswithme.util.sharing.ShareOption;
import com.mapswithme.util.statistics.AlohaHelper;
import com.mapswithme.util.statistics.Statistics;


public class PlacePageView extends RelativeLayout
                        implements View.OnClickListener,
                                   View.OnLongClickListener,
                                   SponsoredHotel.OnPriceReceivedListener
{
  private static final String PREF_USE_DMS = "use_dms";

  private boolean mIsDocked;
  private boolean mIsFloating;

  // Preview.
  private ViewGroup mPreview;
  private Toolbar mToolbar;
  private TextView mTvTitle;
  private TextView mTvSubtitle;
  private ArrowView mAvDirection;
  private TextView mTvDistance;
  private TextView mTvAddress;
  private View mHotelInfo;
  private TextView mTvHotelRating;
  private TextView mTvHotelPrice;
  // Details.
  private ScrollView mDetails;
  private View mPhone;
  private TextView mTvPhone;
  private View mWebsite;
  private TextView mTvWebsite;
  private TextView mTvLatlon;
  private View mOpeningHours;
  private TextView mFullOpeningHours;
  private TextView mTodayOpeningHours;
  private View mWifi;
  private View mEmail;
  private TextView mTvEmail;
  private View mOperator;
  private TextView mTvOperator;
  private View mCuisine;
  private TextView mTvCuisine;
  private View mWiki;
  private View mEntrance;
  private TextView mTvEntrance;
  private View mEditPlace;
  private View mAddOrganisation;
  private View mAddPlace;
  private View mMoreInfo;
  // Bookmark
  private View mBookmarkFrame;
  private TextView mBookmarkNote;
  private boolean mBookmarkSet;
  // Place page buttons
  private PlacePageButtons mButtons;
  private ImageView mBookmarkButtonIcon;

  // Animations
  private BaseShadowController mShadowController;
  private BasePlacePageAnimationController mAnimationController;
  private MwmActivity.LeftAnimationTrackListener mLeftAnimationTrackListener;
  // Data
  private MapObject mMapObject;
  private SponsoredHotel mSponsoredHotel;
  private String mSponsoredHotelPrice;
  private boolean mIsLatLonDms;

  // Downloader`s stuff
  private DownloaderStatusIcon mDownloaderIcon;
  private TextView mDownloaderInfo;
  private int mStorageCallbackSlot;
  private CountryItem mCurrentCountry;

  private final MapManager.StorageCallback mStorageCallback = new MapManager.StorageCallback()
  {
    @Override
    public void onStatusChanged(List<MapManager.StorageCallbackData> data)
    {
      if (mCurrentCountry == null)
        return;

      for (MapManager.StorageCallbackData item : data)
        if (mCurrentCountry.id.equals(item.countryId))
        {
          updateDownloader();
          return;
        }
    }

    @Override
    public void onProgress(String countryId, long localSize, long remoteSize)
    {
      if (mCurrentCountry != null && mCurrentCountry.id.equals(countryId))
        updateDownloader();
    }
  };

  private final Runnable mDownloaderDeferredDetachProc = new Runnable()
  {
    @Override
    public void run()
    {
      detachCountry();
    }
  };

  public enum State
  {
    HIDDEN,
    PREVIEW,
    DETAILS
  }

  public PlacePageView(Context context)
  {
    this(context, null, 0);
  }

  public PlacePageView(Context context, AttributeSet attrs)
  {
    this(context, attrs, 0);
  }

  public PlacePageView(Context context, AttributeSet attrs, int defStyleAttr)
  {
    super(context, attrs);

    mIsLatLonDms = MwmApplication.prefs().getBoolean(PREF_USE_DMS, false);

    init(attrs, defStyleAttr);
  }

  private void initViews()
  {
    LayoutInflater.from(getContext()).inflate(R.layout.place_page, this);

    mPreview = (ViewGroup) findViewById(R.id.pp__preview);
    mTvTitle = (TextView) mPreview.findViewById(R.id.tv__title);
    mToolbar = (Toolbar) findViewById(R.id.toolbar);
    mTvSubtitle = (TextView) mPreview.findViewById(R.id.tv__subtitle);

    View directionFrame = mPreview.findViewById(R.id.direction_frame);
    mTvDistance = (TextView) mPreview.findViewById(R.id.tv__straight_distance);
    mAvDirection = (ArrowView) mPreview.findViewById(R.id.av__direction);
    directionFrame.setOnClickListener(this);

    mTvAddress = (TextView) mPreview.findViewById(R.id.tv__address);

    mHotelInfo = mPreview.findViewById(R.id.hotel_info_frame);
    mTvHotelRating = (TextView) mHotelInfo.findViewById(R.id.tv__hotel_rating);
    mTvHotelPrice = (TextView) mHotelInfo.findViewById(R.id.tv__hotel_price);

    mDetails = (ScrollView) findViewById(R.id.pp__details);
    RelativeLayout address = (RelativeLayout) mDetails.findViewById(R.id.ll__place_name);
    mPhone = mDetails.findViewById(R.id.ll__place_phone);
    mPhone.setOnClickListener(this);
    mTvPhone = (TextView) mDetails.findViewById(R.id.tv__place_phone);
    mWebsite = mDetails.findViewById(R.id.ll__place_website);
    mWebsite.setOnClickListener(this);
    mTvWebsite = (TextView) mDetails.findViewById(R.id.tv__place_website);
    LinearLayout latlon = (LinearLayout) mDetails.findViewById(R.id.ll__place_latlon);
    latlon.setOnClickListener(this);
    mTvLatlon = (TextView) mDetails.findViewById(R.id.tv__place_latlon);
    mOpeningHours = mDetails.findViewById(R.id.ll__place_schedule);
    mFullOpeningHours = (TextView) mDetails.findViewById(R.id.opening_hours);
    mTodayOpeningHours = (TextView) mDetails.findViewById(R.id.today_opening_hours);
    mWifi = mDetails.findViewById(R.id.ll__place_wifi);
    mEmail = mDetails.findViewById(R.id.ll__place_email);
    mEmail.setOnClickListener(this);
    mTvEmail = (TextView) mEmail.findViewById(R.id.tv__place_email);
    mOperator = mDetails.findViewById(R.id.ll__place_operator);
    mOperator.setOnClickListener(this);
    mTvOperator = (TextView) mOperator.findViewById(R.id.tv__place_operator);
    mCuisine = mDetails.findViewById(R.id.ll__place_cuisine);
    mTvCuisine = (TextView) mCuisine.findViewById(R.id.tv__place_cuisine);
    mWiki = mDetails.findViewById(R.id.ll__place_wiki);
    mWiki.setOnClickListener(this);
    mEntrance = mDetails.findViewById(R.id.ll__place_entrance);
    mTvEntrance = (TextView) mEntrance.findViewById(R.id.tv__place_entrance);
    mEditPlace = mDetails.findViewById(R.id.ll__place_editor);
    mEditPlace.setOnClickListener(this);
    mAddOrganisation = mDetails.findViewById(R.id.ll__add_organisation);
    mAddOrganisation.setOnClickListener(this);
    mAddPlace = mDetails.findViewById(R.id.ll__place_add);
    mAddPlace.setOnClickListener(this);
    mMoreInfo = mDetails.findViewById(R.id.ll__more);
    mMoreInfo.setOnClickListener(this);
    latlon.setOnLongClickListener(this);
    address.setOnLongClickListener(this);
    mPhone.setOnLongClickListener(this);
    mWebsite.setOnLongClickListener(this);
    mOpeningHours.setOnLongClickListener(this);
    mEmail.setOnLongClickListener(this);
    mOperator.setOnLongClickListener(this);
    mWiki.setOnLongClickListener(this);

    mBookmarkFrame = mDetails.findViewById(R.id.bookmark_frame);
    mBookmarkNote = (TextView) mBookmarkFrame.findViewById(R.id.tv__bookmark_notes);
    mBookmarkFrame.findViewById(R.id.tv__bookmark_edit).setOnClickListener(this);

    ViewGroup ppButtons = (ViewGroup) findViewById(R.id.pp__buttons);

    mButtons = new PlacePageButtons(this, ppButtons, new PlacePageButtons.ItemListener()
    {
      @Override
      public void onPrepareVisibleView(PlacePageButtons.Item item, View frame, ImageView icon, TextView title)
      {
        int color;

        switch (item)
        {
        case BOOKING:
          frame.setBackgroundResource(R.drawable.button_booking);
          color = Color.WHITE;
          break;

        case BOOKMARK:
          mBookmarkButtonIcon = icon;
          updateButtons();
          color = ThemeUtils.getColor(getContext(), R.attr.iconTint);
          break;

        default:
          color = ThemeUtils.getColor(getContext(), R.attr.iconTint);
          icon.setColorFilter(color);
          break;
        }

        title.setTextColor(color);
      }

      @Override
      public void onItemClick(PlacePageButtons.Item item)
      {
        switch (item)
        {
        case BOOKMARK:
          Statistics.INSTANCE.trackEvent(Statistics.EventName.PP_BOOKMARK);
          AlohaHelper.logClick(AlohaHelper.PP_BOOKMARK);
          toggleIsBookmark();
          break;

        case SHARE:
          Statistics.INSTANCE.trackEvent(Statistics.EventName.PP_SHARE);
          AlohaHelper.logClick(AlohaHelper.PP_SHARE);
          ShareOption.ANY.shareMapObject(getActivity(), mMapObject);
          break;

        case BACK:
          if (ParsedMwmRequest.hasRequest())
          {
            ParsedMwmRequest request = ParsedMwmRequest.getCurrentRequest();
            if (ParsedMwmRequest.isPickPointMode())
              request.setPointData(mMapObject.getLat(), mMapObject.getLon(), mMapObject.getTitle(), "");

            request.sendResponseAndFinish(getActivity(), true);
          }
          else
            getActivity().finish();
          break;

        case ROUTE_FROM:
          if (RoutingController.get().setStartPoint(mMapObject))
            hide();
          break;

        case ROUTE_TO:
          if (RoutingController.get().isPlanning())
          {
            if (RoutingController.get().setEndPoint(mMapObject))
              hide();
          }
          else
          {
            getActivity().startLocationToPoint(Statistics.EventName.PP_ROUTE, AlohaHelper.PP_ROUTE, getMapObject());
          }
          break;

        case BOOKING:
          onBookingClick(true /* book */);
          break;
        }
      }
    });

    mDownloaderIcon = new DownloaderStatusIcon(mPreview.findViewById(R.id.downloader_status_frame))
                          .setOnIconClickListener(new OnClickListener()
                          {
                            @Override
                            public void onClick(View v)
                            {
                              MapManager.warn3gAndDownload(getActivity(), mCurrentCountry.id, new Runnable()
                              {
                                @Override
                                public void run()
                                {
                                  Statistics.INSTANCE.trackEvent(Statistics.EventName.DOWNLOADER_ACTION,
                                                                 Statistics.params()
                                                                           .add(Statistics.EventParam.ACTION, "download")
                                                                           .add(Statistics.EventParam.FROM, "placepage")
                                                                           .add("is_auto", "false")
                                                                           .add("scenario", (mCurrentCountry.isExpandable() ? "download_group"
                                                                                                                            : "download")));
                                }
                              });
                            }
                          }).setOnCancelClickListener(new OnClickListener()
                          {
                            @Override
                            public void onClick(View v)
                            {
                              MapManager.nativeCancel(mCurrentCountry.id);
                              Statistics.INSTANCE.trackEvent(Statistics.EventName.DOWNLOADER_CANCEL,
                                                             Statistics.params().add(Statistics.EventParam.FROM, "placepage"));
                            }
                          });

    mDownloaderInfo = (TextView) mPreview.findViewById(R.id.tv__downloader_details);

    mShadowController = new ScrollViewShadowController((ObservableScrollView) mDetails)
                            .addBottomShadow()
                            .attach();

    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP)
      setElevation(UiUtils.dimen(R.dimen.placepage_elevation));

    if (getResources().getConfiguration().orientation == Configuration.ORIENTATION_LANDSCAPE)
      mDetails.setBackgroundResource(0);

    SponsoredHotel.setListener(this);
  }

  @Override
  public void onPriceReceived(String id, String price, String currencyCode)
  {
    if (mSponsoredHotel == null || !TextUtils.equals(id, mSponsoredHotel.getId()))
      return;

    String text;
    try
    {
      float value = Float.valueOf(price);
      text = NumberFormat.getCurrencyInstance().format(value);
    } catch (NumberFormatException e)
    {
      text = (price + " " + currencyCode);
    }

    mSponsoredHotelPrice = getContext().getString(R.string.place_page_starting_from, text);
    refreshPreview();
  }

  private void onBookingClick(final boolean book)
  {
    // TODO (trashkalmar): Set correct text
    Utils.checkConnection(getActivity(), R.string.common_check_internet_connection_dialog, new Utils.Proc<Boolean>()
    {
      @Override
      public void invoke(Boolean result)
      {
        if (!result)
          return;

        SponsoredHotel info = mSponsoredHotel;
        if (info == null)
          return;

        Map<String, String> params = new HashMap<>();
        params.put("provider", "Booking.Com");
        params.put("hotel_lat", (mMapObject == null ? "N/A" : String.valueOf(mMapObject.getLat())));
        params.put("hotel_lon", (mMapObject == null ? "N/A" : String.valueOf(mMapObject.getLon())));
        params.put("hotel", info.getId());

        String event = (book ? Statistics.EventName.PP_SPONSORED_BOOK
                             : Statistics.EventName.PP_SPONSORED_DETAILS);

        final Location location = LocationHelper.INSTANCE.getLastKnownLocation();
        Statistics.INSTANCE.trackEvent(event, location, params);

        try
        {
          followUrl(book ? info.urlBook : info.urlDescription);
        } catch (ActivityNotFoundException e)
        {
          AlohaHelper.logException(e);
        }
      }
    });
  }

  private void init(AttributeSet attrs, int defStyleAttr)
  {
    initViews();

    if (isInEditMode())
      return;

    final TypedArray attrArray = getContext().obtainStyledAttributes(attrs, R.styleable.PlacePageView, defStyleAttr, 0);
    final int animationType = attrArray.getInt(R.styleable.PlacePageView_animationType, 0);
    mIsDocked = attrArray.getBoolean(R.styleable.PlacePageView_docked, false);
    mIsFloating = attrArray.getBoolean(R.styleable.PlacePageView_floating, false);
    attrArray.recycle();

    mAnimationController = animationType == 0 ? new BottomPlacePageAnimationController(this)
                                              : new LeftPlacePageAnimationController(this);
  }

  public void restore()
  {
//    if (mMapObject != null)
      // FIXME query map object again
  }

  @Override
  public boolean onTouchEvent(@NonNull MotionEvent event)
  {
    return mAnimationController.onTouchEvent(event);
  }

  @Override
  public boolean onInterceptTouchEvent(MotionEvent event)
  {
    return mAnimationController.onInterceptTouchEvent(event);
  }

  public boolean isDocked()
  {
    return mIsDocked;
  }

  public boolean isFloating()
  {
    return mIsFloating;
  }

  public State getState()
  {
    return mAnimationController.getState();
  }

  public void setState(State state)
  {
    mDetails.scrollTo(0, 0);

    if (mMapObject != null)
      mAnimationController.setState(state, mMapObject.getMapObjectType());

    if (!mIsDocked && !mIsFloating)
    {
      // After ninepatch background is set from code, all paddings are lost, so we need to restore it later.
      int bottom = mPreview.getPaddingBottom();
      int left = mPreview.getPaddingLeft();
      int right = mPreview.getPaddingRight();
      int top = mPreview.getPaddingTop();
      mPreview.setBackgroundResource(ThemeUtils.getResource(getContext(), state == State.PREVIEW ? R.attr.ppPreviewHeadClosed
                                                                                                 : R.attr.ppPreviewHeadOpen));
      mPreview.setPadding(left, top, right, bottom);
    }
  }

  public MapObject getMapObject()
  {
    return mMapObject;
  }

  /**
   * @param mapObject new MapObject
   * @param force if true, new object'll be set without comparison with the old one
   */
  public void setMapObject(MapObject mapObject, boolean force)
  {
    if (!force && MapObject.same(mMapObject, mapObject))
      return;

    mMapObject = mapObject;
    mSponsoredHotel = (mMapObject == null ? null : SponsoredHotel.nativeGetCurrent());

    detachCountry();
    if (mMapObject != null)
    {
      if (mSponsoredHotel != null)
      {
        mSponsoredHotel.updateId(mMapObject);
        mSponsoredHotelPrice = mSponsoredHotel.price;

        Currency currency = Currency.getInstance(Locale.getDefault());
        SponsoredHotel.requestPrice(mSponsoredHotel.getId(), currency.getCurrencyCode());
      }

      String country = MapManager.nativeGetSelectedCountry();
      if (country != null)
        attachCountry(country);
    }

    refreshViews();
  }

  public void refreshViews()
  {
    if (mMapObject == null)
      return;

    refreshPreview();
    refreshDetails();
    final Location loc = LocationHelper.INSTANCE.getSavedLocation();

    switch (mMapObject.getMapObjectType())
    {
    case MapObject.BOOKMARK:
      refreshDistanceToObject(loc);
      showBookmarkDetails();
      setButtons(false, true);
      break;
    case MapObject.POI:
    case MapObject.SEARCH:
      refreshDistanceToObject(loc);
      hideBookmarkDetails();
      setButtons(false, true);
      break;
    case MapObject.API_POINT:
      refreshDistanceToObject(loc);
      hideBookmarkDetails();
      setButtons(true, true);
      break;
    case MapObject.MY_POSITION:
      refreshMyPosition(loc);
      hideBookmarkDetails();
      setButtons(false, false);
      break;
    }

    UiThread.runLater(new Runnable()
    {
      @Override
      public void run()
      {
        mShadowController.updateShadows();
        mPreview.requestLayout();
      }
    });
  }

  private void colorizeSubtitle()
  {
    String text = mTvSubtitle.getText().toString();
    if (TextUtils.isEmpty(text))
      return;

    int start = text.indexOf("★");
    if (start > -1)
    {
      SpannableStringBuilder sb = new SpannableStringBuilder(text);
      sb.setSpan(new ForegroundColorSpan(getResources().getColor(R.color.base_yellow)),
                 start, sb.length(), Spanned.SPAN_INCLUSIVE_EXCLUSIVE);

      mTvSubtitle.setText(sb);
    }
  }

  private void refreshPreview()
  {
    UiUtils.setTextAndHideIfEmpty(mTvTitle, mMapObject.getTitle());
    if (mToolbar != null)
      mToolbar.setTitle(mMapObject.getTitle());
    UiUtils.setTextAndHideIfEmpty(mTvSubtitle, mMapObject.getSubtitle());
    colorizeSubtitle();
    UiUtils.hide(mAvDirection);
    UiUtils.setTextAndHideIfEmpty(mTvAddress, mMapObject.getAddress());

    boolean sponsored = (mSponsoredHotel != null);
    UiUtils.showIf(sponsored, mHotelInfo);
    if (sponsored)
    {
      mTvHotelRating.setText(mSponsoredHotel.rating);
      UiUtils.setTextAndHideIfEmpty(mTvHotelPrice, mSponsoredHotelPrice);
    }
  }

  private void refreshDetails()
  {
    refreshLatLon();

    if (mSponsoredHotel == null)
    {
      final String website = mMapObject.getMetadata(Metadata.MetadataType.FMD_WEBSITE);
      refreshMetadataOrHide(TextUtils.isEmpty(website) ? mMapObject.getMetadata(Metadata.MetadataType.FMD_URL) : website, mWebsite, mTvWebsite);
    }
    else
      UiUtils.hide(mWebsite);

    refreshMetadataOrHide(mMapObject.getMetadata(Metadata.MetadataType.FMD_PHONE_NUMBER), mPhone, mTvPhone);
    refreshMetadataOrHide(mMapObject.getMetadata(Metadata.MetadataType.FMD_EMAIL), mEmail, mTvEmail);
    refreshMetadataOrHide(mMapObject.getMetadata(Metadata.MetadataType.FMD_OPERATOR), mOperator, mTvOperator);
    refreshMetadataOrHide(Framework.nativeGetActiveObjectFormattedCuisine(), mCuisine, mTvCuisine);
    refreshMetadataOrHide(mMapObject.getMetadata(Metadata.MetadataType.FMD_WIKIPEDIA), mWiki, null);
    refreshMetadataOrHide(mMapObject.getMetadata(Metadata.MetadataType.FMD_INTERNET), mWifi, null);
    refreshMetadataOrHide(mMapObject.getMetadata(Metadata.MetadataType.FMD_FLATS), mEntrance, mTvEntrance);
    refreshOpeningHours();

    if (RoutingController.get().isNavigating() || MapManager.nativeIsLegacyMode())
    {
      UiUtils.hide(mEditPlace, mAddOrganisation, mAddPlace);
    }
    else
    {
      UiUtils.showIf(Editor.nativeShouldShowEditPlace(), mEditPlace);
      UiUtils.showIf(Editor.nativeShouldShowAddBusiness(), mAddOrganisation);
      UiUtils.showIf(Editor.nativeShouldShowAddPlace(), mAddPlace);
    }

    UiUtils.showIf(mSponsoredHotel != null, mMoreInfo);
  }

  private void refreshOpeningHours()
  {
    final Timetable[] timetables = OpeningHours.nativeTimetablesFromString(mMapObject.getMetadata(Metadata.MetadataType.FMD_OPEN_HOURS));
    if (timetables == null || timetables.length == 0)
    {
      UiUtils.hide(mOpeningHours);
      return;
    }

    UiUtils.show(mOpeningHours);

    final Resources resources = getResources();
    if (timetables[0].isFullWeek())
    {
      refreshTodayOpeningHours((timetables[0].isFullday ? resources.getString(R.string.twentyfour_seven)
                                                        : resources.getString(R.string.daily) + " " + timetables[0].workingTimespan),
                               ThemeUtils.getColor(getContext(), android.R.attr.textColorPrimary));
      UiUtils.hide(mFullOpeningHours);
      return;
    }

    boolean containsCurrentWeekday = false;
    final int currentDay = Calendar.getInstance().get(Calendar.DAY_OF_WEEK);
    for (Timetable tt : timetables)
    {
      if (tt.containsWeekday(currentDay))
      {
        containsCurrentWeekday = true;
        refreshTodayOpeningHours(resources.getString(R.string.today) + " " + tt.workingTimespan,
                                 ThemeUtils.getColor(getContext(), android.R.attr.textColorPrimary));
        break;
      }
    }

    UiUtils.setTextAndShow(mFullOpeningHours, TimeFormatUtils.formatTimetables(timetables));
    if (!containsCurrentWeekday)
      refreshTodayOpeningHours(resources.getString(R.string.day_off_today), resources.getColor(R.color.base_red));
  }

  private void refreshTodayOpeningHours(String text, @ColorInt int color)
  {
    UiUtils.setTextAndShow(mTodayOpeningHours, text);
    mTodayOpeningHours.setTextColor(color);
  }

  private void updateButtons()
  {
    if (mBookmarkButtonIcon == null)
      return;

    if (mBookmarkSet)
      mBookmarkButtonIcon.setImageResource(R.drawable.ic_bookmarks_on);
    else
      mBookmarkButtonIcon.setImageDrawable(Graphics.tint(getContext(), R.drawable.ic_bookmarks_off, R.attr.iconTint));
  }

  private void hideBookmarkDetails()
  {
    mBookmarkSet = false;
    UiUtils.hide(mBookmarkFrame);
    updateButtons();
  }

  private void showBookmarkDetails()
  {
    mBookmarkSet = true;
    UiUtils.show(mBookmarkFrame);
    UiUtils.setTextAndHideIfEmpty(mBookmarkNote, ((Bookmark) mMapObject).getBookmarkDescription());
    updateButtons();
  }

  private void setButtons(boolean showBackButton, boolean showRoutingButton)
  {
    List<PlacePageButtons.Item> buttons = new ArrayList<>();

    if (showBackButton || ParsedMwmRequest.isPickPointMode())
      buttons.add(PlacePageButtons.Item.BACK);

    if (mSponsoredHotel != null)
      buttons.add(PlacePageButtons.Item.BOOKING);

    buttons.add(PlacePageButtons.Item.BOOKMARK);

    if (RoutingController.get().isPlanning())
    {
      buttons.add(PlacePageButtons.Item.ROUTE_FROM);
      buttons.add(PlacePageButtons.Item.ROUTE_TO);
    }
    else
    {
      if (showRoutingButton)
        buttons.add(PlacePageButtons.Item.ROUTE_TO);
    }

    buttons.add(PlacePageButtons.Item.SHARE);

    mButtons.setItems(buttons);
  }

  public void refreshLocation(Location l)
  {
    if (mMapObject == null)
      return;

    if (MapObject.isOfType(MapObject.MY_POSITION, mMapObject))
      refreshMyPosition(l);
    else
      refreshDistanceToObject(l);
  }

  private void refreshMyPosition(Location l)
  {
    UiUtils.hide(mTvDistance);

    if (l == null)
      return;

    final StringBuilder builder = new StringBuilder();
    if (l.hasAltitude())
      builder.append(Framework.nativeFormatAltitude(l.getAltitude()));
    if (l.hasSpeed())
      builder.append("   ")
             .append(Framework.nativeFormatSpeed(l.getSpeed()));
    mTvSubtitle.setText(builder.toString());

    mMapObject.setLat(l.getLatitude());
    mMapObject.setLon(l.getLongitude());
    refreshLatLon();
  }

  private void refreshDistanceToObject(Location l)
  {
    UiUtils.showIf(l != null, mTvDistance);
    if (l == null)
      return;

    mTvDistance.setVisibility(View.VISIBLE);
    DistanceAndAzimut distanceAndAzimuth = Framework.nativeGetDistanceAndAzimuthFromLatLon(mMapObject.getLat(), mMapObject.getLon(),
                                                                                           l.getLatitude(), l.getLongitude(), 0.0);
    mTvDistance.setText(distanceAndAzimuth.getDistance());
  }

  private void refreshLatLon()
  {
    final double lat = mMapObject.getLat();
    final double lon = mMapObject.getLon();
    final String[] latLon = Framework.nativeFormatLatLonToArr(lat, lon, mIsLatLonDms);
    if (latLon.length == 2)
      mTvLatlon.setText(String.format(Locale.US, "%1$s, %2$s", latLon[0], latLon[1]));
  }

  private static void refreshMetadataOrHide(String metadata, View metaLayout, TextView metaTv)
  {
    if (!TextUtils.isEmpty(metadata))
    {
      metaLayout.setVisibility(View.VISIBLE);
      if (metaTv != null)
        metaTv.setText(metadata);
    }
    else
      metaLayout.setVisibility(View.GONE);
  }

  public void refreshAzimuth(double northAzimuth)
  {
    if (isHidden() ||
            mMapObject == null ||
            MapObject.isOfType(MapObject.MY_POSITION, mMapObject))
      return;

    final Location location = LocationHelper.INSTANCE.getSavedLocation();
    if (location == null)
      return;

    final double azimuth = Framework.nativeGetDistanceAndAzimuthFromLatLon(mMapObject.getLat(), mMapObject.getLon(),
                                                                           location.getLatitude(), location.getLongitude(),
                                                                           northAzimuth)
                                    .getAzimuth();
    if (azimuth >= 0)
    {
      UiUtils.show(mAvDirection);
      mAvDirection.setAzimuth(azimuth);
    }
  }

  public void setOnVisibilityChangedListener(BasePlacePageAnimationController.OnVisibilityChangedListener listener)
  {
    mAnimationController.setOnVisibilityChangedListener(listener);
  }

  private void addOrganisation()
  {
    Statistics.INSTANCE.trackEvent(Statistics.EventName.EDITOR_ADD_CLICK,
                                   Statistics.params().add(Statistics.EventParam.FROM, "placepage"));
    getActivity().showPositionChooser(true, false);
  }

  private void addPlace()
  {
    // TODO add statistics
    getActivity().showPositionChooser(false, true);
  }

  @Override
  public void onClick(View v)
  {
    switch (v.getId())
    {
    case R.id.ll__place_editor:
      getActivity().showEditor();
      break;
    case R.id.ll__add_organisation:
      addOrganisation();
      break;
    case R.id.ll__place_add:
      addPlace();
      break;
    case R.id.ll__more:
      onBookingClick(false /* book */);
      break;
    case R.id.ll__place_latlon:
      mIsLatLonDms = !mIsLatLonDms;
      MwmApplication.prefs().edit().putBoolean(PREF_USE_DMS, mIsLatLonDms).commit();
      refreshLatLon();
      break;
    case R.id.ll__place_phone:
      Intent intent = new Intent(Intent.ACTION_DIAL);
      intent.setData(Uri.parse("tel:" + mTvPhone.getText()));
      try
      {
        getContext().startActivity(intent);
      } catch (ActivityNotFoundException e)
      {
        AlohaHelper.logException(e);
      }
      break;
    case R.id.ll__place_website:
      followUrl(mTvWebsite.getText().toString());
      break;
    case R.id.ll__place_wiki:
      // TODO: Refactor and use separate getters for Wiki and all other PP meta info too.
      followUrl(mMapObject.getMetadata(Metadata.MetadataType.FMD_WIKIPEDIA));
      break;
    case R.id.direction_frame:
      Statistics.INSTANCE.trackEvent(Statistics.EventName.PP_DIRECTION_ARROW);
      AlohaHelper.logClick(AlohaHelper.PP_DIRECTION_ARROW);
      showBigDirection();
      break;
    case R.id.ll__place_email:
      intent = new Intent(Intent.ACTION_SENDTO);
      intent.setData(Utils.buildMailUri(mTvEmail.getText().toString(), "", ""));
      getContext().startActivity(intent);
      break;
    case R.id.tv__bookmark_edit:
      Bookmark bookmark = (Bookmark) mMapObject;
      EditBookmarkFragment.editBookmark(bookmark.getCategoryId(), bookmark.getBookmarkId(),
                                        getActivity(), getActivity().getSupportFragmentManager());
      break;
    }
  }

  private void followUrl(String url)
  {
    final Intent intent = new Intent(Intent.ACTION_VIEW);
    if (!url.startsWith("http://") && !url.startsWith("https://"))
      url = "http://" + url;
    intent.setData(Uri.parse(url));
    getContext().startActivity(intent);
  }

  private void toggleIsBookmark()
  {
    if (MapObject.isOfType(MapObject.BOOKMARK, mMapObject))
      setMapObject(Framework.nativeDeleteBookmarkFromMapObject(), true);
    else
      setMapObject(BookmarkManager.INSTANCE.addNewBookmark(BookmarkManager.nativeFormatNewBookmarkName(),
                                                           mMapObject.getLat(), mMapObject.getLon()), true);
    post(new Runnable()
    {
      @Override
      public void run()
      {
        setState(State.DETAILS);
      }
    });
  }

  private void showBigDirection()
  {
    final DirectionFragment fragment = (DirectionFragment) Fragment.instantiate(getActivity(), DirectionFragment.class.getName(), null);
    fragment.setMapObject(mMapObject);
    fragment.show(getActivity().getSupportFragmentManager(), null);
  }

  @Override
  public boolean onLongClick(View v)
  {
    final Object tag = v.getTag();
    final String tagStr = tag == null ? "" : tag.toString();
    AlohaHelper.logLongClick(tagStr);

    final PopupMenu popup = new PopupMenu(getContext(), v);
    final Menu menu = popup.getMenu();
    final List<String> items = new ArrayList<>();
    switch (v.getId())
    {
    case R.id.ll__place_latlon:
      final double lat = mMapObject.getLat();
      final double lon = mMapObject.getLon();
      items.add(Framework.nativeFormatLatLon(lat, lon, false));
      items.add(Framework.nativeFormatLatLon(lat, lon, true));
      break;
    case R.id.ll__place_website:
      items.add(mTvWebsite.getText().toString());
      break;
    case R.id.ll__place_email:
      items.add(mTvEmail.getText().toString());
      break;
    case R.id.ll__place_phone:
      items.add(mTvPhone.getText().toString());
      break;
    case R.id.ll__place_schedule:
      items.add(mFullOpeningHours.getText().toString());
      break;
    case R.id.ll__place_operator:
      items.add(mTvOperator.getText().toString());
      break;
    case R.id.ll__place_wiki:
      items.add(mMapObject.getMetadata(Metadata.MetadataType.FMD_WIKIPEDIA));
      break;
    }

    final String copyText = getResources().getString(android.R.string.copy);
    for (int i = 0; i < items.size(); i++)
      menu.add(Menu.NONE, i, i, String.format("%s %s", copyText, items.get(i)));

    popup.setOnMenuItemClickListener(new PopupMenu.OnMenuItemClickListener()
    {
      @Override
      public boolean onMenuItemClick(MenuItem item)
      {
        final int id = item.getItemId();
        final Context ctx = getContext();
        Utils.copyTextToClipboard(ctx, items.get(id));
        Utils.toastShortcut(ctx, ctx.getString(R.string.copied_to_clipboard, items.get(id)));
        Statistics.INSTANCE.trackEvent(Statistics.EventName.PP_METADATA_COPY + ":" + tagStr);
        AlohaHelper.logClick(AlohaHelper.PP_METADATA_COPY + ":" + tagStr);
        return true;
      }
    });

    popup.show();
    return true;
  }

  int getDockedWidth()
  {
    int res = getWidth();
    return (res == 0 ? getLayoutParams().width : res);
  }

  MwmActivity.LeftAnimationTrackListener getLeftAnimationTrackListener()
  {
    return mLeftAnimationTrackListener;
  }

  public void setLeftAnimationTrackListener(MwmActivity.LeftAnimationTrackListener listener)
  {
    mLeftAnimationTrackListener = listener;
  }

  public void hide()
  {
    detachCountry();
    setState(State.HIDDEN);
  }

  public boolean isHidden()
  {
    return (getState() == State.HIDDEN);
  }

  @SuppressWarnings("SimplifiableIfStatement")
  public boolean hideOnTouch()
  {
    if (mIsDocked || mIsFloating)
      return false;

    if (getState() == State.DETAILS)
    {
      hide();
      return true;
    }

    return false;
  }

  private static boolean isInvalidDownloaderStatus(int status)
  {
    return (status != CountryItem.STATUS_DOWNLOADABLE &&
            status != CountryItem.STATUS_ENQUEUED &&
            status != CountryItem.STATUS_FAILED &&
            status != CountryItem.STATUS_PARTLY &&
            status != CountryItem.STATUS_PROGRESS);
  }

  private void updateDownloader(CountryItem country)
  {
    if (isInvalidDownloaderStatus(country.status))
    {
      if (mStorageCallbackSlot != 0)
        UiThread.runLater(mDownloaderDeferredDetachProc);
      return;
    }

    mDownloaderIcon.update(country);

    StringBuilder sb = new StringBuilder(StringUtils.getFileSizeString(country.totalSize));
    if (country.isExpandable())
      sb.append(String.format(Locale.US, "  •  %s: %d", getContext().getString(R.string.downloader_status_maps), country.totalChildCount));

    mDownloaderInfo.setText(sb.toString());
  }

  private void updateDownloader()
  {
    if (mCurrentCountry == null)
      return;

    mCurrentCountry.update();
    updateDownloader(mCurrentCountry);
  }

  private void attachCountry(String country)
  {
    CountryItem map = CountryItem.fill(country);
    if (isInvalidDownloaderStatus(map.status))
      return;

    mCurrentCountry = map;
    if (mStorageCallbackSlot == 0)
      mStorageCallbackSlot = MapManager.nativeSubscribe(mStorageCallback);

    mDownloaderIcon.show(true);
    UiUtils.show(mDownloaderInfo);
    updateDownloader(mCurrentCountry);
  }

  private void detachCountry()
  {
    if (mStorageCallbackSlot == 0)
      return;

    MapManager.nativeUnsubscribe(mStorageCallbackSlot);
    mStorageCallbackSlot = 0;
    mCurrentCountry = null;
    mDownloaderIcon.show(false);
    UiUtils.hide(mDownloaderInfo);
  }

  MwmActivity getActivity()
  {
    return (MwmActivity) getContext();
  }
}
