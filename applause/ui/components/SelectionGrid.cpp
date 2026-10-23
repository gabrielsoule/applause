#include "SelectionGrid.h"

#include <embedded/applause_fonts.h>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace applause {

void SelectionGridCell::draw(applause::Canvas& canvas) {
    const float hover_amount = isActive() ? hover_amount_.update() : 0.0f;
    const float rounding = value(*this, kRounding);
    if (isPressed())
        canvas.setColor(color(*this, kBackgroundPressed));
    else
        canvas.setColor(color(*this, kBackground));

    canvas.roundedRectangle(0.0f, 0.0f, width(), height(), rounding);

    if (selected()) {
        applause::Color glow = color(*this, kGlowSelected);
        applause::Color center_glow = glow.withMultipliedAlpha(value(*this, kGlowCenterAmount));
        applause::Point center = {width() * 0.5f, height() * 0.5f};
        canvas.setColor(applause::Brush::radial(center_glow, glow, center, width() * 1.0f, height() * 1.0f));
        canvas.roundedRectangle(0.0f, 0.0f, width(), height(), rounding);
    }

    if (selected() && !isPressed() && hover_amount > 0.0f) {
        const applause::Color hover = color(*this, kBackgroundHover);
        canvas.setColor(hover.withMultipliedAlpha(hover_amount));
        canvas.roundedRectangle(0.0f, 0.0f, width(), height(), rounding);
    }

    canvas.setColor(color(*this, selected() ? kBorderSelected : kBorder));
    canvas.roundedRectangleBorder(0.0f, 0.0f, width(), height(), rounding, value(*this, kBorderWidth));

    const float padding = std::max(0.0f, value(*this, kContentPadding));
    canvas.saveState();
    canvas.trimClampBounds(padding, padding, std::max(0.0f, width() - 2.0f * padding),
                           std::max(0.0f, height() - 2.0f * padding));
    drawContent(canvas, hover_amount);
    canvas.restoreState();

    const float hover_glow = hover_amount * value(*this, kGlowAmount);
    if (hover_glow > 0.0f) {
        const applause::Color accent = color(*this, kGlow);
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
    const float padding = std::max(0.0f, value(*this, kContentPadding));
    if (selected())
        canvas.setColor(
            color(*this, kContentSelected).interpolateWith(color(*this, kContentSelectedHover), hover_amount));
    else
        canvas.setColor(color(*this, kContent).interpolateWith(color(*this, kContentHover), hover_amount));
    canvas.text(&text_, padding, padding, std::max(0.0f, width() - 2.0f * padding),
                std::max(0.0f, height() - 2.0f * padding));
}

SelectionGrid::SelectionGrid(int columns, int rows) : columns_(columns), rows_(rows) {
    if (columns <= 0 || rows <= 0) throw std::invalid_argument("SelectionGrid rows and columns must be positive");

    const auto column_count = static_cast<std::size_t>(columns);
    const auto row_count = static_cast<std::size_t>(rows);
    if (column_count > std::numeric_limits<std::size_t>::max() / row_count ||
        column_count * row_count > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw std::invalid_argument("SelectionGrid dimensions are too large");

    cells_.reserve(column_count * row_count);
    group_.onSelectionChanged() += [this](int index) { on_selection_changed_.callback(index); };
    for (std::size_t i = 0; i < column_count * row_count; ++i) {
        auto cell = std::make_unique<SelectionGridCell>();
        configureCell(*cell, static_cast<int>(i));
        group_.addButton(*cell, static_cast<int>(i));
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
    group_.setSelectedId(index);
}

void SelectionGrid::setSelectedIndexAndNotify(int index) {
    validateIndex(index);
    group_.setSelectedIdAndNotify(index);
}

void SelectionGrid::configureCell(SelectionGridCell& cell, int index) {
    cell.setIndex(index);
    cell.setName("Cell " + std::to_string(index));
}

void SelectionGrid::replaceCell(int index, std::unique_ptr<SelectionGridCell> cell) {
    validateIndex(index);
    if (!cell) throw std::invalid_argument("SelectionGrid cell cannot be null");
    if (initialized()) throw std::logic_error("SelectionGrid cells must be replaced before initialization");

    configureCell(*cell, index);
    group_.replaceButton(index, *cell);
    removeAllChildren();
    cells_[index] = std::move(cell);
    rebuildChildren();
    resized();
}

void SelectionGrid::rebuildChildren() {
    for (const auto& cell : cells_) addChild(cell.get());
}

void SelectionGrid::draw(applause::Canvas& canvas) {
    const float rounding = std::max(0.0f, value(*this, kRounding));

    canvas.setColor(color(*this, kBackground));
    canvas.roundedRectangle(0.0f, 0.0f, width(), height(), rounding);

    const float border_width = std::max(0.0f, value(*this, kBorderWidth));
    if (border_width > 0.0f) {
        canvas.setColor(color(*this, kBorder));
        canvas.roundedRectangleBorder(0.0f, 0.0f, width(), height(), rounding, border_width);
    }
}

void SelectionGrid::resized() {
    const float padding = std::max(0.0f, value(*this, kPadding));
    const float horizontal_padding = std::min(padding, std::max(0.0f, width() * 0.5f));
    const float vertical_padding = std::min(padding, std::max(0.0f, height() * 0.5f));
    const float content_width = std::max(0.0f, width() - 2.0f * horizontal_padding);
    const float content_height = std::max(0.0f, height() - 2.0f * vertical_padding);

    float gap = std::max(0.0f, value(*this, kGap));
    if (columns_ > 1) gap = std::min(gap, content_width / static_cast<float>(columns_ - 1));
    if (rows_ > 1) gap = std::min(gap, content_height / static_cast<float>(rows_ - 1));

    const float cell_width = std::max(0.0f, content_width - gap * static_cast<float>(columns_ - 1)) / columns_;
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
