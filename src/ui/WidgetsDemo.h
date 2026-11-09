// Full-screen demo of every widget (issue #23), shown with the serial command `widgets`:
// Buttons (one disabled until the toggle enables it), a Label, a Toggle, an 8-item ListView, a
// Modal opened by a button and the initials Keypad opened by another.
#pragma once

#include "Renderer.h"
#include "Widgets.h"

namespace checkers::ui {

class WidgetsDemo {
 public:
  WidgetsDemo();

  // Installs the demo's group as the renderer's rect painter, drops the cell painter and
  // paints the whole screen.
  void show(Renderer& renderer);
  void hide() { active_ = false; }
  bool active() const { return active_; }

  // Routes a touch to the widgets and queues what changed; the caller then calls
  // renderer.flush().
  void handle(const TouchEvent& ev);

 private:
  static void onShowModal(void* self);
  static void onShowKeypad(void* self);
  static void onPlain(void* self);
  static void onToggle(void* self, bool on);
  static void onList(void* self, int index);
  static void onModal(void* self, int button);
  static void onKeypadOk(void* self, const char* initials);
  void setStatus(const char* fmt, const char* arg);

  Renderer* renderer_ = nullptr;
  bool active_ = false;
  WidgetGroup group_;
  Label title_;
  Button modalBtn_;
  Button keypadBtn_;
  Button plainBtn_;
  Button disabledBtn_;
  Toggle toggle_;
  ListView list_;
  Label status_;
  Modal modal_;
  Keypad keypad_;
  char statusText_[48] = {};
  int taps_ = 0;
};

}  // namespace checkers::ui
