// Chooses the persistent backend (NVS) or, if its begin() fails, an in-RAM fallback.
// Owns both backends and a ForwardingStorage; services hold storage() from static-init time.
// Header-only and templated on the backends so host tests can inject fakes.
//
// Before begin() the forwarder points at the fallback, so an early call can never touch NVS
// that is not initialised. After a failed begin() settings, scores and saves still work for the
// session but are lost on reboot; persistent() reports which case applies.
#pragma once

#include "hal/ForwardingStorage.h"

template <class Persistent, class Fallback>
class StorageSelectorT {
 public:
  StorageSelectorT() { fwd_.setTarget(&fallback_); }

  // Initialises the persistent backend; selects it on success, the fallback otherwise.
  bool begin() {
    persistent_ok_ = primary_.begin();
    fwd_.setTarget(persistent_ok_ ? static_cast<IStorage*>(&primary_)
                                  : static_cast<IStorage*>(&fallback_));
    return persistent_ok_;
  }

  IStorage& storage() { return fwd_; }
  bool persistent() const { return persistent_ok_; }
  Persistent& primary() { return primary_; }
  Fallback& fallback() { return fallback_; }

 private:
  Persistent primary_;
  Fallback fallback_;
  ForwardingStorage fwd_;
  bool persistent_ok_ = false;
};
