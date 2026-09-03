// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMNamedType.h"
#include "UObject/Class.h"
#include "UObject/CoreRedirects.h"
#include "VerseVM/VVMArray.h"
#include "VerseVM/VVMPackage.h"
#include "VerseVM/VVMProgram.h"
#include "VerseVM/VVMValuePrinting.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VNamedType)

template <typename TVisitor>
void VNamedType::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(Package, TEXT("Package"));
	Visitor.Visit(RelativePath, TEXT("RelativePath"));
	Visitor.Visit(BaseName, TEXT("BaseName"));
	Visitor.Visit(AttributeIndices, TEXT("AttributeIndices"));
	Visitor.Visit(Attributes, TEXT("Attributes"));
	Visitor.Visit(AssociatedUEType, TEXT("AssociatedUEType"));
}

void VNamedType::SerializeImpl(FAllocationContext Context, FStructuredArchiveVisitor& Visitor)
{
	Visitor.Visit(Package, TEXT("Package"));
	Visitor.Visit(RelativePath, TEXT("RelativePath"));
	Visitor.Visit(BaseName, TEXT("BaseName"));
	Visitor.Visit(AttributeIndices, TEXT("AttributeIndices"));
	Visitor.Visit(Attributes, TEXT("Attributes"));
	Visitor.Visit(bNativeBound, TEXT("bNativeBound"));
	Visitor.Visit(AssociatedUEType, TEXT("AssociatedUEType"));
}

VNamedType::VNamedType(FAllocationContext Context, VEmergentType* EmergentType, VPackage* InPackage, VArray* InRelativePath, VArray* InBaseName, VArray* InAttributeIndices, VArray* InAttributes, UField* InImportType, bool bInNativeBound)
	: VType(Context, EmergentType)
	, Package(Context, InPackage)
	, RelativePath(Context, InRelativePath)
	, BaseName(Context, InBaseName)
	, AttributeIndices(Context, InAttributeIndices)
	, Attributes(Context, InAttributes)
	, bNativeBound(bInNativeBound)
{
	if (InImportType != nullptr)
	{
		AssociatedUEType.Set(Context, InImportType);
		Package->NotifyUsedImport(Context, this);
	}
}

VNamedType::VNamedType(FAllocationContext Context, VEmergentType* EmergentType)
	: VType(Context, EmergentType)
{
}

void VNamedType::AppendToStringImpl(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth)
{
	Builder.Append(Format == EValueStringFormat::JSON ? UTF8TEXT("\"") : UTF8TEXT(""));
	AppendQualifiedName(Builder);
	Builder.Append(Format == EValueStringFormat::JSON ? UTF8TEXT("\"") : UTF8TEXT(""));
}

void VNamedType::AppendQualifiedName(FUtf8StringBuilderBase& Builder) const
{
	Builder << "(";
	Builder << Package->GetRootPath().AsStringView();
	if (FUtf8StringView Path = RelativePath->AsStringView(); !Path.IsEmpty())
	{
		Builder << "/";
		Builder << Path;
	}
	Builder << ":)";
	Builder << BaseName->AsStringView();
}

void VNamedType::AppendScopeName(FUtf8StringBuilderBase& Builder) const
{
	Builder << Package->GetRootPath().AsStringView();
	if (FUtf8StringView Path = RelativePath->AsStringView(); !Path.IsEmpty())
	{
		Builder << "/";
		Builder << Path;
	}
	Builder << "/";
	Builder << BaseName->AsStringView();
}

void VNamedType::AppendMangledName(FUtf8StringBuilderBase& Builder, UTF8CHAR Separator) const
{
	if (FUtf8StringView Path = RelativePath->AsStringView(); !Path.IsEmpty())
	{
		int32 Slash;
		while (Path.FindChar(UTF8CHAR('/'), Slash))
		{
			Builder << Path.Left(Slash);
			Builder << Separator;
			Path.RightChopInline(Slash + 1);
		}
		Builder << Path;
		Builder << Separator;
	}
	Builder << BaseName->AsStringView();
}

void VNamedType::AddRedirect(ECoreRedirectFlags Kind)
{
#if WITH_EDITORONLY_DATA
	// Verse previously put each individual content and asset class in its own package.
	// Redirect their old names to support uncooked data that was authored using them.

	FUtf8StringView VersePackageName = Package->GetName().AsStringView();

	EVersePackageType PackageType;
	TUtf8StringBuilder<Verse::Names::DefaultNameLength> PackageName = Verse::Names::GetUPackagePath(VersePackageName, &PackageType);
	if (PackageType != EVersePackageType::VNI)
	{
		TUtf8StringBuilder<Verse::Names::DefaultNameLength> MangledName;
		AppendMangledName(MangledName);

		TUtf8StringBuilder<Verse::Names::DefaultNameLength> OldMangledName;
		AppendMangledName(OldMangledName, UTF8CHAR('_'));

		// Redirect the package "/MountPoint/_Verse[/Assets]/Module_type" to "/MountPoint/_Verse[/Assets]".
		TUtf8StringBuilder<Verse::Names::DefaultNameLength> OldPackageName(InPlace, PackageName, '/', OldMangledName);
		TUtf8StringBuilder<Verse::Names::DefaultNameLength> NewPackageName(InPlace, PackageName);
		Package->AddRedirect(FCoreRedirect(ECoreRedirectFlags::Type_Package, FString(OldPackageName), FString(NewPackageName)));

		// Redirect the type "OldPackage.type" to "NewPackage.Module-type".
		TUtf8StringBuilder<Verse::Names::DefaultNameLength> OldTypeName(InPlace, OldPackageName, '.', GetBaseName().AsStringView());
		TUtf8StringBuilder<Verse::Names::DefaultNameLength> NewTypeName(InPlace, NewPackageName, '.', MangledName);
		Package->AddRedirect(FCoreRedirect(Kind, FString(OldTypeName), FString(NewTypeName)));

		// TODO SOL-7612: Remove this and resave any internal assets relying on it. Third party assets have never used them.
		if (OldMangledName.ToView() != MangledName.ToView())
		{
			// Redirect the type "NewPackage.Module_type" to "NewPackage.Module-type"
			TStringBuilder<Names::DefaultNameLength> MidTypeName(InPlace, NewPackageName, '.', OldMangledName);
			Package->AddRedirect(FCoreRedirect(Kind, FString(MidTypeName), FString(NewTypeName)));
		}

		if (Kind == ECoreRedirectFlags::Type_Class)
		{
			// Redirect the object "OldPackage.Default__type" to "NewPackage.Default__Module-type".
			// Not strictly required alongside the package redirect above, but enables things like validation to work with CDOs by name.
			TUtf8StringBuilder<Verse::Names::DefaultNameLength> OldCDOName(InPlace, OldPackageName, '.', DEFAULT_OBJECT_PREFIX, GetBaseName().AsStringView());
			TUtf8StringBuilder<Verse::Names::DefaultNameLength> NewCDOName(InPlace, NewPackageName, '.', DEFAULT_OBJECT_PREFIX, MangledName);
			Package->AddRedirect(FCoreRedirect(ECoreRedirectFlags::Type_Object, FString(OldCDOName), FString(NewCDOName)));

			// TODO SOL-7612: Remove this and resave any internal assets relying on it. Third party assets have never used them.
			if (OldMangledName.ToView() != MangledName.ToView())
			{
				// Redirect the type "NewPackage.Default__Module_type" to "NewPackage.Default__Module-type"
				TStringBuilder<Names::DefaultNameLength> MidCDOName(InPlace, NewPackageName, '.', DEFAULT_OBJECT_PREFIX, OldMangledName);
				Package->AddRedirect(FCoreRedirect(Kind, FString(MidCDOName), FString(NewCDOName)));
			}
		}
	}
#endif
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
