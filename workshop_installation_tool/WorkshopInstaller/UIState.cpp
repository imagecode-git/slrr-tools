#include "UIState.h"

std::atomic<bool> UIState::NeedsActionButtonUpdate{ false };
std::atomic<bool> UIState::NeedsDescriptionUpdate{ false };
std::atomic<bool> UIState::NeedsDataGridUpdate{ false };
std::atomic<bool> UIState::NeedsGamePathLabelUpdate{ false };
std::atomic<bool> UIState::NeedsFullUpdate{ false };