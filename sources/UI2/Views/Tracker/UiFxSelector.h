#pragma once

#include "UI2/Chrome/UiChromeRenderer.h"
#include "UI2/Scene/UiFrameScene.h"
#include "UI2/Scene/UiSceneBuilder.h"

namespace ui2 {
inline constexpr std::array<std::string_view, 27> FxCommands{
    "---", "ARP", "CSH", "DLY", "FCT", "FLT", "FRS", "GOF", "GRV",
    "HOP", "IRT", "KIL", "LEG", "LOF", "MCC", "MCH", "MPC", "PAN",
    "PFT", "POF", "PSL", "RTG", "STP", "TBL", "TPO", "VEL", "VOL"};
inline RectI16 FxSelectorCursorRect(std::string_view selected) {
  for (std::size_t i = 0; i < FxCommands.size(); ++i) {
    if (FxCommands[i] == selected)
      return {static_cast<std::int16_t>(8 + (i % 5) * 45),
              static_cast<std::int16_t>(51 + (i / 5) * 25), 41, 19};
  }
  return {};
}
inline UiBuildStatus BuildFxSelector(
    std::string_view selected, bool table, const UiBottomBarModel &help,
    UiPowerState power, std::string_view elapsed, UiFrameScene &scene,
    RectI16 cursor = {}, bool cursorOverride = false, bool inkVisible = true) {
  scene.Clear();
  scene.topHeight = 34;
  scene.bottomTop = 208;
  scene.topBackground = UiColorToken::SurfaceTopBar;
  scene.bottomBackground = UiColorToken::SurfaceBottomBar;
  auto status = UiChromeRenderer::BuildTop(
      {.title = "FX SELECT", .elapsed = elapsed, .power = power}, scene.top);
  if (status != UiBuildStatus::Built)
    return status;
  status = UiChromeRenderer::BuildBottom(help, scene.bottom);
  if (status != UiBuildStatus::Built)
    return status;
  UiSceneBuilder<256, 1024> builder(scene.content);
  builder.Selection(cursorOverride ? cursor : FxSelectorCursorRect(selected));
  for (std::size_t i = 0; i < FxCommands.size(); ++i) {
    const auto x = static_cast<std::int16_t>(9 + (i % 5) * 45);
    const auto y = static_cast<std::int16_t>(57 + (i / 5) * 25);
    const bool active = FxCommands[i] == selected;
    builder.CenteredText(FxCommands[i], x + 19, y,
                         active && inkVisible ? UiColorToken::TextHighlighted
                         : table && FxCommands[i] == "TBL"
                             ? UiColorToken::DerivedTextFaint
                             : UiColorToken::TextNormal);
  }
  return builder.Ok() ? UiBuildStatus::Built : UiBuildStatus::CommandOverflow;
}
} // namespace ui2
