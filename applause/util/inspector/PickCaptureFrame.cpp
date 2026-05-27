#include "PickCaptureFrame.h"

#ifndef NDEBUG

#include "InspectorWindow.h"

namespace applause::inspector {

PickCaptureFrame::PickCaptureFrame(InspectorWindow& window) : window_(window) {
    setName("PickCaptureFrame");
    setIgnoresMouseEvents(false, false);
    setOnTop(true);

    // Crosshair cursor advertises pick mode anywhere on the editor. visage's
    // setCursorStyle is an immediate OS-level call, not a stored per-frame
    // setting, so it has to fire on enter/exit. Sibling frames in the editor
    // (e.g. knobs) will restore their own cursor on their own mouseEnter.
    onMouseEnter() += [](const applause::MouseEvent&) {
        applause::setCursorStyle(applause::MouseCursor::Crosshair);
    };
    onMouseExit() += [](const applause::MouseEvent&) {
        applause::setCursorStyle(applause::MouseCursor::Arrow);
    };
    onMouseMove() += [this](const applause::MouseEvent& e) {
        window_.handlePickHover(e.windowPosition());
    };
    onMouseDown() += [this](const applause::MouseEvent& e) {
        if (e.isLeftButton())
            window_.handlePickCommit(e.windowPosition());
    };
}

}  // namespace applause::inspector

#endif  // NDEBUG
