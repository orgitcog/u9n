// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/AssertionMacros.h"

namespace UE::Private
{

template <typename TypeTraits>
class TIntrusiveListIterator
{
public:
	using ElementType = typename TypeTraits::ElementType;

	explicit TIntrusiveListIterator(ElementType* InElement)
		: Element(InElement)
	{ }

	ElementType&	operator*() const	{ check(Element); return *Element; }
	explicit		operator bool()		{ return Element != nullptr; }
	void			operator++()		{ check(Element); Element = TypeTraits::GetNext(Element); }
	bool			operator!=(const TIntrusiveListIterator& Other) const { return Element != Other.Element; }

private:
	ElementType* Element;
};

} // namespace UE::Private

template <typename Type>
struct TIntrusiveListElement
{
	using ElementType = Type; 

	static Type* GetNext(const ElementType* Element)
	{
		return Element->Next;
	}

	static void SetNext(ElementType* Element, ElementType* Next)
	{
		Element->Next = Next;
	}
};

template <typename TypeTraits>
class TIntrusiveList
{
public:
	using ElementType		= typename TypeTraits::ElementType;
	using FIterator			= UE::Private::TIntrusiveListIterator<TypeTraits>;
	using FConstIterator	= UE::Private::TIntrusiveListIterator<const TypeTraits>;

	TIntrusiveList() = default;
	TIntrusiveList(const TIntrusiveList&) = delete;
	TIntrusiveList(TIntrusiveList&& Other)
		: Head(Other.Head)
		, Tail(Other.Tail)
	{
		Other.Head = Other.Tail = nullptr;
	}
	explicit TIntrusiveList(ElementType* Element)
	{
		Head = Tail = Element;
	}

	TIntrusiveList& operator=(const TIntrusiveList&) = delete;
	TIntrusiveList& operator=(TIntrusiveList&& Other)
	{
		Head = Other.Head;
		Tail = Other.Tail;
		Other.Head = Other.Tail = nullptr;
		return *this;
	}

	void AddTail(ElementType* Element)
	{
		check(Element != nullptr && TypeTraits::GetNext(Element) == nullptr);

		if (Tail != nullptr)
		{
			check(Head != nullptr);
			TypeTraits::SetNext(Tail, Element);
			Tail = Element;
		}
		else
		{
			check(Head == nullptr);
			Head = Tail = Element;
		}
	}

	void AddTail(ElementType* First, ElementType* Last)
	{
		check(First && Last);
		check(TypeTraits::GetNext(First) != nullptr || First == Last);

		if (Tail != nullptr)
		{
			check(Head != nullptr);
			TypeTraits::SetNext(Tail, First);
			Tail = Last;
		}
		else
		{
			check(Head == nullptr);
			Head = First;
			Tail = Last;
		}
	}

	void AddTail(TIntrusiveList&& Other)
	{
		if (!Other.IsEmpty())
		{
			AddTail(Other.Head, Other.Tail);
			Other.Head = Other.Tail = nullptr;
		}
	}

	ElementType* PopHead()
	{
		ElementType* Element = Head;
		if (Element != nullptr)
		{
			Head = TypeTraits::GetNext(Element);
			if (Head == nullptr)
			{
				Tail = nullptr;
			}
			TypeTraits::SetNext(Element, nullptr);
		}

		return Element;
	}

	ElementType* PeekHead()
	{
		return Head;
	}

	bool Remove(ElementType* Element)
	{
		if (Element == nullptr || IsEmpty())
		{
			return false;
		}

		if (Element == Head)
		{
			PopHead();
			return true;
		}

		ElementType* It = Head;
		ElementType* NextElement = TypeTraits::GetNext(It);
		while (NextElement != nullptr && NextElement != Element)
		{
			It = NextElement;
			NextElement = TypeTraits::GetNext(It);
		}

		if (NextElement != Element)
		{
			return false;
		}

		It->Next = TypeTraits::GetNext(Element);
		TypeTraits::SetNext(Element, nullptr);
		if (Element == Tail)
		{
			Tail = It;
		}

		return true;
	}

	bool				IsEmpty() const { return Head == nullptr; }
	ElementType*		GetHead()		{ return Head; }
	const ElementType*	GetHead() const { return Head; }
	ElementType*		GetTail()		{ return Tail; }
	const ElementType*	GetTail() const { return Tail; }

	FIterator			begin()			{ return FIterator(Head); }
	FConstIterator		begin() const	{ return FConstIterator(Head); }
	FIterator			end()			{ return FIterator(nullptr); }
	FConstIterator		end() const		{ return FConstIterator(nullptr); }

private:
	ElementType* Head = nullptr;
	ElementType* Tail = nullptr;
};
