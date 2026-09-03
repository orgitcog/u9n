// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaSynchronization.h"
#include <list>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>

#if UBA_USE_MIMALLOC
#include <mimalloc.h>
//#include <mimalloc-override.h>
#endif

#if PLATFORM_WINDOWS && !defined(aligned_alloc)
#define aligned_alloc(a, s) _aligned_malloc(s, a)
#define aligned_free(p) _aligned_free(p)
#else
#define aligned_free(p) free(p)
#endif

namespace uba
{
	struct StringView;

	template<typename T>
	constexpr inline T AlignUp(T arg, uintptr_t alignment) { return T(((uintptr_t)arg+(alignment-1)) & ~(alignment-1)); }


	#if UBA_USE_MIMALLOC
	template<typename Type>
	class Allocator {

	public:
		using value_type = Type;

		Allocator() {}
		Allocator(const Allocator& o) {}
		Allocator(Allocator&& o) noexcept {}
		constexpr bool operator==(const Allocator&) const noexcept { return true; }
		template <class _Other>
		constexpr Allocator(const Allocator<_Other>&) noexcept {}

		value_type* allocate(u64 n) { return (value_type*)mi_malloc_aligned(sizeof(value_type)*n, alignof(value_type)); }
		void deallocate(value_type* p, u64 n) { mi_free_size_aligned(p, sizeof(value_type)*n, alignof(value_type)); }
		u64 max_size() const { return static_cast<size_t>(-1) / sizeof(value_type); }
	};
	#elif 0
	template<typename Type>
	class Allocator {
	public:
		using value_type = Type;

		Allocator() {}
		Allocator(const Allocator& o) {}
		Allocator(Allocator&& o) noexcept {}
		constexpr bool operator==(const Allocator&) const noexcept { return true; }
		template <class _Other>
		constexpr Allocator(const Allocator<_Other>&) noexcept {}

		value_type* allocate(u64 n) { return (value_type*)aligned_alloc(16, AlignUp(sizeof(value_type)*n, 16)); }
		void deallocate(value_type* p, u64 n) { free(p); }
		u64 max_size() const { return static_cast<size_t>(-1) / sizeof(value_type); }
	};
	#else
	template<typename Type>
	using Allocator = std::allocator<Type>;
	#endif

	using TString = std::basic_string<tchar, std::char_traits<tchar>, Allocator<tchar>>;

	template<class T> using Function = std::function<T>;

	template<typename Key, typename Value, typename Hash = std::hash<Key>, typename EqualTo = std::equal_to<Key>>
	using UnorderedMap = std::unordered_map<Key, Value, Hash, EqualTo, Allocator<std::pair<const Key, Value>>>;
	template<typename Key, typename Hash = std::hash<Key>, typename EqualTo = std::equal_to<Key>>
	using UnorderedSet = std::unordered_set<Key, Hash, EqualTo, Allocator<Key>>;
	template<typename Key, typename Value, typename Less = std::less<Key>>
	using MultiMap = std::multimap<Key, Value, Less, Allocator<std::pair<const Key, Value>>>;
	template<typename Value, typename Alloc = Allocator<Value>>
	using Vector = std::vector<Value, Alloc>;
	template<typename Value, typename Alloc = Allocator<Value>>
	using List = std::list<Value, Alloc>;
	template<typename Key, typename Value, typename Less = std::less<Key>>
	using Map = std::map<Key, Value, Less, Allocator<std::pair<const Key, Value>>>;
	template<typename Key, typename Less = std::less<Key>>
	using Set = std::set<Key, Less, Allocator<Key>>;

	struct MemoryBlock
	{
		MemoryBlock(u64 reserveSize_, void* baseAddress_ = nullptr);
		MemoryBlock(u8* baseAddress_ = nullptr);
		~MemoryBlock();
		bool Init(u64 reserveSize_, void* baseAddress_ = nullptr, bool useHugePages = false);
		void Deinit();
		void* Allocate(u64 bytes, u64 alignment, const tchar* hint);
		void* AllocateNoLock(u64 bytes, u64 alignment, const tchar* hint);
		void* CommitNoLock(u64 bytes, const tchar* hint);
		void Free(void* p);
		StringView Strdup(const StringView& str);
		tchar* Strdup(const tchar* str);
		void Swap(MemoryBlock& other);
		

		Futex lock;
		u8* memory;
		u64 reserveSize = 0;
		u64 writtenSize = 0;
		u64 committedSize = 0;
	};


	template<typename Type>
	class GrowingAllocator
	{
	public:
		using value_type = Type;

		GrowingAllocator(MemoryBlock& block) : m_block(&block) {}
		GrowingAllocator(const GrowingAllocator& o) : m_block(o.m_block) {}
		GrowingAllocator(GrowingAllocator&& o) noexcept : m_block(o.m_block) {}
		template <class _Other>
		constexpr GrowingAllocator(const GrowingAllocator<_Other>& o) noexcept : m_block(o.m_block) {}

		value_type* allocate(u64 n)
		{
			return (value_type*)m_block->Allocate(sizeof(value_type)*n, alignof(value_type), TC("GrowingAllocator"));
		}

		/// @warning Naive implementation, assumes `p` is valid.
		void deallocate(value_type* p, u64 n)
		{
			(void)n;
			m_block->Free(p);
		}

		u64 max_size() const
		{
			return static_cast<size_t>(-1) / sizeof(value_type);
		}
	
		bool operator==(const GrowingAllocator& o) const { return m_block == o.m_block; }

		MemoryBlock* m_block;
	};

	template<typename Type>
	class GrowingAllocatorNoLock
	{
	public:
		using value_type = Type;

		GrowingAllocatorNoLock(MemoryBlock* block) : m_block(block) {}
		GrowingAllocatorNoLock(const GrowingAllocatorNoLock& o) : m_block(o.m_block) {}
		GrowingAllocatorNoLock(GrowingAllocatorNoLock&& o) noexcept : m_block(o.m_block) {}
		template <class _Other>
		constexpr GrowingAllocatorNoLock(const GrowingAllocatorNoLock<_Other>& o) noexcept : m_block(o.m_block) {}

		value_type* allocate(u64 n)
		{
			return (value_type*)m_block->AllocateNoLock(sizeof(value_type)*n, alignof(value_type), TC("GrowingAllocatorNoLock"));
		}

		/// @warning Naive implementation, assumes `p` is valid.
		void deallocate(value_type*, u64)
		{
		}

		u64 max_size() const
		{
			return static_cast<size_t>(-1) / sizeof(value_type);
		}
	
		bool operator==(const GrowingAllocatorNoLock& o) const { return m_block == o.m_block; }

		MemoryBlock* m_block;
	};

	template<typename Key, typename Value, typename Hash = std::hash<Key>, typename EqualTo = std::equal_to<Key>>
	using GrowingUnorderedMap = std::unordered_map<Key, Value, Hash, EqualTo, GrowingAllocator<std::pair<const Key, Value>>>;
	template<typename Key, typename Hash = std::hash<Key>, typename EqualTo = std::equal_to<Key>>
	using GrowingUnorderedSet = std::unordered_set<Key, Hash, EqualTo, GrowingAllocator<Key>>;

	template<typename Key, typename Value, typename Hash = std::hash<Key>, typename EqualTo = std::equal_to<Key>>
	using GrowingNoLockUnorderedMap = std::unordered_map<Key, Value, Hash, EqualTo, GrowingAllocatorNoLock<std::pair<const Key, Value>>>;
	template<typename Key, typename Hash = std::hash<Key>, typename EqualTo = std::equal_to<Key>>
	using GrowingNoLockUnorderedSet = std::unordered_set<Key, Hash, EqualTo, GrowingAllocatorNoLock<Key>>;

	template<typename Type>
	struct BlockAllocator
	{
	public:
		BlockAllocator(MemoryBlock& memory) : m_memory(memory)
		{
		}

		void* Allocate()
		{
			SCOPED_FUTEX(m_lock, lock);
			if (m_nextFree)
			{
				void* ptr = (void*)m_nextFree;
				m_nextFree = *(u64*)m_nextFree;
				return ptr;
			}
			void* mem = m_memory.Allocate(sizeof(Type), alignof(Type), TC("BlockAllocator"));
			return mem;
		}

		void Free(void* mem)
		{
			#if UBA_DEBUG
			memset(mem, 0xFE, sizeof(Type));
			#endif
			SCOPED_FUTEX(m_lock, lock);
			*(u64*)mem = m_nextFree;
			m_nextFree = u64(mem);
		}

		Futex m_lock;
		MemoryBlock& m_memory;
		u64 m_nextFree = 0;
	};

	inline u8 HexToByte(tchar c)
	{
		if (c >= '0' && c <= '9') return u8(c - '0');
		else if (c >= 'a' && c <= 'f') return u8(c - 'a' + 10);
		else if (c >= 'A' && c <= 'F') return u8(c - 'A' + 10);
		else return 0; // Invalid hex character
	}
	constexpr tchar g_hexChars[] = TC("0123456789abcdef");

	// TODO: These are backwards but changing would break cas storage
	inline u32 ValueToString(tchar* out, int capacity, u64 value)
	{
		(void)capacity;
		tchar* it = out;
		for (int i=0;i!=8;++i)
		{
			*it++ = g_hexChars[(value >> 4) & 0xf];
			*it++ = g_hexChars[value & 0xf];
			value = value >> 8;
			if (!value)
				break;
		}
		*it = 0;
		return u32(it - out);
	}

	// TODO: These are backwards but changing would break cas storage
	inline u64 StringToValue(const tchar* str, u64 len)
	{
		u64 v = 0;
		const tchar* pos = str + len;
		while (pos != str)
		{
			u8 b = HexToByte(*--pos);
			u8 a = HexToByte(*--pos);
			v = u64(v << 8) | u64(a << 4 | b);
		}

		return v;
	}

	inline u64 StringToValue2(const tchar* str, u64 len)
	{
		u64 v = 0;
		const tchar* pos = str;
		while (*pos)
		{
			u8 a = HexToByte(*pos++);
			u8 b = HexToByte(*pos++);
			v = u64(v << 8) | u64(a << 4 | b);
		}

		return v;
	}

	constexpr tchar g_base64Chars[] = TC("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");

	inline u64 StringToValueBase62(const tchar* str, u64 len)
	{
		u64 result = 0;
		const tchar* it = str;
		const tchar* end = it + len;
		while (it != end)
		{
			tchar c = *it++;
			result *= 62;
			if ('0' <= c && c <= '9')
				result += c - '0';
			else if ('A' <= c && c <= 'Z')
				result += c - 'A' + 10;
			else // if ('a' <= c && c <= 'z')
				result += c - 'a' + 36;
		}
		return result;
	}

	inline u32 RoundUpPow2(u64 value)
	{
		u32 v = u32(value);
		v--;
		v |= v >> 1;
		v |= v >> 2;
		v |= v >> 4;
		v |= v >> 8;
		v |= v >> 16;
		v++;
		return v;
	}

	#if 0
	template<typename Key, typename Value, typename Hash = std::hash<Key>, typename EqualTo = std::equal_to<Key>>
	struct SafeUnorderedMap
	{
		using InternalMap = UnorderedMap<Key, Value, Hash, EqualTo>;

		template <typename... Args>
		Value& GetOrCreate(const Key& key, Args&&... args)
		{
			SCOPED_READ_LOCK(lock, read);
			auto findIt = map.find(key);
			if (findIt != map.end())
				return findIt->second;
			read.Leave();
			SCOPED_WRITE_LOCK(lock, write);
			return map.try_emplace(key, std::forward<Args>(args)...).first->second;
		}

		InternalMap::iterator find(const Key& key) { return map.find(key); }
		InternalMap::iterator end() { return map.end(); }

		ReaderWriterLock lock;
		InternalMap map;
	};
	#endif

	bool SupportsHugePages();
	u64 GetHugePageCount();
}
