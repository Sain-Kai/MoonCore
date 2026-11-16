// DynamicBitsetTests.cpp - GoogleTest suite for MC::DynamicBitset.
//
// THE CONTRACT THESE TESTS CHECK (change a test if your intent differs):
//   - DynamicBitset(n): Size() == n, every bit is 0, Capacity() >= n. The default is n = 64.
//   - DynamicBitset(StringView): Size() == length; the FIRST character is the HIGHEST bit
//     (std::bitset convention), so "100" means bit 2 is set. Any other character is a MOON_CHECK.
//   - Set(i, state) with i >= Size() grows Size() to i + 1; new bits are 0, old bits are kept.
//   - Test(i), operator[](i), Flip(i) with i >= Size() are a MOON_CHECK.
//   - Reserve and Shrink never change Size() or any bit.
//   - Count / Any / None / All look only at bits [0, Size()). All() of an empty bitset is true.
//   - operator== compares Size() and bits, never Capacity().
//   - Bitwise operators are used on bitsets of equal Size(); the result keeps that Size().
//   - Shifts keep Size(). "<< k" moves bit i to i + k, ">> k" moves bit i to i - k,
//     bits pushed past either end disappear, and k >= Size() clears everything.
//   - Copies are independent. A moved-from bitset can be destroyed safely.
//
// Every test runs on MCTest::TrackingAllocator, so leaks, double frees, overruns and writes
// after free fail the test that caused them, even without ASan.
//
// Suites:
//   BitsetSized/*         per-size tests, sizes on and around word boundaries ("/Size64")
//   BitsetConstruction    default, string constructor
//   BitsetGrowth          Set() past the end
//   BitsetCapacity        Reserve, Shrink
//   BitsetCopyMove        copy and move construction
//   BitsetAssignment      copy and move assignment (enable with MC_TEST_BITSET_ASSIGNMENT=1)
//   BitsetEquality        ==, !=
//   BitsetBitwise         &, |, ^, ~ and their identities
//   BitsetShift/*         every (size, shift amount) pair ("/Size65_By64")
//   BitsetShiftExtremes   shifting by SIZE_MAX
//   BitsetInterface       explicit constructor, Reference = Reference, const shifts, operator lookup
//   BitsetRandomized/*    differential test against std::vector<bool>, one test per seed ("/Seed7")
//   BitsetDeathTest       MOON_CHECK on out-of-range access and bad input
//   BitsetDefaultAllocator  smoke test with MC::Memory::HeapAllocator

#include <gtest/gtest.h>

#include "MoonCore/Containers/DynamicBitset.hpp"
#include "TrackingAllocator.hpp"

#include <cstdint>
#include <limits>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

// operator= does not compile yet (and a requires-check can't see that, because the declaration
// is fine and only the body is broken). Set this to 1 once it compiles.
#ifndef MC_TEST_BITSET_ASSIGNMENT
#   define MC_TEST_BITSET_ASSIGNMENT 1
#endif

#if defined(MC_DEBUG) || defined(MC_RELEASE)
#   define MC_TEST_CHECKS_ACTIVE 1
#else
#   define MC_TEST_CHECKS_ACTIVE 0
#endif

namespace
{
    using Bitset = MC::DynamicBitset<MCTest::TrackingAllocator<uptr>>;
    using Model = std::vector<bool>;   // the obviously-correct reference

    // ======================================================================== helpers

    struct Rng   // splitmix64: identical sequence on every compiler and platform
    {
        std::uint64_t State;
        std::uint64_t Next()
        {
            std::uint64_t z = (State += 0x9E3779B97F4A7C15ull);
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
            return z ^ (z >> 31);
        }
        usize Below(usize n) { return n ? static_cast<usize>(Next() % n) : 0; }
        bool  Coin() { return (Next() & 1) != 0; }
    };

    Model RandomModel(Rng& rng, usize size)
    {
        Model m(size);
        for (usize i = 0; i < size; ++i) m[i] = rng.Coin();
        return m;
    }

    usize CountOf(const Model& m)
    {
        usize count = 0;
        for (bool bit : m) count += bit;
        return count;
    }

    Model ShiftedLeft(const Model& m, usize k)
    {
        Model r(m.size(), false);
        if (k < m.size())
            for (usize i = k; i < m.size(); ++i) r[i] = m[i - k];
        return r;
    }

    Model ShiftedRight(const Model& m, usize k)
    {
        Model r(m.size(), false);
        if (k < m.size())
            for (usize i = 0; i + k < m.size(); ++i) r[i] = m[i + k];
        return r;
    }

    // Writes the model's 1-bits. The bitset must already have the model's size.
    void Fill(Bitset& b, const Model& m)
    {
        for (usize i = 0; i < m.size(); ++i)
            if (m[i]) b.Set(i);
    }

    // Compares every observable property. Size() is checked first so nothing indexes out of range.
    ::testing::AssertionResult Matches(const Bitset& b, const Model& m)
    {
        if (b.Size() != m.size())
            return ::testing::AssertionFailure() << "Size() is " << b.Size() << ", expected " << m.size();

        for (usize i = 0; i < m.size(); ++i)
            if (b.Test(i) != m[i])
            {
                return ::testing::AssertionFailure() << "bit " << i << " is " << b.Test(i) << ", expected " << m[i];
            }
        const usize expected = CountOf(m);
        if (b.Count() != expected)
            return ::testing::AssertionFailure() << "every bit is right, but Count() is " << b.Count() << ", expected " << expected;
        if (b.Any() != (expected > 0))
            return ::testing::AssertionFailure() << "every bit is right, but Any() is " << b.Any();
        if (b.None() != (expected == 0))
            return ::testing::AssertionFailure() << "every bit is right, but None() is " << b.None();
        if (b.All() != (expected == m.size()))
            return ::testing::AssertionFailure() << "every bit is right, but All() is " << b.All();
        return ::testing::AssertionSuccess();
    }

    // Sizes on and around word boundaries.
    constexpr usize kSizes[] = { 0, 1, 2, 31, 63, 64, 65, 127, 128, 129, 200, 1000 };

    // Shift amounts: zero, sub-word, exactly a word, a word +/- 1, several words, past the end.
    constexpr usize kShifts[] = { 0, 1, 7, 63, 64, 65, 127, 128, 129, 191, 199, 200, 999, 1000, 5000 };

    // Indices that sit on word edges, filtered to the given size.
    std::vector<usize> EdgeIndices(usize n)
    {
        std::vector<usize> result;
        const usize candidates[] = { 0, 1, 31, 62, 63, 64, 65, 126, 127, 128, 129, 199 };
        for (usize i : candidates)
            if (i < n) result.push_back(i);
        if (n >= 2) result.push_back(n - 2);
        if (n >= 1) result.push_back(n - 1);
        return result;
    }

    std::string SizeName(const ::testing::TestParamInfo<usize>& info)
    {
        return "Size" + std::to_string(info.param);
    }

    // Fixtures (all memory-checked).
    class BitsetConstruction : public MCTest::TrackedTest {};
    class BitsetGrowth : public MCTest::TrackedTest {};
    class BitsetCapacity : public MCTest::TrackedTest {};
    class BitsetCopyMove : public MCTest::TrackedTest {};
    class BitsetAssignment : public MCTest::TrackedTest {};
    class BitsetEquality : public MCTest::TrackedTest {};
    class BitsetBitwise : public MCTest::TrackedTest {};
    class BitsetInterface : public MCTest::TrackedTest {};
    class BitsetShiftExtremes : public MCTest::TrackedTest {};
    class BitsetDeathTest : public MCTest::TrackedTest {};
    class BitsetSized : public MCTest::TrackedTestWithParam<usize> {};
    class BitsetShift : public MCTest::TrackedTestWithParam<std::tuple<usize, usize>> {};
    class BitsetRandomized : public MCTest::TrackedTestWithParam<std::uint64_t> {};
}

// ============================================================================ per size

INSTANTIATE_TEST_SUITE_P(WordBoundaries, BitsetSized, ::testing::ValuesIn(kSizes), SizeName);

TEST_P(BitsetSized, ConstructsWithSizeAndAllZero)
{
    const usize n = GetParam();
    Bitset b(n);
    EXPECT_TRUE(Matches(b, Model(n, false)));
    EXPECT_GE(b.Capacity(), n);
}

TEST_P(BitsetSized, SetResetFlipTouchOnlyThatBit)
{
    const usize n = GetParam();
    for (usize i : EdgeIndices(n))
    {
        SCOPED_TRACE("bit " + std::to_string(i));
        Bitset b(n);
        ASSERT_EQ(b.Size(), n);
        Model m(n, false);

        b.Set(i);          m[i] = true;   ASSERT_TRUE(Matches(b, m)) << "after Set(i)";
        b.Set(i, false);   m[i] = false;  ASSERT_TRUE(Matches(b, m)) << "after Set(i, false)";
        b.Flip(i);         m[i] = true;   ASSERT_TRUE(Matches(b, m)) << "after Flip(i) 0 -> 1";
        b.Flip(i);         m[i] = false;  ASSERT_TRUE(Matches(b, m)) << "after Flip(i) 1 -> 0";
        b.Set(i);
        b.Reset(i);                       ASSERT_TRUE(Matches(b, m)) << "after Set(i) then Reset(i)";
    }
}

TEST_P(BitsetSized, ReferenceReadsAndWrites)
{
    const usize n = GetParam();
    for (usize i : EdgeIndices(n))
    {
        SCOPED_TRACE("bit " + std::to_string(i));
        Bitset b(n);
        ASSERT_EQ(b.Size(), n);
        Model m(n, false);

        b[i] = true;   m[i] = true;
        EXPECT_TRUE(b[i]);
        const Bitset& cb = b;
        EXPECT_TRUE(cb[i]);
        ASSERT_TRUE(Matches(b, m)) << "after b[i] = true";

        b[i] = false;  m[i] = false;
        EXPECT_FALSE(b[i]);
        ASSERT_TRUE(Matches(b, m)) << "after b[i] = false";
    }
}

TEST_P(BitsetSized, WholeSetResetFlip)
{
    const usize n = GetParam();
    Bitset b(n);
    ASSERT_EQ(b.Size(), n);
    b.Set();     ASSERT_TRUE(Matches(b, Model(n, true))) << "after Set()";
    b.Flip();    ASSERT_TRUE(Matches(b, Model(n, false))) << "after Set() then Flip()";
    b.Flip();    ASSERT_TRUE(Matches(b, Model(n, true))) << "after Flip() again";
    b.Reset();   ASSERT_TRUE(Matches(b, Model(n, false))) << "after Reset()";
}

TEST_P(BitsetSized, FlipAllOnRandomPattern)
{
    const usize n = GetParam();
    Rng rng{ 100 + n };
    const Model m = RandomModel(rng, n);
    Model flipped(n);
    for (usize i = 0; i < n; ++i) flipped[i] = !m[i];

    Bitset b(n);
    ASSERT_EQ(b.Size(), n);
    Fill(b, m);
    b.Flip();   ASSERT_TRUE(Matches(b, flipped)) << "after Flip()";
    b.Flip();   ASSERT_TRUE(Matches(b, m)) << "after Flip() twice";
}

TEST_P(BitsetSized, QueriesWithOneBitSet)
{
    const usize n = GetParam();
    for (usize i : EdgeIndices(n))
    {
        SCOPED_TRACE("bit " + std::to_string(i));
        Bitset b(n);
        ASSERT_EQ(b.Size(), n);
        b.Set(i);
        EXPECT_EQ(b.Count(), 1u);
        EXPECT_TRUE(b.Any());
        EXPECT_FALSE(b.None());
        EXPECT_EQ(b.All(), n == 1);
    }
}

TEST_P(BitsetSized, QueriesWithOneBitCleared)
{
    const usize n = GetParam();
    for (usize i : EdgeIndices(n))
    {
        SCOPED_TRACE("bit " + std::to_string(i));
        Bitset b(n);
        ASSERT_EQ(b.Size(), n);
        b.Set();
        b.Reset(i);
        EXPECT_EQ(b.Count(), n - 1);
        EXPECT_FALSE(b.All()) << "All() must notice a single cleared bit";
        EXPECT_EQ(b.Any(), n > 1);
        EXPECT_EQ(b.None(), n == 1);
    }
}

TEST_P(BitsetSized, ReserveKeepsSizeAndBits)
{
    const usize n = GetParam();
    Rng rng{ 200 + n };
    const Model m = RandomModel(rng, n);
    Bitset b(n);
    ASSERT_EQ(b.Size(), n);
    Fill(b, m);

    b.Reserve(n + 1000);
    EXPECT_GE(b.Capacity(), n + 1000);
    ASSERT_TRUE(Matches(b, m)) << "after Reserve(Size() + 1000)";

    b.Reserve(1);
    ASSERT_TRUE(Matches(b, m)) << "after Reserve(1), which is smaller than the size";

    b.Reserve(b.Capacity());
    ASSERT_TRUE(Matches(b, m)) << "after Reserve(Capacity())";
}

TEST_P(BitsetSized, ShrinkKeepsSizeAndBits)
{
    const usize n = GetParam();
    Rng rng{ 300 + n };
    const Model m = RandomModel(rng, n);
    Bitset b(n);
    ASSERT_EQ(b.Size(), n);
    Fill(b, m);

    b.Reserve(n + 5000);
    b.Shrink();
    ASSERT_TRUE(Matches(b, m)) << "after Reserve(+5000) then Shrink()";
    EXPECT_GE(b.Capacity(), n);
    EXPECT_LT(b.Capacity(), n + 5000) << "Shrink() should release the reserved space";

    b.Shrink();
    ASSERT_TRUE(Matches(b, m)) << "after a second Shrink()";
}

TEST_P(BitsetSized, CopyIsEqualAndIndependent)
{
    const usize n = GetParam();
    Rng rng{ 400 + n };
    const Model m = RandomModel(rng, n);
    Bitset a(n);
    ASSERT_EQ(a.Size(), n);
    Fill(a, m);

    Bitset b(a);
    ASSERT_TRUE(Matches(b, m)) << "the copy";
    EXPECT_TRUE(a == b);

    if (n > 0)
    {
        b.Flip(0);
        b.Flip(n - 1);
        ASSERT_TRUE(Matches(a, m)) << "the original, after modifying the copy";
    }
}

TEST_P(BitsetSized, MoveTransfersContents)
{
    const usize n = GetParam();
    Rng rng{ 500 + n };
    const Model m = RandomModel(rng, n);
    Bitset a(n);
    ASSERT_EQ(a.Size(), n);
    Fill(a, m);

    Bitset b(MC::Move(a));
    ASSERT_TRUE(Matches(b, m)) << "the move-constructed bitset";
    // `a` is destroyed at the end of the test: the tracker reports a double free if it still owns the block.
}

TEST_P(BitsetSized, ComplementFlipsExactlySizeBits)
{
    const usize n = GetParam();
    Rng rng{ 600 + n };
    const Model m = RandomModel(rng, n);
    Model flipped(n);
    for (usize i = 0; i < n; ++i) flipped[i] = !m[i];

    Bitset b(n);
    ASSERT_EQ(b.Size(), n);
    Fill(b, m);
    {
        Bitset c = ~b;
        ASSERT_TRUE(Matches(c, flipped)) << "~b";
        Bitset cc = ~c;
        EXPECT_TRUE(cc == b) << "~~b == b";
    }
    ASSERT_TRUE(Matches(b, m)) << "b itself after ~b";
}

TEST_P(BitsetSized, CompoundBitwiseOperators)
{
    const usize n = GetParam();
    Rng rng{ 700 + n };
    const Model ma = RandomModel(rng, n), mb = RandomModel(rng, n);
    Model mAnd(n), mOr(n), mXor(n);
    for (usize i = 0; i < n; ++i) { mAnd[i] = ma[i] && mb[i]; mOr[i] = ma[i] || mb[i]; mXor[i] = ma[i] != mb[i]; }

    Bitset right(n);
    ASSERT_EQ(right.Size(), n);
    Fill(right, mb);

    { Bitset x(n); Fill(x, ma); x &= right; ASSERT_TRUE(Matches(x, mAnd)) << "&="; }
    { Bitset x(n); Fill(x, ma); x |= right; ASSERT_TRUE(Matches(x, mOr)) << "|="; }
    { Bitset x(n); Fill(x, ma); x ^= right; ASSERT_TRUE(Matches(x, mXor)) << "^="; }
    ASSERT_TRUE(Matches(right, mb)) << "the right operand must not change";
}

TEST_P(BitsetSized, FreeBitwiseOperators)
{
    const usize n = GetParam();
    Rng rng{ 800 + n };
    const Model ma = RandomModel(rng, n), mb = RandomModel(rng, n);
    Model mAnd(n), mOr(n), mXor(n);
    for (usize i = 0; i < n; ++i) { mAnd[i] = ma[i] && mb[i]; mOr[i] = ma[i] || mb[i]; mXor[i] = ma[i] != mb[i]; }

    Bitset a(n), b(n);
    ASSERT_EQ(a.Size(), n);
    Fill(a, ma);
    Fill(b, mb);
    { Bitset r = a & b; ASSERT_TRUE(Matches(r, mAnd)) << "a & b"; }
    { Bitset r = a | b; ASSERT_TRUE(Matches(r, mOr)) << "a | b"; }
    { Bitset r = a ^ b; ASSERT_TRUE(Matches(r, mXor)) << "a ^ b"; }
    ASSERT_TRUE(Matches(a, ma)) << "left operand must not change";
    ASSERT_TRUE(Matches(b, mb)) << "right operand must not change";
}

TEST_P(BitsetSized, BitwiseIdentities)
{
    const usize n = GetParam();
    Rng rng{ 900 + n };
    const Model m = RandomModel(rng, n);
    Bitset a(n);
    ASSERT_EQ(a.Size(), n);
    Fill(a, m);
    const Bitset notA = ~a;

    { Bitset r = a ^ a;     EXPECT_TRUE(Matches(r, Model(n, false))) << "a ^ a == 0"; }
    { Bitset r = a & notA;  EXPECT_TRUE(Matches(r, Model(n, false))) << "a & ~a == 0"; }
    { Bitset r = a | notA;  EXPECT_TRUE(Matches(r, Model(n, true))) << "a | ~a == all ones"; }
    { Bitset r = a & a;     EXPECT_TRUE(Matches(r, m)) << "a & a == a"; }
    { Bitset r = a | a;     EXPECT_TRUE(Matches(r, m)) << "a | a == a"; }
}

TEST_P(BitsetSized, EqualityDetectsEveryBit)
{
    const usize n = GetParam();
    Rng rng{ 1000 + n };
    const Model m = RandomModel(rng, n);
    Bitset a(n);
    ASSERT_EQ(a.Size(), n);
    Fill(a, m);
    EXPECT_TRUE(a == a);

    for (usize i : EdgeIndices(n))
    {
        SCOPED_TRACE("bit " + std::to_string(i));
        Bitset b(a);
        EXPECT_TRUE(a == b);
        b.Flip(i);
        EXPECT_FALSE(a == b);
        EXPECT_TRUE(a != b);
    }
}

// ============================================================================ construction

TEST_F(BitsetConstruction, DefaultIs64Bits)
{
    Bitset b;
    EXPECT_TRUE(Matches(b, Model(64, false)));
}

TEST_F(BitsetConstruction, FromStringFirstCharacterIsHighestBit)
{
    { Bitset b(MC::StringView(TEXT("1")));    EXPECT_TRUE(Matches(b, (Model{ true }))) << "\"1\""; }
    { Bitset b(MC::StringView(TEXT("0")));    EXPECT_TRUE(Matches(b, (Model{ false }))) << "\"0\""; }
    { Bitset b(MC::StringView(TEXT("100")));  EXPECT_TRUE(Matches(b, (Model{ false, false, true }))) << "\"100\" sets bit 2"; }
    { Bitset b(MC::StringView(TEXT("001")));  EXPECT_TRUE(Matches(b, (Model{ true, false, false }))) << "\"001\" sets bit 0"; }
    { Bitset b(MC::StringView(TEXT("0110"))); EXPECT_TRUE(Matches(b, (Model{ false, true, true, false }))) << "\"0110\""; }
    { Bitset b(MC::StringView(TEXT("0000"))); EXPECT_TRUE(Matches(b, Model(4, false))) << "\"0000\""; }
}

TEST_F(BitsetConstruction, FromStringOnWordBoundaries)
{
    for (usize length : { usize(63), usize(64), usize(65), usize(70), usize(128), usize(130) })
    {
        SCOPED_TRACE("length " + std::to_string(length));
        Rng rng{ length };
        const Model m = RandomModel(rng, length);
        std::vector<char8_t> text(length + 1, 0);
        for (usize i = 0; i < length; ++i) text[i] = m[length - 1 - i] ? u8'1' : u8'0';   // highest bit first

        Bitset b{ MC::StringView(text.data()) };
        EXPECT_TRUE(Matches(b, m));
    }
}

TEST_F(BitsetConstruction, FromStringWithCustomCharacters)
{
    Bitset b(MC::StringView(TEXT("#..#.")), TEXT('.'), TEXT('#'));
    EXPECT_TRUE(Matches(b, (Model{ false, true, false, false, true })));
}

TEST_F(BitsetConstruction, FromEmptyString)
{
    Bitset b(MC::StringView(TEXT("")));
    EXPECT_TRUE(Matches(b, Model{}));
}

TEST_F(BitsetConstruction, EmptyBitsetHandlesEveryOperation)
{
    Bitset b(0);
    ASSERT_TRUE(Matches(b, Model{}));
    b.Set();     ASSERT_TRUE(Matches(b, Model{})) << "Set()";
    b.Flip();    ASSERT_TRUE(Matches(b, Model{})) << "Flip()";
    b.Reset();   ASSERT_TRUE(Matches(b, Model{})) << "Reset()";
    b <<= 0;     ASSERT_TRUE(Matches(b, Model{})) << "<<= 0";
    b <<= 5;     ASSERT_TRUE(Matches(b, Model{})) << "<<= 5";
    b >>= 0;     ASSERT_TRUE(Matches(b, Model{})) << ">>= 0";
    b >>= 5;     ASSERT_TRUE(Matches(b, Model{})) << ">>= 5";
    b.Shrink();  ASSERT_TRUE(Matches(b, Model{})) << "Shrink()";
    { Bitset c = ~b;  EXPECT_TRUE(Matches(c, Model{})) << "~b"; }
    { Bitset c(b);    EXPECT_TRUE(Matches(c, Model{})) << "copy"; }
    Bitset other(0);
    EXPECT_TRUE(b == other);
}

// ============================================================================ growth through Set()

TEST_F(BitsetGrowth, FromEmpty)
{
    Bitset b(0);
    b.Set(0);
    EXPECT_TRUE(Matches(b, (Model{ true })));
}

TEST_F(BitsetGrowth, KeepsOldBitsAndZeroesNewOnes)
{
    Rng rng{ 1 };
    Model m = RandomModel(rng, 10);
    Bitset b(10);
    ASSERT_EQ(b.Size(), 10u);
    Fill(b, m);

    b.Set(200);
    m.resize(201, false);
    m[200] = true;
    EXPECT_TRUE(Matches(b, m));
}

TEST_F(BitsetGrowth, SetFalsePastTheEndStillGrows)
{
    Bitset b(10);
    ASSERT_EQ(b.Size(), 10u);
    b.Set(3);
    b.Set(100, false);
    Model m(101, false);
    m[3] = true;
    EXPECT_TRUE(Matches(b, m));
}

TEST_F(BitsetGrowth, SetInsideTheSizeDoesNotChangeIt)
{
    Bitset b(100);
    ASSERT_EQ(b.Size(), 100u);
    b.Set(99);
    b.Set(0);
    b.Set(64);
    EXPECT_EQ(b.Size(), 100u);
}

TEST_F(BitsetGrowth, OneBitAtATimeAcrossManyWords)
{
    Bitset b(0);
    Model m;
    for (usize i = 0; i < 1000; ++i)
    {
        const bool state = (i % 3) == 0;
        b.Set(i, state);
        m.push_back(state);
        if (i % 64 == 63 || i == 64 || i == 999)
        {
            ASSERT_TRUE(Matches(b, m)) << "after setting bit " << i;
        }
    }
}

TEST_F(BitsetGrowth, JumpsAcrossSeveralWordsAtOnce)
{
    Bitset b(0);
    Model m;
    for (usize i : { usize(5), usize(130), usize(131), usize(700), usize(64 * 20) })
    {
        b.Set(i);
        m.resize(i + 1, false);
        m[i] = true;
        ASSERT_TRUE(Matches(b, m)) << "after Set(" << i << ")";
    }
}

TEST_F(BitsetGrowth, AfterReserve)
{
    Bitset b(10);
    ASSERT_EQ(b.Size(), 10u);
    b.Set(9);
    b.Reserve(1000);
    b.Set(500);
    Model m(501, false);
    m[9] = true;
    m[500] = true;
    EXPECT_TRUE(Matches(b, m));
}

// ============================================================================ capacity

TEST_F(BitsetCapacity, ReserveBeyondCurrentCapacityGrowsIt)
{
    Bitset b(64);
    ASSERT_EQ(b.Size(), 64u);
    b.Reserve(1000);
    EXPECT_GE(b.Capacity(), 1000u);
    EXPECT_EQ(b.Size(), 64u);
}

TEST_F(BitsetCapacity, ShrinkOnEmpty)
{
    Bitset b(0);
    b.Reserve(500);
    b.Shrink();
    EXPECT_TRUE(Matches(b, Model{}));
}

TEST_F(BitsetCapacity, GrowShrinkGrowKeepsBits)
{
    Bitset b(70);
    ASSERT_EQ(b.Size(), 70u);
    b.Set(0); b.Set(63); b.Set(64); b.Set(69);
    Model m(70, false);
    m[0] = m[63] = m[64] = m[69] = true;

    b.Reserve(2000); ASSERT_TRUE(Matches(b, m)) << "Reserve(2000)";
    b.Shrink();      ASSERT_TRUE(Matches(b, m)) << "Shrink()";
    b.Reserve(300);  ASSERT_TRUE(Matches(b, m)) << "Reserve(300)";
    b.Set(250);      m.resize(251, false); m[250] = true;
    ASSERT_TRUE(Matches(b, m)) << "Set(250)";
}

// ============================================================================ copy / move

TEST_F(BitsetCopyMove, MovedFromIsDestroyedSafely)
{
    // No Size() precondition on purpose: the tracker reports a double free by itself.
    {
        Bitset a(130);
        Bitset b(MC::Move(a));
    }
    EXPECT_EQ(MCTest::AllocationTracker::LiveBlockCount(), 0u);
}

TEST_F(BitsetCopyMove, MoveIsNoexcept)
{
    EXPECT_TRUE(std::is_nothrow_move_constructible_v<Bitset>);
}

TEST_F(BitsetCopyMove, CopyOfACopy)
{
    Bitset a(200);
    ASSERT_EQ(a.Size(), 200u);
    a.Set(1); a.Set(199);
    Bitset b(a);
    Bitset c(b);
    EXPECT_TRUE(c == a);
    EXPECT_EQ(c.Count(), 2u);
}

// ============================================================================ assignment

#if MC_TEST_BITSET_ASSIGNMENT

TEST_F(BitsetAssignment, CopySmallFromBig)
{
    Rng rng{ 51 };
    const Model ma = RandomModel(rng, 70), mb = RandomModel(rng, 300);
    Bitset a(70), b(300);
    ASSERT_EQ(a.Size(), 70u);
    ASSERT_EQ(b.Size(), 300u);
    Fill(a, ma);
    Fill(b, mb);

    a = b;   // a's old block must be freed: the tracker reports a LEAK otherwise
    ASSERT_TRUE(Matches(a, mb)) << "a after a = b";
    ASSERT_TRUE(Matches(b, mb)) << "b after a = b";
    b.Flip(0);
    ASSERT_TRUE(Matches(a, mb)) << "a must be independent of b";
}

TEST_F(BitsetAssignment, CopyBigFromSmall)
{
    Rng rng{ 52 };
    const Model ma = RandomModel(rng, 300), mb = RandomModel(rng, 70);
    Bitset a(300), b(70);
    ASSERT_EQ(a.Size(), 300u);
    ASSERT_EQ(b.Size(), 70u);
    Fill(a, ma);
    Fill(b, mb);
    a = b;
    ASSERT_TRUE(Matches(a, mb));
}

TEST_F(BitsetAssignment, CopySelf)
{
    Rng rng{ 53 };
    const Model m = RandomModel(rng, 130);
    Bitset a(130);
    ASSERT_EQ(a.Size(), 130u);
    Fill(a, m);
    Bitset& alias = a;
    a = alias;
    ASSERT_TRUE(Matches(a, m));
}

TEST_F(BitsetAssignment, Move)
{
    Rng rng{ 54 };
    const Model mb = RandomModel(rng, 300);
    Bitset a(70), b(300);
    ASSERT_EQ(b.Size(), 300u);
    Fill(b, mb);
    a = MC::Move(b);   // a's old block must be freed, and b must not free the block a now owns
    ASSERT_TRUE(Matches(a, mb));
}

TEST_F(BitsetAssignment, MoveSelf)
{
    Rng rng{ 55 };
    const Model m = RandomModel(rng, 130);
    Bitset a(130);
    ASSERT_EQ(a.Size(), 130u);
    Fill(a, m);
    Bitset& alias = a;
    a = MC::Move(alias);
    ASSERT_TRUE(Matches(a, m));
}

TEST_F(BitsetAssignment, MovedFromCanBeAssignedAgain)
{
    Bitset a(70), b(70), c(130);
    ASSERT_EQ(c.Size(), 130u);
    c.Set(129);
    a = MC::Move(b);
    b = c;
    EXPECT_TRUE(b == c);
}

#else

TEST_F(BitsetAssignment, Disabled)
{
    GTEST_SKIP() << "6 assignment tests are off. Define MC_TEST_BITSET_ASSIGNMENT=1 once operator= compiles.";
}

#endif

// ============================================================================ equality

TEST_F(BitsetEquality, IgnoresCapacity)
{
    Bitset a(70), b(70);
    ASSERT_EQ(a.Size(), 70u);
    ASSERT_EQ(b.Size(), 70u);
    a.Set(5); a.Set(69);
    b.Set(5); b.Set(69);
    b.Reserve(2000);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
}

TEST_F(BitsetEquality, DifferentSizesAreNotEqual)
{
    { Bitset a(0), b(1);   EXPECT_FALSE(a == b) << "0 vs 1"; }
    { Bitset a(63), b(64);  EXPECT_FALSE(a == b) << "63 vs 64"; }
    { Bitset a(64), b(65);  EXPECT_FALSE(a == b) << "64 vs 65"; }
    { Bitset a(64), b(128); EXPECT_FALSE(a == b) << "64 vs 128"; }
}

TEST_F(BitsetEquality, StringAndSetAgree)
{
    Bitset fromString(MC::StringView(TEXT("10100")));
    Bitset fromSet(5);
    ASSERT_EQ(fromSet.Size(), 5u);
    fromSet.Set(4);
    fromSet.Set(2);
    EXPECT_TRUE(fromString == fromSet);
}

// ============================================================================ shifts

INSTANTIATE_TEST_SUITE_P(AllPairs, BitsetShift,
    ::testing::Combine(::testing::ValuesIn(kSizes), ::testing::ValuesIn(kShifts)),
    [](const ::testing::TestParamInfo<std::tuple<usize, usize>>& info)
    {
        return "Size" + std::to_string(std::get<0>(info.param)) + "_By" + std::to_string(std::get<1>(info.param));
    });

TEST_P(BitsetShift, LeftInPlace)
{
    const auto [n, k] = GetParam();
    Rng rng{ n * 10007 + k };
    const Model m = RandomModel(rng, n);
    Bitset b(n);
    ASSERT_EQ(b.Size(), n);
    Fill(b, m);
    b <<= k;
    EXPECT_TRUE(Matches(b, ShiftedLeft(m, k)));
}

TEST_P(BitsetShift, RightInPlace)
{
    const auto [n, k] = GetParam();
    Rng rng{ n * 10009 + k };
    const Model m = RandomModel(rng, n);
    Bitset b(n);
    ASSERT_EQ(b.Size(), n);
    Fill(b, m);
    b >>= k;
    EXPECT_TRUE(Matches(b, ShiftedRight(m, k)));
}

TEST_P(BitsetShift, AllOnesLeft)
{
    // All ones makes every bit that should have been shifted out visible if it leaks back in.
    const auto [n, k] = GetParam();
    Bitset b(n);
    ASSERT_EQ(b.Size(), n);
    b.Set();
    b <<= k;
    EXPECT_TRUE(Matches(b, ShiftedLeft(Model(n, true), k)));
}

TEST_P(BitsetShift, AllOnesRight)
{
    const auto [n, k] = GetParam();
    Bitset b(n);
    ASSERT_EQ(b.Size(), n);
    b.Set();
    b >>= k;
    EXPECT_TRUE(Matches(b, ShiftedRight(Model(n, true), k)));
}

TEST_P(BitsetShift, ValueOperatorsLeaveOriginalAlone)
{
    const auto [n, k] = GetParam();
    Rng rng{ n * 10037 + k };
    const Model m = RandomModel(rng, n);
    Bitset b(n);
    ASSERT_EQ(b.Size(), n);
    Fill(b, m);
    { Bitset l = b << k; ASSERT_TRUE(Matches(l, ShiftedLeft(m, k))) << "b << k"; }
    { Bitset r = b >> k; ASSERT_TRUE(Matches(r, ShiftedRight(m, k))) << "b >> k"; }
    ASSERT_TRUE(Matches(b, m)) << "b itself after b << k and b >> k";
}

TEST_F(BitsetShiftExtremes, ShiftByMaximumAmountClears)
{
    constexpr usize huge = std::numeric_limits<usize>::max();
    Bitset b(130);
    ASSERT_EQ(b.Size(), 130u);
    b.Set();
    b <<= huge;
    EXPECT_TRUE(Matches(b, Model(130, false))) << "<<= SIZE_MAX";
    b.Set();
    b >>= huge;
    EXPECT_TRUE(Matches(b, Model(130, false))) << ">>= SIZE_MAX";
}

// ============================================================================ interface
// These use requires-expressions, so a missing feature is a FAILED test instead of a compile error.

namespace
{
    template<class B> constexpr bool kReferenceAssignable = requires(B & b) { b[0] = b[1]; };
    template<class B> constexpr bool kConstShiftable = requires(const B & b, usize k) { b << k; b >> k; };

    template<class B>
    void CheckReferenceAssignment()
    {
        if constexpr (kReferenceAssignable<B>)
        {
            B b(130);
            ASSERT_EQ(b.Size(), 130u);
            b.Set(1);
            b.Set(100);
            b[0] = b[1];     // copies true
            b[64] = b[100];   // copies true, across words
            b[100] = b[2];     // copies false
            EXPECT_TRUE(b.Test(0));
            EXPECT_TRUE(b.Test(64));
            EXPECT_FALSE(b.Test(100));
            EXPECT_EQ(b.Count(), 3u);
        }
        else
        {
            ADD_FAILURE() << "`b[0] = b[1]` does not compile (Reference has no usable copy assignment)";
        }
    }

    template<class B>
    void CheckConstShifts()
    {
        if constexpr (kConstShiftable<B>)
        {
            B b(70);
            ASSERT_EQ(b.Size(), 70u);
            b.Set(3);
            b.Set(69);
            const B& cb = b;
            B l = cb << 2;
            B r = cb >> 2;
            EXPECT_TRUE(l.Test(5));
            EXPECT_EQ(l.Count(), 1u) << "bit 69 must fall off the top";
            EXPECT_TRUE(r.Test(1));
            EXPECT_TRUE(r.Test(67));
            EXPECT_EQ(r.Count(), 2u);
        }
        else
        {
            ADD_FAILURE() << "`b << k` / `b >> k` do not compile on a const bitset";
        }
    }
}

namespace AdlProbe
{
    // Any namespace with its own operator& / | / ^ hides global ones from ordinary lookup,
    // so the bitset's operators are only found if argument-dependent lookup can reach them.
    struct Unrelated {};
    Unrelated operator&(Unrelated, Unrelated);
    Unrelated operator|(Unrelated, Unrelated);
    Unrelated operator^(Unrelated, Unrelated);

    template<class B> constexpr bool kFoundByAdl = requires(const B & a, const B & b) { a& b; a | b; a^ b; };

    template<class B>
    void Check()
    {
        if constexpr (kFoundByAdl<B>)
        {
            B a(70), b(70);
            ASSERT_EQ(a.Size(), 70u);
            a.Set(1); a.Set(2);
            b.Set(2); b.Set(3);
            B x = a & b;
            B y = a | b;
            B z = a ^ b;
            EXPECT_EQ(x.Count(), 1u);
            EXPECT_EQ(y.Count(), 3u);
            EXPECT_EQ(z.Count(), 2u);
        }
        else
        {
            ADD_FAILURE() << "a & b, a | b, a ^ b are not found from a namespace that declares its own operators";
        }
    }
}

TEST_F(BitsetInterface, SizeConstructorIsExplicit)
{
    EXPECT_FALSE((std::is_convertible_v<usize, Bitset>)) << "`bitset == 64` compiles by turning 64 into a temporary bitset";
}

TEST_F(BitsetInterface, ReferenceAssignsFromReference) { CheckReferenceAssignment<Bitset>(); }
TEST_F(BitsetInterface, ShiftsWorkOnConstBitset) { CheckConstShifts<Bitset>(); }
TEST_F(BitsetInterface, OperatorsFoundFromOtherNamespaces) { AdlProbe::Check<Bitset>(); }

// ============================================================================ randomized differential test

INSTANTIATE_TEST_SUITE_P(Seeds, BitsetRandomized, ::testing::Range<std::uint64_t>(1, 51),
    [](const ::testing::TestParamInfo<std::uint64_t>& info) { return "Seed" + std::to_string(info.param); });

TEST_P(BitsetRandomized, AgainstVectorBool)
{
    Rng rng{ GetParam() };
    const usize start = rng.Below(300);
    Bitset b(start);
    Model  m(start, false);
    ASSERT_TRUE(Matches(b, m)) << "Bitset(" << start << ")";

    for (int step = 0; step < 400; ++step)
    {
        const usize n = m.size();
        std::string op;
        switch (rng.Below(14))
        {
        case 0: {
            const usize i = n + rng.Below(130); b.Set(i); m.resize(i + 1, false); m[i] = true;
            op = "Set(" + std::to_string(i) + ") [grows]"; break;
        }
        case 1: {
            if (!n) continue; const usize i = rng.Below(n); const bool v = rng.Coin(); b.Set(i, v); m[i] = v;
            op = "Set(" + std::to_string(i) + ", " + std::to_string(v) + ")"; break;
        }
        case 2: {
            if (!n) continue; const usize i = rng.Below(n); b.Reset(i); m[i] = false;
            op = "Reset(" + std::to_string(i) + ")"; break;
        }
        case 3: {
            if (!n) continue; const usize i = rng.Below(n); b.Flip(i); m[i] = !m[i];
            op = "Flip(" + std::to_string(i) + ")"; break;
        }
        case 4: {
            if (!n) continue; const usize i = rng.Below(n); const bool v = rng.Coin(); b[i] = v; m[i] = v;
            op = "b[" + std::to_string(i) + "] = " + std::to_string(v); break;
        }
        case 5: { b.Set();   for (usize i = 0; i < n; ++i) m[i] = true;  op = "Set()";   break; }
        case 6: { b.Reset(); for (usize i = 0; i < n; ++i) m[i] = false; op = "Reset()"; break; }
        case 7: { b.Flip();  for (usize i = 0; i < n; ++i) m[i] = !m[i]; op = "Flip()";  break; }
        case 8: { const usize k = rng.Below(n + 70); b <<= k; m = ShiftedLeft(m, k);  op = "<<= " + std::to_string(k); break; }
        case 9: { const usize k = rng.Below(n + 70); b >>= k; m = ShiftedRight(m, k); op = ">>= " + std::to_string(k); break; }
        case 10:
        {
            const Model om = RandomModel(rng, n);
            Bitset other(n);
            ASSERT_EQ(other.Size(), n);
            Fill(other, om);
            const usize which = rng.Below(3);
            for (usize i = 0; i < n; ++i)
                m[i] = which == 0 ? (m[i] && om[i]) : which == 1 ? (m[i] || om[i]) : (m[i] != om[i]);
            if (which == 0) b &= other; else if (which == 1) b |= other; else b ^= other;
            op = which == 0 ? "&= random" : which == 1 ? "|= random" : "^= random";
            break;
        }
        case 11: { const usize r = n + rng.Below(500); b.Reserve(r); op = "Reserve(" + std::to_string(r) + ")"; break; }
        case 12: { b.Shrink(); op = "Shrink()"; break; }
        default:
        {
            Bitset copy(b);
            Bitset moved(MC::Move(copy));
            ASSERT_TRUE(Matches(moved, m)) << "step " << step << ": copy + move of the current state";
#if MC_TEST_BITSET_ASSIGNMENT
            Bitset target(7);
            target = moved;
            ASSERT_TRUE(Matches(target, m)) << "step " << step << ": copy-assigned from the current state";
#endif
            op = "copy + move round trip";
            break;
        }
        }
        ASSERT_TRUE(Matches(b, m)) << "step " << step << ", after " << op;
    }
}

// ============================================================================ death tests
// Each death test re-runs the executable in a child process, so these are the slow ones.

TEST_F(BitsetDeathTest, TestOutOfRange)
{
    if (!MC_TEST_CHECKS_ACTIVE) GTEST_SKIP() << "MOON_CHECK is compiled out in this configuration";
    Bitset b(10);
    ASSERT_EQ(b.Size(), 10u);
    EXPECT_DEATH((void)b.Test(10), "");
}

TEST_F(BitsetDeathTest, MutableIndexOutOfRange)
{
    if (!MC_TEST_CHECKS_ACTIVE) GTEST_SKIP() << "MOON_CHECK is compiled out in this configuration";
    Bitset b(64);
    ASSERT_EQ(b.Size(), 64u);
    EXPECT_DEATH((void)static_cast<bool>(b[64]), "");
}

TEST_F(BitsetDeathTest, ConstIndexOutOfRange)
{
    if (!MC_TEST_CHECKS_ACTIVE) GTEST_SKIP() << "MOON_CHECK is compiled out in this configuration";
    Bitset b(65);
    ASSERT_EQ(b.Size(), 65u);
    const Bitset& cb = b;
    EXPECT_DEATH((void)cb[65], "");
}

TEST_F(BitsetDeathTest, FlipOutOfRange)
{
    if (!MC_TEST_CHECKS_ACTIVE) GTEST_SKIP() << "MOON_CHECK is compiled out in this configuration";
    Bitset b(10);
    ASSERT_EQ(b.Size(), 10u);
    EXPECT_DEATH(b.Flip(10), "");
}

TEST_F(BitsetDeathTest, StringWithInvalidCharacter)
{
    if (!MC_TEST_CHECKS_ACTIVE) GTEST_SKIP() << "MOON_CHECK is compiled out in this configuration";
    EXPECT_DEATH({ Bitset bad{ MC::StringView(TEXT("10x1")) }; }, "");
}

// ============================================================================ default allocator

TEST(BitsetDefaultAllocator, Smoke)
{
    // Uses MC::Memory::HeapAllocator, so only ASan can see memory errors here.
    MC::DynamicBitset<> b(130);
    ASSERT_EQ(b.Size(), 130u);
    b.Set(0);
    b.Set(129);
    EXPECT_TRUE(b.Test(0));
    EXPECT_TRUE(b.Test(129));
    EXPECT_EQ(b.Count(), 2u);
}