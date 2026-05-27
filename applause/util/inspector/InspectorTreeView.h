#pragma once

#ifndef NDEBUG

#include "CollapsibleTreeView.h"

namespace applause::inspector {

class InspectorWindow;

// Tree view of the live frame hierarchy. Thin adapter over
// CollapsibleTreeView — the row behavior (drawing, click → select / toggle,
// children = frame children) lives in FrameTreeRow inside the .cpp.
class InspectorTreeView : public CollapsibleTreeView {
public:
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseInspectorRowSelectedBackground);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseInspectorRowHoverBackground);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseInspectorRowText);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseInspectorRowTextMuted);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseInspectorRowAccent);

    static constexpr float kRowHeightDefault = 20.0f;
    static constexpr float kIndentPx = 14.0f;
    static constexpr float kTriangleSize = 6.0f;
    static constexpr float kPaddingX = 6.0f;
    static constexpr float kLabelFontSize = 11.0f;

    explicit InspectorTreeView(InspectorWindow& window);
    ~InspectorTreeView() override;

private:
    InspectorWindow& window_;
};

}  // namespace applause::inspector

#endif  // NDEBUG
