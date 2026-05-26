// Alias header. Re-exports Visage symbols under `applause::` so that we aren't mixing namespaces...
// Some aliases are transparent (Canvas, Brush,
// MouseEvent, ...) and resolve to the same type as their Visage counterpart;
// others are subclasses defined elsewhere in `applause::` (Button, Knob, ...).
// generally, you should use the applause::XXX header if available; for example,the applause::Button
// is a little different than the built-in visage::Button, but either namespace works.

#pragma once

#include <visage_app/application_window.h>
#include <visage_file_embed/embedded_file.h>
#include <visage_graphics/animation.h>
#include <visage_graphics/canvas.h>
#include <visage_graphics/color.h>
#include <visage_graphics/font.h>
#include <visage_graphics/gradient.h>
#include <visage_graphics/image.h>
#include <visage_graphics/palette.h>
#include <visage_graphics/path.h>
#include <visage_graphics/text.h>
#include <visage_graphics/theme.h>
#include <visage_ui/events.h>
#include <visage_ui/frame.h>
#include <visage_ui/layout.h>
#include <visage_ui/scroll_bar.h>
#include <visage_ui/svg_frame.h>
#include <visage_ui/undo_history.h>
#include <visage_utils/defines.h>
#include <visage_utils/dimension.h>
#include <visage_utils/events.h>
#include <visage_utils/space.h>
#include <visage_utils/string_utils.h>
#include <visage_widgets/text_editor.h>
#include <visage_windowing/windowing.h>

namespace applause {

using Canvas   = visage::Canvas;
using Brush    = visage::Brush;
using Color    = visage::Color;
using Gradient = visage::Gradient;
using Font     = visage::Font;
using Text     = visage::Text;
using Path     = visage::Path;
using Svg      = visage::Svg;
using SvgFrame = visage::SvgFrame;
using Palette  = visage::Palette;

using Point     = visage::Point;
using Bounds    = visage::Bounds;
using Dimension = visage::Dimension;
using Layout    = visage::Layout;

using Frame             = visage::Frame;
using ScrollableFrame   = visage::ScrollableFrame;
using ApplicationWindow = visage::ApplicationWindow;
using EventTimer        = visage::EventTimer;
using MouseEvent        = visage::MouseEvent;
using MouseCursor       = visage::MouseCursor;

using TextEditor = visage::TextEditor;
using String     = visage::String;

using EmbeddedFile = visage::EmbeddedFile;

using UndoableAction = visage::UndoableAction;

template <typename T>
using Animation = visage::Animation<T>;

template <typename Signature>
using CallbackList = visage::CallbackList<Signature>;

// Free functions
using visage::runOnEventThread;
using visage::setCursorStyle;

// Dimension literals (px, vw, vh, etc.). Plugin code can write
//     using namespace applause::dimension;
// and pick up Visage's UDLs without naming visage:: directly.
namespace dimension {
using namespace visage::dimension;
}

}  // namespace applause

// Theme macros
#define APPLAUSE_THEME_DEFINE_COLOR(name) VISAGE_THEME_DEFINE_COLOR(name)
#define APPLAUSE_THEME_IMPLEMENT_COLOR(container, color, default_color) \
    VISAGE_THEME_IMPLEMENT_COLOR(container, color, default_color)
#define APPLAUSE_THEME_DEFINE_VALUE(name) VISAGE_THEME_DEFINE_VALUE(name)
#define APPLAUSE_THEME_IMPLEMENT_VALUE(container, value, default_value) \
    VISAGE_THEME_IMPLEMENT_VALUE(container, value, default_value)
#define APPLAUSE_LEAK_CHECKER(className) VISAGE_LEAK_CHECKER(className)
