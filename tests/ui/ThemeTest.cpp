#include <catch2/catch_test_macros.hpp>

#include <applause/core/PluginBase.h>
#include <applause/extensions/ParamsExtension.h>
#include <applause/ui/ApplauseEditor.h>
#include <applause/ui/ApplauseUI.h>
#include <applause/ui/PopupMenu.h>
#include <applause/ui/components/Button.h>
#include <applause/ui/components/Panel.h>
#include <applause/ui/components/ParamKnob.h>
#include <applause/ui/components/SelectionGrid.h>
#include <applause/ui/components/TextEditor.h>

#include <algorithm>

namespace {

using applause::Button;
using applause::Color;
using applause::color;
using applause::ColorToken;
using applause::value;
using applause::ValueToken;

const ColorToken kBase{"test.base", 0xff102030};
const ColorToken kFollower{"test.follower", kBase};
const ColorToken kTint{"test.tint", kBase, 0.25f};
const ColorToken kOther{"test.other", 0xff445566};
const ValueToken kSize{"test.size", 4.0f};
const ValueToken kGap{"test.gap", 9.0f};

const clap_plugin_descriptor_t kDescriptor{};

struct TestPlugin : applause::PluginBase {
    applause::ParamsExtension params{8};

    TestPlugin() : PluginBase(&kDescriptor, nullptr) { registerExtension(params); }
    applause::ProcessStatus process(applause::ProcessContext&) noexcept override {
        return applause::ProcessStatus::Continue;
    }
};

struct ResizeCounter : applause::Frame {
    void resized() override { ++resizes; }
    int resizes = 0;
};

}  // namespace

TEST_CASE("A token reads its default when no frame sets it", "[theme]") {
    applause::Frame frame;
    REQUIRE(color(frame, kBase) == Color(0xff102030));
    REQUIRE(value(frame, kSize) == 4.0f);
}

TEST_CASE("A token set on a frame applies to the frames under it, including later ones", "[theme]") {
    applause::Frame root;
    applause::Frame child;
    applause::Frame outside;
    root.addChild(child);
    root.set(kBase, Color(0xffff0000));

    applause::Frame later;
    child.addChild(later);

    REQUIRE(color(child, kBase) == Color(0xffff0000));
    REQUIRE(color(later, kBase) == Color(0xffff0000));
    REQUIRE(color(outside, kBase) == Color(0xff102030));
}

TEST_CASE("The nearest frame that sets a token decides its value", "[theme]") {
    applause::Frame root;
    applause::Frame child;
    root.addChild(child);
    root.set(kSize, 1.0f);
    child.set(kSize, 2.0f);
    REQUIRE(value(root, kSize) == 1.0f);
    REQUIRE(value(child, kSize) == 2.0f);

    child.unset(kSize);
    REQUIRE(value(child, kSize) == 1.0f);
}

TEST_CASE("Frames that don't set a token pass its lookups through", "[theme]") {
    applause::Frame root;
    applause::Frame middle;
    applause::Frame leaf;
    root.addChild(middle);
    middle.addChild(leaf);
    root.set(kSize, 3.0f);
    middle.set(kGap, 1.0f);
    REQUIRE(value(leaf, kSize) == 3.0f);
}

TEST_CASE("A token that follows another reads it from the same frame", "[theme]") {
    applause::Frame root;
    applause::Frame child;
    root.addChild(child);
    child.set(kBase, Color(0xff00ff00));
    REQUIRE(color(child, kFollower) == Color(0xff00ff00));
    REQUIRE(color(root, kFollower) == Color(0xff102030));

    // Setting the follower itself higher up still wins over its base set lower down.
    root.set(kFollower, Color(0xff0000ff));
    REQUIRE(color(child, kFollower) == Color(0xff0000ff));
}

TEST_CASE("A frame can make a token follow another, read from the same frame", "[theme]") {
    applause::Frame root;
    applause::Frame child;
    root.addChild(child);
    child.set(kBase, kOther);
    child.set(kSize, kGap);
    REQUIRE(color(child, kBase) == Color(0xff445566));
    REQUIRE(value(child, kSize) == 9.0f);
    REQUIRE(color(root, kBase) == Color(0xff102030));

    root.set(kOther, Color(0xffabcdef));
    REQUIRE(color(child, kBase) == Color(0xffabcdef));
    REQUIRE(color(child, kFollower) == Color(0xffabcdef));
}

TEST_CASE("A tint scales the alpha of the color it follows", "[theme]") {
    applause::Frame frame;
    REQUIRE(color(frame, kTint) == Color(0xff102030).withAlpha(0.25f));

    frame.set(kBase, Color(0xff102030).withAlpha(0.5f));
    REQUIRE(color(frame, kTint) == Color(0xff102030).withAlpha(0.125f));
}

TEST_CASE("Button colors follow the accent set above them", "[theme]") {
    applause::Frame root;
    applause::Button button("OK");
    root.addChild(button);
    REQUIRE(color(button, Button::kGlow) == Color(0xff9966ff));

    root.set(applause::theme::kAccent, Color(0xff55ccaa));
    REQUIRE(color(button, Button::kGlow) == Color(0xff55ccaa));
    REQUIRE(color(button, Button::kOn) == Color(0xff55ccaa));
    REQUIRE(color(button, Button::kOnGlow) == Color(0xff55ccaa).withAlpha(0.21f));
}

TEST_CASE("Accent-colored parts have no fixed shades of their own", "[theme]") {
    applause::Frame root;
    applause::Button button("OK");
    applause::TextSelectionGridCell cell("Peak");
    root.addChild(button);
    root.addChild(cell);
    root.set(applause::theme::kAccent, Color(0xffffaa33));

    REQUIRE(color(button, Button::kPrimary) == Color(0xffffaa33));
    REQUIRE(color(cell, applause::SelectionGridCell::kContentSelected) == Color(0xffffaa33));
    REQUIRE(color(cell, applause::SelectionGridCell::kContentSelectedHover) == Color(0xffffaa33));
    REQUIRE(color(root, applause::PopupMenu::kSelection) == Color(0xffffaa33));
}

TEST_CASE("A theme value set on one button leaves the others alone", "[theme]") {
    applause::Frame root;
    applause::Button first("A");
    applause::Button second("B");
    root.addChild(first);
    root.addChild(second);
    first.set(Button::kTextHover, Color(0xffffffff));
    REQUIRE(color(first, Button::kTextHover) == Color(0xffffffff));
    REQUIRE(color(second, Button::kTextHover) == Color(0xffdddddd));
}

TEST_CASE("Setting a theme value lays the frames under it out again", "[theme]") {
    applause::Frame root;
    applause::Panel panel;
    root.addChild(panel);
    panel.setBounds(0.0f, 0.0f, 100.0f, 100.0f);
    REQUIRE(panel.content().x() == 4.0f);

    root.set(applause::Panel::kContentMargin, 10.0f);
    REQUIRE(panel.content().x() == 10.0f);
    REQUIRE(panel.content().width() == 80.0f);

    root.unset(applause::Panel::kContentMargin);
    REQUIRE(panel.content().x() == 4.0f);
}

TEST_CASE("A color change redraws the frames under it without laying them out", "[theme]") {
    applause::Frame root;
    ResizeCounter child;
    root.addChild(child);
    std::vector<applause::Frame*> redrawn;
    applause::FrameEventHandler handler;
    handler.request_redraw = [&](applause::Frame* frame) { redrawn.push_back(frame); };
    root.setEventHandler(&handler);

    root.set(kBase, Color(0xffff0000));
    REQUIRE(std::find(redrawn.begin(), redrawn.end(), &child) != redrawn.end());
    REQUIRE(child.resizes == 0);

    root.set(kSize, 2.0f);
    REQUIRE(child.resizes == 1);
    root.unset(kSize);
    REQUIRE(child.resizes == 2);
}

TEST_CASE("Unsetting a token that isn't set changes nothing", "[theme]") {
    applause::Frame root;
    ResizeCounter child;
    root.addChild(child);
    std::vector<applause::Frame*> redrawn;
    applause::FrameEventHandler handler;
    handler.request_redraw = [&](applause::Frame* frame) { redrawn.push_back(frame); };
    root.setEventHandler(&handler);

    root.unset(kBase);
    root.unset(kSize);
    REQUIRE(redrawn.empty());
    REQUIRE(child.resizes == 0);
}

TEST_CASE("The scroll bar width token moves a scrollable frame's bar", "[theme]") {
    applause::Frame root;
    applause::ScrollableFrame scrollable;
    root.addChild(scrollable);
    scrollable.setBounds(0.0f, 0.0f, 200.0f, 100.0f);
    REQUIRE(scrollable.scrollBar().width() == 20.0f);

    root.set(applause::ScrollBar::kWidth, 12.0f);
    REQUIRE(scrollable.scrollBar().width() == 12.0f);
    REQUIRE(scrollable.scrollBar().x() == 188.0f);
}

TEST_CASE("A text editor's state colors follow its base colors unless set", "[theme]") {
    using applause::TextEditor;
    TextEditor editor;
    editor.set(TextEditor::kBackground, Color(0xff202020));
    REQUIRE(color(editor, TextEditor::kBackgroundHover) == Color(0xff202020));
    REQUIRE(color(editor, TextEditor::kBackgroundFocused) == Color(0xff202020));

    editor.set(TextEditor::kBackgroundHover, Color(0xff303030));
    REQUIRE(color(editor, TextEditor::kBackgroundHover) == Color(0xff303030));
    REQUIRE(color(editor, TextEditor::kBackgroundDisabled) == Color(0xff202020));
}

TEST_CASE("A param knob's name follows the knob's text color in both states", "[theme]") {
    TestPlugin plugin;
    plugin.params.registerParam(applause::ParamConfig{
        .string_id = "gain",
        .name = "Gain",
        .min_value = 0.0f,
        .max_value = 1.0f,
        .default_value = 0.5f,
    });
    applause::Frame root;
    applause::ParamKnob knob(plugin.params.getInfo("gain"));
    root.addChild(knob);
    root.set(applause::ParamKnob::kText, Color(0xff00ff00));

    const auto& children = knob.children();
    const auto name = std::find_if(children.begin(), children.end(),
                                   [](const applause::Frame* child) { return child->name() == "param_name"; });
    REQUIRE(name != children.end());
    REQUIRE(color(**name, applause::TextEditor::kText) == Color(0xff00ff00));
    REQUIRE(color(**name, applause::TextEditor::kTextDisabled) == Color(0xff00ff00));
}

TEST_CASE("The editor's theme values apply to everything in it", "[theme]") {
    applause::ApplauseEditor editor;
    applause::Button button("OK");
    editor.addChild(button);
    editor.set(Button::kRounding, 3.0f);
    REQUIRE(value(button, Button::kRounding) == 3.0f);
    REQUIRE(value(button, Button::kBorderWidth) == 1.5f);
}

TEST_CASE("Every token is listed", "[theme]") {
    const auto& colors = ColorToken::all();
    const auto& values = ValueToken::all();
    REQUIRE(std::find(colors.begin(), colors.end(), &Button::kText) != colors.end());
    REQUIRE(std::find(values.begin(), values.end(), &Button::kRounding) != values.end());
}
