#pragma once

#include <applause/ui/components/Button.h>
#include <applause/ui/components/RadioGroup.h>

#include <concepts>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace applause {

class ParamSelectionGrid;
class SelectionGrid;

/// A selectable cell whose selection is managed by its grid.
/// Override drawContent() to provide custom visuals.
class SelectionGridCell : public Button {
public:
    static inline const ColorToken kBackground{"selectionGridCell.background", 0x00000000};
    static inline const ColorToken kBackgroundHover{"selectionGridCell.backgroundHover", 0x11ffffff};
    static inline const ColorToken kBackgroundPressed{"selectionGridCell.backgroundPressed", 0xff1d1d22};
    static inline const ColorToken kGlowSelected{"selectionGridCell.glowSelected", theme::kAccent, 0.33f};
    static inline const ColorToken kBorder{"selectionGridCell.border", 0x00000000};
    static inline const ColorToken kBorderSelected{"selectionGridCell.borderSelected", theme::kAccent};
    static inline const ColorToken kContent{"selectionGridCell.content", 0xffa0a0a8};
    static inline const ColorToken kContentHover{"selectionGridCell.contentHover", 0xffd0d0d8};
    static inline const ColorToken kContentSelected{"selectionGridCell.contentSelected", theme::kAccent};
    static inline const ColorToken kContentSelectedHover{"selectionGridCell.contentSelectedHover", kContentSelected};

    static inline const ValueToken kRounding{"selectionGridCell.rounding", 4.0f};
    static inline const ValueToken kBorderWidth{"selectionGridCell.borderWidth", 1.0f};
    static inline const ValueToken kContentPadding{"selectionGridCell.contentPadding", 6.0f};
    static inline const ValueToken kGlowCenterAmount{"selectionGridCell.glowCenterAmount", 0.35f};

    explicit SelectionGridCell(std::string text = {}) : Button(std::move(text)) { setCheckable(true); }

    [[nodiscard]] bool selected() const noexcept { return isChecked(); }
    /// Returns the cell's index in its grid.
    [[nodiscard]] int index() const noexcept { return index_; }

protected:
    void drawBackground(applause::Canvas& canvas, float hover_amount) override;
    /// Override this to draw text, icons, or waveforms inside the cell.
    /// Coordinates are local to the cell and drawing is clipped inside its content padding.
    /// Use selected() and hover_amount to adjust the appearance.
    void drawContent(applause::Canvas& canvas, float hover_amount) override {}
    Bounds contentBounds() const override;

private:
    friend class SelectionGrid;

    void setIndex(int index) noexcept { index_ = index; }

    int index_ = -1;
};

/// A cell that displays a text label using the grid's content colors and padding.
class TextSelectionGridCell final : public SelectionGridCell {
public:
    using SelectionGridCell::SelectionGridCell;

protected:
    void drawContent(applause::Canvas& canvas, float hover_amount) override;
};

/// A fixed grid of equally sized cells. One cell can be selected at a time.
/// Indices start at 0 and run left to right, then top to bottom.
/// Supply text labels or use emplaceCell() to add your own cell visuals.
class SelectionGrid : public applause::Frame {
public:
    static inline const ColorToken kBackground{"selectionGrid.background", 0xff141418};
    static inline const ColorToken kBorder{"selectionGrid.border", 0xff2a2a2e};

    static inline const ValueToken kGap{"selectionGrid.gap", 4.0f};
    static inline const ValueToken kPadding{"selectionGrid.padding", 4.0f};
    static inline const ValueToken kRounding{"selectionGrid.rounding", 8.0f};
    static inline const ValueToken kBorderWidth{"selectionGrid.borderWidth", 1.0f};

    /// Creates a fixed grid of empty cells, with cell 0 selected.
    /// Rows and columns must be positive.
    SelectionGrid(int columns, int rows);

    /// Creates a grid with a text label in each cell.
    /// Labels follow index order, and the count must equal columns * rows.
    SelectionGrid(int columns, int rows, std::initializer_list<std::string_view> labels);

    /// Returns the selected cell's index, starting at 0.
    [[nodiscard]] int selectedIndex() const noexcept { return group_.selectedId(); }

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
    friend class ParamSelectionGrid;

    void validateIndex(int index) const;
    void configureCell(SelectionGridCell& cell, int index);
    void replaceCell(int index, std::unique_ptr<SelectionGridCell> cell);
    void rebuildChildren();

    int columns_;
    int rows_;
    std::vector<std::unique_ptr<SelectionGridCell>> cells_;
    RadioGroup group_;
    applause::CallbackList<void(int)> on_selection_changed_;
};

}  // namespace applause
