// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlainPropsPrint.h"
#include "PlainPropsBuild.h"
#include "PlainPropsDiff.h"
#include "PlainPropsIndex.h"
#include "Containers/StringConv.h"
#include "Containers/Utf8String.h"
#include "PlainPropsInternalFormat.h"
#include "PlainPropsInternalPrint.h"
#include "PlainPropsInternalRead.h"
#include "PlainPropsInternalText.h"
#include "Misc/AsciiSet.h"
#include "Misc/StringBuilder.h"
#include <charconv>

namespace PlainProps
{

static constexpr bool PrintWithComments = true;

//////////////////////////////////////////////////////////////////////////

const FLiterals GLiterals;

FAnsiStringView ToString(ERangeSizeType Width)
{
	return GLiterals.Ranges[(uint8)Width];
}

FAnsiStringView ToString(FUnpackedLeafType Leaf)
{
	return GLiterals.Leaves[(uint8)Leaf.Type][(uint8)Leaf.Width];
}

FAnsiStringView ToString(ELeafWidth Width)
{
	return GLiterals.Widths[(uint8)Width];
}

FAnsiStringView ToString(ESchemaFormat Format)
{
	switch (Format)
	{
		case ESchemaFormat::InMemoryNames:	return "InMemoryNames";
		case ESchemaFormat::StableNames:	return "StableNames";
	}
	return "Unknown";
}

//////////////////////////////////////////////////////////////////////////

void FIdIndexerBase::InitParameterNames()
{
	for (uint32 T = 0; T < 8; ++T)
	{
		for (uint32 W = 0; W < 4; ++W)
		{
			Leaves[T][W] = InitParameterName(GLiterals.Leaves[T][W].GetData());
		}
	}

	for (uint32 S = 0; S < 9; ++S)
	{
		Ranges[S] = InitParameterName(GLiterals.Ranges[S].GetData());
	}
}

///////////////////////////////////////////////////////////////////////////////
// Escape the quotation mark (U+0022), backslash (U+005C),
// and control characters U+0000 to U+001F (JSON Standard ECMA-404)
constexpr FAsciiSet EscapeSet("\\\""
	"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
	"\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f");

static inline void EscapeChar(FUtf8Builder& Out, UTF8CHAR Char)
{
	switch (Char)
	{
	case '\"': Out.Append("\\\"");	break;
	case '\\': Out.Append("\\\\");	break;
	case '\b': Out.Append("\\b");	break;
	case '\f': Out.Append("\\f");	break;
	case '\n': Out.Append("\\n");	break;
	case '\r': Out.Append("\\r");	break;
	case '\t': Out.Append("\\t");	break;
	default:
		Out.Appendf(UTF8TEXT("\\u%04x"), uint32(Char));
		break;
	}
}

template<Enumeration T>
void Print(FUtf8Builder& Out, T Value)
{
	Out.Append(ToString(Value));
}

template<Arithmetic T> requires (std::is_integral_v<T>)
void Print(FUtf8Builder& Out, T Value)
{
	constexpr size_t BufSize = std::numeric_limits<T>::digits10 + 3;
	char Buf[BufSize]{};
	std::to_chars_result result = std::to_chars(Buf, Buf + BufSize, Value);
	check(result.ec == std::errc());
	Out.Append(Buf);
}

template<Arithmetic T> requires (std::is_floating_point_v<T>)
void Print(FUtf8Builder& Out, T Value)
{
	constexpr size_t BufSize = 32;
	char Buf[BufSize]{};
	std::to_chars_result result = std::to_chars(Buf, Buf + BufSize, Value, std::chars_format::general);
	check(result.ec == std::errc());
	Out.Append(Buf);
}

template<>
void Print(FUtf8Builder& Out, bool Value)
{
	Out.Append(Value ? GLiterals.True : GLiterals.False);
}

template<>
void Print(FUtf8Builder& Out, char8_t Value)
{
	UTF8CHAR Char = static_cast<UTF8CHAR>(Value);
	if (EscapeSet.Contains(Char))
	{
		EscapeChar(Out, Char);
	}
	else
	{
		Out.AppendChar(Char);
	}
}

template<>
void Print(FUtf8Builder& Out, char16_t Value)
{
	if (Value <= 127)
	{
		Print(Out, static_cast<char8_t>(Value));
	}
	else
	{
		Out.Append(FUtf8StringView(StringCast<UTF8CHAR>(reinterpret_cast<UTF16CHAR*>(&Value), 1)));
	}
}

template<>
void Print(FUtf8Builder& Out, char32_t Value)
{
	if (Value <= 127)
	{
		Print(Out, static_cast<char8_t>(Value));
	}
	else
	{
		Out.Append(FUtf8StringView(StringCast<UTF8CHAR>(reinterpret_cast<UTF32CHAR*>(&Value), 1)));
	}
}

///////////////////////////////////////////////////////////////////////////////

static void PrintRangeType(FUtf8Builder& Out, FRangeType Type)
{
	Out.AppendChar('(');
	Out.Append(ToString(Type.MaxSize));
	Out.AppendChar(')');
}

inline void PrintStructFlags(FUtf8Builder& Out, FStructType StructType)
{
	if (StructType.IsDynamic)
	{
		Out.Append(GLiterals.Dynamic);
	}
}

void PrintSchema(FUtf8Builder& Out, const FBatchIds& Ids, FStructType StructType, FStructSchemaId Id)
{
	PrintStructFlags(Out, StructType);
	Ids.AppendString(Out, Id);
}

template<typename IdsType, typename OptionalStructId>
void PrintSchema(FUtf8Builder& Out, const IdsType& Ids, FStructType StructType, OptionalStructId Id)
{
	PrintStructFlags(Out, StructType);
	if (Id)
	{
		Ids.AppendString(Out, Id.Get());
	}
}

template<typename IdsType, typename OptionalEnumId>
void PrintSchema(FUtf8Builder& Out, const IdsType& Ids, FUnpackedLeafType Leaf, OptionalEnumId Id)
{
	if (Id)
	{
		Ids.AppendString(Out, Id.Get());
	}
	else
	{
		Out.Append(ToString(Leaf));
	}	
}

template<typename IdsType, typename OptionalId>
void PrintInnermostSchema(FUtf8Builder& Out, const IdsType& Ids, FMemberType InnermostType, OptionalId InnerSchema)
{
	if (InnermostType.IsStruct())
	{
		PrintSchema(Out, Ids, InnermostType.AsStruct(), ToOptionalStruct(InnerSchema));
	}
	else
	{
		PrintSchema(Out, Ids, InnermostType.AsLeaf(), ToOptionalEnum(InnerSchema));
	}
}

static void PrintSchema(FUtf8Builder& Out, const FBatchIds& Ids, FRangeType Type, FRangeSchema Schema)
{
	PrintInnermostSchema(Out, Ids, GetInnermostType(Schema), Schema.InnermostSchema);

	PrintRangeType(Out, Type);
	FMemberType Inner = Schema.ItemType;
	for (const FMemberType* It = Schema.NestedItemTypes; Inner.IsRange(); Inner = *It++)
	{
		PrintRangeType(Out, Inner.AsRange());
	}
}

void PrintMemberSchema(FUtf8Builder& Out, const FIds& Ids, FMemberSchema Member)
{
	PrintInnermostSchema(Out, Ids, Member.GetInnermostType(), Member.InnerSchema);

	if (Member.Type.IsRange())
	{
		PrintRangeType(Out, Member.Type.AsRange());
		for (FMemberType Inner : Member.GetInnerRangeTypes())
		{
			PrintRangeType(Out, Inner.AsRange());
		}
	}	
}

///////////////////////////////////////////////////////////////////////////////

struct FMemberSchemaView
{
	using FMemberTypeRange = TConstArrayView<FMemberType>;

	FMemberType				Type;
	FSchemaBatchId			Batch;
	FOptionalSchemaId		InnerSchema;
	FMemberTypeRange		InnerRangeTypes;

	FMemberType GetInnermostType() const
	{
		return InnerRangeTypes.Num() > 0 ? InnerRangeTypes.Last() : Type;
	}

	FRangeSchema AsRangeSchema() const
	{
		check(Type.IsRange());
		return { InnerRangeTypes[0], Batch, InnerSchema, InnerRangeTypes.Num() > 1 ? &InnerRangeTypes[1] : nullptr };
	}
};

///////////////////////////////////////////////////////////////////////////////

class FStructSchemaReader
{
public:
	FStructSchemaReader(const FStructSchema& Schema, FSchemaBatchId InBatch);

	FType					GetStruct() const		{ return Struct; }
	bool					IsDense() const			{ return bIsDense; }
	bool					HasSuper() const		{ return bHasSuper; }
	uint16					GetVersion() const		{ return Version; }

	bool					HasMore() const			{ return MemberIdx < NumMembers; }
	
	FOptionalMemberId		PeekName() const;		// @pre HasMore()
	EMemberKind				PeekKind() const;		// @pre HasMore()
	FMemberType				PeekType() const;		// @pre HasMore()

	FStructSchemaHandle		GetSuper() const;		// @pre HasSuper()
	FMemberSchemaView 		GrabMember();			// @pre HasMore()

private:
	const FMemberType*		Footer;
	const FSchemaBatchId	Batch;					// Needed to resolve schemas
	const FType				Struct;
	const bool				bIsDense : 1;
	const bool				bHasSuper : 1;
	const bool				bUsesSuper : 1;
	const uint16			Version;
	const uint32			NumMembers;
	const uint32			NumRangeTypes;			// Number of ranges and nested ranges
	const uint32			NumInnerSchemas;		// Number of static structs and enums

	uint32					MemberIdx = 0;
	uint32					RangeTypeIdx = 0;		// Types of [nested] ranges
	uint32					InnerSchemaIdx = 0;		// Types of static structs and enums

	void					AdvanceToNextMember()	{ ++MemberIdx; }

	using FMemberTypeRange = TConstArrayView<FMemberType>;

	FMemberTypeRange		GrabRangeTypes();
	FSchemaId				GrabInnerSchema();
	FOptionalSchemaId		GrabLeafSchema(FLeafType Leaf);
	FOptionalSchemaId		GrabStructSchema(FStructType Struct);
	FOptionalSchemaId		GrabRangeSchema(FMemberType InnermostType);

	const FMemberType*		GetMemberTypes() const;
	const FMemberType*		GetRangeTypes() const;
	const FSchemaId*		GetInnerSchemas() const;
	const FMemberId*		GetMemberNames() const;
};

///////////////////////////////////////////////////////////////////////////////

class FYamlBuilder
{
public:
	FYamlBuilder(FUtf8Builder& InStringBuilder);
	~FYamlBuilder();

	void BeginDocument();
	void EndDocument();

	void BeginStruct(FUtf8StringView Id);
	void BeginStruct();
	void EndStruct();

	void BeginRange(FUtf8StringView Id);
	void BeginRange();
	void EndRange();

	void AddLeafId(FUtf8StringView Id);
	template<typename T>
	void AddLeafValue(T Value);
	template<typename T>
	void AddLeaf(FUtf8StringView Id, T Value) { AddLeafId(Id); AddLeafValue(Value); }
	template<typename T>
	void AddLeaf(T Value);

	void AddComment(FUtf8StringView Comment);

private:
	void AppendNewLine();
	void AppendIndentation();
	void AppendIdentifier(FUtf8StringView Id);
	template<typename T>
	void AppendValue(T Value);
	template<>
	void AppendValue(FUtf8StringView Value);

	struct FStackInfo
	{
		bool IsEmpty = true;
		bool IsInStruct = true;
	};

	FUtf8Builder& Text;
	TArray<FStackInfo, TInlineAllocator<32>> Stack;
	uint32 IndentLevel = 0;
	bool IsNewLine = true;
};

///////////////////////////////////////////////////////////////////////////////

class FMemberPrinter
{
public:
	FMemberPrinter(FYamlBuilder& InTextBuilder, const FBatchIds& InIds)
	: TextBuilder(InTextBuilder)
	, Ids(InIds)
	{}

	void PrintMembers(FStructView StructView);

private:
	void PrintLeaf(FMemberId Id, FLeafView LeafView);
	void PrintStruct(FOptionalMemberId Id, FStructType StructType, FStructView StructView);
	void PrintRange(FMemberId Id, FRangeType RangeType, const FRangeView& RangeView);

	void PrintLeaves(FUnpackedLeafType Leaf, const FLeafRangeView& LeafRange);
	void PrintStructs(FStructType StructType, const FStructRangeView& StructRange);
	void PrintRanges(FRangeType RangeType, const FNestedRangeView& NestedRange);

	void PrintMembersInternal(FStructType StructType, FStructView StructView, const FStructSchema& Schema);
	void PrintRangeInternal(FRangeType RangeType, const FRangeView& RangeView);

	bool IsUnicodeString(const FRangeView& RangeView);
	void PrintUnicodeLeafValue(FLeafView LeafView);
	void PrintUnicodeRangeAsLeaf(FOptionalMemberId Id, FRangeType RangeType, const FRangeView& RangeView);
	
	template<typename MemberType, typename SchemaType>
	void PrintSchemaComment(MemberType Type, SchemaType Schema)
	{
		if constexpr (PrintWithComments)
		{
			PrintSchema(/* out */ Tmp, Ids, Type, Schema);
			TextBuilder.AddComment(Tmp);
			Tmp.Reset();
		}
	}	

	FYamlBuilder& TextBuilder;
	const FBatchIds& Ids;
	TUtf8StringBuilder<256> Tmp;
};

///////////////////////////////////////////////////////////////////////////////

void PrintYamlBatch(FUtf8Builder& Out, const FBatchIds& Ids, TConstArrayView<FStructView> Objects)
{
	FYamlBuilder YamlBuilder(Out);
	FBatchPrinter Printer(YamlBuilder, Ids);
	YamlBuilder.BeginDocument();
	Printer.PrintSchemas();
	Printer.PrintObjects(Objects);
	YamlBuilder.EndDocument();
}

///////////////////////////////////////////////////////////////////////////////

FStructSchemaReader::FStructSchemaReader(const FStructSchema& Schema, FSchemaBatchId InBatch)
: Footer(Schema.Footer)
, Batch(InBatch)
, Struct(Schema.Type)
, bIsDense(Schema.IsDense)
, bHasSuper(Schema.Inheritance != ESuper::No)
, bUsesSuper(UsesSuper(Schema.Inheritance))
, Version(Schema.Version)
, NumMembers(Schema.NumMembers)
, NumRangeTypes(Schema.NumRangeTypes)
, NumInnerSchemas(Schema.NumInnerSchemas)
, InnerSchemaIdx(SkipDeclaredSuperSchema(Schema.Inheritance))
{
	check(InnerSchemaIdx <= NumInnerSchemas);
	checkf(NumRangeTypes != 0xFFFFu, TEXT("GrabRangeTypes() doesn't check for wrap-around"));
}

FOptionalMemberId FStructSchemaReader::PeekName() const
{
	int32 MemberNameIdx = MemberIdx - bUsesSuper;
	return MemberNameIdx >= 0 ? ToOptional(GetMemberNames()[MemberNameIdx]) : NoId;
}

EMemberKind FStructSchemaReader::PeekKind() const
{
	return PeekType().GetKind();
}

FMemberType	FStructSchemaReader::PeekType() const
{
	check(HasMore());
	return GetMemberTypes()[MemberIdx];
}

FStructSchemaHandle FStructSchemaReader::GetSuper() const
{
	check(HasSuper());
	check(NumInnerSchemas > 0);
	return { static_cast<FStructSchemaId>(GetInnerSchemas()[0]), Batch };
}

FMemberSchemaView FStructSchemaReader::GrabMember()
{
	check(HasMore());
	FMemberType Type = PeekType();
	FMemberSchemaView Out{ Type, Batch };

	switch (PeekKind())
	{
		case EMemberKind::Leaf:
			Out.InnerSchema = GrabLeafSchema(Type.AsLeaf());
			break;
		case EMemberKind::Struct:
			Out.InnerSchema = GrabStructSchema(Type.AsStruct());
			break;
		case EMemberKind::Range:
			Out.InnerRangeTypes = GrabRangeTypes();
			Out.InnerSchema = GrabRangeSchema(Out.InnerRangeTypes.Last());
			break;
	}

	AdvanceToNextMember();

	return Out;
}

FStructSchemaReader::FMemberTypeRange FStructSchemaReader::GrabRangeTypes()
{
	return GrabInnerRangeTypes(MakeArrayView(GetRangeTypes(), NumRangeTypes), /* in-out */ RangeTypeIdx);
}

FSchemaId FStructSchemaReader::GrabInnerSchema()
{
	check(InnerSchemaIdx < NumInnerSchemas);
	uint32 Idx = InnerSchemaIdx++;
	return GetInnerSchemas()[Idx];
}

FOptionalSchemaId FStructSchemaReader::GrabLeafSchema(FLeafType Member)
{
	return Member.Type == ELeafType::Enum ? ToOptional(GrabInnerSchema()) : NoId;
}

FOptionalSchemaId FStructSchemaReader::GrabStructSchema(FStructType Member)
{
	return Member.IsDynamic ? NoId : ToOptional(GrabInnerSchema());
}

FOptionalSchemaId FStructSchemaReader::GrabRangeSchema(FMemberType InnermostType)
{
	check(!InnermostType.IsRange());
	return InnermostType.IsStruct() ?
		GrabStructSchema(InnermostType.AsStruct()) :
		GrabLeafSchema(InnermostType.AsLeaf());
}

const FMemberType* FStructSchemaReader::GetMemberTypes() const
{
	return FStructSchema::GetMemberTypes(Footer);
}

const FMemberType* FStructSchemaReader::GetRangeTypes() const
{
	return FStructSchema::GetRangeTypes(Footer, NumMembers);
}

const FSchemaId* FStructSchemaReader::GetInnerSchemas() const
{
	return FStructSchema::GetInnerSchemas(Footer, NumMembers, NumRangeTypes, NumMembers - bUsesSuper);
}

const FMemberId* FStructSchemaReader::GetMemberNames() const
{
	return FStructSchema::GetMemberNames(Footer, NumMembers, NumRangeTypes);
}

///////////////////////////////////////////////////////////////////////////////

void FYamlBuilderDeleter::operator()(FYamlBuilder* YamlBuilder) const
{
	delete YamlBuilder;
}

FYamlBuilderPtr MakeYamlBuilder(FUtf8Builder& StringBuilder)
{
	return FYamlBuilderPtr(new FYamlBuilder(StringBuilder));
}

///////////////////////////////////////////////////////////////////////////////

FYamlBuilder::FYamlBuilder(FUtf8Builder& InStringBuilder)
: Text(InStringBuilder)
{
	Stack.Emplace();
}

FYamlBuilder::~FYamlBuilder()
{
	Stack.Pop(EAllowShrinking::No);
	check(Stack.IsEmpty());
}

void FYamlBuilder::BeginDocument()
{
	Text << "---";
	IsNewLine = false;
	AppendNewLine();
	Stack.Emplace();
}

void FYamlBuilder::EndDocument()
{
	AppendNewLine();
	Text << "...";
	Stack.Pop(EAllowShrinking::No);
}

void FYamlBuilder::BeginStruct(FUtf8StringView Id)
{
	AppendNewLine();
	AppendIndentation();
	AppendIdentifier(Id);
	Stack.Last().IsEmpty = false;
	Stack.Emplace();
	++IndentLevel;
}

void FYamlBuilder::BeginStruct()
{
	AppendNewLine();
	AppendIndentation();
	Stack.Last().IsEmpty = false;
	Stack.Emplace();
	++IndentLevel;
}

void FYamlBuilder::EndStruct()
{
	--IndentLevel;
	if (Stack.Last().IsEmpty)
	{
		Text << " {}";
		IsNewLine = false;
	}
	Stack.Pop(EAllowShrinking::No);
}

void FYamlBuilder::BeginRange(FUtf8StringView Id)
{
	AppendNewLine();
	AppendIndentation();
	AppendIdentifier(Id);
	Stack.Last().IsEmpty = false;
	Stack.Emplace_GetRef().IsInStruct = false;
	++IndentLevel;
}

void FYamlBuilder::BeginRange()
{
	AppendNewLine();
	AppendIndentation();
	Stack.Last().IsEmpty = false;
	Stack.Emplace_GetRef().IsInStruct = false;
	++IndentLevel;
}

void FYamlBuilder::EndRange()
{
	if (Stack.Last().IsEmpty)
	{
		Text << " []";
		IsNewLine = false;
	}
	Stack.Pop(EAllowShrinking::No);
	--IndentLevel;
}

void FYamlBuilder::AddLeafId(FUtf8StringView Id)
{
	AppendNewLine();
	AppendIndentation();
	AppendIdentifier(Id);
	IsNewLine = false;
	Stack.Last().IsEmpty = false;
}

template<typename T>
void FYamlBuilder::AddLeafValue(T Value)
{
	Text.AppendChar(' ');
	AppendValue(Value);
	IsNewLine = false;
	Stack.Last().IsEmpty = false;
}

template<typename T>
void FYamlBuilder::AddLeaf(T Value)
{
	AppendNewLine();
	AppendIndentation();
	AppendValue(Value);
	IsNewLine = false;
	Stack.Last().IsEmpty = false;
}

void FYamlBuilder::AddComment(FUtf8StringView Comment)
{
	Text << " #" << Comment;
	AppendNewLine();
}

void FYamlBuilder::AppendNewLine()
{
	if (!IsNewLine)
	{
		Text << '\n';
		IsNewLine = true;
	}
}

void FYamlBuilder::AppendIndentation()
{
	for (uint32 I = 0; I < 2*IndentLevel; ++I)
	{
		Text.AppendChar(' ');
	}
	if (!Stack.Last().IsInStruct)
	{
		Text << "- ";
	}
	IsNewLine = false;
}

static void PrintQuotedString(FUtf8Builder& Out, FUtf8StringView Value)
{
	FUtf8StringView Verbatim = FAsciiSet::FindPrefixWithout(Value, EscapeSet | "'");
	if (Verbatim.Len() == Value.Len())
	{
		Out << '\'' << Value << '\'';
		return;
	}

	Out << '\"';
	while (!Value.IsEmpty())
	{
		Out << Verbatim;
		Value.RightChopInline(Verbatim.Len());
		FUtf8StringView Escape = FAsciiSet::FindPrefixWith(Value, EscapeSet);
		for (UTF8CHAR Char : Escape)
		{
			EscapeChar(Out, Char);
		}
		Value.RightChopInline(Escape.Len());
		Verbatim = FAsciiSet::FindPrefixWithout(Value, EscapeSet);
	}
	Out << '\"';
}

void FYamlBuilder::AppendIdentifier(FUtf8StringView Id)
{
	PrintQuotedString(Text, Id);
	Text.AppendChar(':');
	IsNewLine = false;
}

template<typename T>
void FYamlBuilder::AppendValue(T Value)
{
	Text.AppendChar('\'');
	Print(Text, Value);
	Text.AppendChar('\'');
}

template<>
void FYamlBuilder::AppendValue(FUtf8StringView Value)
{
	PrintQuotedString(Text, Value);
}

///////////////////////////////////////////////////////////////////////////////

FBatchPrinter::FBatchPrinter(FYamlBuilder& InTextBuilder, const FBatchIds& InIds)
: TextBuilder(InTextBuilder)
, Ids(InIds)
{}

FBatchPrinter::~FBatchPrinter()
{}

void FBatchPrinter::PrintSchemas()
{
	TextBuilder.BeginRange(GLiterals.Structs);
	for (const FStructSchema& Struct : GetStructSchemas(Ids.GetSchemas()))
	{
		PrintStructSchema(Struct, Ids.GetBatchId());
	}
	TextBuilder.EndRange();

	TextBuilder.BeginRange(GLiterals.Enums);
	for (const FEnumSchema& EnumSchema : GetEnumSchemas(Ids.GetSchemas()))
	{
		PrintEnumSchema(EnumSchema);
	}
	TextBuilder.EndRange();
}

void FBatchPrinter::PrintObjects(TConstArrayView<FStructView> Objects)
{
	TextBuilder.BeginRange(GLiterals.Objects);
	for (FStructView Object : Objects)
	{
		FMemberPrinter(TextBuilder, Ids).PrintMembers(Object);
	}
	TextBuilder.EndRange();
}

static void PrintMemberSchema(FUtf8Builder& Out, const FBatchIds& Ids, const FMemberSchemaView& Schema)
{
	switch (Schema.Type.GetKind())
	{
	case EMemberKind::Leaf:		PrintSchema(Out, Ids, Schema.Type.AsLeaf(), ToOptionalEnum(Schema.InnerSchema)); break;
	case EMemberKind::Range:	PrintSchema(Out, Ids, Schema.Type.AsRange(), Schema.AsRangeSchema()); break;
	case EMemberKind::Struct:	PrintSchema(Out, Ids, Schema.Type.AsStruct(), ToOptionalStruct(Schema.InnerSchema)); break;
	}
}

template <int32 BufferSize>
class FPrintId
{
	TUtf8StringBuilder<BufferSize>		Buffer;
public:
	template <typename T>
	FPrintId(const FBatchIds& Ids, T Id) { Ids.AppendString(Buffer, Id); }
	FUtf8StringView operator*() const	{ return Buffer.ToView(); }
};

void FBatchPrinter::PrintStructSchema(const FStructSchema& Struct, FSchemaBatchId BatchId)
{
	FStructSchemaReader Reader(Struct, BatchId);

	TextBuilder.BeginStruct(*FPrintId<128>(Ids, Reader.GetStruct()));

	if (uint16 Version = Reader.GetVersion())
	{
		TextBuilder.AddLeaf(GLiterals.Version, Version);
	}

	if (Reader.HasSuper())
	{
		const FStructSchema& SuperSchema = Reader.GetSuper().Resolve();
		TextBuilder.AddLeaf(GLiterals.DeclaredSuper, *FPrintId<128>(Ids, SuperSchema.Type));
	}

	TextBuilder.BeginRange(GLiterals.Members);
	TUtf8StringBuilder<256> Buf;
	while (Reader.HasMore())
	{
		Ids.AppendString(Buf, Reader.PeekName());
		TextBuilder.AddLeafId(Buf);
		Buf.Reset();

		PrintMemberSchema(Buf, Ids, Reader.GrabMember());
		TextBuilder.AddLeafValue(FUtf8StringView(Buf));
		Buf.Reset();
	}
	TextBuilder.EndRange();

	TextBuilder.EndStruct();
}

void FBatchPrinter::PrintEnumSchema(const FEnumSchema& Enum)
{
	TextBuilder.BeginStruct(*FPrintId<128>(Ids, Enum.Type));
	TextBuilder.AddLeaf(GLiterals.FlagMode, !!Enum.FlagMode);
	TextBuilder.AddLeaf(GLiterals.Width, Enum.Width);

	TextBuilder.BeginRange(GLiterals.Constants);
	TConstArrayView<FNameId> EnumNames = MakeConstArrayView(Enum.Footer, Enum.Num);
	switch (Enum.Width)
	{
		case ELeafWidth::B8:
			PrintEnumConstants(EnumNames, GetConstants<uint8>(Enum), Enum.FlagMode);
			break;
		case ELeafWidth::B16:
			PrintEnumConstants(EnumNames, GetConstants<uint16>(Enum), Enum.FlagMode);
			break;
		case ELeafWidth::B32:
			PrintEnumConstants(EnumNames, GetConstants<uint32>(Enum), Enum.FlagMode);
			break;
		case ELeafWidth::B64:
			PrintEnumConstants(EnumNames, GetConstants<uint64>(Enum), Enum.FlagMode);
			break;
	}
	TextBuilder.EndRange();

	TextBuilder.EndStruct();
}

template<typename IntType>
void FBatchPrinter::PrintEnumConstants(
	TConstArrayView<FNameId> EnumNames,
	TConstArrayView<IntType> Constants,
	bool bFlagMode)
{
	uint16 NamesNum = IntCastChecked<uint16>(EnumNames.Num());
	if (Constants.Num() > 0)
	{
		check(EnumNames.Num() == Constants.Num());
		for (uint16 Idx = 0; Idx < NamesNum; ++Idx)
		{
			TextBuilder.AddLeaf(*FPrintId<128>(Ids, EnumNames[Idx]), (uint64)Constants[Idx]);
		}
	}
	else if (bFlagMode)
	{
		uint64 Value = 1;
		for (uint16 Idx = 0; Idx < NamesNum; ++Idx)
		{
			TextBuilder.AddLeaf(*FPrintId<128>(Ids, EnumNames[Idx]), Value);
			Value <<= 1;
		}
	}
	else
	{
		for (uint16 Idx = 0; Idx < NamesNum; ++Idx)
		{
			TextBuilder.AddLeaf(*FPrintId<128>(Ids, EnumNames[Idx]), (uint64)Idx);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void FMemberPrinter::PrintMembers(FStructView StructView)
{
	const FStructSchema& Schema = StructView.Schema.Resolve();
	TextBuilder.BeginStruct(*FPrintId<128>(Ids, Schema.Type));
	PrintMembersInternal({ EMemberKind::Struct }, StructView, Schema);
}

void FMemberPrinter::PrintLeaf(FMemberId Id, FLeafView LeafView)
{
	TextBuilder.AddLeafId(*FPrintId<128>(Ids, Id));

	switch (LeafView.Leaf.Type)
	{
	case ELeafType::Bool:
		TextBuilder.AddLeafValue(LeafView.AsBool());
		break;
	case ELeafType::IntS:
		switch (LeafView.Leaf.Width)
		{
			case ELeafWidth::B8:	TextBuilder.AddLeafValue(LeafView.AsS8()); break;
			case ELeafWidth::B16:	TextBuilder.AddLeafValue(LeafView.AsS16()); break;
			case ELeafWidth::B32:	TextBuilder.AddLeafValue(LeafView.AsS32()); break;
			case ELeafWidth::B64:	TextBuilder.AddLeafValue(LeafView.AsS64()); break;
		}
		break;
	case ELeafType::IntU:
		switch (LeafView.Leaf.Width)
		{
			case ELeafWidth::B8:	TextBuilder.AddLeafValue(LeafView.AsU8()); break;
			case ELeafWidth::B16:	TextBuilder.AddLeafValue(LeafView.AsU16()); break;
			case ELeafWidth::B32:	TextBuilder.AddLeafValue(LeafView.AsU32()); break;
			case ELeafWidth::B64:	TextBuilder.AddLeafValue(LeafView.AsU64()); break;
		}
		break;
	case ELeafType::Float:
		if (LeafView.Leaf.Width == ELeafWidth::B32)
		{
			TextBuilder.AddLeafValue(LeafView.AsFloat());
		}
		else
		{
			check(LeafView.Leaf.Width == ELeafWidth::B64);
			TextBuilder.AddLeafValue(LeafView.AsDouble());
		}
		break;
	case ELeafType::Hex:
		check(LeafView.Leaf.Type != ELeafType::Hex);
		break;
	case ELeafType::Enum:
		switch (LeafView.Leaf.Width)
		{
			case ELeafWidth::B8:	TextBuilder.AddLeafValue(LeafView.AsUnderlyingValue<uint8>()); break;
			case ELeafWidth::B16:	TextBuilder.AddLeafValue(LeafView.AsUnderlyingValue<uint16>()); break;
			case ELeafWidth::B32:	TextBuilder.AddLeafValue(LeafView.AsUnderlyingValue<uint32>()); break;
			case ELeafWidth::B64:	TextBuilder.AddLeafValue(LeafView.AsUnderlyingValue<uint64>()); break;
		}
		break;
	case ELeafType::Unicode:
		switch (LeafView.Leaf.Width)
		{
			case ELeafWidth::B8:	TextBuilder.AddLeafValue(LeafView.AsChar8());	break;
			case ELeafWidth::B16:	TextBuilder.AddLeafValue(LeafView.AsChar16());	break;
			case ELeafWidth::B32:	TextBuilder.AddLeafValue(LeafView.AsChar32());	break;
			case ELeafWidth::B64:	check(false);									break;
		};
	}

	PrintSchemaComment(LeafView.Leaf, LeafView.Enum);
}

void FMemberPrinter::PrintStruct(FOptionalMemberId MemberId, FStructType StructType, FStructView StructView)
{
	TextBuilder.BeginStruct(*FPrintId<128>(Ids, MemberId));
	PrintMembersInternal(StructType, StructView, StructView.Schema.Resolve());
}

void FMemberPrinter::PrintRange(FMemberId Id, FRangeType RangeType, const FRangeView& RangeView)
{
	if (IsUnicodeString(RangeView))
	{
		PrintUnicodeRangeAsLeaf(Id, RangeType, RangeView);
	}
	else
	{
		TextBuilder.BeginRange(*FPrintId<128>(Ids, Id));
		PrintRangeInternal(RangeType, RangeView);
	}
}

void FMemberPrinter::PrintLeaves(FUnpackedLeafType Leaf, const FLeafRangeView& LeafRange)
{
	switch (Leaf.Type)
	{
	case ELeafType::Bool:
		for (bool b : LeafRange.AsBools()) { TextBuilder.AddLeaf(b); } break;
	case ELeafType::IntS:
		switch (Leaf.Width)
		{
			case ELeafWidth::B8:  for (const int8&  I : LeafRange.AsS8s())  { TextBuilder.AddLeaf(I); } break;
			case ELeafWidth::B16: for (const int16& I : LeafRange.AsS16s()) { TextBuilder.AddLeaf(I); } break;
			case ELeafWidth::B32: for (const int32& I : LeafRange.AsS32s()) { TextBuilder.AddLeaf(I); } break;
			case ELeafWidth::B64: for (const int64& I : LeafRange.AsS64s()) { TextBuilder.AddLeaf(I); } break;
		}
		break;
	case ELeafType::IntU:
		switch (Leaf.Width)
		{
			case ELeafWidth::B8:  for (const uint8&  U : LeafRange.AsU8s())  { TextBuilder.AddLeaf(U); } break;
			case ELeafWidth::B16: for (const uint16& U : LeafRange.AsU16s()) { TextBuilder.AddLeaf(U); } break;
			case ELeafWidth::B32: for (const uint32& U : LeafRange.AsU32s()) { TextBuilder.AddLeaf(U); } break;
			case ELeafWidth::B64: for (const uint64& U : LeafRange.AsU64s()) { TextBuilder.AddLeaf(U); } break;
		}
		break;
	case ELeafType::Float:
		if (Leaf.Width == ELeafWidth::B32)
		{
			for (const float& f : LeafRange.AsFloats()) { TextBuilder.AddLeaf(f); }
		}
		else
		{
			check(Leaf.Width == ELeafWidth::B64);
			for (const double& d : LeafRange.AsDoubles()) { TextBuilder.AddLeaf(d); }
		}
		break;
	case ELeafType::Hex:
		// PP-TEXT: Implement AddLeaf(Hex)
		check(Leaf.Type != ELeafType::Hex);
		break;
	case ELeafType::Enum:
		switch (Leaf.Width)
		{
			case ELeafWidth::B8:  for (const uint8&  U : LeafRange.AsUnderlyingValues<uint8>())  { TextBuilder.AddLeaf(U); } break;
			case ELeafWidth::B16: for (const uint16& U : LeafRange.AsUnderlyingValues<uint16>()) { TextBuilder.AddLeaf(U); } break;
			case ELeafWidth::B32: for (const uint32& U : LeafRange.AsUnderlyingValues<uint32>()) { TextBuilder.AddLeaf(U); } break;
			case ELeafWidth::B64: for (const uint64& U : LeafRange.AsUnderlyingValues<uint64>()) { TextBuilder.AddLeaf(U); } break;
		}
		break;
	case ELeafType::Unicode:
		checkf(LeafRange.Num() == 0, TEXT("Should have been handled by PrintUnicodeRangeAsLeaf"));
		break;
	}
}

void FMemberPrinter::PrintStructs(FStructType StructType, const FStructRangeView& StructRange)
{
	for (FStructView StructView : StructRange)
	{
		TextBuilder.BeginStruct();
		PrintMembersInternal(StructType, StructView, StructView.Schema.Resolve());
	}
}

void FMemberPrinter::PrintRanges(FRangeType RangeType, const FNestedRangeView& NestedRange)
{
	for (FRangeView RangeView : NestedRange)
	{
		if (IsUnicodeString(RangeView))
		{
			PrintUnicodeRangeAsLeaf(NoId, RangeType, RangeView);
		}
		else
		{
			TextBuilder.BeginRange();
			PrintRangeInternal(RangeType, RangeView);
		}
	}
}

void FMemberPrinter::PrintMembersInternal(FStructType StructType, FStructView StructView, const FStructSchema& Schema)
{
	FMemberReader It(Schema, StructView.Values, StructView.Schema.Batch);

	const bool HasMembers = StructType.IsDynamic || It.HasMore();
	if (HasMembers)
	{
		PrintSchemaComment(StructType, StructView.Schema.Id);
	}

	if (StructType.IsDynamic)
	{
		TextBuilder.AddLeaf(GLiterals.Dynamic, *FPrintId<128>(Ids, Schema.Type));
	}
	while (It.HasMore())
	{
		FOptionalMemberId Id = It.PeekName();
		FMemberType Type = It.PeekType();
		switch (Type.GetKind())
		{
			case EMemberKind::Leaf:
				PrintLeaf(Id.Get(), It.GrabLeaf());
				break;
			case EMemberKind::Struct:
				PrintStruct(Id, Type.AsStruct(), It.GrabStruct());
				break;
			case EMemberKind::Range:
				PrintRange(Id.Get(), Type.AsRange(), It.GrabRange());
				break;
		}
	}
	TextBuilder.EndStruct();

	if (!HasMembers)
	{
		PrintSchemaComment(StructType, StructView.Schema.Id);
	}
}

void FMemberPrinter::PrintRangeInternal(FRangeType RangeType, const FRangeView& RangeView)
{
	const FRangeSchema& Schema = RangeView.Schema;
	if (RangeView.Num() > 0)
	{
		PrintSchemaComment(RangeType, Schema);
	}

	switch (Schema.ItemType.GetKind())
	{
		case EMemberKind::Leaf:
			PrintLeaves(Schema.ItemType.AsLeaf(), RangeView.AsLeaves());
			break;
		case EMemberKind::Struct:
			PrintStructs(Schema.ItemType.AsStruct(), RangeView.AsStructs());
			break;
		case EMemberKind::Range:
			PrintRanges(Schema.ItemType.AsRange(), RangeView.AsRanges());
			break;
	}
	TextBuilder.EndRange();

	if (RangeView.Num() == 0)
	{
		PrintSchemaComment(RangeType, Schema);
	}
}

bool FMemberPrinter::IsUnicodeString(const FRangeView& RangeView)
{
	FMemberType Type = RangeView.Schema.ItemType;
	return RangeView.Num() > 0 && Type.IsLeaf() && Type.AsLeaf().Type == ELeafType::Unicode;
}

template <typename CharType, typename RangeCharType>
static void AddUnicodeRangeLeaf(FYamlBuilder& TextBuilder, FOptionalMemberId Id, TRangeView<RangeCharType> Range)
{
	check(Range.Num() > 0);
	const CharType* Src = reinterpret_cast<const CharType*>(Range.begin());
	const int32 SrcLen = IntCastChecked<int32>(Range.Num());
	const int32 DstLen = FPlatformString::ConvertedLength<UTF8CHAR>(Src, SrcLen);
	TArray<UTF8CHAR, TInlineAllocator<1024>> Buf;
	Buf.Reserve(DstLen);
	UTF8CHAR* Dst = Buf.GetData();
	const UTF8CHAR* DstEnd = FPlatformString::Convert(Dst, DstLen, Src, SrcLen);
	check(DstEnd);
	check(DstEnd - Dst == DstLen);
	if (Id)
	{
		TextBuilder.AddLeafValue(FUtf8StringView(Dst, DstLen));
	}
	else
	{
		TextBuilder.AddLeaf(FUtf8StringView(Dst, DstLen));
	}
}

void FMemberPrinter::PrintUnicodeRangeAsLeaf(FOptionalMemberId Id, FRangeType RangeType, const FRangeView& RangeView)
{
	check(IsUnicodeString(RangeView));

	if (Id)
	{
		TextBuilder.AddLeafId(*FPrintId<128>(Ids, Id));
	}

	const FLeafRangeView LeafRange = RangeView.AsLeaves();
	const FUnpackedLeafType Leaf = RangeView.Schema.ItemType.AsLeaf();

	switch (Leaf.Width)
	{
		case ELeafWidth::B8:	AddUnicodeRangeLeaf<UTF8CHAR >(TextBuilder, Id, LeafRange.AsUtf8());	break;
		case ELeafWidth::B16:	AddUnicodeRangeLeaf<UTF16CHAR>(TextBuilder, Id, LeafRange.AsUtf16());	break;
		case ELeafWidth::B32:	AddUnicodeRangeLeaf<UTF32CHAR>(TextBuilder, Id, LeafRange.AsUtf32());	break;
		case ELeafWidth::B64:	check(false);															break;
	};
	
	PrintSchemaComment(FRangeType(RangeType), RangeView.Schema);
}

///////////////////////////////////////////////////////////////////////////////

void FIdsBase::AppendString(FUtf8Builder& Out, FMemberId Name) const
{
	AppendString(Out, Name.Id);
}

void FIdsBase::AppendString(FUtf8Builder& Out, FOptionalMemberId Name) const
{
	if (Name)
	{
		AppendString(Out, Name.Get().Id);
	}
	else
	{
		Out.Append(GLiterals.Super);
	}
}

void FIdsBase::AppendString(FUtf8Builder& Out, FScopeId Scope) const
{
	if (Scope.IsFlat())
	{
		AppendString(Out, Scope.AsFlat().Name);
	}
	else if (Scope)
	{
		FNestedScope Nested = Resolve(Scope.AsNested());
		AppendString(Out, Nested.Outer);
		Out.AppendChar('.');
		AppendString(Out, Nested.Inner.Name);
	}
}

void FIdsBase::AppendString(FUtf8Builder& Out, FTypenameId Typename) const
{
	if (Typename.IsConcrete())
	{
		AppendString(Out, Typename.AsConcrete().Id);
	}
	else
	{
		FParametricTypeView ParametricType = Resolve(Typename.AsParametric());
		TConstArrayView<FType> Parameters = ParametricType.GetParameters();
	
		if (ParametricType.Name)
		{
			AppendString(Out, ParametricType.Name.Get().Id);
		}

		Out.AppendChar(ParametricType.Name ? '<' : '[');
		for (FType Parameter : Parameters.LeftChop(1))
		{
			AppendString(Out, Parameter);
			Out.AppendChar(',');
		}
		if (Parameters.Num() > 0)
		{
			AppendString(Out, Parameters.Last());
		}
		Out.AppendChar(ParametricType.Name ? '>' : ']');
	}
}

void FIdsBase::AppendString(FUtf8Builder& Out, FType Type) const
{
	if (Type.Scope)
	{
		AppendString(Out, Type.Scope);
		Out.AppendChar('.');
	}
	AppendString(Out, Type.Name);
}

void FIds::AppendString(FUtf8Builder& Out, FEnumId Name) const
{
	AppendString(Out, Resolve(Name));
}

void FIds::AppendString(FUtf8Builder& Out, FStructId Name) const
{
	AppendString(Out, Resolve(Name));
}

void FBatchIds::AppendString(FUtf8Builder& Out, FEnumSchemaId Name) const
{
	AppendString(Out, Resolve(Name));
}

void FBatchIds::AppendString(FUtf8Builder& Out, FStructSchemaId Name) const
{
	AppendString(Out, Resolve(Name));
}

////////////////////////////////////////////////////////////////////////////////

FString FDebugIds::Print(FNameId Name) const
{
	TUtf8StringBuilder<128> Out;
	if (Name.Idx < Ids.NumNames())
	{
		Ids.AppendString(Out, Name);
	}
	else
	{
		Out << GLiterals.Oob;
	}
	return FString(FStringView(StringCast<TCHAR>(Out.GetData(), Out.Len())));
}

FString FDebugIds::Print(FMemberId Name) const
{
	TUtf8StringBuilder<128> Out;
	if (Name.Id.Idx < Ids.NumNames())
	{
		Ids.AppendString(Out, Name.Id);
	}
	else
	{
		Out << GLiterals.Oob;
	}
	return FString(FStringView(StringCast<TCHAR>(Out.GetData(), Out.Len())));
}

FString FDebugIds::Print(FOptionalMemberId Name) const
{
	TUtf8StringBuilder<128> Out;
	if (!Name || Name.Get().Id.Idx < Ids.NumNames())
	{
		Ids.AppendString(Out, Name);
	}
	else
	{
		Out << GLiterals.Oob;
	}
	return FString(FStringView(StringCast<TCHAR>(Out.GetData(), Out.Len())));
}

static const bool IsValidScope(FScopeId Scope, const FIds& Ids)
{
	if (Scope.IsFlat())
	{
		return Scope.AsFlat().Name.Idx < Ids.NumNames();
	}
	else if (Scope)
	{
		return Scope.AsNested().Idx < Ids.NumNestedScopes();
	}
	return !Scope; // Unscoped
}

FString FDebugIds::Print(FScopeId Scope) const
{
	TUtf8StringBuilder<128> Out;
	if (IsValidScope(Scope, Ids))
	{
		Ids.AppendString(Out, Scope);
	}
	else
	{
		Out << GLiterals.Oob;
	}
	return FString(FStringView(StringCast<TCHAR>(Out.GetData(), Out.Len())));
}

static const bool IsValidTypename(FTypenameId Typename, const FIds& Ids)
{
	if (Typename.IsConcrete())
	{
		return Typename.AsConcrete().Id.Idx < Ids.NumNames();
	}
	return Typename.AsParametric().Idx < Ids.NumNames();
}

FString FDebugIds::Print(FTypenameId Typename) const
{
	TUtf8StringBuilder<128> Out;
	if (IsValidTypename(Typename, Ids))
	{
		Ids.AppendString(Out, Typename);
	}
	else
	{
		Out << GLiterals.Oob;
	}
	return FString(FStringView(StringCast<TCHAR>(Out.GetData(), Out.Len())));
}

FString FDebugIds::Print(FConcreteTypenameId Typename) const
{
	return Print(FTypenameId(Typename));
}

FString FDebugIds::Print(FParametricTypeId Typename) const
{
	return Print(FTypenameId(Typename));
}

FString FDebugIds::Print(FType Type) const
{
	TUtf8StringBuilder<128> Out;
	if (IsValidScope(Type.Scope, Ids) && IsValidTypename(Type.Name, Ids))
	{
		Ids.AppendString(Out, Type);
	}
	else
	{
		Out << GLiterals.Oob;
	}
	return FString(FStringView(StringCast<TCHAR>(Out.GetData(), Out.Len())));
}

FString FDebugIds::Print(FEnumId Name) const
{
	TUtf8StringBuilder<128> Out;
	if (Name.Idx < Ids.NumEnums())
	{
		Ids.AppendString(Out, Name);
	}
	else
	{
		Out << GLiterals.Oob;
	}
	return FString(FStringView(StringCast<TCHAR>(Out.GetData(), Out.Len())));
}

FString FDebugIds::Print(FStructId Name) const
{
	TUtf8StringBuilder<128> Out;
	if (Name.Idx < Ids.NumStructs())
	{
		Ids.AppendString(Out, Name);
	}
	else
	{
		Out << GLiterals.Oob;
	}
	return FString(FStringView(StringCast<TCHAR>(Out.GetData(), Out.Len())));
}

///////////////////////////////////////////////////////////////////////////////

void PrintDiff(FUtf8Builder& Out, const FIds& Ids, const FDiffPath& Diff)
{
	check(Diff.Num());

	for (FDiffNode Node : ReverseIterate(Diff))
	{
		Ids.AppendString(Out, Node.Name);
		Out << '.';
	}
	Out.RemoveSuffix(1);
	Out << ' ';
	Out << '(';
	for (FDiffNode Node : ReverseIterate(Diff))
	{
		if (Node.Type.IsStruct())
		{
			Ids.AppendString(Out, Ids.Resolve(Node.Meta.Struct).Name);		
		}
		else if (Node.Type.IsRange())
		{
			Ids.AppendString(Out, FTypenameId(Node.Meta.Range.GetBindName()));	
		}
		else if (FOptionalEnumId Enum = Node.Meta.Leaf)
		{
			Ids.AppendString(Out, Ids.Resolve(Enum.Get()).Name);
		}
		else
		{
			Out << ToString(ToLeafType(Node.Type.AsLeaf()));
		}
		Out << ' ';
	}
	Out.RemoveSuffix(1);
	Out << ')';
}

void PrintDiff(FUtf8Builder& Out, const FBatchIds& Ids, const FReadDiffPath& Diff)
{
	check(Diff.Num());

	bool bWasName = false;
	// print type name for the outermost struct
	if (Diff.Last().Struct)
	{
		Ids.AppendString(Out, Ids.Resolve(Diff.Last().Struct.Get()).Name);
		bWasName = true;
	}
	// print struct members path with range indices
	for (FReadDiffNode Node : ReverseIterate(Diff))
	{
		if (Node.Name || Node.RangeIdx == ~0u)
		{
			if (bWasName)
			{
				Out << '.';
			}
			if (!Node.Name)
			{
				Out << GLiterals.Super;
			}
			else
			{
				Ids.AppendString(Out, Node.Name);
			}
			bWasName = true;
		}
		else if (Node.Type.IsRange())
		{
			Out << "[" << Node.RangeIdx << "]";
			bWasName = false;
		}
	}
}

} // namespace PlainProps
