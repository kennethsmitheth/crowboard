#pragma once
// Enums shared by the services layer (Stats now; later services reuse them).
#include <stdint.h>

namespace checkers {

enum class Difficulty : uint8_t { Easy, Medium, Hard };

// Result of a vs-computer game from the human player's perspective.
enum class Outcome : uint8_t { Win, Loss, Draw };

constexpr uint8_t kDifficultyCount = 3;

}  // namespace checkers
