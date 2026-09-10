#include "SelectionGrid.h"

#include <embedded/applause_fonts.h>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace applause {

APPLAUSE_THEME_IMPLEMENT_COLOR(SelectionGridCell, ApplauseSelectionGridCellBackground, 0x00000000);
APPLAUSE_THEME_IMPLEMENT_COLOR(SelectionGridCell, ApplauseSelectionGridCellBackgroundHover, 0x11ffffff);
APPLAUSE_THEME_IMPLEMENT_COLOR(SelectionGridCell, ApplauseSelectionGridCellBackgroundPressed, 0xff1d1d22);
APPLAUSE_THEME_IMPLEMENT_COLOR(SelectionGridCell, ApplauseSelectionGridCellGlowSelected, 0x559966ff);
APPLAUSE_THEME_IMPLEMENT_COLOR(SelectionGridCell, ApplauseSelectionGridCellBorder, 0x00000000);
APPLAUSE_THEME_IMPLEMENT_COLOR(SelectionGridCell, ApplauseSelectionGridCellBorderSelected, 0xff9966ff);
APPLAUSE_THEME_IMPLEMENT_COLOR(SelectionGridCell, ApplauseSelectionGridCellContent, 0xffa0a0a8);
APPLAUSE_THEME_IMPLEMENT_COLOR(SelectionGridCell, ApplauseSelectionGridCellContentHover, 0xffd0d0d8);
APPLAUSE_THEME_IMPLEMENT_COLOR(SelectionGridCell, ApplauseSelectionGridCellContentSelected, 0xffaa77ff);
APPLAUSE_THEME_IMPLEMENT_COLOR(SelectionGridCell, ApplauseSelectionGridCellContentSelectedHover, 0xffc6a6ff);

APPLAUSE_THEME_IMPLEMENT_VALUE(SelectionGridCell, ApplauseSelectionGridCellRounding, 4.0f);
APPLAUSE_THEME_IMPLEMENT_VALUE(SelectionGridCell, ApplauseSelectionGridCellBorderWidth, 1.0f);
APPLAUSE_THEME_IMPLEMENT_VALUE(SelectionGridCell, ApplauseSelectionGridCellContentPadding, 6.0f);
APPLAUSE_THEME_IMPLEMENT_VALUE(SelectionGridCell, ApplauseSelectionGridCellGlowCenterAmount, 0.35f);

APPLAUSE_THEME_IMPLEMENT_COLOR(SelectionGrid, ApplauseSelectionGridBackground, 0xff141418);
APPLAUSE_THEME_IMPLEMENT_COLOR(SelectionGrid, ApplauseSelectionGridBorder, 0xff2a2a2e);
APPLAUSE_THEME_IMPLEMENT_VALUE(SelectionGrid, ApplauseSelectionGridGap, 4.0f);
APPLAUSE_THEME_IMPLEMENT_VALUE(SelectionGrid, ApplauseSelectionGridPadding, 4.0f);
APPLAUSE_THEME_IMPLEMENT_VALUE(SelectionGrid, ApplauseSelectionGridRounding, 8.0f);
APPLAUSE_THEME_IMPLEMENT_VALUE(SelectionGrid, ApplauseSelectionGridBorderWidth, 1.0f);

bool SelectionGridCell::toggle() {
    if (grid_) grid_->setSelectedIndexAndNotify(index_);
    return selected_;
}

void SelectionGridCell::setSelected(bool selected) {
    if (selected_ == selected) return;
    selected_ = selected;
    redraw();
}

void SelectionGridCell::draw(applause::Canvas& canvas) {
    const float hover_amount = isActive() ? hover_amount_.update() : 0.0f;
    const float rounding = canvas.value(ApplauseSelectionGridCellRounding);
    if (isPressed())
        canvas.setColor(ApplauseSelectionGridCellBackgroundPressed);
    else
        canvas.setColor(ApplauseSelectionGridCellBackground);

    canvas.roundedRectangle(0.0f, 0.0f, width(), height(), rounding);

    if (selected_) {
        applause::Color glow = canvas.color(ApplauseSelectionGridCellGlowSelected).gradient().sample(0.0f);
        applause::Color center_glow =
            glow.withAlpha(glow.alpha() * canvas.value(ApplauseSelectionGridCellGlowCenterAmount));
        applause::Point center = {width() * 0.5f, height() * 0.5f};
        canvas.setColor(applause::Brush::radial(center_glow, glow, center, width() * 1.0f, height() * 1.0f));
        canvas.roundedRectangle(0.0f, 0.0f, width(), height(), rounding);
    }

    if (selected_ && !isPressed() && hover_amount > 0.0f) {
        canvas.setColor(canvas.color(ApplauseSelectionGridCellBackgroundHover).withMultipliedAlpha(hover_amount));
        canvas.roundedRectangle(0.0f, 0.0f, width(), height(), rounding);
    }

    canvas.setColor(selected_ ? ApplauseSelectionGridCellBorderSelected : ApplauseSelectionGridCellBorder);
    canvas.roundedRectangleBorder(0.0f, 0.0f, width(), height(), rounding,
                                  canvas.value(ApplauseSelectionGridCellBorderWidth));

    const float padding = std::max(0.0f, canvas.value(ApplauseSelectionGridCellContentPadding));
    canvas.saveState();
    canvas.trimClampBounds(padding, padding, std::max(0.0f, width() - 2.0f * padding),
                           std::max(0.0f, height() - 2.0f * padding));
    drawContent(canvas, hover_amount);
    canvas.restoreState();

    const float hover_glow = hover_amount * canvas.value(ApplauseButtonGlowAmount);
    if (hover_glow > 0.0f) {
        const applause::Color accent = canvas.color(ApplauseButtonGlow).gradient().sample(0.0f);
        const applause::Point center = {width() * 0.5f, height() * 0.5f};
        canvas.setColor(applause::Brush::radial(accent.withAlpha(hover_glow), applause::Color(0x00000000), center,
                                                width() * 0.5f, height() * 0.5f));
        canvas.rectangle(0, 0, width(), height());
    }

    if (hover_amount_.isAnimating()) redraw();
}

TextSelectionGridCell::TextSelectionGridCell(std::string text) :
    text_(text, applause::Font(12, applause::fonts::Barlow_Medium_ttf)) {}

void TextSelectionGridCell::setText(std::string text) {
    text_.setText(text);
    redraw();
}

void TextSelectionGridCell::setFont(const applause::Font& font) {
    text_.setFont(font);
    redraw();
}

void TextSelectionGridCell::drawContent(applause::Canvas& canvas, float hover_amount) {
    const float padding = std::max(0.0f, canvas.value(ApplauseSelectionGridCellContentPadding));
    if (selected())
        canvas.setBlendedColor(ApplauseSelectionGridCellContentSelected, ApplauseSelectionGridCellContentSelectedHover,
                               hover_amount);
    else
        canvas.setBlendedColor(ApplauseSelectionGridCellContent, ApplauseSelectionGridCellContentHover, hover_amount);
    canvas.text(&text_, padding, padding, std::max(0.0f, width() - 2.0f * padding),
                std::max(0.0f, height() - 2.0f * padding));
}

SelectionGrid::SelectionGrid(int columns, int rows) : columns_(columns), rows_(rows) {
    if (columns <= 0 || rows <= 0)
        throw std::invalid_argument("SelectionGrid rows and columns must be positive");

    const auto column_count = static_cast<std::size_t>(columns);
    const auto row_count = static_cast<std::size_t>(rows);
    if (column_count > std::numeric_limits<std::size_t>::max() / row_count ||
        column_count * row_count > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw std::invalid_argument("SelectionGrid dimensions are too large");

    cells_.reserve(column_count * row_count);
    for (std::size_t i = 0; i < column_count * row_count; ++i) {
        auto cell = std::make_unique<SelectionGridCell>();
        configureCell(*cell, static_cast<int>(i));
        addChild(cell.get());
        cells_.push_back(std::move(cell));
    }
}

SelectionGrid::SelectionGrid(int columns, int rows, std::initializer_list<std::string_view> labels) :
    SelectionGrid(columns, rows) {
    if (labels.size() != cells_.size())
        throw std::invalid_argument("SelectionGrid label count must match its cell count");

    int index = 0;
    for (std::string_view label : labels) emplaceCell<TextSelectionGridCell>(index++, std::string(label));
}

void SelectionGrid::validateIndex(int index) const {
    if (index < 0 || index >= static_cast<int>(cells_.size()))
        throw std::out_of_range("SelectionGrid index is out of range");
}

SelectionGridCell& SelectionGrid::cell(int index) {
    validateIndex(index);
    return *cells_[index];
}

const SelectionGridCell& SelectionGrid::cell(int index) const {
    validateIndex(index);
    return *cells_[index];
}

void SelectionGrid::setSelectedIndex(int index) {
    validateIndex(index);
    if (selected_index_ == index) return;

    cells_[selected_index_]->setSelected(false);
    selected_index_ = index;
    cells_[selected_index_]->setSelected(true);
}

void SelectionGrid::setSelectedIndexAndNotify(int index) {
    validateIndex(index);
    if (selected_index_ == index) return;

    setSelectedIndex(index);
    on_selection_changed_.callback(index);
}

void SelectionGrid::configureCell(SelectionGridCell& cell, int index) {
    cell.setGrid(this);
    cell.setIndex(index);
    cell.setSelected(index == selected_index_);
    cell.setName("Cell " + std::to_string(index));
}

void SelectionGrid::replaceCell(int index, std::unique_ptr<SelectionGridCell> cell) {
    validateIndex(index);
    if (!cell) throw std::invalid_argument("SelectionGrid cell cannot be null");
    if (initialized()) throw std::logic_error("SelectionGrid cells must be replaced before initialization");

    configureCell(*cell, index);
    removeAllChildren();
    cells_[index] = std::move(cell);
    rebuildChildren();
    resized();
}

void SelectionGrid::rebuildChildren() {
    for (const auto& cell : cells_) addChild(cell.get());
}

void SelectionGrid::draw(applause::Canvas& canvas) {
    const float rounding = std::max(0.0f, canvas.value(ApplauseSelectionGridRounding));

    canvas.setColor(ApplauseSelectionGridBackground);
    canvas.roundedRectangle(0.0f, 0.0f, width(), height(), rounding);

    const float border_width = std::max(0.0f, canvas.value(ApplauseSelectionGridBorderWidth));
    if (border_width > 0.0f) {
        canvas.setColor(ApplauseSelectionGridBorder);
        canvas.roundedRectangleBorder(0.0f, 0.0f, width(), height(), rounding, border_width);
    }
}

void SelectionGrid::resized() {
    const float padding = std::max(0.0f, paletteValue(ApplauseSelectionGridPadding));
    const float horizontal_padding = std::min(padding, std::max(0.0f, width() * 0.5f));
    const float vertical_padding = std::min(padding, std::max(0.0f, height() * 0.5f));
    const float content_width = std::max(0.0f, width() - 2.0f * horizontal_padding);
    const float content_height = std::max(0.0f, height() - 2.0f * vertical_padding);

    float gap = std::max(0.0f, paletteValue(ApplauseSelectionGridGap));
    if (columns_ > 1) gap = std::min(gap, content_width / static_cast<float>(columns_ - 1));
    if (rows_ > 1) gap = std::min(gap, content_height / static_cast<float>(rows_ - 1));

    const float cell_width =
        std::max(0.0f, content_width - gap * static_cast<float>(columns_ - 1)) / columns_;
    const float cell_height = std::max(0.0f, content_height - gap * static_cast<float>(rows_ - 1)) / rows_;

    for (int row = 0; row < rows_; ++row) {
        for (int column = 0; column < columns_; ++column) {
            const int index = row * columns_ + column;
            cells_[index]->setBounds(horizontal_padding + column * (cell_width + gap),
                                     vertical_padding + row * (cell_height + gap), cell_width, cell_height);
        }
    }
}

}  // namespace applause
