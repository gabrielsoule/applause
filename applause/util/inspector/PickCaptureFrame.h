#pragma once

#ifndef NDEBUG

#include <applause/ui/ApplauseUI.h>

namespace applause::inspector {

class InspectorWindow;

// Transient, fully transparent Frame added to the editor only while pick mode
// is active. Relays mouse-move / mouse-down to its owning InspectorWindow so
// the user can click a widget in the main editor to inspect it. Draws nothing.
class PickCaptureFrame : public applause::Frame {
public:
    explicit PickCaptureFrame(InspectorWindow& window);

private:
    InspectorWindow& window_;
};

}  // namespace applause::inspector

#endif  // NDEBUG
