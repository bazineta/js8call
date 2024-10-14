#include "AttenuationSlider.hpp"
#include <QApplication>
#include <QPainter>
#include <QPixmapCache>
#include <QStyle>

namespace
{
  constexpr auto handleSize = QSize(40, 20);
  constexpr int  tickLength = 8;

  constexpr auto outline         = QColor(0, 0, 0, 160);
  constexpr auto shadow          = QColor(0, 0, 0,  10);
  constexpr auto highlight       = QColor(255, 255, 255);
  constexpr auto active          = QColor(10, 129, 254);
  constexpr auto grooveColor     = QColor(192, 192, 192);
  constexpr auto innerContrast   = QColor(255, 255, 255, 30);
  constexpr auto handleGradient0 = QColor( 0, 255,  0);
  constexpr auto handleGradient1 = QColor(39, 174, 96);

  auto
  uniqueName(QString const & key,
             QRect   const & rect)
  {
    auto const size = rect.size();
    return QString("%1(%2,%3)").arg(key).arg(size.width()).arg(size.height());
  }

  auto
  makePixmap(QSize const & size)
  {
      auto const pixelRatio = qApp->devicePixelRatio();
      auto       pixmap     = QPixmap(size * pixelRatio);

      pixmap.setDevicePixelRatio(pixelRatio);
      pixmap.fill(Qt::transparent);

      return pixmap;
  }

  auto
  makeGroovePixmap(QRect const & groove)
  {
    auto       pixmap = makePixmap(groove.size());
    auto const rect   = QRect(QPoint(), groove.size());

    QLinearGradient gradient;
    
    gradient.setStart    (rect.left(),  rect.center().y());
    gradient.setFinalStop(rect.right(), rect.center().y());

    gradient.setColorAt(0, grooveColor.darker(110));
    gradient.setColorAt(1, grooveColor.lighter(110));

    QPainter p(&pixmap);

    p.setRenderHint(QPainter::Antialiasing, true);
    p.translate(0.5, 0.5);
    p.setPen(outline);
    p.setBrush(gradient);
    p.drawRoundedRect(rect.adjusted(1, 1, -2, -2), 1, 1);

    return pixmap;
  }

  auto
  makeActivePixmap(QRect const & groove)
  {
    auto       pixmap = makePixmap(groove.size());
    auto const rect   = QRect(QPoint(), groove.size());

    QLinearGradient gradient;
    
    gradient.setStart    (rect.left(),  rect.center().y());
    gradient.setFinalStop(rect.right(), rect.center().y());

    gradient.setColorAt(0, active);
    gradient.setColorAt(1, active.lighter(130));
  
    auto const highlightedoutline = highlight.darker(140);

    QPainter p(&pixmap);

    p.setRenderHint(QPainter::Antialiasing, true);
    p.translate(0.5, 0.5);
    p.setPen(qGray(outline.rgb()) > qGray(highlightedoutline.rgb()) ? highlightedoutline : outline);
    p.setBrush(gradient);
    p.drawRoundedRect(rect.adjusted(1, 1, -2, -2), 1, 1);
    p.setPen(Qt::darkGray);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(rect.adjusted(2, 2, -3, -3), 1, 1);

    return pixmap;
  }

  auto
  makeHandlePixmap(QRect const & handle,
                   bool  const   active)
  {
    auto       pixmap   = makePixmap(handle.size());
    auto const rect     = QRect(QPoint(), handle.size());
    auto const gradRect = rect.adjusted(2, 2, -2, -2);
    auto const r        = rect.adjusted(1, 1, -2, -2);

    auto gradient = QLinearGradient(gradRect.center().x(),
                                    gradRect.top(),
                                    gradRect.center().x(),
                                    gradRect.bottom());
                                  
    gradient.setColorAt(0, handleGradient0);
    gradient.setColorAt(1, handleGradient1);
    
    QPainter p(&pixmap);

    p.setRenderHint(QPainter::Antialiasing, true);
    p.translate(0.5, 0.5);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 40));
    p.drawRect(r.adjusted(-1, 2, 1, -2));
    p.setPen(active ? highlight.darker(140) : outline);
    p.setBrush(gradient);
    p.drawRoundedRect(r, 2, 2);
    p.setBrush(Qt::NoBrush);
    p.setPen(innerContrast);
    p.drawRoundedRect(r.adjusted(1, 1, -1, -1), 2, 2);
    p.setPen(shadow);
    p.drawLine(QPoint(r.left()  + 2, r.bottom() + 1), QPoint(r.right() - 2, r.bottom() + 1));
    p.drawLine(QPoint(r.right() + 1, r.bottom() - 3), QPoint(r.right() + 1, r.top()    + 4));
    p.drawLine(QPoint(r.right() - 1, r.bottom()    ), QPoint(r.right() + 1, r.bottom() - 2));

    return pixmap;
  }
}

void
AttenuationSlider::paintEvent(QPaintEvent *)
{
 // QStyleOptionSlider slider;
 // initStyleOption(&slider);

  // Rectangle representing the slider's handle (position and size).

  auto const handleX = rect().center().x() - handleSize.width() / 2;
  auto const handleY = QStyle::sliderPositionFromValue(minimum(),
                                                       maximum(),
                                                       sliderPosition(),
                                                       rect().height() - handleSize.height(),
                                                      !invertedAppearance());
  auto const handle  = QRect(QPoint(handleX, handleY), handleSize);
  auto const groove  = QRect(rect().center().x() - 5,
                             rect().y()      + handleSize.height() / 2,
                             10,
                             rect().height() - handleSize.height());

  QPainter p(this);
  QPixmap  pixmap;

  // Draw groove.

  if (auto const name = uniqueName("attenuation_slider_groove", groove); !QPixmapCache::find(name, &pixmap)) {
    pixmap = makeGroovePixmap(groove);
    QPixmapCache::insert(name, pixmap);
  }
  p.drawPixmap(groove.topLeft(), pixmap);

  // Draw groove active highlight.
  
  if (auto const name = uniqueName("attenuation_slider_active", groove); !QPixmapCache::find(name, &pixmap)) {
    pixmap = makeActivePixmap(groove);
    QPixmapCache::insert(name, pixmap);
  }

  auto const clipRect = QRect(QPoint(groove.left(), handle.bottom()), groove.bottomRight());

  p.save();
  p.setClipRect(clipRect.adjusted(0, 0, 1, 1), Qt::IntersectClip);
  p.drawPixmap(groove.topLeft(), pixmap);
  p.restore();

  // Draw tick marks.

  p.setPen(Qt::black);

  for (auto value  = minimum();
            value <= maximum();
            value += tickInterval())
  {
    auto const y = handleSize.height() / 2 +
                   QStyle::sliderPositionFromValue(minimum(),
                                                   maximum(),
                                                   value,
                                                   rect().height() - handleSize.height(),
                                                  !invertedAppearance());

    p.drawLine(rect().left(),  y, rect().left()  + tickLength, y);
    p.drawLine(rect().right(), y, rect().right() - tickLength, y);
  }

  // Draw slider handle.

  if (auto const name = uniqueName("attenuation_slider_handle", handle); !QPixmapCache::find(name, &pixmap)) {
    pixmap = makeHandlePixmap(handle, hasFocus() && window()->testAttribute(Qt::WA_KeyboardFocusChange));
    QPixmapCache::insert(name, pixmap);
  }

  p.setFont({"Arial", 12});
  p.drawPixmap(handle.topLeft(), pixmap);
  p.drawText(handle, Qt::AlignCenter, QString::number(-(value() / 10.0)));
}
