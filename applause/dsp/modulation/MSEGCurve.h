#pragma once

#include <array>
#include <cmath>
#include <utility>

namespace applause {

template <int MaxPoints = 64>
struct MSEGCurve {
    using Point = std::pair<float, float>;

    std::array<Point, MaxPoints> points{};
    std::array<float, MaxPoints> curvature_power{}; // curvature per segment, 0 = linear
    int num_points = 0;
    bool loop = true;

    /// Attempt to evaluate the curve at a given phase. If the phase falls outside
    /// the bounds of the curve, returns the value of the nearest endpoint.
    float evaluate(float phase) const {
        if (num_points < 2) return 0.0f;

        // Clamp to curve bounds
        if (phase <= points[0].first) return points[0].second;
        if (phase >= points[num_points - 1].first) return points[num_points - 1].second;

        // Find the segment containing this phase
        int seg = 0;
        for (int i = 0; i < num_points - 1; i++) {
            if (phase < points[i + 1].first) { seg = i; break; }
        }

        const auto& [x0, y0] = points[seg];
        const auto& [x1, y1] = points[seg + 1];
        float dx = x1 - x0;
        if (dx <= 0.0f) return y0;

        float t = (phase - x0) / dx;
        // apply curvature scaling
        t = powerScale(t, curvature_power[seg]);
        return y0 + t * (y1 - y0);
    }

private:
    /// Attempt to apply a power scale to a value, where power controls the
    /// curvature of the interpolation. When power is near zero, the value
    /// is returned unchanged (linear interpolation).
    static float powerScale(float value, float power) {
        constexpr float kMinPower = 0.01f;
        if (std::abs(power) < kMinPower) return value;
        float numerator = std::exp(power * value) - 1.0f;
        float denominator = std::exp(power) - 1.0f;
        return numerator / denominator;
    }
};

} // namespace applause