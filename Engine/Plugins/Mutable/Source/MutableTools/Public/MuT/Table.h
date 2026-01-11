// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Serialisation.h"


namespace mu
{
	// Forward declarations
	class FTable;
	class FMesh;
	class FImage;

	/** Types of the values for the table cells. */
	enum class ETableColumnType : uint32
	{
		None,
		Scalar,
		Color,
		Image,
		Mesh,
		String
	};


	//! A table that contains many rows and defines attributes like meshes, images,
	//! colours, etc. for every column. It is useful to define a big number of similarly structured
	//! objects, by using the NodeDatabase in a model expression.
	//! \ingroup model
	class MUTABLETOOLS_API FTable : public RefCounted
	{
	public:

		FTable();

		//-----------------------------------------------------------------------------------------
		// Own interface
		//-----------------------------------------------------------------------------------------

		//!
		void SetName(const FString&);
		const FString& GetName() const;

		//!
		int32 AddColumn(const FString&, ETableColumnType );

		//! Return the column index with the given name. -1 if not found.
		int32 FindColumn(const FString&) const;

		//!
        void AddRow( uint32 id );

		//!
        void SetCell( int32 Column, uint32 RowId, float Value, const void* ErrorContext = nullptr);
        void SetCell( int32 Column, uint32 RowId, const FVector4f& Value, const void* ErrorContext = nullptr);
		void SetCell( int32 Column, uint32 RowId, TResourceProxy<FImage>* Value, const void* ErrorContext = nullptr);
		void SetCell( int32 Column, uint32 RowId, const TSharedPtr<FMesh>& Value, const void* ErrorContext = nullptr);
        void SetCell( int32 Column, uint32 RowId, const FString& Value, const void* ErrorContext = nullptr);


		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;

	protected:

		/** Forbidden. Manage with the Ptr<> template. */
		~FTable();

	private:

		Private* m_pD;

	};

}
