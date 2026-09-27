// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Utilities/UtilitiesMP4.h"

namespace Electra
{
	namespace UtilitiesMP4
	{
		class FMP4BoxBase : public TSharedFromThis<FMP4BoxBase>
		{
		public:
			static TSharedPtr<FMP4BoxBase, ESPMode::ThreadSafe> Create(const TWeakPtr<FMP4BoxBase, ESPMode::ThreadSafe>& InParent, const FMP4BoxInfo& InBoxInfo)
			{ return MakeShareable<FMP4BoxBase>(new FMP4BoxBase(InParent, InBoxInfo)); }
			virtual ~FMP4BoxBase() = default;

			virtual const FMP4BoxInfo& GetBoxInfo() const
			{ return BoxInfo; }
			virtual uint32 GetType() const
			{ return BoxInfo.Type; }
			virtual int64 GetBoxSize() const
			{ return BoxInfo.Size; }
			virtual TConstArrayView<uint8> GetBoxData() const
			{ return BoxInfo.Data; }
			virtual int64 GetBoxFileOffset() const
			{ return BoxInfo.Offset; }
			virtual int64 GetBoxDataOffset() const
			{ return BoxInfo.DataOffset; }
			virtual bool IsLeafBox() const
			{ return true; }
			virtual bool IsListOfEntries() const
			{ return false; }
			virtual bool IsSampleDescription() const
			{ return false; }

			//-------------------------------------------------------------------------
			// Parent box query methods
			//
			template<typename T>
			TSharedPtr<T, ESPMode::ThreadSafe> FindParentBox(uint32 InType) const
			{
				TSharedPtr<T, ESPMode::ThreadSafe> Parent = StaticCastSharedPtr<T>(ParentBox.Pin());
				while(Parent.IsValid())
				{
					if (Parent->GetType() == InType)
					{
						return Parent;
					}
					Parent = StaticCastSharedPtr<T>(Parent->ParentBox.Pin());
				}
				return Parent;
			}

			//-------------------------------------------------------------------------
			// Child box query methods
			//
			const TArray<TSharedPtr<FMP4BoxBase, ESPMode::ThreadSafe>> GetChildren() const
			{ return Children; }

			// Find the first box of the given type being a child of this box, or any child of a child box.
			template<typename T>
			TSharedPtr<T, ESPMode::ThreadSafe> FindBoxRecursive(uint32 InType, int32 InMaxDepth=8) const
			{
				// First pass, check all children if they are of the type
				for(int32 i=0,iMax=Children.Num(); i<iMax; ++i)
				{
					if (Children[i]->GetType() == InType)
					{
						return StaticCastSharedPtr<T>(Children[i]);
					}
				}
				// Second pass, check children recursively
				if (InMaxDepth > 0)
				{
					for(int32 i=0, iMax=Children.Num(); i<iMax; ++i)
					{
						TSharedPtr<T, ESPMode::ThreadSafe> Box = Children[i]->FindBoxRecursive<T>(InType, InMaxDepth - 1);
						if (Box.IsValid())
						{
							return Box;
						}
					}
				}
				return nullptr;
			}


			// Returns all instances of a given box type from the direct children of THIS BOX ONLY.
			template<typename T>
			void GetAllBoxInstances(TArray<TSharedPtr<T, ESPMode::ThreadSafe>>& OutAllBoxes, uint32 InType) const
			{
				for(int32 i=0,iMax=Children.Num(); i<iMax; ++i)
				{
					if (Children[i]->GetType() == InType)
					{
						OutAllBoxes.Emplace(StaticCastSharedPtr<T>(Children[i]));
					}
				}
			}



			// Called by building the box tree. Not to be called by user code.
			virtual bool AddChildBox(TSharedPtr<FMP4BoxBase, ESPMode::ThreadSafe> InChildBox)
			{
				if (InChildBox.IsValid())
				{
					Children.Emplace(MoveTemp(InChildBox));
					return true;
				}
				return false;
			}

			virtual void ProcessBoxChildrenRecursively(FMP4AtomReaderHelper& InOutReader, const FMP4BoxInfo& InCurrentBoxInfo);

			void SetRootBoxData(const TSharedPtr<FMP4BoxInfo, ESPMode::ThreadSafe>& InRootBoxData)
			{ RootBoxData = InRootBoxData; }
		protected:
			FMP4BoxBase(const TWeakPtr<FMP4BoxBase, ESPMode::ThreadSafe>& InParent, const FMP4BoxInfo& InBoxInfo) : ParentBox(InParent), BoxInfo(InBoxInfo)
			{ }

			TSharedPtr<FMP4BoxInfo, ESPMode::ThreadSafe> RootBoxData;
			TWeakPtr<FMP4BoxBase, ESPMode::ThreadSafe> ParentBox;
			TArray<TSharedPtr<FMP4BoxBase, ESPMode::ThreadSafe>> Children;
			FMP4BoxInfo BoxInfo;
		};

	} // namespace UtilitiesMP4

} // namespace Electra
