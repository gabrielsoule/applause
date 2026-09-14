#pragma once

#include <applause/ui/components/Button.h>

#include <concepts>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace applause {

class SelectionGrid;

/// A selectable cell whose selection is managed by its grid.
/// Override drawContent() to provide custom visuals.
class SelectionGridCell : public Button {
public:
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseSelectionGridCellBackground);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseSelectionGridCellBackgroundHover);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseSelectionGridCellBackgroundPressed);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseSelectionGridCellGlowSelected);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseSelectionGridCellBorder);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseSelectionGridCellBorderSelected);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseSelectionGridCellContent);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseSelectionGridCellContentHover);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseSelectionGridCellContentSelected);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseSelectionGridCellContentSelectedHover);

    APPLAUSE_THEME_DEFINE_VALUE(ApplauseSelectionGridCellRounding);
    APPLAUSE_THEME_DEFINE_VALUE(ApplauseSelectionGridCellBorderWidth);
    APPLAUSE_THEME_DEFINE_VALUE(ApplauseSelectionGridCellContentPadding);
    APPLAUSE_THEME_DEFINE_VALUE(ApplauseSelectionGridCellGlowCenterAmount);

    [[nodiscard]] bool selected() const noexcept { return selected_; }
    /// Returns the cell's index in its grid.
    [[nodiscard]] int index() const noexcept { return index_; }

    /// Selects this cell and notifies the grid if the selection changes.
    bool toggle() override;
    void draw(applause::Canvas& canvas) override;

protected:
    /// Override this to draw text, icons, or waveforms inside the cell.
    /// Coordinates are local to the cell and drawing is clipped inside its content padding.
    /// Use selected() and hover_amount to adjust the appearance.
    virtual void drawContent(applause::Canvas& canvas, float hover_amount) {}

private:
    friend class SelectionGrid;

    void setSelected(bool selected);
    void setIndex(int index) noexcept { index_ = index; }
    void setGrid(SelectionGrid* grid) noexcept { grid_ = grid; }

    SelectionGrid* grid_ = nullptr;
    bool selected_ = false;
    int index_ = -1;
};

/// A cell that displays a text label using the grid's content colors and padding.
class TextSelectionGridCell final : public SelectionGridCell {
public:
    explicit TextSelectionGridCell(std::string text = {});

    const applause::String& text() const noexcept { return text_.text(); }
    void setText(std::string text);
    void setFont(const applause::Font& font);

protected:
    void drawContent(applause::Canvas& canvas, float hover_amount) override;

private:
    applause::Text text_;
};

/// A fixed grid of equally sized cells. One cell can be selected at a time.
/// Indices start at 0 and run left to right, then top to bottom.
/// Supply text labels or use emplaceCell() to add your own cell visuals.
class SelectionGrid : public applause::Frame {
public:
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseSelectionGridBackground);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseSelectionGridBorder);

    APPLAUSE_THEME_DEFINE_VALUE(ApplauseSelectionGridGap);
    APPLAUSE_THEME_DEFINE_VALUE(ApplauseSelectionGridPadding);
    APPLAUSE_THEME_DEFINE_VALUE(ApplauseSelectionGridRounding);
    APPLAUSE_THEME_DEFINE_VALUE(ApplauseSelectionGridBorderWidth);

    /// Creates a fixed grid of empty cells, with cell 0 selected.
    /// Rows and columns must be positive.
    SelectionGrid(int columns, int rows);

    /// Creates a grid with a text label in each cell.
    /// Labels follow index order, and the count must equal columns * rows.
    SelectionGrid(int columns, int rows, std::initializer_list<std::string_view> labels);

    /// Returns the selected cell's index, starting at 0.
    [[nodiscard]] int selectedIndex() const noexcept { return selected_index_; }

    /// Changes the selection without calling onSelectionChanged().
    void setSelectedIndex(int index);

    /// Changes the selection and calls onSelectionChanged(). Does nothing if already selected.
    void setSelectedIndexAndNotify(int index);

    /// Called with the new index when a click or setSelectedIndexAndNotify() changes the selection.
    auto& onSelectionChanged() { return on_selection_changed_; }

    /// Returns the cell at this index. Throws if the index is out of range.
    SelectionGridCell& cell(int index);
    const SelectionGridCell& cell(int index) const;

    /// Replaces a cell with your own SelectionGridCell subclass and returns a reference to it.
    /// The grid owns the new cell. Call this before the grid is initialized.
    template <std::derived_from<SelectionGridCell> T, typename... Args>
    T& emplaceCell(int index, Args&&... args) {
        auto cell = std::make_unique<T>(std::forward<Args>(args)...);
        T& result = *cell;
        replaceCell(index, std::move(cell));
        return result;
    }

    void draw(applause::Canvas& canvas) override;
    void resized() override;

private:
    void validateIndex(int index) const;
    void configureCell(SelectionGridCell& cell, int index);
    void replaceCell(int index, std::unique_ptr<SelectionGridCell> cell);
    void rebuildChildren();

    int columns_;
    int rows_;
    std::vector<std::unique_ptr<SelectionGridCell>> cells_;
    applause::CallbackList<void(int)> on_selection_changed_;
    int selected_index_ = 0;
};

}  // namespace applause
