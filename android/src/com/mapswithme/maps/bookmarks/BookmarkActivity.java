package com.mapswithme.maps.bookmarks;

import java.util.List;

import android.app.AlertDialog;
import android.app.AlertDialog.Builder;
import android.app.Dialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.graphics.Point;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.TextView;

import com.mapswithme.maps.R;
import com.mapswithme.maps.bookmarks.data.Bookmark;
import com.mapswithme.maps.bookmarks.data.Icon;
import com.mapswithme.maps.bookmarks.data.ParcelablePoint;
import com.mapswithme.util.Utils;

public class BookmarkActivity extends AbstractBookmarkActivity
{
  private static final int CONFIRMATION_DIALOG = 11001;
  private static final int BOOKMARK_COLOR_DIALOG = 11002;

  public static final String BOOKMARK_POSITION = "bookmark_position";
  public static final String PIN = "pin";
  public static final String PIN_ICON_ID = "pin";
  public static final String PIN_SET = "pin_set";
  public static final int REQUEST_CODE_SET = 567890;
  public static final String BOOKMARK_NAME = "bookmark_name";

  private Bookmark mPin;
  private EditText mName;
  private TextView mSetName;
  private int mCurrentCategoryId = -1;
  private List<Icon> mIcons;
  private ImageView mChooserImage;
  private EditText mDescr;
  private Icon m_icon = null;

  @Override
  public void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);

    setContentView(R.layout.add_or_edit_bookmark);

    assert(getIntent().getExtras().containsKey(PIN));
    final Point p = ((ParcelablePoint)getIntent().getParcelableExtra(PIN)).getPoint();

    mPin = mManager.getBookmark(p.x, p.y);
    mCurrentCategoryId = mPin.getCategoryId();

    setTitle(mPin.getName());
    setUpViews();
  }

  private void updateColorChooser(Icon icon)
  {
    m_icon = icon;
    mChooserImage.setImageBitmap(m_icon.getIcon());
  }

  private void refreshValuesInViews()
  {
    updateColorChooser(mPin.getIcon());

    mSetName.setText(mPin.getCategoryName());

    Utils.setStringAndCursorToEnd(mName, mPin.getName());

    mDescr.setText(mPin.getBookmarkDescription());
  }

  private void setUpViews()
  {
    View colorChooser = findViewById(R.id.pin_color_chooser);
    mChooserImage = (ImageView)colorChooser.findViewById(R.id.row_color_image);

    mIcons = mManager.getIcons();

    colorChooser.setOnClickListener(new OnClickListener()
    {
      @Override
      public void onClick(View v)
      {
        showDialog(BOOKMARK_COLOR_DIALOG);
      }
    });

    findViewById(R.id.pin_sets).setOnClickListener(new OnClickListener()
    {
      @Override
      public void onClick(View v)
      {
        startActivityForResult(new Intent(BookmarkActivity.this,
                                          ChooseBookmarkCategoryActivity.class)
        .putExtra(PIN_SET, mCurrentCategoryId)
        .putExtra(PIN, new ParcelablePoint(mPin.getCategoryId(), mPin.getBookmarkId())), REQUEST_CODE_SET);
      }
    });

    mSetName = (TextView) findViewById(R.id.pin_button_set_name);
    mName = (EditText) findViewById(R.id.pin_name);
    mDescr = (EditText)findViewById(R.id.pin_description);

    refreshValuesInViews();

    mName.addTextChangedListener(new TextWatcher()
    {
      @Override
      public void onTextChanged(CharSequence s, int start, int before, int count)
      {
        setTitle(s.toString());

        // Note! Do not set actual name here - saving process may be too long
        // see assignPinParams() instead.
      }
      @Override
      public void beforeTextChanged(CharSequence s, int start, int count, int after)
      {
      }
      @Override
      public void afterTextChanged(Editable s)
      {
      }
    });
  }

  private void assignPinParams()
  {
    if (mPin != null)
      mPin.setParams(mName.getText().toString(), m_icon, mDescr.getText().toString());
  }

  @Override
  protected void onPause()
  {
    assignPinParams();

    super.onPause();
  }

  @Override
  @Deprecated
  protected Dialog onCreateDialog(int id)
  {
    if (CONFIRMATION_DIALOG == id)
    {
      AlertDialog.Builder builder = new Builder(this);
      builder.setNegativeButton(android.R.string.cancel, new DialogInterface.OnClickListener()
      {
        @Override
        public void onClick(DialogInterface dialog, int which)
        {
          dialog.dismiss();
        }
      });
      builder.setPositiveButton(android.R.string.yes, new DialogInterface.OnClickListener()
      {
        @Override
        public void onClick(DialogInterface dialog, int which)
        {
          mManager.deleteBookmark(mPin);
          mPin = null;
          dialog.dismiss();
          finish();
        }
      });
      builder.setTitle(R.string.are_you_sure);
      builder.setMessage(getString(R.string.delete)+ " " + mPin.getName() + "?");
      return builder.create();
    }
    else
      if (id == BOOKMARK_COLOR_DIALOG)
      {
        return createColorChooser();
      }
      else
        return super.onCreateDialog(id);
  }

  private Dialog createColorChooser()
  {
    final IconsAdapter adapter = new IconsAdapter(this, mIcons);
    adapter.chooseItem(mIcons.indexOf(mPin.getIcon()));

    return new AlertDialog.Builder(this)
    .setTitle(R.string.bookmark_color)
    .setSingleChoiceItems(adapter, adapter.getCheckedItemPosition(), new DialogInterface.OnClickListener()
    {
      @Override
      public void onClick(DialogInterface dialog, int which)
      {
        updateColorChooser(mIcons.get(which));
        dialog.dismiss();
      }
    })
    .create();
  }

  @Override
  protected void onActivityResult(int requestCode, int resultCode, Intent data)
  {
    if (requestCode == REQUEST_CODE_SET && resultCode == RESULT_OK)
    {
      Point pin = ((ParcelablePoint)data.getParcelableExtra(PIN)).getPoint();
      mPin = mManager.getBookmark(pin.x, pin.y);
      refreshValuesInViews();
      mCurrentCategoryId = mPin.getCategoryId();
    }
    super.onActivityResult(requestCode, resultCode, data);
  }

  public void onOkClick(View v)
  {
    onBackPressed();
  }

  public void onDeleteClick(View v)
  {
    showDialog(CONFIRMATION_DIALOG);
  }
}
