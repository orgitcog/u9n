// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"


// Forward declarations
template <class NodeType>
class TIntrusiveDoubleLinkedListIterator;

template <class ElementType, class ContainerType>
class TIntrusiveDoubleLinkedList;


/**
 * Node of an intrusive double linked list
 * Structs/classes must inherit this, to use it, e.g: struct FMyStruct : public TIntrusiveDoubleLinkedListNode<FMyStruct>
 * TIntrusiveDoubleLinkedListNode can be inherited multiple times, ex: if ElementType needs to be stored in several lists at once
 * by specifying a different ContainerType template parameter to distinguish the nodes.
 */
template <class InElementType, class ContainerType = InElementType>
class TIntrusiveDoubleLinkedListNode
{
public:
	using NodeType = TIntrusiveDoubleLinkedListNode<InElementType, ContainerType>;
	using ElementType = InElementType;

	[[nodiscard]] TIntrusiveDoubleLinkedListNode()
		: Next(GetThisElement())
		, Prev(GetThisElement())
	{}

	FORCEINLINE void Reset()
	{
		Next = Prev = GetThisElement();
	}

	[[nodiscard]] FORCEINLINE bool IsInList() const
	{
		return Next != GetThisElement();
	}
	[[nodiscard]] FORCEINLINE ElementType* GetNext() const
	{
		return Next;
	}
	[[nodiscard]] FORCEINLINE ElementType* GetPrev() const
	{
		return Prev;
	}

	/**
	 * Removes this element from the list in constant time.
	 */
	FORCEINLINE void Remove()
	{
		static_cast<NodeType*>(Next)->Prev = Prev;
		static_cast<NodeType*>(Prev)->Next = Next;
		Next = Prev = GetThisElement();
	}

	/**
	 * Insert this node after the specified node
	 */
	FORCEINLINE void InsertAfter(ElementType* NewPrev)
	{
		ElementType* NewNext = static_cast<NodeType*>(NewPrev)->Next;
		Next = NewNext;
		Prev = NewPrev;
		static_cast<NodeType*>(NewNext)->Prev = GetThisElement();
		static_cast<NodeType*>(NewPrev)->Next = GetThisElement();
	}

	/**
	 * Insert this node before the specified node
	 */
	FORCEINLINE void InsertBefore(ElementType* NewNext)
	{
		ElementType* NewPrev = static_cast<NodeType*>(NewNext)->Prev;
		Next = NewNext;
		Prev = NewPrev;
		static_cast<NodeType*>(NewNext)->Prev = GetThisElement();
		static_cast<NodeType*>(NewPrev)->Next = GetThisElement();
	}

protected:
	friend class TIntrusiveDoubleLinkedListIterator<TIntrusiveDoubleLinkedListNode>;
	friend class TIntrusiveDoubleLinkedList<ElementType, ContainerType>;

	[[nodiscard]] FORCEINLINE ElementType*       GetThisElement()       { return static_cast<ElementType*>(this); }
	[[nodiscard]] FORCEINLINE const ElementType* GetThisElement() const { return static_cast<const ElementType*>(this); }

	ElementType* Next;
	ElementType* Prev;
};  // TIntrusiveDoubleLinkedListNode


/**
 * Iterator for intrusive double linked list.
 */
template <class NodeType>
class TIntrusiveDoubleLinkedListIterator
{
public:
	using ElementType = typename NodeType::ElementType;

	[[nodiscard]] FORCEINLINE explicit TIntrusiveDoubleLinkedListIterator(ElementType* Node)
		: CurrentNode(Node)
	{
	}

	FORCEINLINE TIntrusiveDoubleLinkedListIterator& operator++()
	{
		checkSlow(CurrentNode);
		CurrentNode = CurrentNode->NodeType::Next;
		return *this;
	}

	FORCEINLINE TIntrusiveDoubleLinkedListIterator operator++(int)
	{
		auto Tmp = *this;
		++(*this);
		return Tmp;
	}

	FORCEINLINE TIntrusiveDoubleLinkedListIterator& operator--()
	{
		checkSlow(CurrentNode);
		CurrentNode = CurrentNode->NodeType::Prev;
		return *this;
	}

	FORCEINLINE TIntrusiveDoubleLinkedListIterator operator--(int)
	{
		auto Tmp = *this;
		--(*this);
		return Tmp;
	}

	// Accessors.
	[[nodiscard]] FORCEINLINE ElementType& operator->() const
	{
		checkSlow(CurrentNode);
		return *CurrentNode;
	}

	[[nodiscard]] FORCEINLINE ElementType& operator*() const
	{
		checkSlow(CurrentNode);
		return *CurrentNode;
	}

	[[nodiscard]] FORCEINLINE ElementType* GetNode() const
	{
		checkSlow(CurrentNode);
		return CurrentNode;
	}

	[[nodiscard]] FORCEINLINE bool operator==(const TIntrusiveDoubleLinkedListIterator& Other) const { return CurrentNode == Other.CurrentNode; }
	[[nodiscard]] FORCEINLINE bool operator!=(const TIntrusiveDoubleLinkedListIterator& Other) const { return CurrentNode != Other.CurrentNode; }

private:
	ElementType* CurrentNode;
};  // TIntrusiveDoubleLinkedListIterator


/**
 * Intrusive double linked list.
 *
 * @see TDoubleLinkedList
 */
template <class InElementType, class ContainerType = InElementType>
class TIntrusiveDoubleLinkedList
{
public:

	using ElementType = InElementType;
	using NodeType = TIntrusiveDoubleLinkedListNode<ElementType, ContainerType>;

	[[nodiscard]] FORCEINLINE TIntrusiveDoubleLinkedList()
	{
	}

	FORCEINLINE TIntrusiveDoubleLinkedList(const TIntrusiveDoubleLinkedList&) = delete;
	FORCEINLINE TIntrusiveDoubleLinkedList& operator=(const TIntrusiveDoubleLinkedList&) = delete;

	/**
	 * Fast empty that clears this list *without* changing the links in any elements.
	 * 
	 * @see IsEmpty, IsFilled
	 */
	FORCEINLINE void Reset()
	{
		Sentinel.Reset();
	}

	// Accessors.

	[[nodiscard]] FORCEINLINE bool IsEmpty() const
	{
		return Sentinel.Next == GetSentinel();
	}
	[[nodiscard]] FORCEINLINE bool IsFilled() const
	{
		return Sentinel.Next != GetSentinel();
	}

	// Adding/Removing methods

	FORCEINLINE void AddHead(ElementType* Element)
	{
		static_cast<NodeType*>(Element)->InsertAfter(GetSentinel());
	}

	FORCEINLINE void AddHead(TIntrusiveDoubleLinkedList&& Other)
	{
		if (Other.IsFilled())
		{
			static_cast<NodeType*>(Other.Sentinel.Prev)->Next = Sentinel.Next;
			static_cast<NodeType*>(Other.Sentinel.Next)->Prev = GetSentinel();
			static_cast<NodeType*>(Sentinel.Next)->Prev = Other.Sentinel.Prev;
			Sentinel.Next = Other.Sentinel.Next;
			Other.Sentinel.Next = Other.Sentinel.Prev = Other.GetSentinel();
		}
	}

	FORCEINLINE void AddTail(ElementType* Element)
	{
		static_cast<NodeType*>(Element)->InsertBefore(GetSentinel());
	}

	FORCEINLINE void AddTail(TIntrusiveDoubleLinkedList&& Other)
	{
		if (Other.IsFilled())
		{
			static_cast<NodeType*>(Other.Sentinel.Next)->Prev = Sentinel.Prev;
			static_cast<NodeType*>(Other.Sentinel.Prev)->Next = GetSentinel();
			static_cast<NodeType*>(Sentinel.Prev)->Next = Other.Sentinel.Next;
			Sentinel.Prev = Other.Sentinel.Prev;
			Other.Sentinel.Next = Other.Sentinel.Prev = Other.GetSentinel();
		}
	}

	[[nodiscard]] FORCEINLINE ElementType* GetHead()
	{
		return IsFilled() ? Sentinel.Next : nullptr;
	}

	[[nodiscard]] FORCEINLINE ElementType* GetTail()
	{
		return IsFilled() ? Sentinel.Prev : nullptr;
	}

	[[nodiscard]] FORCEINLINE ElementType* PopHead()
	{
		if (IsEmpty())
		{
			return nullptr;
		}

		ElementType* Head = Sentinel.Next;
		static_cast<NodeType*>(Head)->Remove();
		return Head;
	}

	[[nodiscard]] FORCEINLINE ElementType* PopTail()
	{
		if (IsEmpty())
		{
			return nullptr;
		}

		ElementType* Tail = Sentinel.Prev;
		static_cast<NodeType*>(Tail)->Remove();
		return Tail;
	}

	FORCEINLINE static void Remove(ElementType* Element)
	{
		static_cast<NodeType*>(Element)->Remove();
	}

	FORCEINLINE static void InsertAfter(ElementType* InsertThis, ElementType* AfterThis)
	{
		static_cast<NodeType*>(InsertThis)->InsertAfter(AfterThis);
	}

	FORCEINLINE static void InsertBefore(ElementType* InsertThis, ElementType* BeforeThis)
	{
		static_cast<NodeType*>(InsertThis)->InsertBefore(BeforeThis);
	}

	using TIterator = TIntrusiveDoubleLinkedListIterator<NodeType>;
	using TConstIterator = TIntrusiveDoubleLinkedListIterator<const NodeType>;

	[[nodiscard]] FORCEINLINE TIterator      begin()       { return TIterator(Sentinel.Next); }
	[[nodiscard]] FORCEINLINE TConstIterator begin() const { return TConstIterator(Sentinel.Next); }
	[[nodiscard]] FORCEINLINE TIterator      end()         { return TIterator(GetSentinel()); }
	[[nodiscard]] FORCEINLINE TConstIterator end() const   { return TConstIterator(GetSentinel()); }

private:

	[[nodiscard]] FORCEINLINE ElementType*       GetSentinel()       { return static_cast<ElementType*>(&Sentinel); }       //-V717
	[[nodiscard]] FORCEINLINE const ElementType* GetSentinel() const { return static_cast<const ElementType*>(&Sentinel); } //-V717

	NodeType Sentinel;

};  // TIntrusiveDoubleLinkedList

