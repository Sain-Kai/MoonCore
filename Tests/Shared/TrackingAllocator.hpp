#pragma once
// TrackingAllocator - a MoonCore-compatible heap allocator for tests.
//
// Plug it into any container that takes a heap allocator:
//     MC::DynamicBitset<MCTest::TrackingAllocator<MC::uptr>>
// and derive the test fixture from MCTest::TrackedTest. Each test then automatically fails on:
//     - leaks                      (a block still alive when the test ends)
//     - double frees               (Deallocate on a block that was already freed)
//     - foreign frees              (Deallocate on a pointer this allocator never returned)
//     - size mismatches            (Deallocate(ptr, n) with a different n than Allocate(n))
//     - overruns                   (writes into the 16 guard bytes before or after a block)
//     - writes after free          (freed memory is filled with 0xDD and checked at the end)
// New memory is filled with 0xA5, so "forgot to initialize" shows up as garbage instead of luckily-zero.
// Freed blocks are kept until the end of the test, so addresses are never reused within a test.

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "MoonCore/Types.hpp"

namespace MCTest
{
    class AllocationTracker
    {
    public:
        static constexpr usize     GuardBytes = 16;
        static constexpr unsigned char GuardByte = 0xFD;
        static constexpr unsigned char FreshByte = 0xA5;
        static constexpr unsigned char FreedByte = 0xDD;
        static constexpr usize     AbsurdBytes = usize(1) << 30;   // no test needs 1 GiB

        static void* Allocate(usize bytes) noexcept
        {
            if (bytes >= AbsurdBytes)
            {
                std::fprintf(stderr, "\n[TrackingAllocator] Allocate(%zu bytes) - almost certainly an index that wrapped around.\n"
                    "[TrackingAllocator] Aborting, because the container would dereference a null block next.\n", bytes);
                std::fflush(stderr);
                std::abort();
            }

            unsigned char* raw = static_cast<unsigned char*>(std::malloc(bytes + 2 * GuardBytes));
            if (!raw) { std::fprintf(stderr, "[TrackingAllocator] malloc failed\n"); std::abort(); }

            std::memset(raw, GuardByte, GuardBytes);
            std::memset(raw + GuardBytes, FreshByte, bytes);
            std::memset(raw + GuardBytes + bytes, GuardByte, GuardBytes);
            Blocks().push_back({ raw, bytes, true });
            return raw + GuardBytes;
        }

        static void Deallocate(void* ptr, usize bytes) noexcept
        {
            if (!ptr) return;
            auto& blocks = Blocks();
            for (auto it = blocks.rbegin(); it != blocks.rend(); ++it)
            {
                if (it->Raw + GuardBytes != ptr) continue;
                if (!it->Live)
                {
                    ADD_FAILURE() << "DOUBLE FREE of a " << it->Bytes << "-byte block";
                    return;
                }
                if (bytes != it->Bytes)
                    ADD_FAILURE() << "Deallocate(" << bytes << " bytes) on a block allocated with " << it->Bytes << " bytes";
                CheckGuards(*it, "on Deallocate");
                std::memset(it->Raw + GuardBytes, FreedByte, it->Bytes);
                it->Live = false;
                return;
            }
            ADD_FAILURE() << "Deallocate of a pointer this allocator never returned";
        }

        static usize LiveBlockCount() noexcept
        {
            usize count = 0;
            for (const Block& block : Blocks()) count += block.Live;
            return count;
        }

        // Called before each test.
        static void Reset() noexcept
        {
            for (const Block& block : Blocks()) std::free(block.Raw);
            Blocks().clear();
        }

        // Called after each test: reports problems, then releases everything.
        static void Verify() noexcept
        {
            for (const Block& block : Blocks())
            {
                CheckGuards(block, "at end of test");
                if (block.Live)
                {
                    ADD_FAILURE() << "LEAK: a " << block.Bytes << "-byte block was never freed";
                    continue;
                }
                const unsigned char* user = block.Raw + GuardBytes;
                for (usize i = 0; i < block.Bytes; ++i)
                {
                    if (user[i] != FreedByte)
                    {
                        ADD_FAILURE() << "WRITE AFTER FREE into a " << block.Bytes << "-byte block (byte " << i << ")";
                        break;
                    }
                }
            }
            Reset();
        }

    private:
        struct Block { unsigned char* Raw; usize Bytes; bool Live; };

        static std::vector<Block>& Blocks() noexcept
        {
            static std::vector<Block> blocks;
            return blocks;
        }

        static void CheckGuards(const Block& block, const char* when) noexcept
        {
            const unsigned char* before = block.Raw;
            const unsigned char* after = block.Raw + GuardBytes + block.Bytes;
            for (usize i = 0; i < GuardBytes; ++i)
            {
                if (before[i] != GuardByte) { ADD_FAILURE() << "write BEFORE the start of a " << block.Bytes << "-byte block (" << when << ")"; break; }
            }
            for (usize i = 0; i < GuardBytes; ++i)
            {
                if (after[i] != GuardByte) { ADD_FAILURE() << "write PAST the end of a " << block.Bytes << "-byte block (" << when << ")"; break; }
            }
        }
    };

    // Stateless and trivial, so it satisfies MC::Concept::HeapAllocatorType.
    template<typename T>
    struct TrackingAllocator
    {
        using ValueType = T;

        static ValueType* Allocate(usize count = 1) noexcept
        {
            return static_cast<ValueType*>(AllocationTracker::Allocate(count * sizeof(ValueType)));
        }

        static void Deallocate(ValueType* ptr, usize count = 1) noexcept
        {
            AllocationTracker::Deallocate(ptr, count * sizeof(ValueType));
        }
    };

    // Derive fixtures from this so every test gets a clean tracker and a memory check at the end.
    class TrackedTest : public ::testing::Test
    {
    protected:
        void SetUp() override { AllocationTracker::Reset(); }
        void TearDown() override { AllocationTracker::Verify(); }
    };

    template<typename Param>
    class TrackedTestWithParam : public TrackedTest, public ::testing::WithParamInterface<Param> {};
}