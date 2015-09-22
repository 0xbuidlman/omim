package com.mapswithme.maps;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Paint.Style;
import android.graphics.Path;
import android.util.AttributeSet;
import android.util.Log;
import android.widget.ImageView;

public class ArrowImage extends ImageView
{
  static private String TAG = "ArrowImage";

  private Paint m_paint;
  private boolean m_drawArrow;
  private float m_angle;

  public ArrowImage(Context context, AttributeSet attrs)
  {
    super(context, attrs);

    m_paint = new Paint();
    m_paint.setStyle(Style.FILL);
    m_paint.setColor(Color.WHITE);
  }

  public void setFlag(Resources res, String flag)
  {
    m_drawArrow = false;

    // The aapt can't process resources with name "do". Hack with renaming.
    if (flag.equals("do"))
      flag = "do_hack";

    final int id = res.getIdentifier(flag, "drawable", "com.mapswithme.maps");
    if (id > 0)
      setImageDrawable(res.getDrawable(id));
    else
      Log.e(TAG, "Failed to get resource id from: " + flag);
  }

  public void setAzimut(double azimut)
  {
    setImageDrawable(null);

    m_drawArrow = true;
    m_angle = (float)(azimut / Math.PI * 180.0);
  }

  @Override
  protected void onDraw(Canvas canvas)
  {
    super.onDraw(canvas);

    if (m_drawArrow)
    {
      final float w = getWidth();
      final float h = getHeight();

      if (m_angle < 0.0)
      {
        canvas.drawCircle(w/2, h/2, Math.min(w/2, h/2) - 5, m_paint);
      }
      else
      {
        Path path = new Path();

        path.moveTo(w/3, h/2);
        path.lineTo(0, h/2 - h/3);
        path.lineTo(w, h/2);
        path.lineTo(0, h/2 + h/3);
        path.lineTo(w/3, h/2);
        path.close();

        canvas.rotate(-m_angle, w/2, h/2);
        canvas.drawPath(path, m_paint);
      }
    }
  }
}
