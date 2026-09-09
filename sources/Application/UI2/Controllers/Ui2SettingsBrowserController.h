/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 PicoTracker contributors
 */

#pragma once

#include "Application/Input/ITrackerInputSink.h"
#include "Application/Persistency/PersistenceConstants.h"
#include "Application/UI2/Controllers/Ui2ControllerPrimitives.h"
#include "Application/Views/Ui2BrowserSnapshot.h"
#include "System/FileSystem/FileSystem.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>

namespace ui2 {

enum class Ui2SettingsBrowserMode : std::uint8_t { None, Theme };

enum class Ui2SettingsBrowserCommandType : std::uint8_t {
  None,
  Back,
  ImportTheme,
};

struct Ui2SettingsBrowserCommand {
  Ui2SettingsBrowserCommandType type = Ui2SettingsBrowserCommandType::None;
  std::array<char, MAX_THEME_NAME_LENGTH + 5U> theme{};
};

// Theme uses the approved Browser renderer primitive while remaining separate
// from Project lifecycle semantics. Font browsing is not represented here
// until real NPF discovery and parsing exist. Storage is fixed-capacity and
// Handle/Snapshot allocate no heap memory.
class Ui2SettingsBrowserController {
public:
  bool OpenTheme(const char *currentTheme) {
    Reset(Ui2SettingsBrowserMode::Theme);
    FileSystem *fileSystem = FileSystem::GetInstance();
    if (fileSystem == nullptr || !fileSystem->chdir(THEMES_DIR)) {
      SetError("THEME DIRECTORY UNAVAILABLE");
      return false;
    }
    if (!Refresh())
      return false;
    if (currentTheme != nullptr && currentTheme[0] != '\0') {
      char candidate[Ui2BrowserSnapshot::ItemTextCapacity]{};
      for (std::uint16_t index = 0U; index < themeCount_; ++index) {
        ReadThemeName(index, candidate, sizeof(candidate));
        const std::size_t length = std::strlen(currentTheme);
        if (std::strncmp(candidate, currentTheme, length) == 0 &&
            (candidate[length] == '\0' ||
             std::strcmp(candidate + length, THEME_FILE_EXTENSION) == 0)) {
          selected_ = index;
          break;
        }
      }
    }
    KeepSelectionVisible();
    activeAction_ = themeCount_ == 0U ? 0U : 1U;
    return true;
  }

  void Close() { Reset(Ui2SettingsBrowserMode::None); }

  [[nodiscard]] Ui2SettingsBrowserMode Mode() const { return mode_; }
  [[nodiscard]] bool Active() const {
    return mode_ != Ui2SettingsBrowserMode::None;
  }

  void SetError(const char *message) {
    error_.fill('\0');
    if (message != nullptr)
      std::snprintf(error_.data(), error_.size(), "%s", message);
  }

  Ui2SettingsBrowserCommand Handle(TrackerAction action, bool pressed) {
    if (!input_.Update(action, pressed) || !pressed)
      return {};
    if (!Active())
      return {};
    if (action == TrackerAction::Left && input_.Held(TrackerAction::Option)) {
      Navigate("..");
      return {};
    }
    if (action == TrackerAction::Up) {
      selected_ = Ui2MoveListIndex(
          selected_, ItemCount(), input_.Held(TrackerAction::Option) ? -8 : -1);
      SelectionChanged();
    } else if (action == TrackerAction::Down) {
      selected_ = Ui2MoveListIndex(selected_, ItemCount(),
                                   input_.Held(TrackerAction::Option) ? 8 : 1);
      SelectionChanged();
    } else if (action == TrackerAction::Left) {
      activeAction_ = 0U;
    } else if (action == TrackerAction::Right && ItemCount() != 0U) {
      activeAction_ = 1U;
    } else if (action == TrackerAction::Enter) {
      if (activeAction_ == 0U || ItemCount() == 0U)
        return {.type = Ui2SettingsBrowserCommandType::Back};
      if (mode_ == Ui2SettingsBrowserMode::Theme) {
        if (IsDirectory(selected_)) {
          char name[PFILENAME_SIZE]{};
          ReadThemeName(selected_, name, sizeof(name));
          Navigate(name);
          return {};
        }
        Ui2SettingsBrowserCommand command{
            .type = Ui2SettingsBrowserCommandType::ImportTheme};
        ReadThemeName(selected_, command.theme.data(), command.theme.size());
        if (command.theme[0] == '\0')
          command.type = Ui2SettingsBrowserCommandType::None;
        return command;
      }
    }
    return {};
  }

  [[nodiscard]] Ui2BrowserSnapshot Snapshot() const {
    Ui2BrowserSnapshot snapshot;
    Ui2BrowserSnapshot::CopyText(snapshot.title, "THEMES");
    snapshot.ConfigureWindow(ItemCount(), selected_, top_);
    for (std::uint8_t row = 0U; row < snapshot.visibleItemCount; ++row) {
      const std::uint16_t item =
          static_cast<std::uint16_t>(snapshot.topIndex + row);
      char name[Ui2BrowserSnapshot::ItemTextCapacity]{};
      ReadThemeName(item, name, sizeof(name));
      Ui2BrowserSnapshot::CopyText(snapshot.items[row], name);
    }

    if (error_[0] != '\0') {
      Ui2BrowserSnapshot::CopyText(snapshot.footer, error_.data());
    } else {
      std::snprintf(snapshot.footer.data(), snapshot.footer.size(), "%u ITEM%s",
                    static_cast<unsigned>(ItemCount()),
                    ItemCount() == 1U ? "" : "S");
    }

    Ui2BrowserSnapshot::CopyText(snapshot.actions[0], "CANCEL");
    snapshot.actionCount = 1U;
    if (snapshot.hasSelection) {
      Ui2BrowserSnapshot::CopyText(snapshot.actions[1],
                                   IsDirectory(selected_) ? "OPEN" : "IMPORT");
      snapshot.actionCount = 2U;
    }
    snapshot.activeAction =
        std::min<std::uint8_t>(activeAction_, snapshot.actionCount - 1U);
    return snapshot;
  }

private:
  bool IsDirectory(std::uint16_t index) const {
    auto *fs = FileSystem::GetInstance();
    return fs && index < themeCount_ &&
           fs->getFileType(themeIndices_[index]) == PFT_DIR;
  }
  bool Refresh() {
    themeCount_ = selected_ = top_ = 0;
    auto *fs = FileSystem::GetInstance();
    etl::vector<int, MAX_FILE_INDEX_SIZE> listed;
    if (!fs || !fs->listBrowserChecked(&listed, THEME_FILE_EXTENSION)) {
      SetError("THEME BROWSER UNAVAILABLE");
      return false;
    }
    for (int index : listed) {
      char name[PFILENAME_SIZE]{};
      fs->getFileName(index, name, sizeof(name));
      if (std::strcmp(name, ".") == 0 ||
          (fs->isCurrentRoot() && std::strcmp(name, "..") == 0))
        continue;
      if (themeCount_ < themeIndices_.size())
        themeIndices_[themeCount_++] = index;
    }
    SelectionChanged();
    return true;
  }
  void Navigate(const char *name) {
    auto *fs = FileSystem::GetInstance();
    if (!fs || (std::strcmp(name, "..") == 0 && fs->isCurrentRoot()))
      return;
    if (!fs->chdir(name)) {
      SetError("CANNOT OPEN DIRECTORY");
      return;
    }
    Refresh();
  }
  void Reset(Ui2SettingsBrowserMode mode) {
    mode_ = mode;
    themeCount_ = 0U;
    selected_ = 0U;
    top_ = 0U;
    input_ = {};
    activeAction_ = 0U;
    error_.fill('\0');
  }

  void SelectionChanged() {
    error_.fill('\0');
    activeAction_ = ItemCount() == 0U ? 0U : 1U;
    KeepSelectionVisible();
  }

  void KeepSelectionVisible() {
    top_ = Ui2BrowserSnapshot::ResolveWindowTop(ItemCount(), selected_, top_);
  }

  [[nodiscard]] std::uint16_t ItemCount() const { return themeCount_; }

  void ReadThemeName(std::uint16_t index, char *destination,
                     std::size_t capacity) const {
    if (destination == nullptr || capacity == 0U)
      return;
    destination[0] = '\0';
    if (index >= themeCount_)
      return;
    FileSystem *fileSystem = FileSystem::GetInstance();
    if (fileSystem == nullptr)
      return;
    fileSystem->getFileName(themeIndices_[index], destination,
                            static_cast<int>(capacity));
    destination[capacity - 1U] = '\0';
  }

  std::array<int, MAX_FILE_INDEX_SIZE> themeIndices_{};
  std::array<char, 32> error_{};
  std::uint16_t themeCount_ = 0U;
  std::uint16_t selected_ = 0U;
  std::uint16_t top_ = 0U;
  Ui2ControllerInputState input_{};
  Ui2SettingsBrowserMode mode_ = Ui2SettingsBrowserMode::None;
  std::uint8_t activeAction_ = 0U;
};

static_assert(std::is_trivially_copyable_v<Ui2SettingsBrowserCommand>);
static_assert(std::is_trivially_copyable_v<Ui2SettingsBrowserController>);
static_assert(sizeof(Ui2SettingsBrowserController) <= 1'100U);

} // namespace ui2
