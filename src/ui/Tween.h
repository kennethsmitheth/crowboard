// Pure animation maths (no Arduino, host-testable): easing, colour blend and a tween along a
// polyline of pixel waypoints. Used by Animator.
#pragma once

#include <stdint.h>

namespace checkers::ui {

// Smooth start and end; t is clamped to [0, 1]. easeInOutQuad(0) == 0, (0.5) == 0.5, (1) == 1.
inline float easeInOutQuad(float t) {
  if (t <= 0.0f) return 0.0f;
  if (t >= 1.0f) return 1.0f;
  if (t < 0.5f) return 2.0f * t * t;
  const float u = -2.0f * t + 2.0f;
  return 1.0f - u * u / 2.0f;
}

// RGB565 blend: amount 0 gives a, 255 gives b.
inline uint16_t blend565(uint16_t a, uint16_t b, uint8_t amount) {
  auto mix = [amount](int ca, int cb) { return (ca * (255 - amount) + cb * amount + 127) / 255; };
  const int r = mix((a >> 11) & 31, (b >> 11) & 31);
  const int g = mix((a >> 5) & 63, (b >> 5) & 63);
  const int bl = mix(a & 31, b & 31);
  return static_cast<uint16_t>((r << 11) | (g << 5) | bl);
}

struct Point {
  int16_t x = 0;
  int16_t y = 0;
};

// Moves along waypoints p0 -> p1 -> ... with every segment taking msPerSegment and easing in and
// out on its own, so the piece settles briefly on each landing square.
class PathTween {
 public:
  static constexpr int kMaxPoints = 11;  // origin plus up to 10 landings (kMaxPathLen)

  PathTween() = default;
  PathTween(const Point* pts, int n, uint32_t msPerSegment) { set(pts, n, msPerSegment); }

  void set(const Point* pts, int n, uint32_t msPerSegment) {
    n_ = n < 0 ? 0 : (n > kMaxPoints ? kMaxPoints : n);
    for (int i = 0; i < n_; ++i) pts_[i] = pts[i];
    msPerSeg_ = msPerSegment ? msPerSegment : 1;
  }

  int points() const { return n_; }
  int segments() const { return n_ > 1 ? n_ - 1 : 0; }
  uint32_t duration() const { return static_cast<uint32_t>(segments()) * msPerSeg_; }
  bool done(uint32_t t) const { return t >= duration(); }

  // Overall progress 0..1 (monotonic in t); 1 for an empty or single-point path.
  float progress(uint32_t t) const {
    const uint32_t d = duration();
    if (d == 0 || t >= d) return 1.0f;
    int seg;
    const float f = fraction(t, seg);
    return (static_cast<float>(seg) + f) / static_cast<float>(segments());
  }

  // Interpolated position at time t (ms since the start). *segment receives the current
  // 0-based segment (the last one once done, 0 for a path without segments).
  Point at(uint32_t t, int* segment = nullptr) const {
    if (n_ == 0) {
      if (segment) *segment = 0;
      return Point{};
    }
    if (segments() == 0 || done(t)) {
      if (segment) *segment = segments() > 0 ? segments() - 1 : 0;
      return pts_[n_ - 1];
    }
    int seg;
    const float e = easeInOutQuad(fraction(t, seg));
    if (segment) *segment = seg;
    const Point& a = pts_[seg];
    const Point& b = pts_[seg + 1];
    return Point{lerp(a.x, b.x, e), lerp(a.y, b.y, e)};
  }

 private:
  // Linear fraction within the current segment; seg receives its index.
  float fraction(uint32_t t, int& seg) const {
    seg = static_cast<int>(t / msPerSeg_);
    return static_cast<float>(t % msPerSeg_) / static_cast<float>(msPerSeg_);
  }
  static int16_t lerp(int16_t a, int16_t b, float e) {
    const float d = static_cast<float>(b - a) * e;
    return static_cast<int16_t>(a + static_cast<int>(d < 0 ? d - 0.5f : d + 0.5f));
  }

  Point pts_[kMaxPoints];
  int n_ = 0;
  uint32_t msPerSeg_ = 1;
};

}  // namespace checkers::ui
