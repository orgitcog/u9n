// Copyright Epic Games, Inc. All Rights Reserved.

#include "UsdWrappers/UsdAttribute.h"

#include "UsdWrappers/SdfPath.h"
#include "UsdWrappers/UsdPrim.h"
#include "UsdWrappers/VtValue.h"

#include "USDMemory.h"

#if USE_USD_SDK
#include "USDIncludesStart.h"
#include "pxr/usd/usd/attribute.h"
#include "USDIncludesEnd.h"
#endif	  // #if USE_USD_SDK

namespace UE
{
	namespace Internal
	{
		class FUsdAttributeImpl
		{
		public:
			FUsdAttributeImpl() = default;

#if USE_USD_SDK
			explicit FUsdAttributeImpl(const pxr::UsdAttribute& InUsdAttribute)
				: PxrUsdAttribute(InUsdAttribute)
			{
			}

			explicit FUsdAttributeImpl(pxr::UsdAttribute&& InUsdAttribute)
				: PxrUsdAttribute(MoveTemp(InUsdAttribute))
			{
			}

			TUsdStore<pxr::UsdAttribute> PxrUsdAttribute;
#endif	  // #if USE_USD_SDK
		};
	}
}

namespace UE
{
	FUsdAttribute::FUsdAttribute()
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MakeUnique<Internal::FUsdAttributeImpl>();
	}

	FUsdAttribute::FUsdAttribute(const FUsdAttribute& Other)
	{
#if USE_USD_SDK
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MakeUnique<Internal::FUsdAttributeImpl>(Other.Impl->PxrUsdAttribute.Get());
#endif	  // #if USE_USD_SDK
	}

	FUsdAttribute::FUsdAttribute(FUsdAttribute&& Other) = default;

	FUsdAttribute::~FUsdAttribute()
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl.Reset();
	}

	FUsdAttribute& FUsdAttribute::operator=(const FUsdAttribute& Other)
	{
#if USE_USD_SDK
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MakeUnique<Internal::FUsdAttributeImpl>(Other.Impl->PxrUsdAttribute.Get());
#endif	  // #if USE_USD_SDK
		return *this;
	}

	FUsdAttribute& FUsdAttribute::operator=(FUsdAttribute&& Other)
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MoveTemp(Other.Impl);

		return *this;
	}

	bool FUsdAttribute::operator==(const FUsdAttribute& Other) const
	{
#if USE_USD_SDK
		return Impl->PxrUsdAttribute.Get() == Other.Impl->PxrUsdAttribute.Get();
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	bool FUsdAttribute::operator!=(const FUsdAttribute& Other) const
	{
		return !(*this == Other);
	}

	FUsdAttribute::operator bool() const
	{
#if USE_USD_SDK
		return (bool)Impl->PxrUsdAttribute.Get();
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

#if USE_USD_SDK
	FUsdAttribute::FUsdAttribute(const pxr::UsdAttribute& InUsdAttribute)
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MakeUnique<Internal::FUsdAttributeImpl>(InUsdAttribute);
	}

	FUsdAttribute::FUsdAttribute(pxr::UsdAttribute&& InUsdAttribute)
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MakeUnique<Internal::FUsdAttributeImpl>(MoveTemp(InUsdAttribute));
	}

	FUsdAttribute& FUsdAttribute::operator=(const pxr::UsdAttribute& InUsdAttribute)
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MakeUnique<Internal::FUsdAttributeImpl>(InUsdAttribute);
		return *this;
	}

	FUsdAttribute& FUsdAttribute::operator=(pxr::UsdAttribute&& InUsdAttribute)
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MakeUnique<Internal::FUsdAttributeImpl>(MoveTemp(InUsdAttribute));
		return *this;
	}

	FUsdAttribute::operator pxr::UsdAttribute&()
	{
		return Impl->PxrUsdAttribute.Get();
	}

	FUsdAttribute::operator const pxr::UsdAttribute&() const
	{
		return Impl->PxrUsdAttribute.Get();
	}

	FUsdAttribute::operator pxr::UsdProperty&()
	{
		return Impl->PxrUsdAttribute.Get();
	}

	FUsdAttribute::operator const pxr::UsdProperty&() const
	{
		return Impl->PxrUsdAttribute.Get();
	}
#endif	  // #if USE_USD_SDK

	bool FUsdAttribute::GetMetadata(const TCHAR* Key, UE::FVtValue& Value) const
	{
#if USE_USD_SDK
		return Impl->PxrUsdAttribute.Get().GetMetadata(pxr::TfToken{TCHAR_TO_UTF8(Key)}, &Value.GetUsdValue());
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	bool FUsdAttribute::HasMetadata(const TCHAR* Key) const
	{
#if USE_USD_SDK
		return Impl->PxrUsdAttribute.Get().HasMetadata(pxr::TfToken{TCHAR_TO_UTF8(Key)});
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	bool FUsdAttribute::SetMetadata(const TCHAR* Key, const UE::FVtValue& Value) const
	{
#if USE_USD_SDK
		return Impl->PxrUsdAttribute.Get().SetMetadata(pxr::TfToken{TCHAR_TO_UTF8(Key)}, Value.GetUsdValue());
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	bool FUsdAttribute::ClearMetadata(const TCHAR* Key) const
	{
#if USE_USD_SDK
		return Impl->PxrUsdAttribute.Get().ClearMetadata(pxr::TfToken{TCHAR_TO_UTF8(Key)});
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	FName FUsdAttribute::GetName() const
	{
#if USE_USD_SDK
		return FName(UTF8_TO_TCHAR(Impl->PxrUsdAttribute.Get().GetName().GetString().c_str()));
#else
		return FName();
#endif	  // #if USE_USD_SDK
	}

	FName FUsdAttribute::GetBaseName() const
	{
#if USE_USD_SDK
		return FName(UTF8_TO_TCHAR(Impl->PxrUsdAttribute.Get().GetBaseName().GetString().c_str()));
#else
		return FName();
#endif	  // #if USE_USD_SDK
	}

	FName FUsdAttribute::GetTypeName() const
	{
#if USE_USD_SDK
		return FName(UTF8_TO_TCHAR(Impl->PxrUsdAttribute.Get().GetTypeName().GetAsToken().GetString().c_str()));
#else
		return FName();
#endif	  // #if USE_USD_SDK
	}

	FString FUsdAttribute::GetCPPTypeName() const
	{
#if USE_USD_SDK
		return UTF8_TO_TCHAR(Impl->PxrUsdAttribute.Get().GetTypeName().GetCPPTypeName().c_str());
#else
		return FString();
#endif	  // #if USE_USD_SDK
	}

	bool FUsdAttribute::GetTimeSamples(TArray<double>& Times) const
	{
#if USE_USD_SDK
		FScopedUsdAllocs Allocs;

		std::vector<double> UsdTimes;
		bool bResult = Impl->PxrUsdAttribute.Get().GetTimeSamples(&UsdTimes);
		if (!bResult)
		{
			return false;
		}

		Times.SetNumUninitialized(UsdTimes.size());
		FMemory::Memcpy(Times.GetData(), UsdTimes.data(), UsdTimes.size() * sizeof(double));

		return true;
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	size_t FUsdAttribute::GetNumTimeSamples() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdAttribute.Get().GetNumTimeSamples();
#else
		return 0;
#endif	  // #if USE_USD_SDK
	}

	bool FUsdAttribute::HasValue() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdAttribute.Get().HasValue();
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	bool FUsdAttribute::HasAuthoredValue() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdAttribute.Get().HasAuthoredValue();
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	bool FUsdAttribute::HasFallbackValue() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdAttribute.Get().HasFallbackValue();
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	bool FUsdAttribute::ValueMightBeTimeVarying() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdAttribute.Get().ValueMightBeTimeVarying();
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	bool FUsdAttribute::Get(UE::FVtValue& Value, TOptional<double> Time /*= {} */) const
	{
#if USE_USD_SDK
		FScopedUsdAllocs UsdAllocs;

		pxr::UsdTimeCode TimeCode = Time.IsSet() ? Time.GetValue() : pxr::UsdTimeCode::Default();
		return Impl->PxrUsdAttribute.Get().Get(&Value.GetUsdValue(), TimeCode);
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	bool FUsdAttribute::Set(const UE::FVtValue& Value, TOptional<double> Time /*= {} */) const
	{
#if USE_USD_SDK
		FScopedUsdAllocs UsdAllocs;

		pxr::UsdTimeCode TimeCode = Time.IsSet() ? Time.GetValue() : pxr::UsdTimeCode::Default();
		return Impl->PxrUsdAttribute.Get().Set(Value.GetUsdValue(), TimeCode);
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	bool FUsdAttribute::Clear() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdAttribute.Get().Clear();
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	bool FUsdAttribute::ClearAtTime(double Time) const
	{
#if USE_USD_SDK
		return Impl->PxrUsdAttribute.Get().ClearAtTime(pxr::UsdTimeCode(Time));
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	bool FUsdAttribute::ClearConnections() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdAttribute.Get().ClearConnections();
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	bool FUsdAttribute::GetUnionedTimeSamples(const TArray<UE::FUsdAttribute>& Attrs, TArray<double>& OutTimes)
	{
		bool bResult = false;

#if USE_USD_SDK
		FScopedUsdAllocs UsdAllocs;

		std::vector<pxr::UsdAttribute> UsdAttrs;
		UsdAttrs.reserve(Attrs.Num());
		for (const UE::FUsdAttribute& Attr : Attrs)
		{
			UsdAttrs.push_back(Attr);
		}

		std::vector<double> UsdTimes;

		bResult = pxr::UsdAttribute::GetUnionedTimeSamples(UsdAttrs, &UsdTimes);
		if (bResult)
		{
			OutTimes.SetNumUninitialized(UsdTimes.size());
			FMemory::Memcpy(OutTimes.GetData(), UsdTimes.data(), OutTimes.Num() * OutTimes.GetTypeSize());
		}
#endif	  // #if USE_USD_SDK

		return bResult;
	}

	FSdfPath FUsdAttribute::GetPath() const
	{
#if USE_USD_SDK
		return FSdfPath(Impl->PxrUsdAttribute.Get().GetPath());
#else
		return FSdfPath();
#endif	  // #if USE_USD_SDK
	}

	FUsdPrim FUsdAttribute::GetPrim() const
	{
#if USE_USD_SDK
		return FUsdPrim(Impl->PxrUsdAttribute.Get().GetPrim());
#else
		return FUsdPrim();
#endif	  // #if USE_USD_SDK
	}

#if USE_USD_SDK
	// Utility to help map from a UE type to an USD type within the Get<T> function
	template<typename T>
	struct USDTypeHelper
	{
	};

	template<>
	struct USDTypeHelper<float>
	{
		using USDType = float;
	};

	template<>
	struct USDTypeHelper<double>
	{
		using USDType = double;
	};

	template<>
	struct USDTypeHelper<FFloat16>
	{
		using USDType = pxr::GfHalf;
	};

	template<>
	struct USDTypeHelper<FQuat4f>
	{
		using USDType = pxr::GfQuatf;
		using VecType = pxr::GfVec3f;
	};

	template<>
	struct USDTypeHelper<FQuat4d>
	{
		using USDType = pxr::GfQuatd;
		using VecType = pxr::GfVec3d;
	};

	template<>
	struct USDTypeHelper<FVector2DHalf>
	{
		using USDType = pxr::GfVec2h;
	};

	template<>
	struct USDTypeHelper<FVector2f>
	{
		using USDType = pxr::GfVec2f;
	};

	template<>
	struct USDTypeHelper<FVector2d>
	{
		using USDType = pxr::GfVec2d;
	};

	template<>
	struct USDTypeHelper<FIntPoint>
	{
		using USDType = pxr::GfVec2i;
	};

	template<>
	struct USDTypeHelper<FVector3f>
	{
		using USDType = pxr::GfVec3f;
	};

	template<>
	struct USDTypeHelper<FVector3d>
	{
		using USDType = pxr::GfVec3d;
	};

	template<>
	struct USDTypeHelper<FIntVector>
	{
		using USDType = pxr::GfVec3i;
	};

	template<>
	struct USDTypeHelper<FVector4f>
	{
		using USDType = pxr::GfVec4f;
	};

	template<>
	struct USDTypeHelper<FVector4d>
	{
		using USDType = pxr::GfVec4d;
	};

	template<>
	struct USDTypeHelper<FIntRect>
	{
		using USDType = pxr::GfVec4i;
	};
#endif	  // USE_USD_SDK

	template<typename T>
	bool FUsdAttribute::Get(T& Value, TOptional<double> Time) const
	{
#if USE_USD_SDK
		FScopedUsdAllocs UsdAllocs;

		pxr::UsdTimeCode TimeCode = Time.IsSet() ? Time.GetValue() : pxr::UsdTimeCode::Default();
		pxr::UsdAttribute Attr = Impl->PxrUsdAttribute.Get();

		if constexpr (std::is_floating_point_v<T> || std::is_same_v<T, FFloat16>)
		{
			using USDType = typename USDTypeHelper<T>::USDType;

			pxr::VtValue VtValue;
			if (Attr.Get(&VtValue, TimeCode))
			{
				// Cast all floating-point-like types to the desired floating point type
				pxr::VtValue CastVtValue = VtValue.Cast<USDType>();
				if (!CastVtValue.IsEmpty())
				{
					Value = static_cast<T>(CastVtValue.Get<USDType>());
					return true;
				}

				if (VtValue.IsHolding<pxr::SdfTimeCode>())
				{
					const pxr::SdfTimeCode& TimeCodeValue = VtValue.Get<pxr::SdfTimeCode>();
					Value = static_cast<T>(TimeCodeValue.GetValue());
					return true;
				}
			}
		}
		else if constexpr (std::is_same_v<T, FString>)
		{
			pxr::TfToken TypeName = Attr.GetTypeName().GetAsToken();
			if (TypeName == pxr::SdfValueTypeNames->String)
			{
				std::string PxrValue;
				if (Attr.Get(&PxrValue, TimeCode))
				{
					Value = UTF8_TO_TCHAR(PxrValue.c_str());
					return true;
				}
			}
			else if (TypeName == pxr::SdfValueTypeNames->Token)
			{
				pxr::TfToken PxrValue;
				if (Attr.Get(&PxrValue, TimeCode))
				{
					Value = UTF8_TO_TCHAR(PxrValue.GetString().c_str());
					return true;
				}
			}
			else if (TypeName == pxr::SdfValueTypeNames->Asset)
			{
				pxr::SdfAssetPath PxrValue;
				if (Attr.Get(&PxrValue, TimeCode))
				{
					Value = UTF8_TO_TCHAR(PxrValue.GetAssetPath().c_str());
					return true;
				}
			}
			// As a convenience, let's use USD to stringify any attribute to a string, so we can do Get<FString> on anything
			else
			{
				pxr::VtValue VtValue;
				if (Attr.Get(&VtValue, TimeCode))
				{
					std::string UsdStr = pxr::TfStringify(VtValue);
					Value = UTF8_TO_TCHAR(UsdStr.c_str());
					return true;
				}
			}
		}
		else if constexpr (std::is_same_v<T, FMatrix44f> || std::is_same_v<T, FMatrix44d>)
		{
			pxr::TfToken TypeName = Attr.GetTypeName().GetAsToken();
			if (TypeName == pxr::SdfValueTypeNames->Matrix2d)
			{
				pxr::GfMatrix2d Matrix;
				if (Attr.Get(&Matrix, TimeCode))
				{
					Value = T(
						Math::TPlane<typename T::FReal>(Matrix[0][0], Matrix[0][1], 0.0f, 0.0f),
						Math::TPlane<typename T::FReal>(Matrix[1][0], Matrix[1][1], 0.0f, 0.0f),
						Math::TPlane<typename T::FReal>(0.0f, 0.0f, 0.0f, 0.0f),
						Math::TPlane<typename T::FReal>(0.0f, 0.0f, 0.0f, 0.0f)
					);
					return true;
				}
			}
			else if (TypeName == pxr::SdfValueTypeNames->Matrix3d)
			{
				pxr::GfMatrix3d Matrix;
				if (Attr.Get(&Matrix, TimeCode))
				{
					Value = T(
						Math::TPlane<typename T::FReal>(Matrix[0][0], Matrix[0][1], Matrix[0][2], 0.0f),
						Math::TPlane<typename T::FReal>(Matrix[1][0], Matrix[1][1], Matrix[1][2], 0.0f),
						Math::TPlane<typename T::FReal>(Matrix[2][0], Matrix[2][1], Matrix[2][2], 0.0f),
						Math::TPlane<typename T::FReal>(0.0f, 0.0f, 0.0f, 0.0f)
					);
					return true;
				}
			}
			else if (TypeName == pxr::SdfValueTypeNames->Matrix4d || TypeName == pxr::SdfValueTypeNames->Frame4d)
			{
				pxr::GfMatrix4d Matrix;
				if (Attr.Get(&Matrix, TimeCode))
				{
					Value = T(
						Math::TPlane<typename T::FReal>(Matrix[0][0], Matrix[0][1], Matrix[0][2], Matrix[0][3]),
						Math::TPlane<typename T::FReal>(Matrix[1][0], Matrix[1][1], Matrix[1][2], Matrix[1][3]),
						Math::TPlane<typename T::FReal>(Matrix[2][0], Matrix[2][1], Matrix[2][2], Matrix[2][3]),
						Math::TPlane<typename T::FReal>(Matrix[3][0], Matrix[3][1], Matrix[3][2], Matrix[3][3])
					);
					return true;
				}
			}
		}
		else if constexpr (std::is_same_v<T, FQuat4f> || std::is_same_v<T, FQuat4d>)
		{
			using USDType = typename USDTypeHelper<T>::USDType;
			using VecType = typename USDTypeHelper<T>::VecType;

			pxr::VtValue VtValue;
			if (Attr.Get(&VtValue, TimeCode))
			{
				if (VtValue.CanCast<USDType>())
				{
					pxr::VtValue CastVtValue = VtValue.Cast<USDType>();
					const USDType& CastValue = CastVtValue.Get<USDType>();
					const VecType& Imaginary = CastValue.GetImaginary();
					Value = T{Imaginary[0], Imaginary[1], Imaginary[2], CastValue.GetReal()};
					return true;
				}
				// It doesn't seem like USD has conversions between the quaternion types, so we must check
				// for Quath manually
				else if (VtValue.CanCast<pxr::GfQuath>())
				{
					pxr::VtValue CastVtValue = VtValue.Cast<pxr::GfQuath>();
					const pxr::GfQuath& CastValue = CastVtValue.Get<pxr::GfQuath>();
					const pxr::GfVec3h& Imaginary = CastValue.GetImaginary();
					Value = T{Imaginary[0], Imaginary[1], Imaginary[2], CastValue.GetReal()};
					return true;
				}
			}
		}
		else if constexpr (std::is_same_v<T, FVector2DHalf> ||	  //
						   std::is_same_v<T, FVector2f> ||		  //
						   std::is_same_v<T, FVector2d> ||		  //
						   std::is_same_v<T, FIntPoint>)
		{
			using USDType = typename USDTypeHelper<T>::USDType;

			pxr::VtValue VtValue;
			if (Attr.Get(&VtValue, TimeCode))
			{
				if (VtValue.CanCast<USDType>())
				{
					// This will handle all types with C++ type Vec2h/f/g/i (including roles like point, color, etc.)
					pxr::VtValue CastVtValue = VtValue.Cast<USDType>();
					const USDType& CastValue = CastVtValue.template Get<USDType>();
					Value = T{CastValue[0], CastValue[1]};
					return true;
				}
			}
		}
		else if constexpr (std::is_same_v<T, FVector3f> ||	  //
						   std::is_same_v<T, FVector3d> ||	  //
						   std::is_same_v<T, FIntVector>)
		{
			using USDType = typename USDTypeHelper<T>::USDType;

			pxr::VtValue VtValue;
			if (Attr.Get(&VtValue, TimeCode))
			{
				if (VtValue.CanCast<USDType>())
				{
					// This will handle all types with C++ type Vec3h/f/g/i (including roles like point, color, etc.)
					pxr::VtValue CastVtValue = VtValue.Cast<USDType>();
					const USDType& CastValue = CastVtValue.template Get<USDType>();
					Value = T{CastValue[0], CastValue[1], CastValue[2]};
					return true;
				}
			}
		}
		else if constexpr (std::is_same_v<T, FVector4f> ||	  //
						   std::is_same_v<T, FVector4d> ||	  //
						   std::is_same_v<T, FIntRect>)
		{
			using USDType = typename USDTypeHelper<T>::USDType;

			pxr::VtValue VtValue;
			if (Attr.Get(&VtValue, TimeCode))
			{
				if (VtValue.CanCast<USDType>())
				{
					// This will handle all types with C++ type Vec4h/f/g/i (including roles like point, color, etc.)
					pxr::VtValue CastVtValue = VtValue.Cast<USDType>();
					const USDType& CastValue = CastVtValue.template Get<USDType>();
					Value = T{CastValue[0], CastValue[1], CastValue[2], CastValue[3]};
					return true;
				}
			}
		}
		else if constexpr (std::is_same_v<T, FLinearColor> || std::is_same_v<T, FColor>)
		{
			pxr::VtValue VtValue;
			if (Attr.Get(&VtValue, TimeCode))
			{
				// Vec3/Vec4 of floating types can have the color role in USD, so let's convert only those
				if (VtValue.CanCast<pxr::GfVec4f>())
				{
					pxr::VtValue CastVtValue = VtValue.Cast<pxr::GfVec4f>();
					const pxr::GfVec4f& CastValue = CastVtValue.Get<pxr::GfVec4f>();
					if constexpr (std::is_same_v<T, FColor>)
					{
						// Color in USD is always energy linear, so perform sRGB conversion if going to FColor
						const bool bSRGB = true;
						Value = FLinearColor{CastValue[0], CastValue[1], CastValue[2], CastValue[3]}.ToFColor(bSRGB);
					}
					else
					{
						Value = FLinearColor{CastValue[0], CastValue[1], CastValue[2], CastValue[3]};
					}
					return true;
				}
				else if (VtValue.CanCast<pxr::GfVec3f>())
				{
					pxr::VtValue CastVtValue = VtValue.Cast<pxr::GfVec3f>();
					const pxr::GfVec3f& CastValue = CastVtValue.Get<pxr::GfVec3f>();
					if constexpr (std::is_same_v<T, FColor>)
					{
						const bool bSRGB = true;
						Value = FLinearColor{CastValue[0], CastValue[1], CastValue[2], 1.0f}.ToFColor(bSRGB);
					}
					else
					{
						Value = FLinearColor{CastValue[0], CastValue[1], CastValue[2], 1.0f};
					}
					return true;
				}
			}
		}
		else if constexpr (std::is_same_v<T, TArray<uint8>>)
		{
			pxr::TfToken TypeName = Attr.GetTypeName().GetAsToken();
			if (TypeName == pxr::SdfValueTypeNames->UCharArray)
			{
				pxr::VtArray<unsigned char> UsdValue;
				if (Attr.Get(&UsdValue, TimeCode))
				{
					Value.SetNumUninitialized(UsdValue.size());
					FMemory::Memcpy(Value.GetData(), UsdValue.cdata(), Value.Num() * Value.GetTypeSize());
					return true;
				}
			}
		}
		else if constexpr (std::is_same_v<T, int64>)
		{
			// Note: The Unreal int64 and uint64 are typedefs of "long long" and "unsigned long long".
			// On Windows / MSVC these end up with a type that matches int64_t and uint64_t. On Linux / Clang this is not the case.
			//
			// USD only has templates specialized for int64_t/uint64_t, so if we use our typedef'd "long long"s here we will get
			// a compile error on Linux / Clang, so we have to go through the other type.
			int64_t TypedValue;
			if (Attr.Get(&TypedValue, TimeCode))
			{
				Value = static_cast<T>(TypedValue);
				return true;
			}
		}
		else if constexpr (std::is_same_v<T, uint64>)
		{
			// See comment on the int64 case right above this
			uint64_t TypedValue;
			if (Attr.Get(&TypedValue, TimeCode))
			{
				Value = static_cast<T>(TypedValue);
				return true;
			}
		}
		else
		{
			return Attr.Get(&Value, TimeCode);
		}
#endif	  // #if USE_USD_SDK

		return false;
	}

	template UNREALUSDWRAPPER_API bool FUsdAttribute::Get(bool& Value, TOptional<double> Time) const;

	template UNREALUSDWRAPPER_API bool FUsdAttribute::Get(uint8& Value, TOptional<double> Time) const;
	template UNREALUSDWRAPPER_API bool FUsdAttribute::Get(int32& Value, TOptional<double> Time) const;
	template UNREALUSDWRAPPER_API bool FUsdAttribute::Get(uint32& Value, TOptional<double> Time) const;

	template UNREALUSDWRAPPER_API bool FUsdAttribute::Get(int64& Value, TOptional<double> Time) const;
	template UNREALUSDWRAPPER_API bool FUsdAttribute::Get(uint64& Value, TOptional<double> Time) const;

	template UNREALUSDWRAPPER_API bool FUsdAttribute::Get(FFloat16& Value, TOptional<double> Time) const;
	template UNREALUSDWRAPPER_API bool FUsdAttribute::Get(float& Value, TOptional<double> Time) const;
	template UNREALUSDWRAPPER_API bool FUsdAttribute::Get(double& Value, TOptional<double> Time) const;

	template UNREALUSDWRAPPER_API bool FUsdAttribute::Get(FString& Value, TOptional<double> Time) const;

	template UNREALUSDWRAPPER_API bool FUsdAttribute::Get(FMatrix44f& Value, TOptional<double> Time) const;
	template UNREALUSDWRAPPER_API bool FUsdAttribute::Get(FMatrix44d& Value, TOptional<double> Time) const;

	template UNREALUSDWRAPPER_API bool FUsdAttribute::Get(FQuat4f& Value, TOptional<double> Time) const;
	template UNREALUSDWRAPPER_API bool FUsdAttribute::Get(FQuat4d& Value, TOptional<double> Time) const;

	template UNREALUSDWRAPPER_API bool FUsdAttribute::Get(FVector2DHalf& Value, TOptional<double> Time) const;
	template UNREALUSDWRAPPER_API bool FUsdAttribute::Get(FVector2f& Value, TOptional<double> Time) const;
	template UNREALUSDWRAPPER_API bool FUsdAttribute::Get(FVector2d& Value, TOptional<double> Time) const;
	template UNREALUSDWRAPPER_API bool FUsdAttribute::Get(FIntPoint& Value, TOptional<double> Time) const;

	template UNREALUSDWRAPPER_API bool FUsdAttribute::Get(FVector3f& Value, TOptional<double> Time) const;
	template UNREALUSDWRAPPER_API bool FUsdAttribute::Get(FVector3d& Value, TOptional<double> Time) const;
	template UNREALUSDWRAPPER_API bool FUsdAttribute::Get(FIntVector& Value, TOptional<double> Time) const;

	template UNREALUSDWRAPPER_API bool FUsdAttribute::Get(FVector4f& Value, TOptional<double> Time) const;
	template UNREALUSDWRAPPER_API bool FUsdAttribute::Get(FVector4d& Value, TOptional<double> Time) const;
	template UNREALUSDWRAPPER_API bool FUsdAttribute::Get(FIntRect& Value, TOptional<double> Time) const;

	template UNREALUSDWRAPPER_API bool FUsdAttribute::Get(FLinearColor& Value, TOptional<double> Time) const;
	template UNREALUSDWRAPPER_API bool FUsdAttribute::Get(FColor& Value, TOptional<double> Time) const;

	template UNREALUSDWRAPPER_API bool FUsdAttribute::Get(TArray<uint8>& Value, TOptional<double> Time) const;
}	 // namespace UE

namespace UsdUtils
{
	template<typename ValueType>
	ValueType GetAttributeValue(const UE::FUsdPrim& Prim, FStringView AttributeName, TOptional<double> Time)
	{
		if (!Prim)
		{
			return ValueType{};
		}

		UE::FUsdAttribute Attribute = Prim.GetAttribute(AttributeName.GetData());

		ValueType Value{};
		if (Attribute)
		{
			Attribute.Get(Value, Time);
		}

		return Value;
	}

	template UNREALUSDWRAPPER_API bool GetAttributeValue(const UE::FUsdPrim& Prim, FStringView AttributeName, TOptional<double> Time);

	template UNREALUSDWRAPPER_API uint8 GetAttributeValue(const UE::FUsdPrim& Prim, FStringView AttributeName, TOptional<double> Time);
	template UNREALUSDWRAPPER_API int32 GetAttributeValue(const UE::FUsdPrim& Prim, FStringView AttributeName, TOptional<double> Time);
	template UNREALUSDWRAPPER_API uint32 GetAttributeValue(const UE::FUsdPrim& Prim, FStringView AttributeName, TOptional<double> Time);
	template UNREALUSDWRAPPER_API int64 GetAttributeValue(const UE::FUsdPrim& Prim, FStringView AttributeName, TOptional<double> Time);
	template UNREALUSDWRAPPER_API uint64 GetAttributeValue(const UE::FUsdPrim& Prim, FStringView AttributeName, TOptional<double> Time);

	template UNREALUSDWRAPPER_API FFloat16 GetAttributeValue(const UE::FUsdPrim& Prim, FStringView AttributeName, TOptional<double> Time);
	template UNREALUSDWRAPPER_API float GetAttributeValue(const UE::FUsdPrim& Prim, FStringView AttributeName, TOptional<double> Time);
	template UNREALUSDWRAPPER_API double GetAttributeValue(const UE::FUsdPrim& Prim, FStringView AttributeName, TOptional<double> Time);

	template UNREALUSDWRAPPER_API FString GetAttributeValue(const UE::FUsdPrim& Prim, FStringView AttributeName, TOptional<double> Time);

	template UNREALUSDWRAPPER_API FMatrix44f GetAttributeValue(const UE::FUsdPrim& Prim, FStringView AttributeName, TOptional<double> Time);
	template UNREALUSDWRAPPER_API FMatrix44d GetAttributeValue(const UE::FUsdPrim& Prim, FStringView AttributeName, TOptional<double> Time);

	template UNREALUSDWRAPPER_API FQuat4f GetAttributeValue(const UE::FUsdPrim& Prim, FStringView AttributeName, TOptional<double> Time);
	template UNREALUSDWRAPPER_API FQuat4d GetAttributeValue(const UE::FUsdPrim& Prim, FStringView AttributeName, TOptional<double> Time);

	template UNREALUSDWRAPPER_API FVector2DHalf GetAttributeValue(const UE::FUsdPrim& Prim, FStringView AttributeName, TOptional<double> Time);
	template UNREALUSDWRAPPER_API FVector2f GetAttributeValue(const UE::FUsdPrim& Prim, FStringView AttributeName, TOptional<double> Time);
	template UNREALUSDWRAPPER_API FVector2d GetAttributeValue(const UE::FUsdPrim& Prim, FStringView AttributeName, TOptional<double> Time);
	template UNREALUSDWRAPPER_API FIntPoint GetAttributeValue(const UE::FUsdPrim& Prim, FStringView AttributeName, TOptional<double> Time);

	template UNREALUSDWRAPPER_API FVector3f GetAttributeValue(const UE::FUsdPrim& Prim, FStringView AttributeName, TOptional<double> Time);
	template UNREALUSDWRAPPER_API FVector3d GetAttributeValue(const UE::FUsdPrim& Prim, FStringView AttributeName, TOptional<double> Time);
	template UNREALUSDWRAPPER_API FIntVector GetAttributeValue(const UE::FUsdPrim& Prim, FStringView AttributeName, TOptional<double> Time);

	template UNREALUSDWRAPPER_API FVector4f GetAttributeValue(const UE::FUsdPrim& Prim, FStringView AttributeName, TOptional<double> Time);
	template UNREALUSDWRAPPER_API FVector4d GetAttributeValue(const UE::FUsdPrim& Prim, FStringView AttributeName, TOptional<double> Time);
	template UNREALUSDWRAPPER_API FIntRect GetAttributeValue(const UE::FUsdPrim& Prim, FStringView AttributeName, TOptional<double> Time);

	template UNREALUSDWRAPPER_API FLinearColor GetAttributeValue(const UE::FUsdPrim& Prim, FStringView AttributeName, TOptional<double> Time);
	template UNREALUSDWRAPPER_API FColor GetAttributeValue(const UE::FUsdPrim& Prim, FStringView AttributeName, TOptional<double> Time);

	template UNREALUSDWRAPPER_API TArray<uint8> GetAttributeValue(const UE::FUsdPrim& Prim, FStringView AttributeName, TOptional<double> Time);
}	 // namespace UsdUtils
