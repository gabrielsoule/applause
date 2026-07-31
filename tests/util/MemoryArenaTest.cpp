#include <catch2/catch_test_macros.hpp>
#include <applause/util/MemoryArena.h>

#include <array>
#include <cstdint>
#include <limits>

TEST_CASE("snapPointerToAlignment", "[util][memory]")
{
    SECTION("Already aligned pointer unchanged")
    {
        alignas(64) std::byte buffer[128];
        auto* ptr = buffer;
        REQUIRE(applause::snapPointerToAlignment(ptr, 64) == ptr);
    }

    SECTION("Unaligned pointer snapped forward")
    {
        alignas(64) std::byte buffer[128];
        auto* ptr = buffer + 1;
        auto* aligned = applause::snapPointerToAlignment(ptr, 64);
        REQUIRE(reinterpret_cast<std::uintptr_t>(aligned) % 64 == 0);
        REQUIRE(aligned > ptr);
        REQUIRE(aligned <= buffer + 64);
    }
}

TEST_CASE("MemoryArena basic allocation", "[util][memory]")
{
    alignas(64) std::array<std::byte, 1024> backing{};
    applause::MemoryArena arena{backing.data(), backing.size()};

    SECTION("Initial state is empty")
    {
        REQUIRE(arena.getBytesUsed() == 0);
    }

    SECTION("allocateBytes tracks usage")
    {
        void* ptr = arena.allocateBytes(100);
        REQUIRE(ptr != nullptr);
        REQUIRE(arena.getBytesUsed() >= 100);
    }

    SECTION("allocate<T> returns typed pointer")
    {
        float* floats = arena.allocate<float>(10);
        REQUIRE(floats != nullptr);
        REQUIRE(arena.getBytesUsed() >= 10 * sizeof(float));

        // Verify we can write to the memory
        for (int i = 0; i < 10; ++i)
            floats[i] = static_cast<float>(i);

        REQUIRE(floats[5] == 5.0f);
    }

    SECTION("Aligned allocation respects alignment")
    {
        // Force some offset first
        arena.allocateBytes(3);

        float* aligned = arena.allocate<float>(4, 32);
        REQUIRE(reinterpret_cast<std::uintptr_t>(aligned) % 32 == 0);
    }

    SECTION("clear() resets usage to zero")
    {
        arena.allocateBytes(500);
        REQUIRE(arena.getBytesUsed() >= 500);

        arena.clear();
        REQUIRE(arena.getBytesUsed() == 0);
    }

    SECTION("clear() allows memory reuse from same address")
    {
        void* first = arena.allocateBytes(100);
        arena.clear();
        void* second = arena.allocateBytes(100);
        REQUIRE(second == first);
    }

    SECTION("Sequential allocations do not overlap")
    {
        int* a = arena.allocate<int>(10);
        int* b = arena.allocate<int>(10);
        REQUIRE(b >= a + 10);
    }

    SECTION("Move construction transfers ownership")
    {
        arena.allocateBytes(100);
        size_t original_used = arena.getBytesUsed();
        void* original_data = arena.raw_data_.data();

        applause::MemoryArena moved{std::move(arena)};

        REQUIRE(moved.getBytesUsed() == original_used);
        REQUIRE(moved.raw_data_.data() == original_data);
    }

    // Over-capacity allocation deliberately asserts; not exercised here.
}

TEST_CASE("MemoryArena frame behavior", "[util][memory]")
{
    alignas(64) std::array<std::byte, 1024> backing{};
    applause::MemoryArena arena{backing.data(), backing.size()};

    SECTION("Frame restores state on destruction")
    {
        arena.allocateBytes(100);
        size_t before_frame = arena.getBytesUsed();

        {
            auto frame = arena.createFrame();
            arena.allocateBytes(200);
            REQUIRE(arena.getBytesUsed() >= before_frame + 200);
        }

        REQUIRE(arena.getBytesUsed() == before_frame);
    }

    SECTION("Nested frames work correctly")
    {
        arena.allocateBytes(50);
        size_t level0 = arena.getBytesUsed();

        {
            auto frame1 = arena.createFrame();
            arena.allocateBytes(100);
            size_t level1 = arena.getBytesUsed();

            {
                auto frame2 = arena.createFrame();
                arena.allocateBytes(150);
                REQUIRE(arena.getBytesUsed() > level1);
            }

            REQUIRE(arena.getBytesUsed() == level1);
        }

        REQUIRE(arena.getBytesUsed() == level0);
    }

    SECTION("resetToFrame restores manually")
    {
        arena.allocateBytes(100);
        auto frame = arena.createFrame();
        size_t at_frame = arena.getBytesUsed();

        arena.allocateBytes(200);
        REQUIRE(arena.getBytesUsed() > at_frame);

        arena.resetToFrame(frame);
        REQUIRE(arena.getBytesUsed() == at_frame);
    }
}

TEST_CASE("MemoryArena makeSpan", "[util][memory]")
{
    alignas(64) std::array<std::byte, 1024> backing{};
    applause::MemoryArena arena{backing.data(), backing.size()};

    SECTION("Returns correctly sized span")
    {
        auto span = arena.makeSpan<int>(20);
        REQUIRE(span.size() == 20);

        for (size_t i = 0; i < span.size(); ++i)
            span[i] = static_cast<int>(i * 2);

        REQUIRE(span[10] == 20);
    }
}

TEST_CASE("MemoryArena data accessor", "[util][memory]")
{
    alignas(64) std::array<std::byte, 1024> backing{};
    applause::MemoryArena arena{backing.data(), backing.size()};

    SECTION("data<T>() returns pointer at byte offset")
    {
        auto* base = arena.data<std::byte>(0);
        REQUIRE(base == backing.data());

        auto* offset = arena.data<std::byte>(100);
        REQUIRE(offset == backing.data() + 100);
    }

    SECTION("data<T>() with typed pointer")
    {
        auto* floats = arena.data<float>(64);
        REQUIRE(reinterpret_cast<std::byte*>(floats) == backing.data() + 64);
    }
}

TEST_CASE("MemoryArena allocateAudioBuffer", "[util][memory]")
{
    alignas(64) std::array<std::byte, 4096> backing{};
    applause::MemoryArena arena{backing.data(), backing.size()};

    SECTION("Stereo buffer allocates its pointer table and sample planes")
    {
        constexpr size_t frames = 64;
        constexpr size_t channels = 2;
        constexpr size_t pointer_table_bytes = channels * sizeof(float*);
        constexpr size_t sample_offset =
            (pointer_table_bytes + applause::defaultByteAlignment - 1) &
            ~(applause::defaultByteAlignment - 1);
        constexpr size_t expected_bytes =
            sample_offset + frames * channels * sizeof(float);

        auto buffer = arena.allocateAudioBuffer<float>(channels, frames);

        REQUIRE(buffer.numChannels() == channels);
        REQUIRE(buffer.numFrames() == frames);
        REQUIRE(arena.getBytesUsed() == expected_bytes);

        auto** stored_channels = reinterpret_cast<float**>(backing.data());
        REQUIRE(stored_channels[0] == buffer.channelSamples(0));
        REQUIRE(stored_channels[1] == buffer.channelSamples(1));
        REQUIRE(buffer.channelSamples(1) ==
                buffer.channelSamples(0) + frames);
    }

    SECTION("Runtime mono and large channel counts are supported")
    {
        auto mono = arena.allocateAudioBuffer<float>(1, 16);
        auto large = arena.allocateAudioBuffer<float>(12, 8);

        REQUIRE(mono.numChannels() == 1);
        REQUIRE(mono.numFrames() == 16);
        REQUIRE(large.numChannels() == 12);
        REQUIRE(large.numFrames() == 8);
        for (size_t channel = 1; channel < large.numChannels(); ++channel) {
            REQUIRE(large.channelSamples(channel) ==
                    large.channelSamples(channel - 1) + large.numFrames());
        }
    }

    SECTION("A zero dimension returns a canonical empty view")
    {
        auto no_channels = arena.allocateAudioBuffer<float>(0, 64);
        auto no_frames = arena.allocateAudioBuffer<float>(2, 0);

        REQUIRE(arena.getBytesUsed() == 0);
        REQUIRE(no_channels.numChannels() == 0);
        REQUIRE(no_channels.numFrames() == 0);
        REQUIRE(no_frames.numChannels() == 0);
        REQUIRE(no_frames.numFrames() == 0);
    }

    SECTION("Requested sample alignment is respected")
    {
        alignas(128) std::array<std::byte, 4096> aligned_backing{};
        applause::MemoryArena aligned_arena{aligned_backing.data(),
                                            aligned_backing.size()};
        aligned_arena.allocateBytes(3);

        auto buffer = aligned_arena.allocateAudioBuffer<float>(2, 17, 128);

        REQUIRE(reinterpret_cast<std::uintptr_t>(buffer.channelSamples(0)) %
                    128 ==
                0);
        REQUIRE(buffer.channelSamples(1) ==
                buffer.channelSamples(0) + buffer.numFrames());
    }

    SECTION("SIMD sample planes are aligned and correctly spaced")
    {
        using Batch = xsimd::batch<float>;
        constexpr size_t frames = 7;
        constexpr size_t width = applause::sampleWidth<Batch>();

        auto buffer = arena.allocateAudioBuffer<Batch>(3, frames);

        REQUIRE(reinterpret_cast<std::uintptr_t>(buffer.channelScalars(0)) %
                    applause::sampleAlignment<Batch>() ==
                0);
        REQUIRE(buffer.channelScalars(1) ==
                buffer.channelScalars(0) + frames * width);
        REQUIRE(buffer.channelScalars(2) ==
                buffer.channelScalars(1) + frames * width);

        buffer.store(1, 3, applause::set1<Batch>(4.0f));
        buffer.add(1, 3, 2.0f);
        const auto result = buffer.load(1, 3);
        for (size_t lane = 0; lane < Batch::size; ++lane) {
            REQUIRE(result.get(lane) == 6.0f);
            REQUIRE(buffer.channelScalars(1)[3 * width + lane] == 6.0f);
        }
    }

    SECTION("Double and double-SIMD sample planes remain aligned")
    {
        constexpr size_t frames = 7;
        auto scalar = arena.allocateAudioBuffer<double>(2, frames);

        REQUIRE(reinterpret_cast<std::uintptr_t>(scalar.channelSamples(0)) %
                    alignof(double) ==
                0);
        REQUIRE(scalar.channelSamples(1) ==
                scalar.channelSamples(0) + frames);

        using Batch = xsimd::batch<double>;
        constexpr size_t width = applause::sampleWidth<Batch>();
        auto simd = arena.allocateAudioBuffer<Batch>(2, frames);

        REQUIRE(reinterpret_cast<std::uintptr_t>(simd.channelScalars(0)) %
                    applause::sampleAlignment<Batch>() ==
                0);
        REQUIRE(simd.channelScalars(1) ==
                simd.channelScalars(0) + frames * width);
    }

    SECTION("Independent buffers do not overlap")
    {
        constexpr size_t frames = 32;
        auto first = arena.allocateAudioBuffer<float>(2, frames);
        auto second = arena.allocateAudioBuffer<float>(2, frames);

        const auto first_end = reinterpret_cast<std::uintptr_t>(
            first.channelSamples(1) + frames);
        const auto second_start =
            reinterpret_cast<std::uintptr_t>(second.channelSamples(0));
        REQUIRE(first_end <= second_start);
    }

    SECTION("A writable buffer converts to a borrowed read-only view")
    {
        auto buffer = arena.allocateAudioBuffer<float>(2, 32);
        applause::BufferView<const float> read_only = buffer;

        REQUIRE(read_only.numChannels() == buffer.numChannels());
        REQUIRE(read_only.numFrames() == buffer.numFrames());
        REQUIRE(read_only.channelSamples(0) == buffer.channelSamples(0));
        REQUIRE(read_only.channelSamples(1) == buffer.channelSamples(1));
    }

    SECTION("Works correctly with Frame")
    {
        arena.allocateBytes(64);
        size_t before = arena.getBytesUsed();
        {
            auto frame = arena.createFrame();
            auto buffer = arena.allocateAudioBuffer<float>(2, 64);
            REQUIRE(arena.getBytesUsed() > before);
        }
        REQUIRE(arena.getBytesUsed() == before);
    }

#ifdef NDEBUG
    SECTION("Invalid alignment does not consume arena memory")
    {
        arena.allocateBytes(3);
        const auto bytes_before = arena.getBytesUsed();
        auto buffer = arena.allocateAudioBuffer<float>(2, 64, 3);

        REQUIRE(buffer.numChannels() == 0);
        REQUIRE(buffer.numFrames() == 0);
        REQUIRE(arena.getBytesUsed() == bytes_before);
    }

    SECTION("Overflow does not consume arena memory")
    {
        arena.allocateBytes(3);
        const auto bytes_before = arena.getBytesUsed();
        auto buffer = arena.allocateAudioBuffer<float>(
            std::numeric_limits<size_t>::max(), 2);

        REQUIRE(buffer.numChannels() == 0);
        REQUIRE(buffer.numFrames() == 0);
        REQUIRE(arena.getBytesUsed() == bytes_before);
    }

    SECTION("Exhaustion does not consume partial arena memory")
    {
        alignas(64) std::array<std::byte, 32> small_backing{};
        applause::MemoryArena small_arena{small_backing.data(),
                                          small_backing.size()};
        small_arena.allocateBytes(3);
        const auto bytes_before = small_arena.getBytesUsed();

        auto buffer = small_arena.allocateAudioBuffer<float>(2, 64);

        REQUIRE(buffer.numChannels() == 0);
        REQUIRE(buffer.numFrames() == 0);
        REQUIRE(small_arena.getBytesUsed() == bytes_before);
    }
#endif
}
