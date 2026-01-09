// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/MutableMath.h"

#include "Curves/RichCurve.h"
#include "Math/Vector.h"
#include "Math/IntVector.h"
#include "Math/Vector4.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Templates/SharedPointer.h"

#define UE_API MUTABLERUNTIME_API

namespace mu
{    
    class FImage;
	class FModel;
	enum class EDataType : uint8;

#define MUTABLE_DEFINE_POD_SERIALISABLE(Type)									\
	void DLLEXPORT operator<<(mu::FOutputArchive& Arch, const Type& T);			\
	void DLLEXPORT operator>>(mu::FInputArchive& Arch, Type& T);

#define MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(Type)							\
	template<typename Alloc>													\
	void DLLEXPORT operator<<(mu::FOutputArchive& Arch, const TArray<Type, Alloc>& V);	\
	template<typename Alloc>													\
	void DLLEXPORT operator>>(mu::FInputArchive& Arch, TArray<Type, Alloc>& V);

#define MUTABLE_DEFINE_ENUM_SERIALISABLE(Type)									\
    void DLLEXPORT operator<<(mu::FOutputArchive& Arch, const Type& T);			\
    void DLLEXPORT operator>>(mu::FInputArchive& Arch, Type& T);
	
	
    /** This repesents a handle to Mutable resource. */
    template<class R>
    class TResourceProxy : public RefCounted
    {
    public:

        virtual TSharedPtr<const R> Get() = 0;

    };


	/** Proxy implementation that always has the resource loaded in memory. */
	template<class R>
	class TResourceProxyMemory : public TResourceProxy<R>
	{
	private:

		TSharedPtr<const R> Resource;

	public:

		TResourceProxyMemory(const TSharedPtr<const R>& InResource)
		{
			Resource = InResource;
		}

		// TResourceProxy interface
		TSharedPtr<const R> Get() override
		{
			return Resource;
		}
	};


	/** */
	class FModelReader
    {
    public:

        //-----------------------------------------------------------------------------------------
        // Life cycle
        //-----------------------------------------------------------------------------------------

        //! Ensure virtual destruction.
        virtual ~FModelReader() {}

        //-----------------------------------------------------------------------------------------
        // Reading interface
        //-----------------------------------------------------------------------------------------

        //! Identifier of reading data operations sent to this interface.
		//! Negative values indicate an error.
        typedef int32 FOperationID;

		virtual bool DoesBlockExist(const FModel*, uint32 BlockKey) = 0;

		//! \brief Start a data request operation.
		//! \param Model.
        //! \param BlockKey key identifying the model data fragment that is requested.
        //!         This key interpretation depends on the implementation of the ModelStreamer,
		//! \param Buffer is an already-allocated buffer big enough to receive the expected data.
		//! \param size is the size of the pBuffer buffer, which must match the size of the data
		//! requested with the key identifiers.
		//! \param CompletionCallback Optional callback. Copied inside the called function. Will always be called.
		//! \return a previously unused identifier, now used for this operation, that can be used in
		//! calls to the other methods of this interface. If the return value is negative it indicates
		//! an unrecoverable error.
		virtual FOperationID BeginReadBlock(const FModel*, uint32 BlockKey, void* Buffer, uint64 Size, EDataType DataType, TFunction<void(bool bSuccess)>* CompletionCallback = nullptr) = 0;

        //! Check if a data request operation has been completed.
        //! This is a weak check than *may* return true if the given operation has completed, but
        //! it is not mandatory. It is used as a hint by the System to optimise its opertaions.
        //! There is no guarantee that this method will ever be called, and it is safe to always
        //! return false.
        virtual bool IsReadCompleted(FOperationID) = 0;

        /** Complete a data request operation.This method has to block until a data request issued
        * with BeginReadBlock has been completed. After returning from this call, the ID cannot be used
        * any more to identify the same operation and becomes free.
		* \return true if the data was loaded successfully.
		*/
        virtual bool EndRead(FOperationID) = 0;
    };


	/** */
	class FModelWriter
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		//! Ensure virtual destruction.
		virtual ~FModelWriter() {}

		//-----------------------------------------------------------------------------------------
		// Writing interface
		//-----------------------------------------------------------------------------------------

		/** */
		virtual void OpenWriteFile(uint32 BlockKey, bool bIsStreamable) = 0;

		/** */
		virtual void Write(const void* Buffer, uint64 Size) = 0;

		//! \brief Close the file open for writing in a previous call to OpenWriteFile in this
		//! object.
		virtual void CloseWriteFile() = 0;


	};


    /** Interface for any input stream to be use with InputArchives. */
    class FInputStream
    {
    public:

		/** Ensure virtual destruction. */
		virtual ~FInputStream() {}

        /** Read a byte buffer
         * \param pData destination buffer, must have at least size bytes allocated.
         * \param size amount of bytes to read from the stream.
		 */
        virtual void Read( void* Data, uint64 Size ) = 0;
    };


    /** Interface for any output stream to be used with OutputArchives. */
    class FOutputStream
    {
    public:

        /** Ensure virtual destruction. */
        virtual ~FOutputStream() {}

        /** Write a byte buffer
         * \param pData source buffer where data will be read from.
         * \param size amount of data to write to the stream.
		 */
        virtual void Write( const void* Data, uint64 Size ) = 0;

    };


    /** Archive containing data to be deserialised. */
    class FInputArchive
    {
    public:

        /** Construct form an input stream.The stream will not be owned by the archive and the
         * caller must make sure it is not modified or destroyed while serialisation is happening.
		 */
        UE_API FInputArchive( FInputStream* );

		/** Ensure virtual destruction. */
		virtual ~FInputArchive() {}

        //
        UE_API virtual Ptr<TResourceProxy<FImage>> NewImageProxy();

		/** Not owned. */
		FInputStream* Stream = nullptr;

		/** Already read pointers. */
		TArray< TSharedPtr<void> > History;

    };


    /** Archive where data can be serialised to. */
    class FOutputArchive
    {
    public:

        /** Construct form an output stream.The stream will not be owned by the archive and the
         * caller must make sure it is not modified or destroyed while serialisation is happening.
		 */
        UE_API FOutputArchive( FOutputStream* );

		/** Not owned. */
		FOutputStream* Stream = nullptr;

		/** Already written pointers and their ids. */
		TMap< const void*, int32 > History;

    };


    //!
    class FOutputMemoryStream : public FOutputStream
    {
    public:

        /** Create the stream with an optional buffer size in bytes.
		* The internal buffer will be enlarged as much as necessary.
		*/
        UE_API FOutputMemoryStream( uint64 Reserve = 0 );

        // FOutputStream interface
        UE_API virtual void Write( const void* Data, uint64 Size ) override;

        // Own interface

        /** Get the serialised data buffer pointer. This pointer invalidates after a Write
         * operation has been done, and you need to get it again.
		 */
        UE_API const uint8* GetBuffer() const;

        /** Get the amount of data in the stream, in bytes. */
        UE_API int32 GetBufferSize() const;

		/** Clear the internal buffer. */
		UE_API void Reset();

    private:

		TArray64<uint8> Buffer;

    };


	/** This stream doesn't store any data: it just counts the amount of data serialised. */
	class FOutputSizeStream : public FOutputStream
	{
	public:

		// FOutputStream interface
		UE_API void Write(const void* Data, uint64 Size) override;

		// Own interface

		/** Get the amount of data serialised, in bytes. */
		UE_API uint64 GetBufferSize() const;

	private:

		uint64 WrittenBytes = 0;

	};


	/** This stream doesn't store any data: it just calculates of a hash of the data as it receives it. */
	class FOutputHashStream : public FOutputStream
	{
	public:

		// FOutputStream interface
		UE_API void Write(const void* Data, uint64 Size) override;

		// Own interface

		/** Return the hash of the data written so far. */
		UE_API uint64 GetHash() const;

	private:

		uint64 Hash = 0;

	};


	template<typename Type>
	void operator<<(FOutputArchive& Arch, const Type& Value)
	{
        Value.Serialise(Arch);
	}

	template<typename Type>
	void operator>>(FInputArchive& Arch, Type& Value)
	{
        Value.Unserialise(Arch);
	}

	MUTABLE_DEFINE_POD_SERIALISABLE(float);
	MUTABLE_DEFINE_POD_SERIALISABLE(double);

    MUTABLE_DEFINE_POD_SERIALISABLE(int8);
    MUTABLE_DEFINE_POD_SERIALISABLE(int16);
    MUTABLE_DEFINE_POD_SERIALISABLE(int32);
    MUTABLE_DEFINE_POD_SERIALISABLE(int64);

    MUTABLE_DEFINE_POD_SERIALISABLE(uint8);
    MUTABLE_DEFINE_POD_SERIALISABLE(uint16);
    MUTABLE_DEFINE_POD_SERIALISABLE(uint32);
	MUTABLE_DEFINE_POD_SERIALISABLE(uint64);

	MUTABLE_DEFINE_POD_SERIALISABLE(FUintVector2);
	MUTABLE_DEFINE_POD_SERIALISABLE(FIntVector2);
	MUTABLE_DEFINE_POD_SERIALISABLE(UE::Math::TIntVector2<uint16>);
	MUTABLE_DEFINE_POD_SERIALISABLE(UE::Math::TIntVector2<int16>);
	MUTABLE_DEFINE_POD_SERIALISABLE(FVector2f);
	MUTABLE_DEFINE_POD_SERIALISABLE(FVector4f);
	MUTABLE_DEFINE_POD_SERIALISABLE(FMatrix44f);
	MUTABLE_DEFINE_POD_SERIALISABLE(FRichCurveKey);

	MUTABLE_DEFINE_POD_SERIALISABLE(FGuid);

	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(float);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(double);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(uint8);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(uint16);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(uint32);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(uint64);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(int8);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(int16);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(int32);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(int64);

	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(FVector2f);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(FMatrix44f);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(FIntVector2);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(TCHAR);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(FRichCurveKey);

	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(FUintVector2);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(UE::Math::TIntVector2<uint16>);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(UE::Math::TIntVector2<int16>);
	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(FVector4f);
	
	//---------------------------------------------------------------------------------------------
	MUTABLERUNTIME_API void operator<<(FOutputArchive&, const FString&);
	MUTABLERUNTIME_API void operator>>(FInputArchive&, FString&);

	//---------------------------------------------------------------------------------------------
	MUTABLERUNTIME_API void operator<<(FOutputArchive&, const FRichCurve&);
	MUTABLERUNTIME_API void operator>>(FInputArchive&, FRichCurve&);


	//---------------------------------------------------------------------------------------------
	MUTABLERUNTIME_API void operator<<(FOutputArchive&, const FName&);
	MUTABLERUNTIME_API void operator>>(FInputArchive&, FName&);


	//---------------------------------------------------------------------------------------------
	template<typename T, typename Alloc> 
	void operator<<(FOutputArchive& Arch, const TArray<T, Alloc>& V)
	{
		const uint32 Num = (uint32)V.Num();
		Arch << Num;
		
		for (SIZE_T Index = 0; Index < Num; ++Index)
		{
			Arch << V[Index];
		}
	}

	template<typename T, typename Alloc> 
	void operator>>(FInputArchive& Arch, TArray<T, Alloc>& V)
	{
		uint32 Num;
		Arch >> Num;
		V.SetNum(Num);

		for (SIZE_T Index = 0; Index < Num; ++Index)
		{
			Arch >> V[Index];
		}
	}


	// Bool size is not a standard
	MUTABLERUNTIME_API void operator<<(FOutputArchive&, const bool&);
	MUTABLERUNTIME_API void operator>>(FInputArchive&, bool&);
}

#undef UE_API
