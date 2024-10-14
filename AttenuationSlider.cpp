#include "AttenuationSlider.hpp"
#include <QApplication>
#include <QPainter>
#include <QPixmapCache>
#include <QStyle>

/******************************************************************************/
// Private Implementation
/******************************************************************************/

namespace
{
  // Tunable dimensions.

  constexpr auto grooveWidth   = 10;
  constexpr auto handleSize    = QSize(40, 20);
  constexpr auto textPointSize = 12;
  constexpr auto tickLength    = 8;

  // Colors

  constexpr auto activeColor      = QColor( 10, 129, 254);
  constexpr auto grooveColor      = QColor(192, 192, 192);
  constexpr auto handleStartColor = QColor(  0, 255,   0);
  constexpr auto handleStopColor  = QColor( 39, 174,  96);
  constexpr auto outlineColor     = QColor(  0,   0,   0, 160);
  constexpr auto contrastColor    = QColor(255, 255, 255,  30);

  // Given a size, return a transparently-filled pixmap, with a pixel
  // ratio appropriate to the device in play.

  auto
  makePixmap(QSize const size)
  {
    auto const pixelRatio = qApp->devicePixelRatio();
    auto       pixmap     = QPixmap(size * pixelRatio);

    pixmap.setDevicePixelRatio(pixelRatio);
    pixmap.fill(Qt::transparent);

    return pixmap;
  }

  // Create and return a pixmap for the groove, using the provided size.

  auto
  makeGroovePixmap(QSize const size)
  {
    auto       pixmap   = makePixmap(size);
    auto const rect     = QRect(QPoint(), size);
    auto       gradient = QLinearGradient(rect.left(),
                                          rect.center().y(),
                                          rect.right(),
                                          rect.center().y());

    gradient.setColorAt(0, grooveColor.darker(110));
    gradient.setColorAt(1, grooveColor.lighter(110));

    QPainter p(&pixmap);

    p.setRenderHint(QPainter::Antialiasing, true);
    p.translate(0.5, 0.5);
    p.setPen(outlineColor);
    p.setBrush(gradient);
    p.drawRoundedRect(rect.adjusted(1, 1, -2, -2), 1, 1);

    return pixmap;
  }

  // Create and return a pixmap for the groove active highlight, using
  // the provided size.

  auto
  makeActivePixmap(QSize const size)
  {
    auto       pixmap   = makePixmap(size);
    auto const rect     = QRect(QPoint(), size);
    auto       gradient = QLinearGradient(rect.left(),
                                          rect.center().y(),
                                          rect.right(),
                                          rect.center().y());

    gradient.setColorAt(0, activeColor);
    gradient.setColorAt(1, activeColor.lighter(130));

    QPainter p(&pixmap);

    p.setRenderHint(QPainter::Antialiasing, true);
    p.translate(0.5, 0.5);
    p.setPen(outlineColor);
    p.setBrush(gradient);
    p.drawRoundedRect(rect.adjusted(1, 1, -2, -2), 1, 1);
    p.setPen(Qt::darkGray);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(rect.adjusted(2, 2, -3, -3), 1, 1);

    return pixmap;
  }

  // Create and return a slider handle, using the provided size.

  auto
  makeHandlePixmap(QSize const size)
  {
    auto       pixmap   = makePixmap(size);
    auto const rect     = QRect(QPoint(), size);
    auto const r        = rect.adjusted(1, 1, -2, -2);
    auto const gradRect = rect.adjusted(2, 2, -2, -2);
    auto       gradient = QLinearGradient(gradRect.center().x(),
                                          gradRect.top(),
                                          gradRect.center().x(),
                                          gradRect.bottom());
                                  
    gradient.setColorAt(0, handleStartColor);
    gradient.setColorAt(1, handleStopColor);
    
    QPainter p(&pixmap);

    p.setRenderHint(QPainter::Antialiasing, true);
    p.translate(0.5, 0.5);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 40));
    p.drawRect(r.adjusted(-1, 2, 1, -2));
    p.setPen(outlineColor);
    p.setBrush(gradient);
    p.drawRoundedRect(r, 2, 2);
    p.setBrush(Qt::NoBrush);
    p.setPen(contrastColor);
    p.drawRoundedRect(r.adjusted(1, 1, -1, -1), 2, 2);
    p.setPen(QColor(0, 0, 0, 10));
    p.drawLine(QPoint(r.left()  + 2, r.bottom() + 1), QPoint(r.right() - 2, r.bottom() + 1));
    p.drawLine(QPoint(r.right() + 1, r.bottom() - 3), QPoint(r.right() + 1, r.top()    + 4));
    p.drawLine(QPoint(r.right() - 1, r.bottom()    ), QPoint(r.right() + 1, r.bottom() - 2));

    return pixmap;
  }

  // Convenience type definition for the three pixmap creation functions above.

  using MakePixmap = QPixmap(*)(QSize);

  // Look for a matching pixmap in the global pixmap cache, returning it if
  // found, creating and caching it if it wasn't present in the cache.

  auto
  cachedPixmap(QSize        const size,
               const char * const name,
               MakePixmap   const make)
  {
    auto const key = QString("attenuation_slider_%1(%2,%3)").arg(name).arg(size.width()).arg(size.height());
    QPixmap    pixmap;

    if (!QPixmapCache::find(key, &pixmap))
    {
      pixmap = make(size);
      QPixmapCache::insert(key, pixmap);
    }

    return pixmap;
  }
}

/******************************************************************************/
// Public Implementation
/******************************************************************************/

void
AttenuationSlider::paintEvent(QPaintEvent *)
{
  auto const handleX = rect().center().x() - handleSize.width() / 2;
  auto const handleY = QStyle::sliderPositionFromValue(minimum(),
                                                       maximum(),
                                                       sliderPosition(),
                                                       rect().height() - handleSize.height(),
                                                      !invertedAppearance());

  auto const handle = QRect(QPoint(handleX, handleY), handleSize);
  auto const groove = QRect(rect().center().x() - grooveWidth / 2,
                            rect().y()      + handleSize.height() / 2,
                            grooveWidth,
                            rect().height() - handleSize.height());

  QPainter p(this);

  // Draw groove.

  p.drawPixmap(groove.topLeft(), cachedPixmap(groove.size(), "groove", &makeGroovePixmap));

  // Draw groove active highlight, clipping it to the active portion.

  auto const clipRect = QRect(QPoint(groove.left(), handle.bottom()), groove.bottomRight());

  p.save();
  p.setClipRect(clipRect.adjusted(0, 0, 1, 1), Qt::IntersectClip);
  p.drawPixmap(groove.topLeft(), cachedPixmap(groove.size(), "active", &makeActivePixmap));
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

  p.drawPixmap(handle.topLeft(), cachedPixmap(handle.size(), "handle", &makeHandlePixmap));

  // Draw attenuation level text; value is 10x the attenuation level.

  p.setFont({"Arial", textPointSize});
  p.drawText(handle, Qt::AlignCenter, QString::number(-(value() / 10.0)));
}

/******************************************************************************/
