#ifndef W_F_HPP__
#define W_F_HPP__

#include <QMetaType>
#include <QList>
#include <QVector>
#include <QColor>
#include "commons.h"

class QString;

namespace WF
{
  Q_NAMESPACE

  // Spectrum type, defines the types of waterfall spectrum displays.

  enum class Spectrum
  {
    Current,
    Cumulative,
    LinearAvg
  }; Q_ENUM_NS(Spectrum)

  // Maximum width of the screen in pixels.

  static constexpr std::size_t MaxScreenWidth = 2048;

  // Waterfall data storage types.

  using SPlot = std::array<float, NSMAX>;
  using SWide = std::array<float, MaxScreenWidth>;

  // The wide graph class drains into the plotter class, driven by a
  // timer based on the desired frames per second that the waterfall
  // should display. Since the wide graph itself acts as a sink for
  // the detector, and we may or may not have averaging in play, we
  // end up with 3 distinct states that the waterfall might be in:
  //
  //   1. Drained - No new data has arrived in the wide graph sink
  //                since it was last drained to the plotter. Any
  //                frame of data sent to the plotter in this state
  //                is a duplicate of the last frame.
  //
  //   2. Summary - New data arrived in the wide graph sink; adjunct
  //                summary data referenced by the plotter will have
  //                changed, but averaging is active, and the amount
  //                of new data is as of yet insufficient to cause a
  //                new frame of data to be sent to the plotter; any
  //                frame sent is a duplicate of the last frame.
  //
  //   3. Current - New data arrived in the wide graph sink. Adjunct
  //                summary data will have changed; sufficient new
  //                data has arrived to guarantee that the frame
  //                sent to the plotter is not a duplicate.

  enum class State
  {
    Drained,
    Summary,
    Current
  };

  //
  // Class Palette
  //
  //	Encapsulates  a waterfall palette  description.  A  colour gradient
  //	over 256 intervals is described  by a list of RGB colour triplets.
  //	The list of  colours are use to interpolate  the full 256 interval
  //	waterfall colour gradient.
  //
  // Responsibilities
  //
  //	Construction from  a string which is  a path to  a file containing
  //	colour  descriptions  in  the   form  rrr;ggg;bbb  on  up  to  256
  //	consecutive lines, where rrr, ggg and, bbb are integral numbers in
  //	the range 0<=n<256.
  //
  //	Construction from a list of QColor instances.  Up to the first 256
  //	list elements are used.
  //
  //	Includes a design GUI to create or adjust a Palette.
  //
  class Palette
  {
  public:
    using Colours = QList<QColor>;

    Palette () = default;
    explicit Palette (Colours const&);
    explicit Palette (QString const& file_path);
    Palette (Palette const&) = default;
    Palette& operator = (Palette const&) = default;

    Colours colours () const {return colours_;}

    // interpolate a gradient over 256 steps
    QVector<QColor> interpolate () const;

    // returns true if colours have been modified
    bool design ();

  private:
    Colours colours_;
  };
}

Q_DECLARE_METATYPE (WF::Spectrum);
Q_DECLARE_METATYPE (WF::Palette::Colours);

#endif
