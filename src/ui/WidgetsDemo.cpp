#include "WidgetsDemo.h"

#include <stdio.h>

namespace checkers::ui {
namespace {

const char* const kItems[] = {"Alice", "Bob", "Carol", "Dave", "Erin", "Frank", "Grace", "Heidi"};

}  // namespace

WidgetsDemo::WidgetsDemo() {
  title_.init(16, 8, 768, 52, "Widgets demo", Label::Size::Title, Label::Align::Left);
  modalBtn_.init(16, 76, 232, kButtonH, "Show modal", onShowModal, this);
  keypadBtn_.init(16, 156, 232, kButtonH, "Enter initials", onShowKeypad, this);
  plainBtn_.init(16, 236, 232, kButtonH, "Tap me", onPlain, this);
  disabledBtn_.init(16, 316, 232, kButtonH, "Disabled", onPlain, this);
  disabledBtn_.setEnabled(false);
  toggle_.init(16, 396, 232, 72, "Enable", false, onToggle, this);

  list_.init(272, 76, 512, 4 * kButtonH, 4, onList, this);
  list_.setItems(kItems, 8);
  setStatus("%s", "Ready");
  status_.init(272, 348, 512, 48, statusText_, Label::Size::Body, Label::Align::Left);

  modal_.init(160, 120, 480, 240, onModal, this);
  keypad_.init(0, 0, kScreenW, kScreenH, onKeypadOk, this);
  keypad_.setVisible(false);

  group_.add(&title_);
  group_.add(&modalBtn_);
  group_.add(&keypadBtn_);
  group_.add(&plainBtn_);
  group_.add(&disabledBtn_);
  group_.add(&toggle_);
  group_.add(&list_);
  group_.add(&status_);
  group_.add(&keypad_);
  group_.add(&modal_);  // last: on top
}

void WidgetsDemo::show(Renderer& renderer) {
  renderer_ = &renderer;
  active_ = true;
  renderer.setCellPainter(nullptr);  // the demo owns the whole screen
  renderer.setRectPainter(&group_);
  group_.invalidateAll();
  group_.collectDirty(renderer.dirty());
  renderer.flush();
}

void WidgetsDemo::handle(const TouchEvent& ev) {
  if (!active_ || !renderer_) return;
  group_.dispatch(ev);
  group_.collectDirty(renderer_->dirty());
}

void WidgetsDemo::setStatus(const char* fmt, const char* arg) {
  snprintf(statusText_, sizeof statusText_, fmt, arg);
  status_.setText(statusText_);
}

void WidgetsDemo::onShowModal(void* self) {
  auto* d = static_cast<WidgetsDemo*>(self);
  d->modal_.show("Reset scores?", "This clears the high-score table.\nThis cannot be undone.",
                 "Cancel", "Reset");
}

void WidgetsDemo::onShowKeypad(void* self) {
  auto* d = static_cast<WidgetsDemo*>(self);
  d->keypad_.clear();
  d->keypad_.setVisible(true);
}

void WidgetsDemo::onPlain(void* self) {
  auto* d = static_cast<WidgetsDemo*>(self);
  char buf[16];
  snprintf(buf, sizeof buf, "%d", ++d->taps_);
  d->setStatus("Taps: %s", buf);
}

void WidgetsDemo::onToggle(void* self, bool on) {
  auto* d = static_cast<WidgetsDemo*>(self);
  d->disabledBtn_.setEnabled(on);
  d->setStatus("Toggle: %s", on ? "on (button enabled)" : "off (button disabled)");
}

void WidgetsDemo::onList(void* self, int index) {
  static_cast<WidgetsDemo*>(self)->setStatus("Selected: %s", kItems[index]);
}

void WidgetsDemo::onModal(void* self, int button) {
  static_cast<WidgetsDemo*>(self)->setStatus("Modal: %s", button == 0 ? "Cancel" : "Reset");
}

void WidgetsDemo::onKeypadOk(void* self, const char* initials) {
  auto* d = static_cast<WidgetsDemo*>(self);
  d->keypad_.setVisible(false);
  d->setStatus("Initials: %s", initials);
}

}  // namespace checkers::ui
