#include <catch2/catch_test_macros.hpp>
#include <applause/util/MemoryArena.h>

#include <array>
#include <cstdint>

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
        auto* ptr = buffer + 1;  // Offset by 1, now unaligned
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

    // Note...allocation beyond capacity triggers assert(false) in debug builds,
    // which is intentional; the arena expects pre-allocated sufficient space.
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

        // After frame destruction, should be back to original
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

            // After frame2 destroyed
            REQUIRE(arena.getBytesUsed() == level1);
        }

        // After frame1 destroyed
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

        // Verify writable
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

    SECTION("Stereo buffer allocates correct bytes")
    {
        constexpr size_t frames = 64;
        auto buffer = arena.allocateAudioBuffer<float, 2>(frames);
        REQUIRE(arena.getBytesUsed() >= frames * 2 * sizeof(float));
    }

    SECTION("Mono buffer allocates correct bytes")
    {
        constexpr size_t frames = 128;
        auto buffer = arena.allocateAudioBuffer<float, 1>(frames);
        REQUIRE(arena.getBytesUsed() >= frames * 1 * sizeof(float));
    }

    SECTION("Zero frame count returns empty buffer")
    {
        auto buffer = arena.allocateAudioBuffer<float, 2>(0);
        REQUIRE(arena.getBytesUsed() == 0);
        REQUIRE(buffer.numFrames() == 0);
    }

    SECTION("Works correctly with Frame")
    {
        arena.allocateBytes(64);
        size_t before = arena.getBytesUsed();
        {
            auto frame = arena.createFrame();
            auto buffer = arena.allocateAudioBuffer<float, 2>(64);
            REQUIRE(arena.getBytesUsed() > before);
        }
        REQUIRE(arena.getBytesUsed() == before);
    }
}
