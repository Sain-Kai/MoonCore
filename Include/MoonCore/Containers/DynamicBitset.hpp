#pragma once
#include "MoonCore/Types.hpp"
#include "MoonCore/Utility/Debug.hpp"
#include "MoonCore/Utility/Algorithm.hpp"
#include "MoonCore/Containers/Details/Bit.inl"
#include "MoonCore/Containers/String/StringView.hpp"
#include "MoonCore/Memory/Allocators/AllocatorShared.hpp"
#include "MoonCore/Memory/Allocators/Heap/HeapAllocator.hpp"

namespace MC
{
    template<Concept::HeapAllocatorType Allocator = Memory::HeapAllocator<uptr>>
        requires Concept::SameAs<typename Allocator::ValueType, uptr>
    class DynamicBitset
    {
    public:
        struct Reference
        {
            constexpr Reference(uptr& word, u32 bit) : Word(word), Bit(bit) {}

            Reference& operator=(bool state) noexcept
            {
                Word = (Word & ~(static_cast<uptr>(1) << Bit)) | (static_cast<uptr>(state) << Bit);
                return *this;
            }

            Reference& operator=(const Reference& other) noexcept
            {
                return *this = static_cast<bool>(other);
            }

            operator bool() const noexcept
            {
                return (Word & static_cast<uptr>(1) << Bit) != 0;
            }

            uptr& Word;
            u32 Bit;
        };

        using ValueType = uptr;
        using AllocatorType = Allocator;

    public:
        static constexpr usize BitsPerWord = sizeof(uptr) * 8;
        static constexpr usize CalculateWordCount(usize bitCount) { return (bitCount + BitsPerWord - 1) / BitsPerWord; }

        static constexpr uptr TailMask(usize bitCount) noexcept
        {
            const usize remainder = bitCount % BitsPerWord;
            return remainder == 0 ? NumericLimit<uptr>::Max : NumericLimit<uptr>::Max >> (BitsPerWord - remainder);
        }

    public:
        explicit DynamicBitset(usize initialBitCount = 64) noexcept
        {
            if (initialBitCount == 0) return;
            Allocate(CalculateWordCount(initialBitCount));
            m_BitCount = initialBitCount;
        }

        DynamicBitset(const StringView& str,
            char8 zero = TEXT('0'), char8 one = TEXT('1')) noexcept
        {
            Reserve(str.Size());
            m_BitCount = str.Size();

            for (usize i = 0; i < m_BitCount; ++i)
            {
                MOON_CHECK(str[i] == zero || str[i] == one, "Character != zero nor one");
                if (str[m_BitCount - 1 - i] == zero) continue;
                Set(i, true);
            }
        }

        DynamicBitset(const DynamicBitset& other) noexcept
        {
            m_Allocator = other.m_Allocator;
            m_Words = nullptr;

            if (other.m_WordCount == 0) return;

            Allocate(other.m_WordCount);

            m_BitCount = other.m_BitCount;
            memcpy(m_Words, other.m_Words, m_WordCount * sizeof(uptr));
        }

        DynamicBitset(DynamicBitset&& other) noexcept
        {
            m_Allocator = Move(other.m_Allocator);
            m_BitCount = other.m_BitCount;
            m_WordCount = other.m_WordCount;
            m_Words = other.m_Words;

            other.m_Words = nullptr;   // the moved-from bitset is empty, not a size without storage
            other.m_WordCount = 0;
            other.m_BitCount = 0;
        }

        ~DynamicBitset() noexcept
        {
            if (m_Words)
            {
                m_Allocator.Deallocate(m_Words, m_WordCount);
                m_Words = nullptr;
            }
        }

    public:
        constexpr usize Size() const noexcept { return m_BitCount; }
        constexpr usize Capacity() const noexcept { return m_WordCount * BitsPerWord; }
        uptr* Words() noexcept { return m_Words; }
        const uptr* Words() const noexcept { return m_Words; }

    public:
        bool Reserve(usize bitCount) noexcept
        {
            usize wordCount = CalculateWordCount(bitCount);
            if (wordCount < m_WordCount) return false;
            if (wordCount == m_WordCount) return true;   // nothing to do; don't reallocate
            Allocate(wordCount);
            return true;
        }

        void Shrink() noexcept
        {
            usize wordCount = CalculateWordCount(m_BitCount);
            if (wordCount < m_WordCount) Allocate(wordCount);
        }

    public:
        DynamicBitset& operator=(const DynamicBitset& other) noexcept
        {
            if (this == &other) return *this;
            if (m_Words) m_Allocator.Deallocate(m_Words, m_WordCount);

            m_Allocator = other.m_Allocator;
            m_Words = nullptr;
            m_WordCount = 0;
            m_BitCount = 0;

            if (other.m_WordCount == 0) return *this;

            Allocate(other.m_WordCount);

            m_BitCount = other.m_BitCount;
            memcpy(m_Words, other.m_Words, m_WordCount * sizeof(uptr));
            return *this;
        }

        DynamicBitset& operator=(DynamicBitset&& other) noexcept
        {
            if (this == &other) return *this;
            if (m_Words) m_Allocator.Deallocate(m_Words, m_WordCount);

            m_Allocator = Move(other.m_Allocator);
            m_BitCount = other.m_BitCount;
            m_WordCount = other.m_WordCount;
            m_Words = other.m_Words;

            other.m_Words = nullptr;
            other.m_WordCount = 0;
            other.m_BitCount = 0;
            return *this;
        }

        constexpr Reference operator[](usize bitIndex) noexcept
        {
            MOON_CHECK(bitIndex < m_BitCount, "bitIndex >= BitCount");
            return Reference{ m_Words[GetWord(bitIndex)], static_cast<u32>(bitIndex % BitsPerWord) };
        }

        constexpr bool operator[](usize bitIndex) const noexcept
        {
            MOON_CHECK(bitIndex < m_BitCount, "bitIndex >= BitCount");
            return Get(GetWord(bitIndex), static_cast<u32>(bitIndex % BitsPerWord));
        }

        constexpr bool operator==(const DynamicBitset& other) const noexcept
        {
            if (other.m_BitCount != m_BitCount) return false;
            const usize usedWords = UsedWords();   // same for both, because the sizes match
            for (usize i = 0; i < usedWords; ++i)
                if (m_Words[i] != other.m_Words[i]) return false;
            return true;
        }

        constexpr DynamicBitset& operator&=(const DynamicBitset& other) noexcept
        {
            MOON_CHECK(other.m_BitCount == m_BitCount, "Bitwise operation on bitsets of different sizes");
            const usize usedWords = UsedWords();
            for (usize i = 0; i < usedWords; i++)
                m_Words[i] &= other.m_Words[i];
            return *this;
        }

        constexpr DynamicBitset& operator|=(const DynamicBitset& other) noexcept
        {
            MOON_CHECK(other.m_BitCount == m_BitCount, "Bitwise operation on bitsets of different sizes");
            const usize usedWords = UsedWords();
            for (usize i = 0; i < usedWords; i++)
                m_Words[i] |= other.m_Words[i];
            return *this;
        }

        constexpr DynamicBitset& operator^=(const DynamicBitset& other) noexcept
        {
            MOON_CHECK(other.m_BitCount == m_BitCount, "Bitwise operation on bitsets of different sizes");
            const usize usedWords = UsedWords();
            for (usize i = 0; i < usedWords; i++)
                m_Words[i] ^= other.m_Words[i];
            return *this;
        }

        constexpr DynamicBitset operator~() const noexcept
        {
            DynamicBitset copy = *this;
            copy.Flip();
            return copy;
        }

        constexpr DynamicBitset& operator>>=(usize position) noexcept
        {
            if (position >= m_BitCount) return Reset();   // also covers m_BitCount == 0 and huge shifts
            const usize usedWords = UsedWords();
            const usize wordShift = position / BitsPerWord;
            if (wordShift != 0)
                for (usize i = 0; i < usedWords; ++i)
                    m_Words[i] = i + wordShift < usedWords ? m_Words[i + wordShift] : 0;
            if ((position %= BitsPerWord) != 0)
            {
                for (usize i = 0; i + 1 < usedWords; ++i)
                    m_Words[i] = (m_Words[i] >> position) | (m_Words[i + 1] << (BitsPerWord - position));
                m_Words[usedWords - 1] >>= position;
            }
            return *this;   // nothing moved up, so the padding is still 0
        }

        DynamicBitset operator>>(usize position) const noexcept
        {
            DynamicBitset result = *this;
            result >>= position;
            return result;
        }

        constexpr DynamicBitset& operator<<=(usize position) noexcept
        {
            if (position >= m_BitCount) return Reset();
            const usize usedWords = UsedWords();
            const usize wordShift = position / BitsPerWord;
            if (wordShift != 0)
                for (usize i = usedWords; i-- > 0;)
                    m_Words[i] = wordShift <= i ? m_Words[i - wordShift] : 0;
            if ((position %= BitsPerWord) != 0)
            {
                for (usize i = usedWords - 1; i > 0; --i)
                    m_Words[i] = (m_Words[i] << position) | (m_Words[i - 1] >> (BitsPerWord - position));
                m_Words[0] <<= position;
            }
            Trim();   // bits pushed past Size() must not survive
            return *this;
        }

        DynamicBitset operator<<(usize position) const noexcept
        {
            DynamicBitset result = *this;
            result <<= position;
            return result;
        }

    public:
        constexpr bool Test(usize bitIndex) const noexcept
        {
            MOON_CHECK(bitIndex < m_BitCount, "bitIndex >= BitCount");
            return Get(GetWord(bitIndex), static_cast<u32>(bitIndex % BitsPerWord));
        }

        constexpr bool All() const noexcept
        {
            const usize usedWords = UsedWords();
            if (usedWords == 0) return true;
            for (usize i = 0; i + 1 < usedWords; ++i)
                if (m_Words[i] != NumericLimit<uptr>::Max) return false;
            return m_Words[usedWords - 1] == TailMask(m_BitCount);   // padding is 0, so == is exact
        }

        constexpr bool Any() const noexcept
        {
            const usize usedWords = UsedWords();
            for (usize i = 0; i < usedWords; ++i)
                if (m_Words[i] != 0)
                    return true;
            return false;
        }

        constexpr bool None() const noexcept
        {
            return !Any();
        }

        constexpr usize Count() const noexcept
        {
            usize result = 0;
            const usize usedWords = UsedWords();
            for (usize i = 0; i < usedWords; ++i)
            {
                uptr word = m_Words[i];
                for (usize chunk = 0; chunk < BitsPerWord; chunk += 16)
                    result += Details::PopCount(static_cast<u16>(word >> chunk));
            }
            return result;
        }

        constexpr DynamicBitset& Set() noexcept
        {
            const usize usedWords = UsedWords();
            for (usize i = 0; i < usedWords; ++i) m_Words[i] = NumericLimit<uptr>::Max;
            Trim();
            return *this;
        }

        DynamicBitset& Set(usize bitIndex, bool state = true) noexcept
        {
            Set(GetWord(bitIndex), bitIndex % BitsPerWord, state);
            return *this;
        }

        constexpr DynamicBitset& Reset() noexcept
        {
            for (usize i = 0; i < m_WordCount; ++i) m_Words[i] = 0;
            return *this;
        }

        constexpr DynamicBitset& Reset(usize bitIndex) noexcept
        {
            Set(GetWord(bitIndex), bitIndex % BitsPerWord, false);
            return *this;
        }

        constexpr DynamicBitset& Flip() noexcept
        {
            const usize usedWords = UsedWords();
            for (usize i = 0; i < usedWords; ++i) m_Words[i] = ~m_Words[i];
            Trim();
            return *this;
        }

        constexpr DynamicBitset& Flip(usize bitIndex) noexcept
        {
            Set(GetWord(bitIndex), bitIndex % BitsPerWord, !Test(bitIndex));
            return *this;
        }

    private:
        void Set(usize word, usize bit, bool state) noexcept
        {
            if (word >= m_WordCount) Allocate(word + 1);
            m_BitCount = Max(word * BitsPerWord + bit + 1, m_BitCount);
            m_Words[word] = (m_Words[word] & ~(static_cast<uptr>(1) << bit)) | (static_cast<uptr>(state) << bit);
        }

        constexpr bool Get(usize word, usize bit) const noexcept
        {
            return (m_Words[word] & (static_cast<uptr>(1) << bit)) != 0;
        }

        static constexpr usize GetWord(usize bitIndex) noexcept
        {
            return bitIndex / BitsPerWord;
        }

        constexpr usize UsedWords() const noexcept
        {
            return CalculateWordCount(m_BitCount);
        }

    private:
        constexpr void Trim() noexcept
        {
            const usize usedWords = UsedWords();
            if (usedWords == 0) return;
            m_Words[usedWords - 1] &= TailMask(m_BitCount);
        }

    private:
        void Allocate(usize newWordCount) noexcept
        {
            ValueType* newBlock = m_Allocator.Allocate(newWordCount);

            usize elementsToMove = (m_Words && m_WordCount > 0) ? Min(m_WordCount, newWordCount) : 0;

            memset(newBlock, 0, newWordCount * sizeof(uptr));

            if (m_Words && m_WordCount > 0)
            {
                memmove(newBlock, m_Words, elementsToMove * sizeof(ValueType));
                m_Allocator.Deallocate(m_Words, m_WordCount);
            }

            m_Words = newBlock;
            m_WordCount = newWordCount;

            m_BitCount = Min(m_BitCount, m_WordCount * BitsPerWord);
        }

    private:
        uptr* m_Words = nullptr;
        usize m_WordCount = 0;
        usize m_BitCount = 0;

        AllocatorType m_Allocator;
    };

    template<MC::Concept::HeapAllocatorType Allocator>
    MC::DynamicBitset<Allocator> operator&(const MC::DynamicBitset<Allocator>& left,
        const MC::DynamicBitset<Allocator>& right) noexcept
    {
        MC::DynamicBitset<Allocator> result = left;
        result &= right;
        return result;
    }

    template<MC::Concept::HeapAllocatorType Allocator>
    MC::DynamicBitset<Allocator> operator|(const MC::DynamicBitset<Allocator>& left,
        const MC::DynamicBitset<Allocator>& right) noexcept
    {
        MC::DynamicBitset<Allocator> result = left;
        result |= right;
        return result;
    }

    template<MC::Concept::HeapAllocatorType Allocator>
    MC::DynamicBitset<Allocator> operator^(const MC::DynamicBitset<Allocator>& left,
        const MC::DynamicBitset<Allocator>& right) noexcept
    {
        MC::DynamicBitset<Allocator> result = left;
        result ^= right;
        return result;
    }
}