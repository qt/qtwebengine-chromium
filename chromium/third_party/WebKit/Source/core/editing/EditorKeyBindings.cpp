/*
 * Copyright (C) 2006, 2007 Apple, Inc.  All rights reserved.
 * Copyright (C) 2012 Google, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/editing/Editor.h"

#include "core/events/KeyboardEvent.h"
#include "core/frame/Frame.h"
#include "core/page/EditorClient.h"
#include "platform/KeyboardCodes.h"
#include "platform/PlatformKeyboardEvent.h"

namespace WebCore {

//
// The below code was adapted from the WebKit file webview.cpp
//

static const unsigned CtrlKey = 1 << 0;
static const unsigned AltKey = 1 << 1;
static const unsigned ShiftKey = 1 << 2;
static const unsigned MetaKey = 1 << 3;
#if OS(MACOSX)
// Aliases for the generic key defintions to make kbd shortcuts definitions more
// readable on OS X.
static const unsigned OptionKey  = AltKey;

// Do not use this constant for anything but cursor movement commands. Keys
// with cmd set have their |isSystemKey| bit set, so chances are the shortcut
// will not be executed. Another, less important, reason is that shortcuts
// defined in the renderer do not blink the menu item that they triggered. See
// http://crbug.com/25856 and the bugs linked from there for details.
static const unsigned CommandKey = MetaKey;
#endif

// Keys with special meaning. These will be delegated to the editor using
// the execCommand() method
struct KeyDownEntry {
    unsigned virtualKey;
    unsigned modifiers;
    const char* name;
};

struct KeyPressEntry {
    unsigned charCode;
    unsigned modifiers;
    const char* name;
};

// Key bindings with command key on Mac and alt key on other platforms are
// marked as system key events and will be ignored (with the exception
// of Command-B and Command-I) so they shouldn't be added here.
static const KeyDownEntry keyDownEntries[] = {
    { VKEY_LEFT,   0,                  "MoveLeft"                             },
    { VKEY_LEFT,   ShiftKey,           "MoveLeftAndModifySelection"           },
#if OS(MACOSX)
    { VKEY_LEFT,   OptionKey,          "MoveWordLeft"                         },
    { VKEY_LEFT,   OptionKey | ShiftKey,
        "MoveWordLeftAndModifySelection"                                      },
#else
    { VKEY_LEFT,   CtrlKey,            "MoveWordLeft"                         },
    { VKEY_LEFT,   CtrlKey | ShiftKey,
        "MoveWordLeftAndModifySelection"                                      },
#endif
    { VKEY_RIGHT,  0,                  "MoveRight"                            },
    { VKEY_RIGHT,  ShiftKey,           "MoveRightAndModifySelection"          },
#if OS(MACOSX)
    { VKEY_RIGHT,  OptionKey,          "MoveWordRight"                        },
    { VKEY_RIGHT,  OptionKey | ShiftKey, "MoveWordRightAndModifySelection"    },
#else
    { VKEY_RIGHT,  CtrlKey,            "MoveWordRight"                        },
    { VKEY_RIGHT,  CtrlKey | ShiftKey, "MoveWordRightAndModifySelection"      },
#endif
    { VKEY_UP,     0,                  "MoveUp"                               },
    { VKEY_UP,     ShiftKey,           "MoveUpAndModifySelection"             },
    { VKEY_PRIOR,  ShiftKey,           "MovePageUpAndModifySelection"         },
    { VKEY_DOWN,   0,                  "MoveDown"                             },
    { VKEY_DOWN,   ShiftKey,           "MoveDownAndModifySelection"           },
    { VKEY_NEXT,   ShiftKey,           "MovePageDownAndModifySelection"       },
#if !OS(MACOSX)
    { VKEY_UP,     CtrlKey,            "MoveParagraphBackward"                },
    { VKEY_UP,     CtrlKey | ShiftKey, "MoveParagraphBackwardAndModifySelection" },
    { VKEY_DOWN,   CtrlKey,            "MoveParagraphForward"                },
    { VKEY_DOWN,   CtrlKey | ShiftKey, "MoveParagraphForwardAndModifySelection" },
    { VKEY_PRIOR,  0,                  "MovePageUp"                           },
    { VKEY_NEXT,   0,                  "MovePageDown"                         },
#endif
    { VKEY_HOME,   0,                  "MoveToBeginningOfLine"                },
    { VKEY_HOME,   ShiftKey,
        "MoveToBeginningOfLineAndModifySelection"                             },
#if OS(MACOSX)
    { VKEY_PRIOR,  OptionKey,          "MovePageUp"                           },
    { VKEY_NEXT,   OptionKey,          "MovePageDown"                         },
#endif
#if !OS(MACOSX)
    { VKEY_HOME,   CtrlKey,            "MoveToBeginningOfDocument"            },
    { VKEY_HOME,   CtrlKey | ShiftKey,
        "MoveToBeginningOfDocumentAndModifySelection"                         },
#endif
    { VKEY_END,    0,                  "MoveToEndOfLine"                      },
    { VKEY_END,    ShiftKey,           "MoveToEndOfLineAndModifySelection"    },
#if !OS(MACOSX)
    { VKEY_END,    CtrlKey,            "MoveToEndOfDocument"                  },
    { VKEY_END,    CtrlKey | ShiftKey,
        "MoveToEndOfDocumentAndModifySelection"                               },
#endif
    { VKEY_BACK,   0,                  "DeleteBackward"                       },
    { VKEY_BACK,   ShiftKey,           "DeleteBackward"                       },
    { VKEY_DELETE, 0,                  "DeleteForward"                        },
#if OS(MACOSX)
    { VKEY_BACK,   OptionKey,          "DeleteWordBackward"                   },
    { VKEY_DELETE, OptionKey,          "DeleteWordForward"                    },
#else
    { VKEY_BACK,   CtrlKey,            "DeleteWordBackward"                   },
    { VKEY_DELETE, CtrlKey,            "DeleteWordForward"                    },
#endif
#if OS(MACOSX)
    { 'B',         CommandKey,         "ToggleBold"                           },
    { 'I',         CommandKey,         "ToggleItalic"                         },
#else
    { 'B',         CtrlKey,            "ToggleBold"                           },
    { 'I',         CtrlKey,            "ToggleItalic"                         },
#endif
    { 'U',         CtrlKey,            "ToggleUnderline"                      },
    { VKEY_ESCAPE, 0,                  "Cancel"                               },
    { VKEY_OEM_PERIOD, CtrlKey,        "Cancel"                               },
    { VKEY_TAB,    0,                  "InsertTab"                            },
    { VKEY_TAB,    ShiftKey,           "InsertBacktab"                        },
    { VKEY_RETURN, 0,                  "InsertNewline"                        },
    { VKEY_RETURN, CtrlKey,            "InsertNewline"                        },
    { VKEY_RETURN, AltKey,             "InsertNewline"                        },
    { VKEY_RETURN, AltKey | ShiftKey,  "InsertNewline"                        },
    { VKEY_RETURN, ShiftKey,           "InsertLineBreak"                      },
    { VKEY_INSERT, CtrlKey,            "Copy"                                 },
    { VKEY_INSERT, ShiftKey,           "Paste"                                },
    { VKEY_DELETE, ShiftKey,           "Cut"                                  },
#if !OS(MACOSX)
    // On OS X, we pipe these back to the browser, so that it can do menu item
    // blinking.
    { 'C',         CtrlKey,            "Copy"                                 },
    { 'V',         CtrlKey,            "Paste"                                },
    { 'V',         CtrlKey | ShiftKey, "PasteAndMatchStyle"                   },
    { 'X',         CtrlKey,            "Cut"                                  },
    { 'A',         CtrlKey,            "SelectAll"                            },
    { 'Z',         CtrlKey,            "Undo"                                 },
    { 'Z',         CtrlKey | ShiftKey, "Redo"                                 },
    { 'Y',         CtrlKey,            "Redo"                                 },
#endif
    { VKEY_INSERT, 0,                  "OverWrite"                            },
};

static const KeyPressEntry keyPressEntries[] = {
    { '\t',   0,                  "InsertTab"                                 },
    { '\t',   ShiftKey,           "InsertBacktab"                             },
    { '\r',   0,                  "InsertNewline"                             },
    { '\r',   CtrlKey,            "InsertNewline"                             },
    { '\r',   ShiftKey,           "InsertLineBreak"                           },
    { '\r',   AltKey,             "InsertNewline"                             },
    { '\r',   AltKey | ShiftKey,  "InsertNewline"                             },
};

const char* Editor::interpretKeyEvent(const KeyboardEvent* evt)
{
    const PlatformKeyboardEvent* keyEvent = evt->keyEvent();
    if (!keyEvent)
        return "";

    static HashMap<int, const char*>* keyDownCommandsMap = 0;
    static HashMap<int, const char*>* keyPressCommandsMap = 0;

    if (!keyDownCommandsMap) {
        keyDownCommandsMap = new HashMap<int, const char*>;
        keyPressCommandsMap = new HashMap<int, const char*>;

        for (unsigned i = 0; i < arraysize(keyDownEntries); i++) {
            keyDownCommandsMap->set(keyDownEntries[i].modifiers << 16 | keyDownEntries[i].virtualKey, keyDownEntries[i].name);
        }

        for (unsigned i = 0; i < arraysize(keyPressEntries); i++) {
            keyPressCommandsMap->set(keyPressEntries[i].modifiers << 16 | keyPressEntries[i].charCode, keyPressEntries[i].name);
        }
    }

    unsigned modifiers = 0;
    if (keyEvent->shiftKey())
        modifiers |= ShiftKey;
    if (keyEvent->altKey())
        modifiers |= AltKey;
    if (keyEvent->ctrlKey())
        modifiers |= CtrlKey;
    if (keyEvent->metaKey())
        modifiers |= MetaKey;

    if (keyEvent->type() == PlatformEvent::RawKeyDown) {
        int mapKey = modifiers << 16 | evt->keyCode();
        return mapKey ? keyDownCommandsMap->get(mapKey) : 0;
    }

    int mapKey = modifiers << 16 | evt->charCode();
    return mapKey ? keyPressCommandsMap->get(mapKey) : 0;
}

bool Editor::handleEditingKeyboardEvent(KeyboardEvent* evt)
{
    const PlatformKeyboardEvent* keyEvent = evt->keyEvent();
    // do not treat this as text input if it's a system key event
    if (!keyEvent || keyEvent->isSystemKey())
        return false;

    String commandName = interpretKeyEvent(evt);
    Command command = this->command(commandName);

    if (keyEvent->type() == PlatformEvent::RawKeyDown) {
        // WebKit doesn't have enough information about mode to decide how
        // commands that just insert text if executed via Editor should be treated,
        // so we leave it upon WebCore to either handle them immediately
        // (e.g. Tab that changes focus) or let a keypress event be generated
        // (e.g. Tab that inserts a Tab character, or Enter).
        if (command.isTextInsertion() || commandName.isEmpty())
            return false;
        if (command.execute(evt)) {
            client().didExecuteCommand(commandName);
            return true;
        }
        return false;
    }

    if (command.execute(evt)) {
        client().didExecuteCommand(commandName);
        return true;
    }

    // Here we need to filter key events.
    // On Gtk/Linux, it emits key events with ASCII text and ctrl on for ctrl-<x>.
    // In Webkit, EditorClient::handleKeyboardEvent in
    // WebKit/gtk/WebCoreSupport/EditorClientGtk.cpp drop such events.
    // On Mac, it emits key events with ASCII text and meta on for Command-<x>.
    // These key events should not emit text insert event.
    // Alt key would be used to insert alternative character, so we should let
    // through. Also note that Ctrl-Alt combination equals to AltGr key which is
    // also used to insert alternative character.
    // http://code.google.com/p/chromium/issues/detail?id=10846
    // Windows sets both alt and meta are on when "Alt" key pressed.
    // http://code.google.com/p/chromium/issues/detail?id=2215
    // Also, we should not rely on an assumption that keyboards don't
    // send ASCII characters when pressing a control key on Windows,
    // which may be configured to do it so by user.
    // See also http://en.wikipedia.org/wiki/Keyboard_Layout
    // FIXME(ukai): investigate more detail for various keyboard layout.
    if (evt->keyEvent()->text().length() == 1) {
        UChar ch = evt->keyEvent()->text()[0U];

        // Don't insert null or control characters as they can result in
        // unexpected behaviour
        if (ch < ' ')
            return false;
#if !OS(WIN)
        // Don't insert ASCII character if ctrl w/o alt or meta is on.
        // On Mac, we should ignore events when meta is on (Command-<x>).
        if (ch < 0x80) {
            if (evt->keyEvent()->ctrlKey() && !evt->keyEvent()->altKey())
                return false;
#if OS(MACOSX)
            if (evt->keyEvent()->metaKey())
            return false;
#endif
        }
#endif
    }

    if (!canEdit())
        return false;

    return insertText(evt->keyEvent()->text(), evt);
}

void Editor::handleKeyboardEvent(KeyboardEvent* evt)
{
    // Give the embedder a chance to handle the keyboard event.
    if (client().handleKeyboardEvent() || handleEditingKeyboardEvent(evt))
        evt->setDefaultHandled();
}

} // namesace WebCore
