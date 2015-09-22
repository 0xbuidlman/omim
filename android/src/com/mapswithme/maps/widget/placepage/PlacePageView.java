package com.mapswithme.maps.widget.placepage;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.res.TypedArray;
import android.location.Location;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentActivity;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.PopupMenu;
import android.widget.RatingBar;
import android.widget.RelativeLayout;
import android.widget.TextView;

import com.mapswithme.maps.BuildConfig;
import com.mapswithme.maps.Framework;
import com.mapswithme.maps.R;
import com.mapswithme.maps.api.ParsedMmwRequest;
import com.mapswithme.maps.bookmarks.BookmarkActivity;
import com.mapswithme.maps.bookmarks.ChooseBookmarkCategoryActivity;
import com.mapswithme.maps.bookmarks.data.Bookmark;
import com.mapswithme.maps.bookmarks.data.BookmarkManager;
import com.mapswithme.maps.bookmarks.data.DistanceAndAzimut;
import com.mapswithme.maps.bookmarks.data.Icon;
import com.mapswithme.maps.bookmarks.data.MapObject;
import com.mapswithme.maps.bookmarks.data.MapObject.MapObjectType;
import com.mapswithme.maps.bookmarks.data.MapObject.Poi;
import com.mapswithme.maps.bookmarks.data.Metadata;
import com.mapswithme.maps.bookmarks.data.ParcelablePoint;
import com.mapswithme.maps.location.LocationHelper;
import com.mapswithme.maps.widget.ArrowView;
import com.mapswithme.util.InputUtils;
import com.mapswithme.util.LocationUtils;
import com.mapswithme.util.ShareAction;
import com.mapswithme.util.Utils;
import com.mapswithme.util.statistics.Statistics;


public class PlacePageView extends RelativeLayout implements View.OnClickListener, View.OnLongClickListener, TextView.OnEditorActionListener
{
  // Preview
  private TextView mTvTitle;
  private TextView mTvSubtitle;
  private TextView mTvOpened;
  private ArrowView mAvDirection;
  private TextView mTvDistance;
  private RatingBar mRbStars;
  private TextView mTvElevation;
  // Place page details
  private ViewGroup mPpDetails;
  private LinearLayout mLlAddress;
  private TextView mTvAddress;
  private LinearLayout mLlPhone;
  private TextView mTvPhone;
  private LinearLayout mLlWebsite;
  private TextView mTvWebsite;
  private LinearLayout mLlLatlon;
  private TextView mTvLatlon;
  private LinearLayout mLlSchedule;
  private TextView mTvSchedule;
  private LinearLayout mLlWifi;
  private LinearLayout mLlEmail;
  private TextView mTvEmail;
  private LinearLayout mLlOperator;
  private TextView mTvOperator;
  // Bookmark
  private ImageView mIvColor;
  private EditText mEtBookmarkName;
  private TextView mTvBookmarkNotes;
  private EditText mEtBookmarkNotes;
  private TextView mTvBookmarkGroup;
  // Place page buttons
  private LinearLayout mLlApiBack;
  private ImageView mIvBookmark;
  // Animations
  private BasePlacePageAnimationController mAnimationController;
  // Data
  private MapObject mMapObject;

  private MapObject mBookmarkedMapObject;
  private boolean mIsLatLonDms;
  private static final String PREF_USE_DMS = "use_dms";

  public enum State
  {
    HIDDEN,
    PREVIEW,
    BOOKMARK,
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

    mIsLatLonDms = context.getSharedPreferences(context.getString(R.string.pref_file_name),
        Context.MODE_PRIVATE).getBoolean(PREF_USE_DMS, false);

    initViews();

    initAnimationController(attrs, defStyleAttr);
    setVisibility(View.INVISIBLE);
  }

  private void initViews()
  {
    LayoutInflater.from(getContext()).inflate(R.layout.place_page, this);

    ViewGroup ppPreview = (ViewGroup) findViewById(R.id.pp__preview);
    mTvTitle = (TextView) ppPreview.findViewById(R.id.tv__title);
    mTvSubtitle = (TextView) ppPreview.findViewById(R.id.tv__subtitle);
    mTvOpened = (TextView) ppPreview.findViewById(R.id.tv__opened_till);
    mTvDistance = (TextView) ppPreview.findViewById(R.id.tv__straight_distance);
    mAvDirection = (ArrowView) ppPreview.findViewById(R.id.av__direction);
    mAvDirection.setOnClickListener(this);
    mAvDirection.setImageResource(R.drawable.selector_direction);
    mRbStars = (RatingBar) ppPreview.findViewById(R.id.rb__stars);
    mTvElevation = (TextView) ppPreview.findViewById(R.id.tv__peak_elevation);

    mPpDetails = (ViewGroup) findViewById(R.id.pp__details);
    mLlAddress = (LinearLayout) mPpDetails.findViewById(R.id.ll__place_name);
    mTvAddress = (TextView) mPpDetails.findViewById(R.id.tv__place_address);
    mLlPhone = (LinearLayout) mPpDetails.findViewById(R.id.ll__place_phone);
    mLlPhone.setOnClickListener(this);
    mTvPhone = (TextView) mPpDetails.findViewById(R.id.tv__place_phone);
    mLlWebsite = (LinearLayout) mPpDetails.findViewById(R.id.ll__place_website);
    mLlWebsite.setOnClickListener(this);
    mTvWebsite = (TextView) mPpDetails.findViewById(R.id.tv__place_website);
    mLlLatlon = (LinearLayout) mPpDetails.findViewById(R.id.ll__place_latlon);
    mTvLatlon = (TextView) mPpDetails.findViewById(R.id.tv__place_latlon);
    mLlLatlon.setOnClickListener(this);
    mLlSchedule = (LinearLayout) mPpDetails.findViewById(R.id.ll__place_schedule);
    mTvSchedule = (TextView) mPpDetails.findViewById(R.id.tv__place_schedule);
    mLlWifi = (LinearLayout) mPpDetails.findViewById(R.id.ll__place_wifi);
    mIvColor = (ImageView) mPpDetails.findViewById(R.id.iv__bookmark_color);
    mIvColor.setOnClickListener(this);
    mLlEmail = (LinearLayout) mPpDetails.findViewById(R.id.ll__place_email);
    mLlEmail.setOnClickListener(this);
    mTvEmail = (TextView) mLlEmail.findViewById(R.id.tv__place_email);
    mLlOperator = (LinearLayout) mPpDetails.findViewById(R.id.ll__place_operator);
    mLlOperator.setOnClickListener(this);
    mTvOperator = (TextView) mPpDetails.findViewById(R.id.tv__place_operator);
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB)
    {
      mLlLatlon.setOnLongClickListener(this);
      mLlAddress.setOnLongClickListener(this);
      mLlPhone.setOnLongClickListener(this);
      mLlWebsite.setOnLongClickListener(this);
      mLlSchedule.setOnLongClickListener(this);
      mLlEmail.setOnLongClickListener(this);
      mLlOperator.setOnLongClickListener(this);
    }

    mEtBookmarkName = (EditText) mPpDetails.findViewById(R.id.et__bookmark_name);
    mEtBookmarkName.setOnEditorActionListener(this);
    mTvBookmarkNotes = (TextView) mPpDetails.findViewById(R.id.tv__bookmark_notes_title);
    mEtBookmarkNotes = (EditText) mPpDetails.findViewById(R.id.et__bookmark_notes);
    mEtBookmarkNotes.setOnEditorActionListener(this);
    mTvBookmarkGroup = (TextView) mPpDetails.findViewById(R.id.tv__bookmark_group);
    mTvBookmarkGroup.setOnClickListener(this);

    ViewGroup ppButtons = (ViewGroup) findViewById(R.id.pp__buttons);
    mLlApiBack = (LinearLayout) ppButtons.findViewById(R.id.rl__api_back);
    mLlApiBack.setOnClickListener(this);
    final ViewGroup bookmarkGroup = (ViewGroup) ppButtons.findViewById(R.id.rl__bookmark);
    bookmarkGroup.setOnClickListener(this);
    mIvBookmark = (ImageView) bookmarkGroup.findViewById(R.id.iv__bookmark);
    ppButtons.findViewById(R.id.rl__share).setOnClickListener(this);
  }

  private void initAnimationController(AttributeSet attrs, int defStyleAttr)
  {
    final TypedArray attrArray = getContext().obtainStyledAttributes(attrs, R.styleable.PlacePageView, defStyleAttr, 0);
    final int animationType = attrArray.getInt(R.styleable.PlacePageView_animationType, 0);
    attrArray.recycle();
    // switch with values from "animationType" from attrs.xml
    switch (animationType)
    {
    case 0:
      mAnimationController = new BottomPlacePageAnimationController(this);
      break;
    case 2:
      mAnimationController = new LeftFloatPlacePageAnimationController(this);
      break;
    case 3:
      mAnimationController = new LeftFullPlacePageAnimationController(this);
      break;
    }
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

  public State getState()
  {
    return mAnimationController.getState();
  }

  public void setState(State state)
  {
    InputUtils.hideKeyboard(mEtBookmarkName);
    if (mMapObject != null)
      mAnimationController.setState(state, mMapObject.getType());
  }

  public MapObject getMapObject()
  {
    return mMapObject;
  }

  public void setMapObject(MapObject mo)
  {
    if (!hasMapObject(mo))
    {
      mMapObject = mo;
      refreshViews();
    }
  }

  public boolean hasMapObject(MapObject mo)
  {
    if (mo == null && mMapObject == null)
      return true;
    else if (mMapObject != null)
      return mMapObject.equals(mo);

    return false;
  }

  private void refreshViews()
  {
    if (mMapObject != null)
    {
      mMapObject.setDefaultIfEmpty(getResources());

      refreshPreview();
      refreshDetails();
      switch (mMapObject.getType())
      {
      case BOOKMARK:
        refreshDistanceToObject(LocationHelper.INSTANCE.getLastLocation());
        refreshBookmarkDetails(true);
        refreshButtons(false);
        break;
      case POI:
      case ADDITIONAL_LAYER:
        refreshDistanceToObject(LocationHelper.INSTANCE.getLastLocation());
        refreshBookmarkDetails(false);
        refreshButtons(false);
        break;
      case API_POINT:
        refreshDistanceToObject(LocationHelper.INSTANCE.getLastLocation());
        refreshBookmarkDetails(false);
        refreshButtons(true);
        break;
      case MY_POSITION:
        refreshMyPosition(LocationHelper.INSTANCE.getLastLocation());
        refreshBookmarkDetails(false);
        refreshButtons(false);
        break;
      }
    }
  }

  private void refreshBookmarkDetails(boolean isBookmark)
  {
    if (isBookmark)
    {
      final Bookmark bookmark = (Bookmark) mMapObject;
      mEtBookmarkName.setText(bookmark.getName());
      final String notes = bookmark.getBookmarkDescription();
      mEtBookmarkNotes.setText(notes);
      mTvBookmarkNotes.setVisibility(notes.isEmpty() ? View.INVISIBLE : View.VISIBLE);
      mTvBookmarkGroup.setText(bookmark.getCategoryName(getContext()));
      mIvColor.setImageResource(bookmark.getIcon().getSelectedResId());
      mIvBookmark.setImageResource(R.drawable.ic_bookmark_on);
    }
    else
      mIvBookmark.setImageResource(R.drawable.ic_bookmark_off);
  }

  private void refreshPreview()
  {
    mTvTitle.setText(mMapObject.getName());
    String subtitle = mMapObject.getPoiTypeName();
    final String cuisine = mMapObject.getMetadata(Metadata.MetadataType.FMD_CUISINE);
    if (cuisine != null)
      subtitle += ", " + translateCuisine(cuisine);
    mTvSubtitle.setText(subtitle);
    mAvDirection.setVisibility(View.GONE);
    // TODO show/hide mTvOpened after schedule fill be parsed
  }

  public String translateCuisine(String cuisine)
  {
    if (!TextUtils.isEmpty(cuisine))
    {
      // cuisines translations can contain unsupported symbols, and res ids
      // replace them with supported "_"( so ', ' and ' ' are replaced with underlines)
      final String[] cuisines = cuisine.split(";");
      String result = "";
      // search translations for each cuisine
      for (String cuisineRaw : cuisines)
      {
        final String cuisineKey = cuisineRaw.replace(", ", "_").replace(' ', '_').toLowerCase();
        int resId = getResources().getIdentifier("cuisine_" + cuisineKey, "string", BuildConfig.APPLICATION_ID);
        result += resId == 0 ? cuisineRaw : getResources().getString(resId);
      }
      return result;
    }

    return cuisine;
  }

  private void refreshDetails()
  {
    refreshLatLon();
    final String website = mMapObject.getMetadata(Metadata.MetadataType.FMD_WEBSITE);
    if (website != null)
      refreshMetadataOrHide(website, mLlWebsite, mTvWebsite);
    else
      refreshMetadataOrHide(mMapObject.getMetadata(Metadata.MetadataType.FMD_URL), mLlWebsite, mTvWebsite);
    refreshMetadataOrHide(mMapObject.getMetadata(Metadata.MetadataType.FMD_PHONE_NUMBER), mLlPhone, mTvPhone);
    refreshMetadataOrHide(mMapObject.getMetadata(Metadata.MetadataType.FMD_EMAIL), mLlEmail, mTvEmail);
    refreshMetadataOrHide(mMapObject.getMetadata(Metadata.MetadataType.FMD_OPERATOR), mLlOperator, mTvOperator);

    // TODO throw away parsing hack when data will be parsed correctly in core
    final String rawSchedule = mMapObject.getMetadata(Metadata.MetadataType.FMD_OPEN_HOURS);
    if (!TextUtils.isEmpty(rawSchedule))
      refreshMetadataOrHide(rawSchedule.replace("; ", "\n").replace(';', '\n'), mLlSchedule, mTvSchedule);
    else
      refreshMetadataOrHide(null, mLlSchedule, mTvSchedule);

    refreshMetadataStars(mMapObject.getMetadata(Metadata.MetadataType.FMD_STARS));

    final String elevation = mMapObject.getMetadata(Metadata.MetadataType.FMD_ELE);
    if (!TextUtils.isEmpty(elevation))
    {
      mTvElevation.setVisibility(View.VISIBLE);
      mTvElevation.setText(elevation);
    }
    else
      mTvElevation.setVisibility(View.GONE);
  }

  private void refreshButtons(boolean showBackButton)
  {
    if (showBackButton ||
        (ParsedMmwRequest.hasRequest() && ParsedMmwRequest.getCurrentRequest().isPickPointMode()))
      mLlApiBack.setVisibility(View.VISIBLE);
    else
      mLlApiBack.setVisibility(View.GONE);
  }

  public void refreshLocation(Location l)
  {
    if (mMapObject == null)
      return;

    if (mMapObject.getType() == MapObjectType.MY_POSITION)
      refreshMyPosition(l);
    else
      refreshDistanceToObject(l);
  }

  private void refreshMyPosition(Location myLocation)
  {
    final StringBuilder builder = new StringBuilder();
    if (myLocation.hasAltitude())
      builder.append(Framework.nativeFormatAltitude(myLocation.getAltitude()));
    if (myLocation.hasSpeed())
      builder.append("   ").
          append(Framework.nativeFormatSpeed(myLocation.getSpeed()));
    mTvSubtitle.setText(builder.toString());

    mTvDistance.setVisibility(View.GONE);

    mMapObject.setLat(myLocation.getLatitude());
    mMapObject.setLon(myLocation.getLongitude());
    refreshLatLon();
  }

  private void refreshDistanceToObject(Location myLocation)
  {
    if (myLocation != null)
    {
      mTvDistance.setVisibility(View.VISIBLE);
      final DistanceAndAzimut distanceAndAzimuth = Framework.nativeGetDistanceAndAzimutFromLatLon(mMapObject.getLat(),
          mMapObject.getLon(), myLocation.getLatitude(), myLocation.getLongitude(), 0.0);
      mTvDistance.setText(distanceAndAzimuth.getDistance());
    }
    else
      mTvDistance.setVisibility(View.GONE);
  }

  private void refreshLatLon()
  {
    final double lat = mMapObject.getLat();
    final double lon = mMapObject.getLon();
    final String[] latLon = Framework.nativeFormatLatLonToArr(lat, lon, mIsLatLonDms);
    if (latLon.length == 2)
      mTvLatlon.setText(latLon[0] + ", " + latLon[1]);
  }

  private void refreshMetadataOrHide(String metadata, LinearLayout metaLayout, TextView metaTv)
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

  private void refreshMetadataStars(String stars)
  {
    if (TextUtils.isEmpty(stars))
    {
      mRbStars.setVisibility(View.GONE);
      return;
    }

    try
    {
      mRbStars.setRating(Float.parseFloat(stars));
      mRbStars.setVisibility(View.VISIBLE);
    } catch (NumberFormatException e)
    {
      mRbStars.setVisibility(View.GONE);
    }
  }

  public void refreshAzimuth(double northAzimuth)
  {
    if (mMapObject != null && mMapObject.getType() != MapObjectType.MY_POSITION)
    {
      final Location l = LocationHelper.INSTANCE.getLastLocation();
      if (l != null)
      {
        final DistanceAndAzimut da = Framework.nativeGetDistanceAndAzimutFromLatLon(
            mMapObject.getLat(), mMapObject.getLon(),
            l.getLatitude(), l.getLongitude(), northAzimuth);

        if (da.getAthimuth() >= 0)
        {
          mAvDirection.setVisibility(View.VISIBLE);
          mAvDirection.setAzimut(da.getAthimuth());
        }
      }
    }
  }

  public void setOnVisibilityChangedListener(BasePlacePageAnimationController.OnVisibilityChangedListener listener)
  {
    mAnimationController.setOnVisibilityChangedListener(listener);
  }

  public void onResume()
  {
    if (mMapObject == null)
      return;

    checkBookmarkWasDeleted();
    checkApiWasCanceled();
  }

  private void checkApiWasCanceled()
  {
    if ((mMapObject.getType() == MapObjectType.API_POINT) && !ParsedMmwRequest.hasRequest())
    {
      setMapObject(null);

      // FIXME
      //      mAnimationController.hidePlacePage();
    }
  }

  private void checkBookmarkWasDeleted()
  {
    // We need to check, if content of body is still valid
    if (mMapObject.getType() == MapObjectType.BOOKMARK)
    {
      final Bookmark bmk = (Bookmark) mMapObject;
      boolean deleted = false;

      if (BookmarkManager.INSTANCE.getCategoriesCount() <= bmk.getCategoryId())
        deleted = true;
      else if (BookmarkManager.INSTANCE.getCategoryById(bmk.getCategoryId()).getBookmarksCount() <= bmk.getBookmarkId())
        deleted = true;
      else if (BookmarkManager.INSTANCE.getBookmark(bmk.getCategoryId(), bmk.getBookmarkId()).getLat() != bmk.getLat())
        deleted = true;
      // We can do check above, because lat/lon cannot be changed from edit screen.

      if (deleted)
      {
        // Make Poi from bookmark
        final MapObject p = new Poi(mMapObject.getName(), mMapObject.getLat(), mMapObject.getLon(), null);
        setMapObject(p);
        // TODO how to handle the case, when bookmark was moved to another group?
      }
      else
      {
        // Update data for current bookmark
        final Bookmark updatedBmk = BookmarkManager.INSTANCE.getBookmark(bmk.getCategoryId(), bmk.getBookmarkId());
        setMapObject(null);
        setMapObject(updatedBmk);
      }
    }
  }

  @Override
  public void onClick(View v)
  {
    switch (v.getId())
    {
    case R.id.iv__bookmark_color:
      selectBookmarkColor();
      break;
    case R.id.rl__bookmark:
      toggleIsBookmark();
      break;
    case R.id.rl__share:
      ShareAction.getAnyShare().shareMapObject((Activity) getContext(), mMapObject);
      break;
    case R.id.rl__api_back:
      final Activity activity = (Activity) getContext();
      if (ParsedMmwRequest.hasRequest())
      {
        final ParsedMmwRequest request = ParsedMmwRequest.getCurrentRequest();
        if (request.isPickPointMode())
          request.setPointData(mMapObject.getLat(), mMapObject.getLon(), mMapObject.getName(), "");
        request.sendResponseAndFinish(activity, true);
      }
      else
        activity.finish();
      break;
    case R.id.ll__place_latlon:
      mIsLatLonDms = !mIsLatLonDms;
      getContext().getSharedPreferences(getContext().getString(R.string.pref_file_name),
          Context.MODE_PRIVATE).edit().putBoolean(PREF_USE_DMS, mIsLatLonDms).commit();
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
        e.printStackTrace();
      }
      break;
    case R.id.ll__place_website:
      intent = new Intent(Intent.ACTION_VIEW);
      String website = mTvWebsite.getText().toString();
      if (!website.startsWith("http://") && !website.startsWith("https://"))
        website = "http://" + website;
      intent.setData(Uri.parse(website));
      getContext().startActivity(intent);
      break;
    case R.id.tv__bookmark_group:
      selectBookmarkSet();
      break;
    case R.id.av__direction:
      showBigDirection();
      break;
    case R.id.ll__place_email:
      intent = new Intent(Intent.ACTION_SENDTO);
      intent.setData(Utils.buildMailUri(mTvEmail.getText().toString(), "", ""));
      getContext().startActivity(intent);
      break;
    default:
      break;
    }
  }

  private void toggleIsBookmark()
  {
    if (mMapObject == null)
      return;
    if (mMapObject.getType() == MapObjectType.BOOKMARK)
    {
      MapObject p;
      if (mBookmarkedMapObject != null && LocationUtils.areLatLonEqual(mMapObject, mBookmarkedMapObject))
        // use cached POI of bookmark, if it corresponds to current object
        p = mBookmarkedMapObject;
      else
        p = Framework.nativeGetMapObjectForPoint(mMapObject.getLat(), mMapObject.getLon());

      BookmarkManager.INSTANCE.deleteBookmark((Bookmark) mMapObject);
      setMapObject(p);
      setState(State.DETAILS);
    }
    else
    {
      mBookmarkedMapObject = mMapObject;
      final Bookmark newBmk = BookmarkManager.INSTANCE.getBookmark(BookmarkManager.INSTANCE.addNewBookmark(
          mMapObject.getName(), mMapObject.getLat(), mMapObject.getLon()));
      setMapObject(newBmk);
      setState(State.BOOKMARK);
    }
    Framework.invalidate();
  }

  private void selectBookmarkSet()
  {
    final Activity activity = (Activity) getContext();
    final Bookmark bookmark = (Bookmark) mMapObject;
    final Intent intent = new Intent(activity, ChooseBookmarkCategoryActivity.class)
        .putExtra(BookmarkActivity.PIN_SET, bookmark.getCategoryId())
        .putExtra(BookmarkActivity.PIN, new ParcelablePoint(bookmark.getCategoryId(), bookmark.getBookmarkId()));
    activity.startActivityForResult(intent, BookmarkActivity.REQUEST_CODE_SET);
  }

  private void selectBookmarkColor()
  {
    final Bundle args = new Bundle();
    args.putString(BookmarkColorDialogFragment.ICON_TYPE, ((Bookmark) mMapObject).getIcon().getType());
    final BookmarkColorDialogFragment dialogFragment = (BookmarkColorDialogFragment) BookmarkColorDialogFragment.
        instantiate(getContext(), BookmarkColorDialogFragment.class.getName(), args);

    dialogFragment.setOnColorSetListener(new BookmarkColorDialogFragment.OnBookmarkColorChangeListener()
    {
      @Override
      public void onBookmarkColorSet(int colorPos)
      {
        Bookmark bmk = (Bookmark) mMapObject;
        final Icon newIcon = BookmarkManager.INSTANCE.getIcons().get(colorPos);
        final String from = bmk.getIcon().getName();
        final String to = newIcon.getName();
        if (!TextUtils.equals(from, to))
          Statistics.INSTANCE.trackColorChanged(from, to);

        bmk.setParams(bmk.getName(), newIcon, bmk.getBookmarkDescription());
        bmk = BookmarkManager.INSTANCE.getBookmark(bmk.getCategoryId(), bmk.getBookmarkId());
        setMapObject(bmk);
      }
    });

    dialogFragment.show(((FragmentActivity) getContext()).getSupportFragmentManager(), null);
  }

  private void showBigDirection()
  {
    final FragmentActivity hostActivity = (FragmentActivity) getContext();
    final DirectionFragment fragment = (DirectionFragment) Fragment.instantiate(hostActivity, DirectionFragment.class.getName(), null);
    fragment.setMapObject(mMapObject);
    fragment.show(hostActivity.getSupportFragmentManager(), null);
  }

  @Override
  public boolean onLongClick(View v)
  {
    final PopupMenu popup = new PopupMenu(getContext(), v);
    final Menu menu = popup.getMenu();
    String[] arr = null;
    switch (v.getId())
    {
    case R.id.ll__place_latlon:
      final double lat = mMapObject.getLat();
      final double lon = mMapObject.getLon();

      arr = new String[]{
          Framework.nativeFormatLatLon(lat, lon, false),
          Framework.nativeFormatLatLon(lat, lon, true)};

      break;
    case R.id.ll__place_website:
      arr = new String[]{mTvWebsite.getText().toString()};
      break;
    case R.id.ll__place_email:
      arr = new String[]{mTvEmail.getText().toString()};
      break;
    case R.id.ll__place_phone:
      arr = new String[]{mTvPhone.getText().toString()};
      break;
    case R.id.ll__place_schedule:
      arr = new String[]{mTvSchedule.getText().toString()};
      break;
    case R.id.ll__place_operator:
      arr = new String[]{mTvOperator.getText().toString()};
      break;
    }

    final String copyText = getResources().getString(android.R.string.copy);
    for (int i = 0; i < arr.length; i++)
      menu.add(Menu.NONE, i, i, String.format("%s %s", copyText, arr[i]));

    final String[] finalArr = arr;
    popup.setOnMenuItemClickListener(new PopupMenu.OnMenuItemClickListener()
    {
      @Override
      public boolean onMenuItemClick(MenuItem item)
      {
        final int id = item.getItemId();
        if (id >= 0 && id < 1)
        {
          final Context ctx = getContext();
          Utils.copyTextToClipboard(ctx, finalArr[id]);
          Utils.toastShortcut(ctx, ctx.getString(R.string.copied_to_clipboard, finalArr[id]));
        }
        return true;
      }
    });

    popup.show();
    return true;
  }

  @Override
  public boolean onEditorAction(TextView v, int actionId, KeyEvent event)
  {
    if (actionId == EditorInfo.IME_ACTION_DONE)
    {
      switch (v.getId())
      {
      case R.id.et__bookmark_name:
        Bookmark bookmark = (Bookmark) mMapObject;
        final String name = mEtBookmarkName.getText().toString().trim();
        bookmark.setParams(name, null, bookmark.getBookmarkDescription());
        break;
      case R.id.et__bookmark_notes:
        // TODO webview if content is html
        bookmark = (Bookmark) mMapObject;
        final String notes = mEtBookmarkNotes.getText().toString().trim();
        mTvBookmarkNotes.setVisibility(notes.isEmpty() ? View.INVISIBLE : View.VISIBLE);
        final String oldNotes = bookmark.getBookmarkDescription().trim();

        if (!TextUtils.equals(notes, oldNotes))
          Statistics.INSTANCE.trackDescriptionChanged();
        bookmark.setParams(mEtBookmarkName.getText().toString(), null, notes);
        break;
      }
    }
    return false;
  }
}
