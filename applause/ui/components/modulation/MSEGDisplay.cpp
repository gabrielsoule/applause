#include "MSEGDisplay.h"

#include <visage_graphics/canvas.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace applause {

VISAGE_THEME_IMPLEMENT_COLOR(MSEGDisplay, MSEGDisplayLine, 0xff9966ff);
VISAGE_THEME_IMPLEMENT_COLOR(MSEGDisplay, MSEGDisplayFill, 0x309966ff);
VISAGE_THEME_IMPLEMENT_COLOR(MSEGDisplay, MSEGDisplayPoint, 0xffdddddd);
VISAGE_THEME_IMPLEMENT_COLOR(MSEGDisplay, MSEGDisplayPointHover, 0xffffffff);
VISAGE_THEME_IMPLEMENT_COLOR(MSEGDisplay, MSEGDisplayMidpoint, 0x99aaaaaa);
VISAGE_THEME_IMPLEMENT_COLOR(MSEGDisplay, MSEGDisplayMidpointHover, 0xccdddddd);
VISAGE_THEME_IMPLEMENT_VALUE(MSEGDisplay, MSEGDisplayLineWidth, 2.0f);
VISAGE_THEME_IMPLEMENT_VALUE(MSEGDisplay, MSEGDisplayPointRadius, 5.0f);
VISAGE_THEME_IMPLEMENT_VALUE(MSEGDisplay, MSEGDisplayMidpointRadius, 3.5f);

static constexpr int kSamplesPerSegment = 32;
static constexpr float kPointEpsilon = 0.001f;
static constexpr float kMaxCurvature = 32.0f;

MSEGDisplay::MSEGDisplay(MSEGCurve<>* curve) : curve_(curve) {}

float MSEGDisplay::curveXToScreen(float cx) const {
    return cx * width();
}

float MSEGDisplay::curveYToScreen(float cy) const {
    if (y_max_ <= y_min_)
        return height() * 0.5f;
    return height() * (1.0f - (cy - y_min_) / (y_max_ - y_min_));
}

float MSEGDisplay::screenXToCurve(float sx) const {
    return width() > 0.0f ? sx / width() : 0.0f;
}

float MSEGDisplay::screenYToCurve(float sy) const {
    if (height() <= 0.0f || y_max_ <= y_min_)
        return y_min_;
    return y_min_ + (1.0f - sy / height()) * (y_max_ - y_min_);
}

int MSEGDisplay::hitTestPoint(float sx, float sy) const {
    if (!curve_)
        return -1;
    float hit_radius = paletteValue(MSEGDisplayPointRadius) * 1.5f;
    float hit_r2 = hit_radius * hit_radius;
    for (int i = 0; i < curve_->num_points; i++) {
        float px = curveXToScreen(curve_->points[i].first);
        float py = curveYToScreen(curve_->points[i].second);
        float dx = sx - px;
        float dy = sy - py;
        if (dx * dx + dy * dy <= hit_r2)
            return i;
    }
    return -1;
}

int MSEGDisplay::hitTestMidpoint(float sx, float sy) const {
    if (!curve_ || curve_->num_points < 2)
        return -1;
    float hit_radius = paletteValue(MSEGDisplayMidpointRadius) * 1.5f;
    float hit_r2 = hit_radius * hit_radius;
    for (int s = 0; s < curve_->num_points - 1; s++) {
        float mid_x = (curve_->points[s].first + curve_->points[s + 1].first) * 0.5f;
        float mid_y = curve_->evaluate(mid_x);
        float px = curveXToScreen(mid_x);
        float py = curveYToScreen(mid_y);
        float dx = sx - px;
        float dy = sy - py;
        if (dx * dx + dy * dy <= hit_r2)
            return s;
    }
    return -1;
}

void MSEGDisplay::draw(visage::Canvas& canvas) {
    if (!curve_ || curve_->num_points < 2)
        return;

    int n = curve_->num_points;
    float line_width = canvas.value(MSEGDisplayLineWidth);
    float point_radius = canvas.value(MSEGDisplayPointRadius);

    // Build polyline samples by evaluating the curve at many points
    samples_.clear();
    samples_.reserve((n - 1) * kSamplesPerSegment + 1);

    for (int s = 0; s < n - 1; s++) {
        float x0 = curve_->points[s].first;
        float x1 = curve_->points[s + 1].first;
        int start = (s == 0) ? 0 : 1; // avoid duplicating shared endpoints
        for (int i = start; i <= kSamplesPerSegment; i++) {
            float phase = x0 + (x1 - x0) * (static_cast<float>(i) / kSamplesPerSegment);
            float y = curve_->evaluate(phase);
            samples_.emplace_back(curveXToScreen(phase), curveYToScreen(y));
        }
    }

    if (samples_.empty())
        return;

    // Fill under the curve
    visage::Path fill_path;
    fill_path.moveTo(samples_[0].x, samples_[0].y);
    for (size_t i = 1; i < samples_.size(); i++)
        fill_path.lineTo(samples_[i].x, samples_[i].y);
    fill_path.lineTo(samples_.back().x, static_cast<float>(height()));
    fill_path.lineTo(samples_[0].x, static_cast<float>(height()));
    fill_path.close();

    visage::Color fill_color = canvas.color(MSEGDisplayFill).gradient().sample(0.0f);
    canvas.setColor(visage::Brush::vertical(fill_color, fill_color.withAlpha(0.0f)));
    canvas.fill(fill_path);

    // Stroke the curve line as segments
    canvas.setColor(MSEGDisplayLine);
    for (size_t i = 0; i + 1 < samples_.size(); i++)
        canvas.segment(samples_[i].x, samples_[i].y, samples_[i + 1].x, samples_[i + 1].y, line_width, true);

    // Draw midpoint handles for curvature adjustment
    float midpoint_radius = canvas.value(MSEGDisplayMidpointRadius);
    float midpoint_diameter = midpoint_radius * 2.0f;
    for (int s = 0; s < n - 1; s++) {
        float mid_x = (curve_->points[s].first + curve_->points[s + 1].first) * 0.5f;
        float mid_y = curve_->evaluate(mid_x);
        float px = curveXToScreen(mid_x);
        float py = curveYToScreen(mid_y);

        if (s == hovered_midpoint_ || s == dragged_segment_)
            canvas.setColor(MSEGDisplayMidpointHover);
        else
            canvas.setColor(MSEGDisplayMidpoint);

        canvas.circle(px - midpoint_radius, py - midpoint_radius, midpoint_diameter);
    }

    // Draw point handles
    float diameter = point_radius * 2.0f;
    for (int i = 0; i < n; i++) {
        float px = curveXToScreen(curve_->points[i].first);
        float py = curveYToScreen(curve_->points[i].second);

        if (i == hovered_point_ || i == dragged_point_)
            canvas.setColor(MSEGDisplayPointHover);
        else
            canvas.setColor(MSEGDisplayPoint);

        canvas.circle(px - point_radius, py - point_radius, diameter);
    }
}

void MSEGDisplay::mouseMove(const visage::MouseEvent& e) {
    int point_hit = hitTestPoint(e.position.x, e.position.y);
    int mid_hit = point_hit < 0 ? hitTestMidpoint(e.position.x, e.position.y) : -1;
    if (point_hit != hovered_point_ || mid_hit != hovered_midpoint_) {
        hovered_point_ = point_hit;
        hovered_midpoint_ = mid_hit;
        redraw();
    }
}

void MSEGDisplay::mouseExit(const visage::MouseEvent& e) {
    if (hovered_point_ != -1 || hovered_midpoint_ != -1) {
        hovered_point_ = -1;
        hovered_midpoint_ = -1;
        redraw();
    }
}

void MSEGDisplay::mouseDown(const visage::MouseEvent& e) {
    if (!curve_)
        return;

    int point_hit = hitTestPoint(e.position.x, e.position.y);

    if (point_editing_enabled_ && e.repeatClickCount() == 2) {
        if (point_hit >= 0) {
            // Remove point (keep at least 2)
            if (curve_->num_points <= 2)
                return;
            for (int i = point_hit; i < curve_->num_points - 1; i++)
                curve_->points[i] = curve_->points[i + 1];
            int merged_seg = std::max(0, point_hit - 1);
            curve_->curvature_power[merged_seg] = 0.0f;
            for (int i = point_hit; i < curve_->num_points - 2; i++)
                curve_->curvature_power[i] = curve_->curvature_power[i + 1];
            curve_->num_points--;
            hovered_point_ = -1;
            on_curve_changed.callback();
            redraw();
        } else {
            // Add point
            if (curve_->num_points >= 64)
                return;
            float new_x = std::clamp(screenXToCurve(e.position.x), 0.0f, 1.0f);
            float new_y = std::clamp(screenYToCurve(e.position.y), y_min_, y_max_);

            // Find insertion index
            int insert_idx = curve_->num_points;
            for (int i = 0; i < curve_->num_points; i++) {
                if (new_x < curve_->points[i].first) {
                    insert_idx = i;
                    break;
                }
            }

            // Shift points and curvature right
            for (int i = curve_->num_points; i > insert_idx; i--)
                curve_->points[i] = curve_->points[i - 1];
            curve_->points[insert_idx] = {new_x, new_y};

            for (int i = curve_->num_points - 1; i > insert_idx; i--)
                curve_->curvature_power[i] = curve_->curvature_power[i - 1];
            if (insert_idx > 0)
                curve_->curvature_power[insert_idx - 1] = 0.0f;
            curve_->curvature_power[insert_idx] = 0.0f;

            curve_->num_points++;
            on_curve_changed.callback();
            redraw();
        }
        return;
    }

    // Single-click on point: start dragging
    if (point_hit >= 0) {
        dragged_point_ = point_hit;
        return;
    }

    // Click on midpoint handle: start curvature drag
    int mid_hit = hitTestMidpoint(e.position.x, e.position.y);
    if (mid_hit >= 0)
        dragged_segment_ = mid_hit;
}

void MSEGDisplay::mouseDrag(const visage::MouseEvent& e) {
    if (!curve_)
        return;

    if (dragged_point_ >= 0) {
        float cx = screenXToCurve(e.position.x);
        float cy = std::clamp(screenYToCurve(e.position.y), y_min_, y_max_);

        // Pin first/last point X, clamp interior points between neighbors
        if (dragged_point_ == 0) {
            cx = curve_->points[0].first;
        } else if (dragged_point_ == curve_->num_points - 1) {
            cx = curve_->points[curve_->num_points - 1].first;
        } else {
            float lo = curve_->points[dragged_point_ - 1].first + kPointEpsilon;
            float hi = curve_->points[dragged_point_ + 1].first - kPointEpsilon;
            cx = std::clamp(cx, lo, hi);
        }

        curve_->points[dragged_point_] = {cx, cy};
        on_curve_changed.callback();
        redraw();
        return;
    }

    if (dragged_segment_ >= 0) {
        // solve for the curvature power
        // that makes the curve pass through the mouse position at the segment midpoint
        float y0 = curve_->points[dragged_segment_].second;
        float y1 = curve_->points[dragged_segment_ + 1].second;
        float dy = y1 - y0;
        if (std::abs(dy) < kPointEpsilon)
            return; // flat segment; curvature has no visible effect

        float desired_y = std::clamp(screenYToCurve(e.position.y), y_min_, y_max_);
        float u = (desired_y - y0) / dy;
        u = std::clamp(u, 0.0001f, 0.9999f);
        float power = 2.0f * std::log((1.0f - u) / u);
        power = std::clamp(power, -kMaxCurvature, kMaxCurvature);
        curve_->curvature_power[dragged_segment_] = power;
        on_curve_changed.callback();
        redraw();
    }
}

void MSEGDisplay::mouseUp(const visage::MouseEvent& e) {
    dragged_point_ = -1;
    dragged_segment_ = -1;
    hovered_point_ = hitTestPoint(e.position.x, e.position.y);
    redraw();
}

} // namespace applause
