// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_REMOTE_ROOT_WINDOW_HOST_WIN_H_
#define UI_AURA_REMOTE_ROOT_WINDOW_HOST_WIN_H_

#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/remote_input_method_delegate_win.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/metro_viewer/ime_types.h"

namespace base {
class FilePath;
}

namespace ui {
class RemoteInputMethodPrivateWin;
class ViewProp;
}

namespace IPC {
class Message;
class Sender;
}

namespace aura {

typedef base::Callback<void(const base::FilePath&, int, void*)>
    OpenFileCompletion;

typedef base::Callback<void(const std::vector<base::FilePath>&, void*)>
    OpenMultipleFilesCompletion;

typedef base::Callback<void(const base::FilePath&, int, void*)>
    SaveFileCompletion;

typedef base::Callback<void(const base::FilePath&, int, void*)>
    SelectFolderCompletion;

typedef base::Callback<void(void*)> FileSelectionCanceled;

// Handles the open file operation for Metro Chrome Ash. The on_success
// callback passed in is invoked when we receive the opened file name from
// the metro viewer. The on failure callback is invoked on failure.
AURA_EXPORT void HandleOpenFile(const base::string16& title,
                                const base::FilePath& default_path,
                                const base::string16& filter,
                                const OpenFileCompletion& on_success,
                                const FileSelectionCanceled& on_failure);

// Handles the open multiple file operation for Metro Chrome Ash. The
// on_success callback passed in is invoked when we receive the opened file
// names from the metro viewer. The on failure callback is invoked on failure.
AURA_EXPORT void HandleOpenMultipleFiles(
    const base::string16& title,
    const base::FilePath& default_path,
    const base::string16& filter,
    const OpenMultipleFilesCompletion& on_success,
    const FileSelectionCanceled& on_failure);

// Handles the save file operation for Metro Chrome Ash. The on_success
// callback passed in is invoked when we receive the saved file name from
// the metro viewer. The on failure callback is invoked on failure.
AURA_EXPORT void HandleSaveFile(const base::string16& title,
                                const base::FilePath& default_path,
                                const base::string16& filter,
                                int filter_index,
                                const base::string16& default_extension,
                                const SaveFileCompletion& on_success,
                                const FileSelectionCanceled& on_failure);

// Handles the select folder for Metro Chrome Ash. The on_success
// callback passed in is invoked when we receive the folder name from the
// metro viewer. The on failure callback is invoked on failure.
AURA_EXPORT void HandleSelectFolder(const base::string16& title,
                                    const SelectFolderCompletion& on_success,
                                    const FileSelectionCanceled& on_failure);

// Handles the activate desktop command for Metro Chrome Ash.   The |ash_exit|
// parameter indicates whether the Ash process would be shutdown after
// activating the desktop.
AURA_EXPORT void HandleActivateDesktop(
    const base::FilePath& shortcut,
    bool ash_exit);

// Handles the metro exit command.  Notifies the metro viewer to shutdown
// gracefully.
AURA_EXPORT void HandleMetroExit();

// RootWindowHost implementaton that receives events from a different
// process. In the case of Windows this is the Windows 8 (aka Metro)
// frontend process, which forwards input events to this class.
class AURA_EXPORT RemoteRootWindowHostWin
    : public RootWindowHost,
      public ui::internal::RemoteInputMethodDelegateWin {
 public:
  // Returns the only RemoteRootWindowHostWin, if this is the first time
  // this function is called, it will call Create() wiht empty bounds.
  static RemoteRootWindowHostWin* Instance();
  static RemoteRootWindowHostWin* Create(const gfx::Rect& bounds);

  // Called when the remote process has established its IPC connection.
  // The |host| can be used when we need to send a message to it and
  // |remote_window| is the actual window owned by the viewer process.
  void Connected(IPC::Sender* host, HWND remote_window);
  // Called when the remote process has closed its IPC connection.
  void Disconnected();

  // Called when we have a message from the remote process.
  bool OnMessageReceived(const IPC::Message& message);

  void HandleOpenURLOnDesktop(const base::FilePath& shortcut,
                              const base::string16& url);

  // The |ash_exit| parameter indicates whether the Ash process would be
  // shutdown after activating the desktop.
  void HandleActivateDesktop(const base::FilePath& shortcut, bool ash_exit);

  // Notify the metro viewer that it should shut itself down.
  void HandleMetroExit();

  void HandleOpenFile(const base::string16& title,
                      const base::FilePath& default_path,
                      const base::string16& filter,
                      const OpenFileCompletion& on_success,
                      const FileSelectionCanceled& on_failure);

  void HandleOpenMultipleFiles(const base::string16& title,
                               const base::FilePath& default_path,
                               const base::string16& filter,
                               const OpenMultipleFilesCompletion& on_success,
                               const FileSelectionCanceled& on_failure);

  void HandleSaveFile(const base::string16& title,
                      const base::FilePath& default_path,
                      const base::string16& filter,
                      int filter_index,
                      const base::string16& default_extension,
                      const SaveFileCompletion& on_success,
                      const FileSelectionCanceled& on_failure);

  void HandleSelectFolder(const base::string16& title,
                          const SelectFolderCompletion& on_success,
                          const FileSelectionCanceled& on_failure);

  void HandleWindowSizeChanged(uint32 width, uint32 height);

  // Returns the active ASH root window.
  Window* GetAshWindow();

  // Returns true if the remote window is the foreground window according to the
  // OS.
  bool IsForegroundWindow();

 private:
  explicit RemoteRootWindowHostWin(const gfx::Rect& bounds);
  virtual ~RemoteRootWindowHostWin();

  // IPC message handing methods:
  void OnMouseMoved(int32 x, int32 y, int32 flags);
  void OnMouseButton(int32 x,
                     int32 y,
                     int32 extra,
                     ui::EventType type,
                     ui::EventFlags flags);
  void OnKeyDown(uint32 vkey,
                 uint32 repeat_count,
                 uint32 scan_code,
                 uint32 flags);
  void OnKeyUp(uint32 vkey,
               uint32 repeat_count,
               uint32 scan_code,
               uint32 flags);
  void OnChar(uint32 key_code,
              uint32 repeat_count,
              uint32 scan_code,
              uint32 flags);
  void OnWindowActivated();
  void OnTouchDown(int32 x, int32 y, uint64 timestamp, uint32 pointer_id);
  void OnTouchUp(int32 x, int32 y, uint64 timestamp, uint32 pointer_id);
  void OnTouchMoved(int32 x, int32 y, uint64 timestamp, uint32 pointer_id);
  void OnFileSaveAsDone(bool success,
                        const base::FilePath& filename,
                        int filter_index);
  void OnFileOpenDone(bool success, const base::FilePath& filename);
  void OnMultiFileOpenDone(bool success,
                           const std::vector<base::FilePath>& files);
  void OnSelectFolderDone(bool success, const base::FilePath& folder);
  void OnSetCursorPosAck();

  // For Input Method support:
  ui::RemoteInputMethodPrivateWin* GetRemoteInputMethodPrivate();
  void OnImeCandidatePopupChanged(bool visible);
  void OnImeCompositionChanged(
      const string16& text,
      int32 selection_start,
      int32 selection_end,
      const std::vector<metro_viewer::UnderlineInfo>& underlines);
  void OnImeTextCommitted(const string16& text);
  void OnImeInputSourceChanged(uint16 language_id, bool is_ime);

  // RootWindowHost overrides:
  virtual RootWindow* GetRootWindow() OVERRIDE;
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void ToggleFullScreen() OVERRIDE;
  virtual gfx::Rect GetBounds() const OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual gfx::Insets GetInsets() const OVERRIDE;
  virtual void SetInsets(const gfx::Insets& insets) OVERRIDE;
  virtual gfx::Point GetLocationOnNativeScreen() const OVERRIDE;
  virtual void SetCapture() OVERRIDE;
  virtual void ReleaseCapture() OVERRIDE;
  virtual void SetCursor(gfx::NativeCursor cursor) OVERRIDE;
  virtual bool QueryMouseLocation(gfx::Point* location_return) OVERRIDE;
  virtual bool ConfineCursorToRootWindow() OVERRIDE;
  virtual void UnConfineCursor() OVERRIDE;
  virtual void OnCursorVisibilityChanged(bool show) OVERRIDE;
  virtual void MoveCursorTo(const gfx::Point& location) OVERRIDE;
  virtual void PostNativeEvent(const base::NativeEvent& native_event) OVERRIDE;
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE;
  virtual void PrepareForShutdown() OVERRIDE;

  // ui::internal::RemoteInputMethodDelegateWin overrides:
  virtual void CancelComposition() OVERRIDE;
  virtual void OnTextInputClientUpdated(
      const std::vector<int32>& input_scopes,
      const std::vector<gfx::Rect>& composition_character_bounds) OVERRIDE;

  // Helper function to dispatch a keyboard message to the desired target.
  // The default target is the RootWindowHostDelegate. For nested message loop
  // invocations we post a synthetic keyboard message directly into the message
  // loop. The dispatcher for the nested loop would then decide how this
  // message is routed.
  void DispatchKeyboardMessage(ui::EventType type,
                               uint32 vkey,
                               uint32 repeat_count,
                               uint32 scan_code,
                               uint32 flags,
                               bool is_character);

  // Sets the event flags. |flags| is a bitmask of EventFlags. If there is a
  // change the system virtual key state is updated as well. This way if chrome
  // queries for key state it matches that of event being dispatched.
  void SetEventFlags(uint32 flags);

  uint32 mouse_event_flags() const {
    return event_flags_ & (ui::EF_LEFT_MOUSE_BUTTON |
                           ui::EF_MIDDLE_MOUSE_BUTTON |
                           ui::EF_RIGHT_MOUSE_BUTTON);
  }

  uint32 key_event_flags() const {
    return event_flags_ & (ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                           ui::EF_ALT_DOWN | ui::EF_CAPS_LOCK_DOWN);
  }

  HWND remote_window_;
  IPC::Sender* host_;
  scoped_ptr<ui::ViewProp> prop_;

  // Saved callbacks which inform the caller about the result of the open file/
  // save file/select operations.
  OpenFileCompletion file_open_completion_callback_;
  OpenMultipleFilesCompletion multi_file_open_completion_callback_;
  SaveFileCompletion file_saveas_completion_callback_;
  SelectFolderCompletion select_folder_completion_callback_;
  FileSelectionCanceled failure_callback_;

  // Set to true if we need to ignore mouse messages until the SetCursorPos
  // operation is acked by the viewer.
  bool ignore_mouse_moves_until_set_cursor_ack_;

  // Tracking last click event for synthetically generated mouse events.
  scoped_ptr<ui::MouseEvent> last_mouse_click_event_;

  // State of the keyboard/mouse at the time of the last input event. See
  // description of SetEventFlags().
  uint32 event_flags_;

  // Current size of this root window.
  gfx::Size window_size_;

  DISALLOW_COPY_AND_ASSIGN(RemoteRootWindowHostWin);
};

}  // namespace aura

#endif  // UI_AURA_REMOTE_ROOT_WINDOW_HOST_WIN_H_
