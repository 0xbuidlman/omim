package com.mapswithme.maps.bookmarks;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.RadioButton;
import android.widget.TextView;

import com.mapswithme.maps.R;

public class ChooseBookmarkCategoryAdapter extends AbstractBookmarkCategoryAdapter implements OnItemClickListener, Chooseable
{
  private int mCheckedPosition = -1;

  public ChooseBookmarkCategoryAdapter(Context context, int pos)
  {
    super(context);
    mCheckedPosition = pos;
  }

  @Override
  public View getView(int position, View convertView, ViewGroup parent)
  {
    if (convertView == null)
    {
      convertView = LayoutInflater.from(getContext()).inflate(R.layout.set_chooser_item, null);
      convertView.setTag(new SingleChoiceHolder((TextView) convertView.findViewById(R.id.sci_set_name),
          (RadioButton) convertView.findViewById(R.id.sci_checkbox)));
    }
    SingleChoiceHolder holder = (SingleChoiceHolder) convertView.getTag();
    boolean checked = mCheckedPosition == position;
    holder.name.setText(getItem(position).getName());
    holder.name.setTextAppearance(getContext(), checked ? android.R.style.TextAppearance_Large : android.R.style.TextAppearance_Medium);
    holder.checked.setChecked(checked);
    return convertView;
  }

  private static class SingleChoiceHolder
  {
    TextView name;
    RadioButton checked;

    public SingleChoiceHolder(TextView name, RadioButton checked)
    {
      this.name = name;
      this.checked = checked;
    }
  }

  @Override
  public void chooseItem(int position)
  {
    mCheckedPosition = position;
    notifyDataSetChanged();
  }

  @Override
  public int getCheckedItemPosition()
  {
    return mCheckedPosition;
  }

  @Override
  public void onItemClick(AdapterView<?> parent, View view, int position, long id)
  {
    chooseItem(position);
  }
}
