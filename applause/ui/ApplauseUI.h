// Umbrella header for Applause's UI layer: graphics, frames, events, and windowing.

#pragma once

#include <applause/graphics/Animation.h>
#include <applause/graphics/Canvas.h>
#include <applause/graphics/Color.h>
#include <applause/graphics/Font.h>
#include <applause/graphics/Gradient.h>
#include <applause/graphics/Image.h>
#include <applause/graphics/Palette.h>
#include <applause/graphics/Path.h>
#include <applause/graphics/Text.h>
#include <applause/graphics/Theme.h>
#include <applause/ui/ApplicationWindow.h>
#include <applause/ui/Events.h>
#include <applause/ui/Frame.h>
#include <applause/ui/Layout.h>
#include <applause/ui/ScrollBar.h>
#include <applause/ui/SvgFrame.h>
#include <applause/ui/UndoHistory.h>
#include <applause/util/Defines.h>
#include <applause/util/Dimension.h>
#include <applause/util/EmbeddedFile.h>
#include <applause/util/Events.h>
#include <applause/util/Space.h>
#include <applause/util/StringUtils.h>
#include <applause/windowing/Windowing.h>

// Last, because TextEditor.h includes this header.
#include <applause/ui/components/TextEditor.h>
