#include <applause/util/SampleType.h>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <utility>

namespace {

using FloatBatch = xsimd::batch<float>;
using DoubleBatch = xsimd::batch<double>;
using IntegerBatch = xsimd::batch<int>;
using CommonBatch = xsimd::batch<float, xsimd::common>;

struct BatchLookalike {
    using value_type = float;
    using arch_type = xsimd::default_arch;
    using batch_bool_type = xsimd::batch_bool<float>;
    static constexpr std::size_t size = FloatBatch::size;
};

static_assert(applause::Scalar<float>);
static_assert(applause::Scalar<double>);
static_assert(!applause::Scalar<const float>);
static_assert(!applause::Scalar<long double>);
static_assert(!applause::Scalar<int>);

static_assert(applause::SimdBatch<FloatBatch>);
static_assert(applause::SimdBatch<DoubleBatch>);
static_assert(!applause::SimdBatch<IntegerBatch>);
static_assert(!applause::SimdBatch<CommonBatch>);
static_assert(!applause::SimdBatch<typename FloatBatch::batch_bool_type>);
static_assert(!applause::SimdBatch<BatchLookalike>);

static_assert(applause::Sample<float>);
static_assert(applause::Sample<DoubleBatch>);
static_assert(!applause::Sample<const double>);
static_assert(!applause::Sample<IntegerBatch>);

static_assert(std::same_as<applause::scalar_t<float>, float>);
static_assert(std::same_as<applause::scalar_t<DoubleBatch>, double>);
static_assert(std::same_as<applause::mask_t<float>, bool>);
static_assert(std::same_as<applause::mask_t<FloatBatch>,
                           xsimd::mask_type_t<FloatBatch>>);

static_assert(applause::sampleWidth<float>() == 1);
static_assert(applause::sampleWidth<FloatBatch>() == FloatBatch::size);
static_assert(applause::sample_width_v<double> == 1);
static_assert(applause::sample_width_v<DoubleBatch> == DoubleBatch::size);

static_assert(applause::sampleAlignment<float>() == alignof(float));
static_assert(applause::sampleAlignment<double>() == alignof(double));
static_assert(applause::sampleAlignment<FloatBatch>() ==
              FloatBatch::arch_type::alignment());
static_assert(applause::sampleAlignment<DoubleBatch>() ==
              DoubleBatch::arch_type::alignment());

static_assert(noexcept(applause::set1<float>(1.0f)));
static_assert(noexcept(applause::load_aligned<FloatBatch>(
    static_cast<const float*>(nullptr))));
static_assert(noexcept(applause::store_aligned(
    std::declval<const FloatBatch&>(), static_cast<float*>(nullptr))));
static_assert(noexcept(applause::select<float>(true, 1.0f, 0.0f)));

template <applause::Sample S>
void requireAllEqual(const S& value, applause::scalar_t<S> expected) {
    if constexpr (applause::SimdBatch<S>) {
        for (std::size_t lane = 0; lane < S::size; ++lane)
            REQUIRE(value.get(lane) == expected);
    } else {
        REQUIRE(value == expected);
    }
}

}  // namespace

TEMPLATE_TEST_CASE("SampleType broadcasts and loads and stores samples",
                   "[util][sample][simd]", float, double,
                   xsimd::batch<float>, xsimd::batch<double>)
{
    using S = TestType;
    using Scalar = applause::scalar_t<S>;
    constexpr std::size_t width = applause::sample_width_v<S>;

    requireAllEqual(applause::set1<S>(Scalar{3}), Scalar{3});

    alignas(applause::sampleAlignment<S>())
        std::array<Scalar, width> aligned_input{};
    alignas(applause::sampleAlignment<S>())
        std::array<Scalar, width> aligned_output{};
    for (std::size_t lane = 0; lane < width; ++lane)
        aligned_input[lane] = static_cast<Scalar>(lane + 1);

    const S aligned = applause::load_aligned<S>(aligned_input.data());
    applause::store_aligned(aligned, aligned_output.data());
    for (std::size_t lane = 0; lane < width; ++lane)
        REQUIRE(aligned_output[lane] == aligned_input[lane]);

    std::array<Scalar, width + 1> unaligned_input{};
    std::array<Scalar, width + 1> unaligned_output{};
    for (std::size_t lane = 0; lane < width; ++lane)
        unaligned_input[lane + 1] = static_cast<Scalar>(lane + 5);

    const S unaligned =
        applause::load_unaligned<S>(unaligned_input.data() + 1);
    applause::store_unaligned(unaligned, unaligned_output.data() + 1);
    for (std::size_t lane = 0; lane < width; ++lane)
        REQUIRE(unaligned_output[lane + 1] == unaligned_input[lane + 1]);
}

TEMPLATE_TEST_CASE("SampleType math helpers agree for scalar and SIMD samples",
                   "[util][sample][simd]", float, double,
                   xsimd::batch<float>, xsimd::batch<double>)
{
    using S = TestType;
    using Scalar = applause::scalar_t<S>;

    const S two = applause::set1<S>(Scalar{2});
    const S three = applause::set1<S>(Scalar{3});
    const S four = applause::set1<S>(Scalar{4});
    const S negative = applause::set1<S>(Scalar{-3});

    requireAllEqual(applause::fma(two, three, four), Scalar{10});
    requireAllEqual(applause::min(two, three), Scalar{2});
    requireAllEqual(applause::max(two, three), Scalar{3});
    requireAllEqual(applause::abs(negative), Scalar{3});
}

TEMPLATE_TEST_CASE("SampleType select uses scalar and SIMD masks",
                   "[util][sample][simd]", float, double,
                   xsimd::batch<float>, xsimd::batch<double>)
{
    using S = TestType;
    using Scalar = applause::scalar_t<S>;
    const S true_value = applause::set1<S>(Scalar{10});
    const S false_value = applause::set1<S>(Scalar{-10});

    if constexpr (applause::SimdBatch<S>) {
        constexpr std::size_t width = applause::sample_width_v<S>;
        alignas(applause::sampleAlignment<S>())
            std::array<Scalar, width> lanes{};
        for (std::size_t lane = 0; lane < width; ++lane)
            lanes[lane] = static_cast<Scalar>(lane);

        const S lane_values = applause::load_aligned<S>(lanes.data());
        const auto mask = lane_values < applause::set1<S>(
                                            static_cast<Scalar>(width / 2));
        const S selected =
            applause::select<S>(mask, true_value, false_value);

        for (std::size_t lane = 0; lane < width; ++lane) {
            const Scalar expected =
                lane < width / 2 ? Scalar{10} : Scalar{-10};
            REQUIRE(selected.get(lane) == expected);
        }
    } else {
        REQUIRE(applause::select<S>(true, true_value, false_value) ==
                true_value);
        REQUIRE(applause::select<S>(false, true_value, false_value) ==
                false_value);
    }
}
