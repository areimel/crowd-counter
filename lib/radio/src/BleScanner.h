#pragma once
#include "ObservationSink.h"

namespace cc {

// NimBLE passive (receive-only) scan feeding an ObservationSink. Never
// advertises, connects or sends scan requests. Duplicate filtering is off and
// no results are stored, so every advert reaches the callback once and is
// then freed by NimBLE.
//
// start()/stop() are called by the RadioScheduler task only, never from the
// scan callback (which runs on the NimBLE host task).
class BleScanner {
 public:
  bool begin(ObservationSink& sink);
  bool start();  // continuous until stop()
  void stop();
  bool ready() const { return ready_; }

 private:
  bool ready_ = false;
};

}  // namespace cc
