#ifndef ATTENUATION_SLIDER_H__
#define ATTENUATION_SLIDER_H__

#include <QSlider>

class AttenuationSlider final : public QSlider
{
public:
  explicit AttenuationSlider(QWidget * parent = nullptr)
  : QSlider{parent}
  {}

protected:

  void paintEvent(QPaintEvent *) override;
};

#endif
