#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <applause/dsp/BufferView.h>
#include <applause/util/MemoryArena.h>
#include <applause/util/SampleType.h>

#include <array>
#include <concepts>
#include <span>
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

using MutableFloatBuffer = applause::BufferView<float, 2>;
using ReadOnlyFloatBuffer = applause::BufferView<const float, 2>;

static_assert(std::constructible_from<ReadOnlyFloatBuffer,
                                      MutableFloatBuffer>);
static_assert(!std::constructible_from<MutableFloatBuffer,
                                       ReadOnlyFloatBuffer>);
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

}  // namespace

TEST_CASE("BufferView default construction", "[dsp][buffer]")
{
    applause::BufferView<float, 2> buffer;

    SECTION("Empty buffer has zero frames")
    {
        REQUIRE(buffer.numFrames() == 0);
    }

    SECTION("Empty buffer has zero channels")
    {
        REQUIRE(buffer.numChannels() == 0);
    }

    SECTION("Empty buffer is valid")
    {
        REQUIRE(buffer.isValid());
    }
}

TEST_CASE("BufferView contiguous construction", "[dsp][buffer]")
{
    constexpr std::size_t frames = 64;
    constexpr std::size_t channels = 4;
    alignas(64) std::array<float, frames * channels> backing{};

    SECTION("2-arg constructor uses MaxChannels")
    {
        applause::BufferView<float, 4> buffer{backing.data(), frames};
        REQUIRE(buffer.numChannels() == 4);
        REQUIRE(buffer.numFrames() == frames);
    }

    SECTION("3-arg constructor sets explicit channel count")
    {
        applause::BufferView<float, 8> buffer{backing.data(), 2, frames};
        REQUIRE(buffer.numChannels() == 2);
        REQUIRE(buffer.numFrames() == frames);
    }

    SECTION("Contiguous buffer reports isContiguous")
    {
        applause::BufferView<float, 4> buffer{backing.data(), channels, frames};
        REQUIRE(buffer.isContiguous());
    }

    SECTION("Channel pointers are sequential")
    {
        applause::BufferView<float, 4> buffer{backing.data(), channels, frames};
        for (std::size_t ch = 0; ch < channels; ++ch)
        {
            float* expected = backing.data() + ch * frames;
            REQUIRE(buffer.channelSamples(ch) == expected);
        }
    }

    SECTION("Null pointer with zero frames is valid")
    {
        applause::BufferView<float, 2> buffer{nullptr, 0};
        REQUIRE(buffer.isValid());
        REQUIRE(buffer.numFrames() == 0);
    }
}

TEST_CASE("BufferView non-contiguous construction", "[dsp][buffer]")
{
    constexpr std::size_t frames = 64;
    alignas(64) std::array<float, frames> ch0_data{};
    alignas(64) std::array<float, frames> ch1_data{};

    std::array<float*, 2> channel_ptrs = {ch0_data.data(), ch1_data.data()};

    SECTION("Host pointer array construction")
    {
        applause::BufferView<float, 2> buffer{channel_ptrs.data(), 2, frames};
        REQUIRE(buffer.numChannels() == 2);
        REQUIRE(buffer.numFrames() == frames);
        REQUIRE(buffer.isValid());
    }

    SECTION("Non-contiguous buffer reports correctly")
    {
        applause::BufferView<float, 2> buffer{channel_ptrs.data(), 2, frames};
        REQUIRE_FALSE(buffer.isContiguous());
    }

    SECTION("Channel pointers match input")
    {
        applause::BufferView<float, 2> buffer{channel_ptrs.data(), 2, frames};
        REQUIRE(buffer.channelSamples(0) == ch0_data.data());
        REQUIRE(buffer.channelSamples(1) == ch1_data.data());
    }
}

TEST_CASE("BufferView accessors", "[dsp][buffer]")
{
    constexpr std::size_t frames = 64;
    constexpr std::size_t channels = 2;
    alignas(64) std::array<float, frames * channels> backing{};
    applause::BufferView<float, 4> buffer{backing.data(), channels, frames};

    SECTION("numFrames returns frame count")
    {
        REQUIRE(buffer.numFrames() == frames);
    }

    SECTION("numChannels returns channel count")
    {
        REQUIRE(buffer.numChannels() == channels);
    }

    SECTION("samplesPerChannel equals numFrames for scalar types")
    {
        REQUIRE(buffer.samplesPerChannel() == frames);
    }

    SECTION("scalarsPerChannel accounts for width")
    {
        REQUIRE(buffer.scalarsPerChannel() == frames * 1);
    }

    SECTION("channelSamples returns correct pointer")
    {
        REQUIRE(buffer.channelSamples(0) != nullptr);
        REQUIRE(buffer.channelSamples(1) != nullptr);
        REQUIRE(buffer.channelSamples(0) == backing.data());
        REQUIRE(buffer.channelSamples(1) == backing.data() + frames);
    }

    SECTION("channelSampleSpan has correct size")
    {
        auto span0 = buffer.channelSampleSpan(0);
        auto span1 = buffer.channelSampleSpan(1);
        REQUIRE(span0.size() == frames);
        REQUIRE(span1.size() == frames);
    }

    SECTION("const channelSamples works")
    {
        const auto& const_buffer = buffer;
        REQUIRE(const_buffer.channelSamples(0) == backing.data());
    }
}

TEST_CASE("BufferView load/store", "[dsp][buffer]")
{
    constexpr std::size_t frames = 32;
    constexpr std::size_t channels = 2;
    alignas(64) std::array<float, frames * channels> backing{};
    applause::BufferView<float, 2> buffer{backing.data(), channels, frames};

    SECTION("store and load round-trip")
    {
        buffer.store(0, 5, 42.0f);
        REQUIRE(buffer.load(0, 5) == 42.0f);

        buffer.store(1, 10, -3.14f);
        REQUIRE(buffer.load(1, 10) == -3.14f);
    }

    SECTION("store affects correct location")
    {
        buffer.clear();
        buffer.store(0, 5, 100.0f);

        REQUIRE(buffer.load(0, 5) == 100.0f);
        REQUIRE(buffer.load(0, 0) == 0.0f);
        REQUIRE(buffer.load(0, 4) == 0.0f);
        REQUIRE(buffer.load(0, 6) == 0.0f);
        REQUIRE(buffer.load(1, 5) == 0.0f);
    }

    SECTION("Multiple frames accessible")
    {
        for (std::size_t fr = 0; fr < frames; ++fr)
        {
            buffer.store(0, fr, static_cast<float>(fr));
        }

        for (std::size_t fr = 0; fr < frames; ++fr)
        {
            REQUIRE(buffer.load(0, fr) == static_cast<float>(fr));
        }
    }

    SECTION("Channel boundary respected")
    {
        for (std::size_t fr = 0; fr < frames; ++fr)
        {
            buffer.store(0, fr, 1000.0f + static_cast<float>(fr));
            buffer.store(1, fr, 2000.0f + static_cast<float>(fr));
        }

        for (std::size_t fr = 0; fr < frames; ++fr)
        {
            REQUIRE(buffer.load(0, fr) == 1000.0f + static_cast<float>(fr));
            REQUIRE(buffer.load(1, fr) == 2000.0f + static_cast<float>(fr));
        }
    }
}

TEST_CASE("BufferView const element access", "[dsp][buffer]")
{
    constexpr std::size_t frames = 16;
    alignas(64) std::array<float, frames * 2> backing{};
    std::array<float*, 2> channels{backing.data(), backing.data() + frames};
    applause::BufferView<float, 2> mutable_buffer{channels.data(), 2, frames};

    SECTION("mutable host channel pointers construct a read-only view")
    {
        applause::BufferView<const float, 2> read_only_buffer{
            channels.data(), 2, frames};

        REQUIRE(read_only_buffer.numChannels() == 2);
        REQUIRE(read_only_buffer.numFrames() == frames);
        REQUIRE(read_only_buffer.channelSamples(0) == backing.data());
        REQUIRE(read_only_buffer.channel(1).data() == backing.data() + frames);
    }

    SECTION("mutable views convert to read-only views without copying")
    {
        mutable_buffer.store(1, 4, 12.0f);
        applause::BufferView<const float, 2> read_only_buffer = mutable_buffer;

        REQUIRE(read_only_buffer.channelSamples(1) ==
                mutable_buffer.channelSamples(1));
        REQUIRE(read_only_buffer.load(1, 4) == 12.0f);

        mutable_buffer.store(1, 4, 24.0f);
        REQUIRE(read_only_buffer.load(1, 4) == 24.0f);
    }

    SECTION("read-only subviews preserve const element access")
    {
        applause::BufferView<const float, 2> read_only_buffer = mutable_buffer;
        auto subview = read_only_buffer.getSubView(3, 9);

        static_assert(std::same_as<decltype(subview),
                                   applause::BufferView<const float, 2>>);
        REQUIRE(subview.numFrames() == 6);
        REQUIRE(subview.channelSamples(0) == backing.data() + 3);
    }

    SECTION("const mutable descriptors retain shallow write access")
    {
        const auto& descriptor = mutable_buffer;
        descriptor.store(0, 2, 7.0f);
        descriptor.channel(1).store(3, 8.0f);

        REQUIRE(backing[2] == 7.0f);
        REQUIRE(backing[frames + 3] == 8.0f);
    }
}

TEST_CASE("BufferView add operation", "[dsp][buffer]")
{
    constexpr std::size_t frames = 16;
    alignas(64) std::array<float, frames * 2> backing{};
    applause::BufferView<float, 2> buffer{backing.data(), 2, frames};

    SECTION("add accumulates scalar value")
    {
        buffer.clear();
        buffer.store(0, 5, 10.0f);
        buffer.add(0, 5, 3.0f);
        REQUIRE(buffer.load(0, 5) == 13.0f);
    }

    SECTION("add works on zeroed buffer")
    {
        buffer.clear();
        buffer.add(0, 5, 7.0f);
        REQUIRE(buffer.load(0, 5) == 7.0f);
    }

    SECTION("Multiple adds accumulate")
    {
        buffer.clear();
        buffer.add(1, 10, 1.0f);
        buffer.add(1, 10, 2.0f);
        buffer.add(1, 10, 3.0f);
        REQUIRE(buffer.load(1, 10) == 6.0f);
    }
}

TEST_CASE("BufferView ChannelView", "[dsp][buffer]")
{
    constexpr std::size_t frames = 32;
    alignas(64) std::array<float, frames * 2> backing{};
    applause::BufferView<float, 2> buffer{backing.data(), 2, frames};

    SECTION("channel() returns valid view")
    {
        auto view = buffer.channel(0);
        REQUIRE(view.data() != nullptr);
        REQUIRE(view.frames() == frames);
    }

    SECTION("ChannelView load/store round-trip")
    {
        auto view = buffer.channel(1);
        view.store(10, 99.0f);
        REQUIRE(view.load(10) == 99.0f);
    }

    SECTION("ChannelView add works")
    {
        buffer.clear();
        auto view = buffer.channel(0);
        view.store(5, 10.0f);
        view.add(5, 5.0f);
        REQUIRE(view.load(5) == 15.0f);
    }

    SECTION("ChannelView data() returns base")
    {
        auto view = buffer.channel(0);
        REQUIRE(view.data() == buffer.channelSamples(0));
    }

    SECTION("ChannelView frames() correct")
    {
        auto view = buffer.channel(1);
        REQUIRE(view.frames() == buffer.numFrames());
    }

    SECTION("samplePtr returns correct address")
    {
        auto view = buffer.channel(0);
        REQUIRE(view.samplePtr(5) == view.data() + 5);
    }

    SECTION("framePtr returns scalar pointer")
    {
        auto view = buffer.channel(0);
        auto* frame_ptr = view.framePtr(5);
        REQUIRE(frame_ptr == reinterpret_cast<float*>(view.data() + 5));
    }
}

TEST_CASE("BufferView getSubView", "[dsp][buffer]")
{
    constexpr std::size_t frames = 64;
    constexpr std::size_t channels = 2;
    alignas(64) std::array<float, frames * channels> backing{};
    applause::BufferView<float, 2> buffer{backing.data(), channels, frames};

    // Fill with distinctive values
    for (std::size_t ch = 0; ch < channels; ++ch)
    {
        for (std::size_t fr = 0; fr < frames; ++fr)
        {
            buffer.store(ch, fr, static_cast<float>(ch * 1000 + fr));
        }
    }

    SECTION("SubView has correct frame count")
    {
        auto sub = buffer.getSubView(10, 30);
        REQUIRE(sub.numFrames() == 20);
    }

    SECTION("SubView preserves channel count")
    {
        auto sub = buffer.getSubView(10, 30);
        REQUIRE(sub.numChannels() == channels);
    }

    SECTION("SubView data points to correct offset")
    {
        auto sub = buffer.getSubView(10, 30);
        // SubView's channel 0 should point to parent's frame 10
        REQUIRE(sub.channelSamples(0) == buffer.channelSamples(0) + 10);
    }

    SECTION("Full-range subview equals original")
    {
        auto sub = buffer.getSubView(0, frames);
        REQUIRE(sub.numFrames() == buffer.numFrames());
        REQUIRE(sub.channelSamples(0) == buffer.channelSamples(0));
    }

    SECTION("Empty subview for equal bounds")
    {
        auto sub = buffer.getSubView(20, 20);
        REQUIRE(sub.numFrames() == 0);
    }

    SECTION("Writes through subview affect parent")
    {
        auto sub = buffer.getSubView(10, 30);
        sub.store(0, 5, 12345.0f);  // sub[0,5] aliases parent[0,15]
        REQUIRE(buffer.load(0, 15) == 12345.0f);
    }

    SECTION("SubView of SubView works")
    {
        auto sub1 = buffer.getSubView(10, 50);
        auto sub2 = sub1.getSubView(5, 15);  // = parent frames 15-24

        REQUIRE(sub2.numFrames() == 10);
        REQUIRE(sub2.channelSamples(0) == buffer.channelSamples(0) + 15);

        REQUIRE(sub2.load(0, 0) == 15.0f);
        REQUIRE(sub2.load(0, 9) == 24.0f);
    }
}

TEST_CASE("BufferView clear operations", "[dsp][buffer]")
{
    constexpr std::size_t frames = 32;
    constexpr std::size_t channels = 2;
    alignas(64) std::array<float, frames * channels> backing{};
    applause::BufferView<float, 2> buffer{backing.data(), channels, frames};

    SECTION("clear zeros all channels")
    {
        for (std::size_t ch = 0; ch < channels; ++ch)
        {
            for (std::size_t fr = 0; fr < frames; ++fr)
            {
                buffer.store(ch, fr, 100.0f);
            }
        }

        buffer.clear();

        for (std::size_t ch = 0; ch < channels; ++ch)
        {
            for (std::size_t fr = 0; fr < frames; ++fr)
            {
                REQUIRE(buffer.load(ch, fr) == 0.0f);
            }
        }
    }

    SECTION("clear on empty buffer is safe")
    {
        applause::BufferView<float, 2> empty;
        empty.clear();
        REQUIRE(empty.numFrames() == 0);
    }

    SECTION("clearChannel zeros one channel")
    {
        for (std::size_t fr = 0; fr < frames; ++fr)
        {
            buffer.store(0, fr, 50.0f);
            buffer.store(1, fr, 100.0f);
        }

        buffer.clearChannel(0);

        for (std::size_t fr = 0; fr < frames; ++fr)
        {
            REQUIRE(buffer.load(0, fr) == 0.0f);
        }
    }

    SECTION("clearChannel preserves others")
    {
        for (std::size_t fr = 0; fr < frames; ++fr)
        {
            buffer.store(0, fr, 50.0f);
            buffer.store(1, fr, 100.0f);
        }

        buffer.clearChannel(0);

        for (std::size_t fr = 0; fr < frames; ++fr)
        {
            REQUIRE(buffer.load(1, fr) == 100.0f);
        }
    }

    SECTION("clear after writes resets all")
    {
        buffer.store(0, 10, 999.0f);
        buffer.store(1, 20, 888.0f);
        buffer.clear();
        REQUIRE(buffer.load(0, 10) == 0.0f);
        REQUIRE(buffer.load(1, 20) == 0.0f);
    }
}

TEST_CASE("BufferView edge cases", "[dsp][buffer]")
{
    SECTION("Single channel buffer works")
    {
        alignas(64) std::array<float, 64> backing{};
        applause::BufferView<float, 1> buffer{backing.data(), 1, 64};

        REQUIRE(buffer.numChannels() == 1);
        buffer.store(0, 10, 42.0f);
        REQUIRE(buffer.load(0, 10) == 42.0f);
    }

    SECTION("Single frame buffer works")
    {
        alignas(64) std::array<float, 2> backing{};
        applause::BufferView<float, 2> buffer{backing.data(), 2, 1};

        REQUIRE(buffer.numFrames() == 1);
        buffer.store(0, 0, 1.0f);
        buffer.store(1, 0, 2.0f);
        REQUIRE(buffer.load(0, 0) == 1.0f);
        REQUIRE(buffer.load(1, 0) == 2.0f);
    }

    SECTION("Max channels respects template")
    {
        alignas(64) std::array<float, 512> backing{};
        applause::BufferView<float, 8> buffer{backing.data(), 8, 64};
        REQUIRE(buffer.max_channel_count == 8);
        REQUIRE(buffer.numChannels() == 8);
    }

    SECTION("Zero frame buffer clear is safe")
    {
        applause::BufferView<float, 2> buffer{nullptr, 0};
        buffer.clear();
        REQUIRE(buffer.numFrames() == 0);
    }

    SECTION("Large frame count works")
    {
        constexpr std::size_t large_frames = 2048;
        std::vector<float> backing(large_frames * 2, 0.0f);
        applause::BufferView<float, 2> buffer{backing.data(), 2, large_frames};

        REQUIRE(buffer.numFrames() == large_frames);

        buffer.store(0, 0, 1.0f);
        buffer.store(0, large_frames - 1, 2.0f);
        buffer.store(1, large_frames - 1, 3.0f);

        REQUIRE(buffer.load(0, 0) == 1.0f);
        REQUIRE(buffer.load(0, large_frames - 1) == 2.0f);
        REQUIRE(buffer.load(1, large_frames - 1) == 3.0f);
    }
}

TEMPLATE_TEST_CASE("BufferView with SIMD types", "[dsp][buffer][simd]",
                   float, double, xsimd::batch<float>, xsimd::batch<double>)
{
    using SampleType = TestType;
    using Scalar = applause::scalar_t<SampleType>;
    constexpr std::size_t width = applause::sampleWidth<SampleType>();
    constexpr std::size_t frames = 32;
    constexpr std::size_t channels = 2;

    alignas(64) std::vector<Scalar> backing(frames * channels * width, Scalar{0});
    applause::BufferView<SampleType, 2> buffer{backing.data(), channels, frames};

    SECTION("sample_width correct")
    {
        if constexpr (applause::SimdBatch<SampleType>)
        {
            REQUIRE(buffer.sample_width == SampleType::size);
        }
        else
        {
            REQUIRE(buffer.sample_width == 1);
        }
    }

    SECTION("is_simd correct")
    {
        if constexpr (applause::SimdBatch<SampleType>)
        {
            REQUIRE(buffer.is_simd == true);
        }
        else
        {
            REQUIRE(buffer.is_simd == false);
        }
    }

    SECTION("scalarsPerChannel correct")
    {
        REQUIRE(buffer.scalarsPerChannel() == frames * width);
    }

    SECTION("store/load round-trip")
    {
        if constexpr (applause::SimdBatch<SampleType>)
        {
            SampleType value = applause::set1<SampleType>(Scalar{42});
            buffer.store(0, 5, value);
            SampleType loaded = buffer.load(0, 5);

            for (std::size_t i = 0; i < SampleType::size; ++i)
            {
                REQUIRE(loaded.get(i) == Scalar{42});
            }
        }
        else
        {
            buffer.store(0, 5, Scalar{42});
            REQUIRE(buffer.load(0, 5) == Scalar{42});
        }
    }

    SECTION("add broadcasts scalar to lanes")
    {
        buffer.clear();

        if constexpr (applause::SimdBatch<SampleType>)
        {
            SampleType initial = applause::set1<SampleType>(Scalar{10});
            buffer.store(0, 5, initial);

            buffer.add(0, 5, Scalar{5});

            SampleType result = buffer.load(0, 5);
            for (std::size_t i = 0; i < SampleType::size; ++i)
            {
                REQUIRE(result.get(i) == Scalar{15});
            }
        }
        else
        {
            buffer.store(0, 5, Scalar{10});
            buffer.add(0, 5, Scalar{5});
            REQUIRE(buffer.load(0, 5) == Scalar{15});
        }
    }

    SECTION("mutable view converts to const element view")
    {
        applause::BufferView<const SampleType, 2> read_only = buffer;
        REQUIRE(read_only.numChannels() == channels);
        REQUIRE(read_only.numFrames() == frames);
        REQUIRE(read_only.channelSamples(0) == buffer.channelSamples(0));
    }
}

TEST_CASE("Invalid BufferView construction returns an empty view",
          "[dsp][buffer]")
{
    constexpr std::size_t frames = 8;
    alignas(64) std::array<float, frames * 2> backing{};

    SECTION("channel count exceeding capacity")
    {
        applause::BufferView<float, 1> buffer{backing.data(), 2, frames};
        REQUIRE(buffer.numChannels() == 0);
        REQUIRE(buffer.numFrames() == 0);
        REQUIRE(buffer.isValid());
    }

    SECTION("null contiguous storage with frames")
    {
        applause::BufferView<float, 2> buffer{
            static_cast<float*>(nullptr), 2, frames};
        REQUIRE(buffer.numChannels() == 0);
        REQUIRE(buffer.numFrames() == 0);
        REQUIRE(buffer.isValid());
    }

    SECTION("null channel storage with frames")
    {
        std::array<float*, 2> channels{backing.data(), nullptr};
        applause::BufferView<float, 2> buffer{channels.data(), 2, frames};
        REQUIRE(buffer.numChannels() == 0);
        REQUIRE(buffer.numFrames() == 0);
        REQUIRE(buffer.isValid());
    }
}

TEST_CASE("BufferView type aliases", "[dsp][buffer]")
{
    SECTION("MonoBuffer has 1 channel max")
    {
        REQUIRE(applause::MonoBuffer::max_channel_count == 1);
    }

    SECTION("StereoBuffer has 2 channel max")
    {
        REQUIRE(applause::StereoBuffer::max_channel_count == 2);
    }

    SECTION("SurroundBuffer has 8 channel max")
    {
        REQUIRE(applause::SurroundBuffer::max_channel_count == 8);
    }

    SECTION("FlexBuffer has 8 channel max")
    {
        REQUIRE(applause::FlexBuffer::max_channel_count == 8);
    }
}

TEST_CASE("BufferView with MemoryArena allocation", "[dsp][buffer][memory]")
{
    alignas(64) std::array<std::byte, 8192> arena_backing{};
    applause::MemoryArena arena{arena_backing.data(), arena_backing.size()};

    SECTION("Arena-allocated buffer is valid")
    {
        auto buffer = arena.allocateAudioBuffer<float, 2>(64);
        REQUIRE(buffer.isValid());
        REQUIRE(buffer.numFrames() == 64);
        REQUIRE(buffer.numChannels() == 2);
    }

    SECTION("Arena-allocated buffer is contiguous")
    {
        auto buffer = arena.allocateAudioBuffer<float, 2>(64);
        REQUIRE(buffer.isContiguous());
    }

    SECTION("Buffer works within arena frame")
    {
        auto frame = arena.createFrame();
        auto buffer = arena.allocateAudioBuffer<float, 2>(32);

        buffer.store(0, 10, 42.0f);
        REQUIRE(buffer.load(0, 10) == 42.0f);

        buffer.clear();
        REQUIRE(buffer.load(0, 10) == 0.0f);
    }

    SECTION("Multiple buffers from arena are independent")
    {
        auto buffer1 = arena.allocateAudioBuffer<float, 2>(32);
        auto buffer2 = arena.allocateAudioBuffer<float, 2>(32);

        buffer1.store(0, 0, 100.0f);
        buffer2.store(0, 0, 200.0f);

        REQUIRE(buffer1.load(0, 0) == 100.0f);
        REQUIRE(buffer2.load(0, 0) == 200.0f);

        REQUIRE(buffer1.channelSamples(0) != buffer2.channelSamples(0));
    }
}
