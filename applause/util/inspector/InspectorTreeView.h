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
    static inline const ColorToken kRowAccent{"inspectorTreeView.rowAccent", 0xff66bbff};
    static inline const ColorToken kRowSelectedBackground{"inspectorTreeView.rowSelectedBackground", kRowAccent, 0.27f};
    static inline const ColorToken kRowHoverBackground{"inspectorTreeView.rowHoverBackground", 0x22ffffff};
    static inline const ColorToken kRowText{"inspectorTreeView.rowText", 0xffd8d8dc};
    static inline const ColorToken kRowTextMuted{"inspectorTreeView.rowTextMuted", 0xff7a7a80};

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
