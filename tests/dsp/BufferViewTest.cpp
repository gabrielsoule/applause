#include <applause/dsp/BufferView.h>
#include <applause/util/MemoryArena.h>
#include <applause/util/SampleType.h>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

template <typename Buffer>
concept HasBufferMutation = requires(const Buffer& buffer,
                                     typename Buffer::Value value) {
    buffer.store(0, 0, value);
    buffer.add(0, 0, typename Buffer::Scalar{});
    buffer.clear();
    buffer.clearChannel(0);
};

template <typename Buffer>
concept HasChannelMutation = requires(const Buffer& buffer,
                                      typename Buffer::Value value) {
    buffer.channel(0).store(0, value);
    buffer.channel(0).add(0, value);
};

template <typename SampleType>
struct PlanarStorage {
    using Value = std::remove_const_t<SampleType>;
    using Scalar = applause::scalar_t<Value>;

    PlanarStorage(std::size_t channels, std::size_t frames)
        : scalars(channels * frames * applause::sampleWidth<Value>() +
                  applause::sampleAlignment<Value>() / sizeof(Scalar)),
          channel_ptrs(channels),
          frame_count(frames) {
        constexpr std::size_t alignment =
            applause::sampleAlignment<Value>();
        static_assert(alignment % alignof(Scalar) == 0);

        const auto address =
            reinterpret_cast<std::uintptr_t>(scalars.data());
        const auto aligned_address =
            (address + alignment - 1) & ~(alignment - 1);
        scalar_data = reinterpret_cast<Scalar*>(aligned_address);

        for (std::size_t channel = 0; channel < channels; ++channel) {
            channel_ptrs[channel] =
                frames == 0
                    ? nullptr
                    : scalar_data +
                          channel * frame_count *
                              applause::sampleWidth<Value>();
        }
    }

    [[nodiscard]] applause::BufferView<Value> view() noexcept {
        return {channel_ptrs.data(), channel_ptrs.size(), frame_count};
    }

    std::vector<Scalar> scalars;
    std::vector<Scalar*> channel_ptrs;
    std::size_t frame_count = 0;
    Scalar* scalar_data = nullptr;
};

using MutableFloatBuffer = applause::BufferView<float>;
using ReadOnlyFloatBuffer = applause::BufferView<const float>;

static_assert(std::constructible_from<ReadOnlyFloatBuffer,
                                      MutableFloatBuffer>);
static_assert(!std::constructible_from<MutableFloatBuffer,
                                       ReadOnlyFloatBuffer>);
static_assert(std::constructible_from<ReadOnlyFloatBuffer, float* const*,
                                      std::size_t, std::size_t>);
static_assert(std::constructible_from<ReadOnlyFloatBuffer,
                                      const float* const*, std::size_t,
                                      std::size_t>);
static_assert(!std::constructible_from<MutableFloatBuffer,
                                       const float* const*, std::size_t,
                                       std::size_t>);
static_assert(HasBufferMutation<MutableFloatBuffer>);
static_assert(HasChannelMutation<MutableFloatBuffer>);
static_assert(!HasBufferMutation<ReadOnlyFloatBuffer>);
static_assert(!HasChannelMutation<ReadOnlyFloatBuffer>);
static_assert(std::same_as<
              decltype(std::declval<const MutableFloatBuffer&>()
                           .channelSamples(0)),
              float*>);
static_assert(std::same_as<
              decltype(std::declval<const ReadOnlyFloatBuffer&>()
                           .channelSamples(0)),
              const float*>);
static_assert(std::same_as<
              decltype(std::declval<const ReadOnlyFloatBuffer&>()
                           .channelSampleSpan(0)),
              std::span<const float>>);
using FloatBatch = xsimd::batch<float>;
using SimdFloatBuffer = applause::BufferView<FloatBatch>;
using ReadOnlySimdFloatBuffer = applause::BufferView<const FloatBatch>;

template <typename Buffer>
concept HasChannelSamples = requires(const Buffer& buffer) {
    buffer.channelSamples(0);
    buffer.channelSampleSpan(0);
};

template <typename Buffer>
concept HasSampleTypedChannelPointers = requires(const Buffer& buffer) {
    buffer.channel(0).data();
    buffer.channel(0).samplePtr(0);
};

static_assert(!HasChannelSamples<SimdFloatBuffer>);
static_assert(!HasChannelSamples<ReadOnlySimdFloatBuffer>);
static_assert(!HasSampleTypedChannelPointers<SimdFloatBuffer>);
static_assert(!HasSampleTypedChannelPointers<ReadOnlySimdFloatBuffer>);
static_assert(HasBufferMutation<SimdFloatBuffer>);
static_assert(HasChannelMutation<SimdFloatBuffer>);
static_assert(!HasBufferMutation<ReadOnlySimdFloatBuffer>);
static_assert(!HasChannelMutation<ReadOnlySimdFloatBuffer>);
static_assert(std::same_as<
              decltype(std::declval<const SimdFloatBuffer&>()
                           .channelScalars(0)),
              float*>);
static_assert(std::same_as<
              decltype(std::declval<const SimdFloatBuffer&>()
                           .channelScalarSpan(0)),
              std::span<float>>);
static_assert(std::same_as<
              decltype(std::declval<const ReadOnlySimdFloatBuffer&>()
                           .channelScalars(0)),
              const float*>);
static_assert(std::same_as<
              decltype(std::declval<const ReadOnlySimdFloatBuffer&>()
                           .channelScalarSpan(0)),
              std::span<const float>>);

}  // namespace

TEST_CASE("BufferView default construction", "[dsp][buffer]")
{
    applause::BufferView<float> buffer;

    REQUIRE(buffer.numFrames() == 0);
    REQUIRE(buffer.numChannels() == 0);
    REQUIRE(buffer.isValid());
    REQUIRE(buffer.isContiguous());
}

TEST_CASE("BufferView borrows a runtime-sized channel table", "[dsp][buffer]")
{
    constexpr std::size_t frames = 32;

    SECTION("More than eight contiguous channels are supported")
    {
        PlanarStorage<float> storage{12, frames};
        auto buffer = storage.view();

        REQUIRE(buffer.numChannels() == 12);
        REQUIRE(buffer.numFrames() == frames);
        REQUIRE(buffer.isValid());
        REQUIRE(buffer.isContiguous());

        for (std::size_t channel = 0; channel < 12; ++channel) {
            REQUIRE(buffer.channelSamples(channel) ==
                    storage.scalar_data + channel * frames);
        }
    }

    SECTION("The channel table is borrowed rather than copied")
    {
        alignas(64) std::array<float, frames> first{};
        alignas(64) std::array<float, frames> replacement{};
        std::array<float*, 1> channels{first.data()};
        applause::BufferView<float> buffer{channels.data(), channels.size(),
                                           frames};

        channels[0] = replacement.data();

        REQUIRE(buffer.channelSamples(0) == replacement.data());
    }

    SECTION("Non-contiguous channel planes are supported")
    {
        alignas(64) std::array<float, frames * 2 + 7> backing{};
        std::array<float*, 2> channels{
            backing.data(), backing.data() + frames + 7};
        applause::BufferView<float> buffer{channels.data(), channels.size(),
                                           frames};

        REQUIRE(buffer.isValid());
        REQUIRE_FALSE(buffer.isContiguous());
        REQUIRE(buffer.channelSamples(0) == channels[0]);
        REQUIRE(buffer.channelSamples(1) == channels[1]);
    }

    SECTION("Zero channels may use a null table")
    {
        applause::BufferView<float> buffer{
            static_cast<float* const*>(nullptr), 0, frames};

        REQUIRE(buffer.numChannels() == 0);
        REQUIRE(buffer.numFrames() == frames);
        REQUIRE(buffer.isValid());
    }

    SECTION("Zero frames permit null channel planes")
    {
        std::array<float*, 2> channels{nullptr, nullptr};
        applause::BufferView<float> buffer{channels.data(), channels.size(), 0};

        REQUIRE(buffer.numChannels() == 2);
        REQUIRE(buffer.numFrames() == 0);
        REQUIRE(buffer.isValid());
        REQUIRE(buffer.channelSampleSpan(0).empty());
    }
}

TEST_CASE("Invalid BufferView construction returns an empty view",
          "[dsp][buffer]")
{
    constexpr std::size_t frames = 8;
    alignas(64) std::array<float, frames> backing{};

    SECTION("Null channel table")
    {
        applause::BufferView<float> buffer{
            static_cast<float* const*>(nullptr), 1, frames};

        REQUIRE(buffer.numChannels() == 0);
        REQUIRE(buffer.numFrames() == 0);
        REQUIRE(buffer.isValid());
    }

    SECTION("Null channel plane")
    {
        std::array<float*, 2> channels{backing.data(), nullptr};
        applause::BufferView<float> buffer{channels.data(), channels.size(),
                                           frames};

        REQUIRE(buffer.numChannels() == 0);
        REQUIRE(buffer.numFrames() == 0);
        REQUIRE(buffer.isValid());
    }

    SECTION("Misaligned channel plane")
    {
        alignas(64) std::array<std::byte, 64> bytes{};
        std::array<float*, 1> channels{
            reinterpret_cast<float*>(bytes.data() + 1)};
        applause::BufferView<float> buffer{channels.data(), channels.size(), 1};

        REQUIRE(buffer.numChannels() == 0);
        REQUIRE(buffer.numFrames() == 0);
        REQUIRE(buffer.isValid());
    }

    SECTION("A scalar-aligned plane may still be misaligned for SIMD")
    {
        using Batch = xsimd::batch<float>;
        alignas(64) std::array<std::byte, 128> bytes{};
        std::array<float*, 1> channels{
            reinterpret_cast<float*>(bytes.data() + sizeof(float))};
        applause::BufferView<Batch> buffer{
            channels.data(), channels.size(), 1};

        REQUIRE(buffer.numChannels() == 0);
        REQUIRE(buffer.numFrames() == 0);
        REQUIRE(buffer.isValid());
    }

    SECTION("Frame byte count overflow")
    {
        std::array<float*, 1> channels{backing.data()};
        constexpr std::size_t overflowing_frames =
            std::numeric_limits<std::size_t>::max() / sizeof(float) + 1;
        applause::BufferView<float> buffer{
            channels.data(), channels.size(), overflowing_frames};

        REQUIRE(buffer.numChannels() == 0);
        REQUIRE(buffer.numFrames() == 0);
        REQUIRE(buffer.isValid());
    }
}

TEST_CASE("BufferView sample access", "[dsp][buffer]")
{
    constexpr std::size_t frames = 32;
    PlanarStorage<float> storage{2, frames};
    auto buffer = storage.view();

    SECTION("Accessors describe the view")
    {
        REQUIRE(buffer.numFrames() == frames);
        REQUIRE(buffer.numChannels() == 2);
        REQUIRE(buffer.samplesPerChannel() == frames);
        REQUIRE(buffer.scalarsPerChannel() == frames);
        REQUIRE(buffer.channelSampleSpan(0).size() == frames);
        REQUIRE(buffer.channelSampleSpan(1).data() ==
                storage.scalar_data + frames);
    }

    SECTION("Store, load, and add affect the selected sample")
    {
        buffer.store(0, 5, 40.0f);
        buffer.add(0, 5, 2.0f);

        REQUIRE(buffer.load(0, 5) == 42.0f);
        REQUIRE(buffer.load(0, 4) == 0.0f);
        REQUIRE(buffer.load(1, 5) == 0.0f);
    }

    SECTION("ChannelView accesses the selected plane")
    {
        auto channel = buffer.channel(1);
        channel.store(10, 99.0f);
        channel.add(10, 1.0f);

        REQUIRE(channel.frames() == frames);
        REQUIRE(channel.data() == buffer.channelSamples(1));
        REQUIRE(channel.samplePtr(10) == channel.data() + 10);
        REQUIRE(channel.framePtr(10) ==
                reinterpret_cast<float*>(channel.data() + 10));
        REQUIRE(channel.load(10) == 100.0f);
    }

    SECTION("A const descriptor retains shallow write access")
    {
        const auto& descriptor = buffer;
        descriptor.store(0, 2, 7.0f);
        descriptor.channel(1).store(3, 8.0f);

        REQUIRE(buffer.load(0, 2) == 7.0f);
        REQUIRE(buffer.load(1, 3) == 8.0f);
    }
}

TEST_CASE("BufferView read-only conversion shares descriptor state",
          "[dsp][buffer]")
{
    constexpr std::size_t frames = 16;
    PlanarStorage<float> storage{2, frames};
    auto mutable_buffer = storage.view();

    SECTION("Mutable host pointers construct a read-only view")
    {
        applause::BufferView<const float> read_only_buffer{
            storage.channel_ptrs.data(), storage.channel_ptrs.size(), frames};

        REQUIRE(read_only_buffer.numChannels() == 2);
        REQUIRE(read_only_buffer.numFrames() == frames);
        REQUIRE(read_only_buffer.channelSamples(0) == storage.scalar_data);
    }

    SECTION("Writable views convert without copying samples or pointers")
    {
        applause::BufferView<const float> read_only_buffer = mutable_buffer;
        mutable_buffer.store(1, 4, 12.0f);

        REQUIRE(read_only_buffer.channelSamples(1) ==
                mutable_buffer.channelSamples(1));
        REQUIRE(read_only_buffer.load(1, 4) == 12.0f);

        alignas(64) std::array<float, frames> replacement{};
        storage.channel_ptrs[1] = replacement.data();
        replacement[4] = 24.0f;

        REQUIRE(read_only_buffer.channelSamples(1) == replacement.data());
        REQUIRE(read_only_buffer.load(1, 4) == 24.0f);
    }

    SECTION("Read-only conversion preserves a subview offset")
    {
        auto mutable_subview = mutable_buffer.getSubView(3, 9);
        applause::BufferView<const float> read_only_subview = mutable_subview;

        static_assert(std::same_as<decltype(read_only_subview),
                                   applause::BufferView<const float>>);
        REQUIRE(read_only_subview.numFrames() == 6);
        REQUIRE(read_only_subview.channelSamples(0) ==
                storage.scalar_data + 3);
    }
}

TEST_CASE("BufferView subviews are offset-aware", "[dsp][buffer]")
{
    constexpr std::size_t frames = 64;
    PlanarStorage<float> storage{2, frames};
    auto buffer = storage.view();

    for (std::size_t channel = 0; channel < buffer.numChannels(); ++channel) {
        for (std::size_t frame = 0; frame < frames; ++frame) {
            buffer.store(channel, frame,
                         static_cast<float>(channel * 1000 + frame));
        }
    }

    SECTION("Subview exposes the requested frame range")
    {
        auto subview = buffer.getSubView(10, 30);

        REQUIRE(subview.numFrames() == 20);
        REQUIRE(subview.numChannels() == 2);
        REQUIRE(subview.channelSamples(0) ==
                buffer.channelSamples(0) + 10);
        REQUIRE(subview.channelSampleSpan(1).data() ==
                buffer.channelSamples(1) + 10);
        REQUIRE(subview.load(0, 0) == 10.0f);
        REQUIRE(subview.load(1, 19) == 1029.0f);
    }

    SECTION("Nested subviews accumulate their frame offsets")
    {
        auto first = buffer.getSubView(10, 50);
        auto second = first.getSubView(5, 15);

        REQUIRE(second.numFrames() == 10);
        REQUIRE(second.channelSamples(0) ==
                buffer.channelSamples(0) + 15);
        REQUIRE(second.load(0, 0) == 15.0f);
        REQUIRE(second.load(0, 9) == 24.0f);

        second.store(1, 3, 12345.0f);
        REQUIRE(buffer.load(1, 18) == 12345.0f);
    }

    SECTION("Only a full-width multi-channel subview is contiguous")
    {
        REQUIRE(buffer.getSubView(0, frames).isContiguous());
        REQUIRE_FALSE(buffer.getSubView(10, 30).isContiguous());

        PlanarStorage<float> mono_storage{1, frames};
        REQUIRE(mono_storage.view().getSubView(10, 30).isContiguous());
    }

    SECTION("Empty subview retains the selected position")
    {
        auto subview = buffer.getSubView(20, 20);

        REQUIRE(subview.numFrames() == 0);
        REQUIRE(subview.channelSamples(0) ==
                buffer.channelSamples(0) + 20);
        REQUIRE(subview.channelSampleSpan(0).empty());
    }
}

TEST_CASE("BufferView clear operations honor subview offsets",
          "[dsp][buffer]")
{
    constexpr std::size_t frames = 12;
    PlanarStorage<float> storage{2, frames};
    auto buffer = storage.view();

    for (std::size_t channel = 0; channel < buffer.numChannels(); ++channel) {
        for (std::size_t frame = 0; frame < frames; ++frame) {
            buffer.store(channel, frame, 1.0f);
        }
    }

    SECTION("clear zeros only the selected frame range")
    {
        buffer.getSubView(3, 9).clear();

        for (std::size_t channel = 0; channel < buffer.numChannels();
             ++channel) {
            for (std::size_t frame = 0; frame < frames; ++frame) {
                REQUIRE(buffer.load(channel, frame) ==
                        (frame >= 3 && frame < 9 ? 0.0f : 1.0f));
            }
        }
    }

    SECTION("clearChannel zeros one channel in the selected frame range")
    {
        buffer.getSubView(3, 9).clearChannel(0);

        for (std::size_t frame = 0; frame < frames; ++frame) {
            REQUIRE(buffer.load(0, frame) ==
                    (frame >= 3 && frame < 9 ? 0.0f : 1.0f));
            REQUIRE(buffer.load(1, frame) == 1.0f);
        }
    }

    SECTION("Clearing a default or zero-frame view is safe")
    {
        applause::BufferView<float> empty;
        empty.clear();
        buffer.getSubView(5, 5).clear();

        REQUIRE(empty.numFrames() == 0);
        REQUIRE(buffer.load(0, 5) == 1.0f);
    }
}

TEMPLATE_TEST_CASE("BufferView supports scalar and SIMD samples",
                   "[dsp][buffer][simd]", float, double,
                   xsimd::batch<float>, xsimd::batch<double>)
{
    using SampleType = TestType;
    using Scalar = applause::scalar_t<SampleType>;
    constexpr std::size_t width = applause::sampleWidth<SampleType>();
    constexpr std::size_t frames = 32;

    PlanarStorage<SampleType> storage{2, frames};
    auto buffer = storage.view();

    REQUIRE(buffer.sample_width == width);
    REQUIRE(buffer.is_simd == applause::SimdBatch<SampleType>);
    REQUIRE(buffer.scalarsPerChannel() == frames * width);
    REQUIRE(buffer.isContiguous());
    REQUIRE(reinterpret_cast<std::uintptr_t>(buffer.channelScalars(0)) %
                applause::sampleAlignment<SampleType>() ==
            0);
    REQUIRE(buffer.channelScalarSpan(0).size() == frames * width);
    REQUIRE(buffer.channelScalars(1) ==
            buffer.channelScalars(0) + frames * width);

    SampleType initial = applause::set1<SampleType>(Scalar{10});
    buffer.store(0, 5, initial);
    for (std::size_t lane = 0; lane < width; ++lane) {
        REQUIRE(buffer.channelScalars(0)[5 * width + lane] == Scalar{10});
    }

    buffer.add(0, 5, Scalar{5});
    SampleType result = buffer.load(0, 5);

    if constexpr (applause::SimdBatch<SampleType>) {
        for (std::size_t lane = 0; lane < SampleType::size; ++lane) {
            REQUIRE(result.get(lane) == Scalar{15});
        }
    } else {
        REQUIRE(result == Scalar{15});
    }

    auto channel = buffer.channel(1);
    channel.store(7, applause::set1<SampleType>(Scalar{20}));
    channel.add(7, Scalar{2});
    REQUIRE(channel.scalarData() == buffer.channelScalars(1));
    REQUIRE(channel.framePtr(7) ==
            channel.scalarData() + 7 * width);
    const auto channel_result = channel.load(7);
    if constexpr (applause::SimdBatch<SampleType>) {
        for (std::size_t lane = 0; lane < SampleType::size; ++lane) {
            REQUIRE(channel_result.get(lane) == Scalar{22});
        }
    } else {
        REQUIRE(channel_result == Scalar{22});
    }

    applause::BufferView<const SampleType> read_only = buffer;
    REQUIRE(read_only.channelScalars(0) == buffer.channelScalars(0));

    for (std::size_t channel = 0; channel < buffer.numChannels(); ++channel) {
        for (auto& sample : buffer.channelScalarSpan(channel))
            sample = static_cast<Scalar>(channel + 1);
    }

    auto subview = buffer.getSubView(4, 8);
    REQUIRE(subview.channelScalars(0) ==
            buffer.channelScalars(0) + 4 * width);
    subview.clear();
    for (std::size_t channel = 0; channel < buffer.numChannels(); ++channel) {
        for (std::size_t frame = 0; frame < frames; ++frame) {
            const Scalar expected = frame >= 4 && frame < 8 ? Scalar{0} : static_cast<Scalar>(channel + 1);
            for (std::size_t lane = 0; lane < width; ++lane) {
                CAPTURE(channel, frame, lane);
                REQUIRE(buffer.channelScalars(channel)[frame * width + lane] == expected);
            }
        }
    }
}

TEST_CASE("BufferView with runtime MemoryArena allocation",
          "[dsp][buffer][memory]")
{
    alignas(64) std::array<std::byte, 32768> arena_backing{};
    applause::MemoryArena arena{arena_backing.data(), arena_backing.size()};

    SECTION("Runtime channel counts are valid and contiguous")
    {
        constexpr std::size_t channels = 12;
        constexpr std::size_t frames = 32;
        auto buffer = arena.allocateAudioBuffer<float>(channels, frames);

        REQUIRE(buffer.isValid());
        REQUIRE(buffer.isContiguous());
        REQUIRE(buffer.numChannels() == channels);
        REQUIRE(buffer.numFrames() == frames);
        REQUIRE(buffer.channelSamples(11) ==
                buffer.channelSamples(0) + 11 * frames);
        REQUIRE(arena.getBytesUsed() >=
                channels * sizeof(float*) +
                    channels * frames * sizeof(float));
    }

    SECTION("SIMD planes are aligned and correctly spaced")
    {
        using Batch = xsimd::batch<float>;
        constexpr std::size_t frames = 16;
        constexpr std::size_t width = applause::sampleWidth<Batch>();
        auto buffer = arena.allocateAudioBuffer<Batch>(2, frames);

        REQUIRE(reinterpret_cast<std::uintptr_t>(buffer.channelScalars(0)) %
                    applause::sampleAlignment<Batch>() ==
                0);
        REQUIRE(buffer.channelScalars(1) ==
                buffer.channelScalars(0) + frames * width);
    }

    SECTION("Multiple buffers are independent")
    {
        auto first = arena.allocateAudioBuffer<float>(2, 32);
        auto second = arena.allocateAudioBuffer<float>(2, 32);

        first.store(0, 0, 100.0f);
        second.store(0, 0, 200.0f);

        REQUIRE(first.load(0, 0) == 100.0f);
        REQUIRE(second.load(0, 0) == 200.0f);
        REQUIRE(first.channelSamples(0) != second.channelSamples(0));
    }

    SECTION("Zero dimensions return a canonical empty view without allocation")
    {
        const auto initial_bytes = arena.getBytesUsed();
        auto no_channels = arena.allocateAudioBuffer<float>(0, 32);
        auto no_frames = arena.allocateAudioBuffer<float>(2, 0);

        REQUIRE(no_channels.numChannels() == 0);
        REQUIRE(no_channels.numFrames() == 0);
        REQUIRE(no_frames.numChannels() == 0);
        REQUIRE(no_frames.numFrames() == 0);
        REQUIRE(arena.getBytesUsed() == initial_bytes);
    }

    SECTION("Arena frames reclaim both the table and sample storage")
    {
        const auto initial_bytes = arena.getBytesUsed();
        {
            auto frame = arena.createFrame();
            auto buffer = arena.allocateAudioBuffer<float>(2, 32);
            REQUIRE(buffer.isValid());
            REQUIRE(arena.getBytesUsed() > initial_bytes);
        }

        REQUIRE(arena.getBytesUsed() == initial_bytes);
    }

    SECTION("Arena buffers convert to read-only views")
    {
        auto mutable_buffer = arena.allocateAudioBuffer<float>(2, 32);
        applause::BufferView<const float> read_only = mutable_buffer;

        mutable_buffer.store(1, 3, 42.0f);
        REQUIRE(read_only.load(1, 3) == 42.0f);
    }
}
