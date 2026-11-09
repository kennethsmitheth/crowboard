#include "Scanout.h"

#include <Arduino.h>
#include <esp_intr_alloc.h>
#include <esp_timer.h>
#include <hal/dma_types.h>
#include <hal/lcd_ll.h>
#include <soc/gdma_channel.h>
#include <soc/gdma_struct.h>
#include <soc/interrupts.h>
#include <soc/lcd_cam_struct.h>

#include "Log.h"
#include "crowpanel7_lgfx.h"

namespace checkers::hal {
namespace {

intr_handle_t s_vsyncHandle = nullptr;
volatile uint32_t s_frames = 0;
volatile int64_t s_lastUs = 0;
volatile uint32_t s_periodUs = 0;

int s_dmaCh = -1;
const uint8_t* s_fb = nullptr;
size_t s_fbLen = 0;
uint32_t s_lineBytes = 0;
uint32_t s_lineNs = 0;

// Observe only: Bus_RGB's own handler (registered first, so it runs after ours) clears the
// status bits and restarts the DMA chain.
void IRAM_ATTR onVsync(void*) {
  const int64_t now = esp_timer_get_time();
  if (s_lastUs) s_periodUs = static_cast<uint32_t>(now - s_lastUs);
  s_lastUs = now;
  s_frames = s_frames + 1;
}

}  // namespace

bool Scanout::begin(LGFX& gfx) {
  auto* panel = gfx.getPanel();
  auto* bus = panel ? panel->getBus() : nullptr;
  if (!bus) {
    LOG_W("scanout: no bus");
    return false;
  }

  // Panel timing: pixel clock and horizontal total give the line period.
  {
    const auto& cfg = static_cast<lgfx::Bus_RGB*>(bus)->config();
    const uint32_t htotal = static_cast<uint32_t>(cfg.hsync_pulse_width) + cfg.hsync_back_porch +
                            cfg.hsync_front_porch + gfx.width();
    s_lineNs =
        static_cast<uint32_t>((static_cast<uint64_t>(htotal) * 1000000000ULL) / cfg.freq_write);
  }

  // Frame buffer and the GDMA channel LovyanGFX chose for the LCD peripheral.
  s_fb = bus->getDMABuffer(0);
  s_lineBytes = static_cast<uint32_t>(gfx.width()) * 2;
  s_fbLen = static_cast<size_t>(s_lineBytes) * gfx.height();
  s_dmaCh = -1;
  for (int ch = 0; ch < SOC_GDMA_PAIRS_PER_GROUP; ++ch) {
    if (GDMA.channel[ch].out.peri_sel.sel == SOC_GDMA_TRIG_PERIPH_LCD0) {
      s_dmaCh = ch;
      break;
    }
  }

  // Second shared handler on the LCD_CAM interrupt, gated on the vsync status bit.
  if (!s_vsyncHandle) {
    const esp_err_t err =
        esp_intr_alloc_intrstatus(ETS_LCD_CAM_INTR_SOURCE, ESP_INTR_FLAG_SHARED,
                                  reinterpret_cast<uint32_t>(&LCD_CAM.lc_dma_int_st),
                                  LCD_LL_EVENT_VSYNC_END, onVsync, nullptr, &s_vsyncHandle);
    if (err != ESP_OK) {
      s_vsyncHandle = nullptr;
      LOG_W("scanout: vsync hook failed (%d)", static_cast<int>(err));
    }
  }

  LOG_I("scanout: fb %p len %u dma ch %d line %u ns vsync %s", s_fb, static_cast<unsigned>(s_fbLen),
        s_dmaCh, static_cast<unsigned>(s_lineNs), s_vsyncHandle ? "hooked" : "none");
  return hasBeam();
}

bool Scanout::hasBeam() {
  return s_dmaCh >= 0 && s_fb != nullptr;
}

bool Scanout::hasVsync() {
  return s_vsyncHandle != nullptr;
}

uint32_t Scanout::frames() {
  return s_frames;
}

int64_t Scanout::lastVsyncUs() {
  return s_lastUs;
}

uint32_t Scanout::periodUs() {
  return s_periodUs;
}

uint32_t Scanout::lineNs() {
  return s_lineNs;
}

int Scanout::beamLine() {
  if (s_dmaCh < 0 || !s_fb) return -1;
  const uint32_t addr = GDMA.channel[s_dmaCh].out.dscr;
  if (!addr) return -1;
  const auto* desc = reinterpret_cast<const dma_descriptor_t*>(addr);
  const uint8_t* buf = static_cast<const uint8_t*>(desc->buffer);
  if (buf < s_fb || buf >= s_fb + s_fbLen) return -1;
  return static_cast<int>((buf - s_fb) / s_lineBytes);
}

bool Scanout::waitVsync(uint32_t timeoutUs) {
  if (!s_vsyncHandle) return false;
  const uint32_t start = s_frames;
  const uint32_t t0 = micros();
  while (s_frames == start) {
    if (micros() - t0 > timeoutUs) return false;
  }
  return true;
}

uint32_t Scanout::waitBeamClear(int y0, int y1, int opLines, uint32_t timeoutUs) {
  const uint32_t t0 = micros();
  while (!beamClear(beamLine(), y0, y1, opLines)) {
    if (micros() - t0 > timeoutUs) break;
  }
  return micros() - t0;
}

}  // namespace checkers::hal
