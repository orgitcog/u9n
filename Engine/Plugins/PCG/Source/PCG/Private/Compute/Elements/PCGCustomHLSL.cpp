// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compute/Elements/PCGCustomHLSL.h"

#include "PCGContext.h"
#include "PCGComponent.h"
#include "PCGEdge.h"
#include "PCGModule.h"
#include "PCGPoint.h"
#include "PCGSubsystem.h"
#include "Compute/PCGComputeSource.h"
#include "Compute/Elements/PCGCustomHLSLKernel.h"
#include "Data/PCGPointData.h"
#include "Graph/PCGGPUGraphCompilationContext.h"

#include "Internationalization/Regex.h"
#include "Containers/StaticArray.h"

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGCustomHLSL)

#define LOCTEXT_NAMESPACE "PCGCustomHLSLElement"

namespace PCGHLSLElement
{
	const FString PinDeclTemplateStr = TEXT("{pin}");
}

UPCGCustomHLSLSettings::UPCGCustomHLSLSettings()
{
	bExecuteOnGPU = true;

#if WITH_EDITOR
	if (!HasAnyFlags(RF_ClassDefaultObject) && !FPCGContext::IsInitializingSettings())
	{
		if (!UPCGComputeSource::OnModifiedDelegate.IsBoundToObject(this))
		{
			UPCGComputeSource::OnModifiedDelegate.AddUObject(this, &UPCGCustomHLSLSettings::OnComputeSourceModified);
		}
	}
#endif
}

#if WITH_EDITOR
void UPCGCustomHLSLSettings::PostLoad()
{
	Super::PostLoad();

	// Note: We update here so that Custom HLSL nodes will have the correct pin settings & declarations on load.
	UpdatePinSettings();
	UpdateAttributeKeys();
	UpdateDeclarations();
}

void UPCGCustomHLSLSettings::PostInitProperties()
{
	Super::PostInitProperties();

	// Note: We update here so that Custom HLSL nodes will have the correct pin settings & declarations on creation.
	UpdatePinSettings();
	UpdateAttributeKeys();
	UpdateDeclarations();
}

void UPCGCustomHLSLSettings::BeginDestroy()
{
	UPCGComputeSource::OnModifiedDelegate.RemoveAll(this);

	Super::BeginDestroy();
}
#endif

TArray<FPCGPinProperties> UPCGCustomHLSLSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	Algo::Transform(OutputPins, PinProperties, [](const FPCGPinPropertiesGPU& InPropertiesGPU) { return InPropertiesGPU; });
	return PinProperties;
}

#if WITH_EDITOR
void UPCGCustomHLSLSettings::PreEditChange(FProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	// If a pin label is about to change, cache all input label names to diff against in PostEditChangeProperty. We'll use this to fix-up pin label references.
	if (PropertyAboutToChange && PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(FPCGPinProperties, Label))
	{
		InputPinLabelsPreEditChange.Reset();

		for (const FPCGPinProperties& PinProps : InputPins)
		{
			InputPinLabelsPreEditChange.Add(PinProps.Label);
		}
	}
}

void UPCGCustomHLSLSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Apply any pin setup before refreshing the node.
	UpdatePinSettings();
	UpdateAttributeKeys();

	const FName MemberProperty = PropertyChangedEvent.MemberProperty ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;
	const FName Property = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (MemberProperty == GET_MEMBER_NAME_CHECKED(UPCGCustomHLSLSettings, OutputPins)
		&& Property == GET_MEMBER_NAME_CHECKED(UPCGCustomHLSLSettings, OutputPins)
		&& PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayAdd)
	{
		// Whenever a new output pin is created, we should default initialize 'PinsToInitializeFrom' with the first input pin label (if it exists).
		if (const UPCGPin* FirstInputPin = GetFirstInputPin())
		{
			check(!OutputPins.IsEmpty());

			FPCGPinPropertiesGPU& PinProps = OutputPins.Last();
			PinProps.PropertiesGPU.PinsToInititalizeFrom.Add(FirstInputPin->Properties.Label);
		}
	}
	else if (MemberProperty == GET_MEMBER_NAME_CHECKED(UPCGCustomHLSLSettings, InputPins)
		&& Property == GET_MEMBER_NAME_CHECKED(FPCGPinProperties, Label))
	{
		check(InputPinLabelsPreEditChange.Num() == InputPins.Num());

		// Fix-up pin input pin label references if an input pin label changed.
		for (int Index = 0; Index < InputPinLabelsPreEditChange.Num(); ++Index)
		{
			const FName InputLabelBeforeChange = InputPinLabelsPreEditChange[Index];
			const FName InputLabelAfterChange = InputPins[Index].Label;

			if (InputLabelBeforeChange != InputLabelAfterChange)
			{
				for (FPCGPinPropertiesGPU& OutPinProps : OutputPins)
				{
					for (FName& InitPinLabel : OutPinProps.PropertiesGPU.PinsToInititalizeFrom)
					{
						if (InitPinLabel == InputLabelBeforeChange)
						{
							InitPinLabel = InputLabelAfterChange;
						}
					}
				}

				// TODO: Could also find/replace to fix-up the kernel source
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);

	UpdateDeclarations();
}
#endif

FPCGElementPtr UPCGCustomHLSLSettings::CreateElement() const
{
	return MakeShared<FPCGCustomHLSLElement>();
}

#if WITH_EDITOR
TArray<FPCGPreConfiguredSettingsInfo> UPCGCustomHLSLSettings::GetPreconfiguredInfo() const
{
	return FPCGPreConfiguredSettingsInfo::PopulateFromEnum<EPCGKernelType>();
}

EPCGChangeType UPCGCustomHLSLSettings::GetChangeTypeForProperty(const FName& InPropertyName) const
{
	EPCGChangeType ChangeType = Super::GetChangeTypeForProperty(InPropertyName);

	if (InPropertyName == GET_MEMBER_NAME_CHECKED(UPCGCustomHLSLSettings, ShaderSource)
		|| InPropertyName == GET_MEMBER_NAME_CHECKED(UPCGCustomHLSLSettings, ShaderFunctions))
	{
		ChangeType |= EPCGChangeType::ShaderSource;
	}

	// Any settings change to this node could change the compute graph.
	ChangeType |= EPCGChangeType::Structural;

	return ChangeType;
}
#endif

void UPCGCustomHLSLSettings::ApplyPreconfiguredSettings(const FPCGPreConfiguredSettingsInfo& PreconfiguredInfo)
{
	if (const UEnum* EnumPtr = StaticEnum<EPCGKernelType>())
	{
		if (EnumPtr->IsValidEnumValue(PreconfiguredInfo.PreconfiguredIndex))
		{
			KernelType = EPCGKernelType(PreconfiguredInfo.PreconfiguredIndex);

			// Generators don't utilize the default input pin, so let's not add it by default.
			if (IsGeneratorKernel())
			{
				InputPins.Empty();
			}

#if WITH_EDITOR
			UpdatePinSettings();
#endif

			// Default to initializing the first output pin's from the first input pin's data.
			if (const UPCGPin* FirstInputPin = GetFirstInputPin())
			{
				if (!OutputPins.IsEmpty())
				{
					FPCGPinPropertiesGPU& PinProps = OutputPins.Last();
					PinProps.PropertiesGPU.PinsToInititalizeFrom.Add(FirstInputPin->Properties.Label);
				}
			}

#if WITH_EDITOR
			UpdateDeclarations();
#endif
		}
	}
}

FString UPCGCustomHLSLSettings::GetAdditionalTitleInformation() const
{
	if (const UEnum* EnumPtr = StaticEnum<EPCGKernelType>())
	{
		return EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(KernelType)).ToString();
	}

	return FString();
}

const UPCGPin* UPCGCustomHLSLSettings::GetInputPin(FName Label) const
{
	const UPCGNode* Node = Cast<UPCGNode>(GetOuter());
	return Node ? Node->GetInputPin(Label) : nullptr;
}

const UPCGPin* UPCGCustomHLSLSettings::GetOutputPin(FName Label) const
{
	const UPCGNode* Node = Cast<UPCGNode>(GetOuter());
	return Node ? Node->GetOutputPin(Label) : nullptr;
}

const UPCGPin* UPCGCustomHLSLSettings::GetFirstInputPin() const
{
	if (const UPCGNode* Node = Cast<UPCGNode>(GetOuter()))
	{
		return !Node->GetInputPins().IsEmpty() ? Node->GetInputPins()[0] : nullptr;
	}

	return nullptr;
}

const UPCGPin* UPCGCustomHLSLSettings::GetFirstOutputPin() const
{
	if (const UPCGNode* Node = Cast<UPCGNode>(GetOuter()))
	{
		return !Node->GetOutputPins().IsEmpty() ? Node->GetOutputPins()[0] : nullptr;
	}

	return nullptr;
}

#if WITH_EDITOR
FString UPCGCustomHLSLSettings::GetDeclarationsText() const
{
	return InputDeclarations + TEXT("\n\n") + OutputDeclarations + TEXT("\n\n") + HelperDeclarations;
}

FString UPCGCustomHLSLSettings::GetShaderFunctionsText() const
{
	return ShaderFunctions;
}

FString UPCGCustomHLSLSettings::GetShaderText() const
{
	return ShaderSource;
}

void UPCGCustomHLSLSettings::SetShaderFunctionsText(const FString& NewFunctionsText)
{
	const FName PropertyName = GET_MEMBER_NAME_CHECKED(UPCGCustomHLSLSettings, ShaderFunctions);
	FProperty* Property = FindFProperty<FProperty>(StaticClass(), PropertyName);
	FPropertyChangedEvent PropertyChangedEvent(Property, EPropertyChangeType::ValueSet);

	{
		FScopedTransaction Transaction(LOCTEXT("OnSetShaderFunctionsText", "Set Shader Functions Text"));
	
		PreEditChange(Property);
		Modify();
		ShaderFunctions = NewFunctionsText;
		PostEditChangeProperty(PropertyChangedEvent);
	}
	
	OnSettingsChangedDelegate.Broadcast(this, GetChangeTypeForProperty(PropertyName));
}

void UPCGCustomHLSLSettings::SetShaderText(const FString& NewText)
{
	const FName PropertyName = GET_MEMBER_NAME_CHECKED(UPCGCustomHLSLSettings, ShaderSource);
	FProperty* Property = FindFProperty<FProperty>(StaticClass(), PropertyName);
	FPropertyChangedEvent PropertyChangedEvent(Property, EPropertyChangeType::ValueSet);
	
	{
		FScopedTransaction Transaction(LOCTEXT("OnSetShaderSourceText", "Set Shader Source Text"));

		PreEditChange(Property);
		Modify();
		ShaderSource = NewText;
		PostEditChangeProperty(PropertyChangedEvent);
	}

	OnSettingsChangedDelegate.Broadcast(this, GetChangeTypeForProperty(PropertyName));
}

bool UPCGCustomHLSLSettings::IsShaderTextReadOnly() const
{
	return KernelSourceOverride != nullptr;
}

void UPCGCustomHLSLSettings::CreateKernels(FPCGGPUCompilationContext& InOutContext, UObject* InObjectOuter, TArray<UPCGComputeKernel*>& OutKernels, TArray<FPCGKernelEdge>& OutEdges) const
{
	FPCGComputeKernelParams KernelParams;
	KernelParams.Settings = this;
	KernelParams.bLogDescriptions = bDumpDataDescriptions;

	UPCGCustomHLSLKernel* Kernel = InOutContext.NewObject_AnyThread<UPCGCustomHLSLKernel>(InObjectOuter);
	Kernel->Initialize(KernelParams);
	OutKernels.Add(Kernel);

	// Connect node pins to kernel pins
	for (const FPCGPinProperties& Pin : InputPins)
	{
		OutEdges.Emplace(FPCGPinReference(Pin.Label), FPCGPinReference(Kernel, Pin.Label));
	}

	for (const FPCGPinPropertiesGPU& Pin : OutputPins)
	{
		OutEdges.Emplace(FPCGPinReference(Kernel, Pin.Label), FPCGPinReference(Pin.Label));
	}
}
#endif // WITH_EDITOR

const FPCGPinPropertiesGPU* UPCGCustomHLSLSettings::GetOutputPinPropertiesGPU(const FName& InPinLabel) const
{
	return OutputPins.FindByPredicate([InPinLabel](const FPCGPinPropertiesGPU& InProperties)
	{
		return InProperties.Label == InPinLabel;
	});
}

#if WITH_EDITOR
void UPCGCustomHLSLSettings::UpdateDeclarations()
{
	// Reference: UOptimusNode_CustomComputeKernel::UpdatePreamble
	UpdateInputDeclarations();
	UpdateOutputDeclarations();
	UpdateHelperDeclarations();

	// TODO: Should data labels be explained/exemplified in the declarations?
}

void UPCGCustomHLSLSettings::UpdateInputDeclarations()
{
	InputDeclarations.Reset();

	// Constants category
	{
		if (IsGeneratorKernel())
		{
			InputDeclarations += TEXT("/*** INPUT CONSTANTS ***/\n\n");
		}

		if (KernelType == EPCGKernelType::PointGenerator)
		{
			InputDeclarations += FString::Format(TEXT(
				"const uint NumElements = {0};\n"
				"const uint NumPoints = {0}; // Deprecated, use NumElements instead.\n" // Deprecated 5.6
				"\n"
			), { PointCount });
		}
		else if (KernelType == EPCGKernelType::TextureGenerator)
		{
			InputDeclarations += FString::Format(TEXT("const uint2 NumElements = uint2({0}, {1});\n\n"), { NumElements2D.X, NumElements2D.Y });
		}

		InputDeclarations += TEXT("/*** INPUT PER-THREAD CONSTANTS ***/\n\n");
		InputDeclarations += TEXT("const uint ThreadIndex;\n");

		if (IsProcessorKernel())
		{
			const UPCGPin* InputPin = GetFirstInputPin();
			const UPCGPin* OutputPin = GetFirstOutputPin();

			if (InputPin && OutputPin)
			{
				InputDeclarations += FString::Format(TEXT(
					"const uint {0}_DataIndex;\n"
					"const uint {1}_DataIndex;\n"),
					{ InputPin->Properties.Label.ToString(),  OutputPin->Properties.Label.ToString() });
			}
		}
		else if (IsGeneratorKernel())
		{
			if (const UPCGPin* PointProcessingOutputPin = GetFirstOutputPin())
			{
				InputDeclarations += FString::Format(
					TEXT("const uint {0}_DataIndex;\n"),
					{ PointProcessingOutputPin->Properties.Label.ToString() });
			}
		}

		if (IsPointKernel())
		{
			InputDeclarations += TEXT("const uint ElementIndex;\n");
		}
		else if (IsTextureKernel())
		{
			InputDeclarations += TEXT("const uint2 ElementIndex;\n");
		}

		InputDeclarations += TEXT("\n");
	}

	TArray<FString> DataCollectionDataPins;
	TArray<FString> PointDataPins;
	TArray<FString> LandscapeDataPins;
	TArray<FString> TextureDataPins;
	TArray<FString> VirtualTextureDataPins;
	TArray<FString> RawBufferDataPins;
	TArray<FString> StaticMeshDataPins;

	for (const FPCGPinProperties& Pin : InputPinProperties())
	{
		if (PCGComputeHelpers::IsTypeAllowedInDataCollection(Pin.AllowedTypes))
		{
			DataCollectionDataPins.Add(Pin.Label.ToString());
		}

		if (!!(Pin.AllowedTypes & EPCGDataType::Point))
		{
			PointDataPins.Add(Pin.Label.ToString());
		}

		if (!!(Pin.AllowedTypes & EPCGDataType::Landscape))
		{
			LandscapeDataPins.Add(Pin.Label.ToString());
		}

		if (!!(Pin.AllowedTypes & EPCGDataType::BaseTexture))
		{
			TextureDataPins.Add(Pin.Label.ToString());
		}

		if (!!(Pin.AllowedTypes & EPCGDataType::VirtualTexture))
		{
			VirtualTextureDataPins.Add(Pin.Label.ToString());
		}

		if (!!(Pin.AllowedTypes & EPCGDataType::StaticMeshResource))
		{
			StaticMeshDataPins.Add(Pin.Label.ToString());
		}
	}

	if (!DataCollectionDataPins.IsEmpty())
	{
		InputDeclarations += TEXT("/*** INPUT DATA FUNCTIONS ***/\n\n");

		const bool bMultiPin = DataCollectionDataPins.Num() > 1;

		if (bMultiPin)
		{
			InputDeclarations += TEXT("// Valid pins: ") + FString::Join(DataCollectionDataPins, TEXT(", ")) + TEXT("\n");
		}

		InputDeclarations += FString::Format(TEXT(
			"uint {0}_GetNumData();\n"
			"uint {0}_GetNumElements();\n"
			"uint {0}_GetNumElements(uint DataIndex);\n"
			"\n"
			"// Valid types: bool, int, float, float2, float3, float4, Rotator (float3), Quat (float4), Transform (float4x4), StringKey (int), Name (uint2)\n"
			"\n"
			"{type} {0}_Get{type}(uint DataIndex, uint ElementIndex, 'AttributeName');\n"
			"\n"
			"// Example: {0}_GetFloat({0}_DataIndex, ElementIndex, 'MyFloatAttr');\n"),
			{ bMultiPin ? PCGHLSLElement::PinDeclTemplateStr : DataCollectionDataPins[0] });

		InputDeclarations += TEXT("\n");
	}

	if (!PointDataPins.IsEmpty())
	{
		InputDeclarations += TEXT("/*** INPUT POINT DATA FUNCTIONS ***/\n\n");

		const bool bMultiPin = PointDataPins.Num() > 1;

		if (bMultiPin)
		{
			InputDeclarations += TEXT("// Valid pins: ") + FString::Join(PointDataPins, TEXT(", ")) + TEXT("\n\n");
		}

		InputDeclarations += FString::Format(TEXT(
			"float3 {0}_GetPosition(uint DataIndex, uint ElementIndex);\n"
			"float4 {0}_GetRotation(uint DataIndex, uint ElementIndex);\n"
			"float3 {0}_GetScale(uint DataIndex, uint ElementIndex);\n"
			"float3 {0}_GetBoundsMin(uint DataIndex, uint ElementIndex);\n"
			"float3 {0}_GetBoundsMax(uint DataIndex, uint ElementIndex);\n"
			"float4 {0}_GetColor(uint DataIndex, uint ElementIndex);\n"
			"float {0}_GetDensity(uint DataIndex, uint ElementIndex);\n"
			"int {0}_GetSeed(uint DataIndex, uint ElementIndex);\n"
			"float {0}_GetSteepness(uint DataIndex, uint ElementIndex);\n"
			"float4x4 {0}_GetPointTransform(uint DataIndex, uint ElementIndex);\n"
			"bool {0}_IsPointRemoved(uint DataIndex, uint ElementIndex);\n"),
			{ bMultiPin ? PCGHLSLElement::PinDeclTemplateStr : PointDataPins[0] });

		InputDeclarations += TEXT("\n");
	}

	if (!LandscapeDataPins.IsEmpty())
	{
		InputDeclarations += TEXT("/*** INPUT LANDSCAPE DATA FUNCTIONS ***/\n\n");

		const bool bMultiPin = LandscapeDataPins.Num() > 1;

		if (bMultiPin)
		{
			InputDeclarations += TEXT("// Valid pins: ") + FString::Join(LandscapeDataPins, TEXT(", ")) + TEXT("\n\n");
		}

		InputDeclarations += FString::Format(TEXT(
			"float {0}_GetHeight(float3 WorldPos);\n"
			"float3 {0}_GetNormal(float3 WorldPos);\n"
			"float3 {0}_GetBaseColor(float3 WorldPos);\n"),
			{ bMultiPin ? PCGHLSLElement::PinDeclTemplateStr : LandscapeDataPins[0] });

		InputDeclarations += TEXT("\n");
	}

	if (!TextureDataPins.IsEmpty())
	{
		InputDeclarations += TEXT("/*** INPUT TEXTURE DATA FUNCTIONS ***/\n\n");

		const bool bMultiPin = TextureDataPins.Num() > 1;

		if (bMultiPin)
		{
			InputDeclarations += TEXT("// Valid pins: ") + FString::Join(TextureDataPins, TEXT(", ")) + TEXT("\n\n");
		}

		InputDeclarations += FString::Format(TEXT(
			"uint {0}_GetNumData();\n"
			"uint2 {0}_GetNumElements(uint DataIndex);\n"
			"// Computes a texture coordinate as if WorldPos lies in a UV plane defined by WorldMin and WorldMax.\n"
			"float2 {0}_GetTexCoords(float2 WorldPos, float2 WorldMin, float2 WorldMax);\n"
			"float4 {0}_Sample(uint DataIndex, float2 TexCoords);\n"
			"// Computes sample coordinates of the WorldPos relative to the texture data's bounds.\n"
			"float4 {0}_SampleWorldPos(uint DataIndex, float2 WorldPos);\n"
			"float4 {0}_Load(uint DataIndex, uint2 ElementIndex);\n"),
			{ bMultiPin ? PCGHLSLElement::PinDeclTemplateStr : TextureDataPins[0] });

		InputDeclarations += TEXT("\n");
	}

	if (!VirtualTextureDataPins.IsEmpty())
	{
		InputDeclarations += TEXT("/*** INPUT VIRTUAL TEXTURE DATA FUNCTIONS ***/\n\n");

		const bool bMultiPin = VirtualTextureDataPins.Num() > 1;

		if (bMultiPin)
		{
			InputDeclarations += TEXT("// Valid pins: ") + FString::Join(VirtualTextureDataPins, TEXT(", ")) + TEXT("\n\n");
		}

		InputDeclarations += FString::Format(TEXT(
			"// Samples a virtual texture and gets all values that are available. Otherwise returns default values."
			"void {0}_SampleVirtualTexture(\n"
			"	uint InDataIndex,\n"
			"	float3 InWorldPos,\n"
			"	out bool bOutInsideVolume,\n"
			"	out float3 OutBaseColor,\n"
			"	out float OutSpecular,\n"
			"	out float OutRoughness,\n"
			"	out float OutWorldHeight,\n"
			"	out float3 OutNormal,\n"
			"	out float OutDisplacement,\n"
			"	out float OutMask,\n"
			"	out float4 OutMask4);\n"),
			{ bMultiPin ? PCGHLSLElement::PinDeclTemplateStr : VirtualTextureDataPins[0] });

		InputDeclarations += TEXT("\n");
	}

	if (!RawBufferDataPins.IsEmpty())
	{
		InputDeclarations += TEXT("/*** INPUT BYTE ADDRESS BUFFER DATA FUNCTIONS ***/\n\n");

		const bool bMultiPin = RawBufferDataPins.Num() > 1;

		if (bMultiPin)
		{
			InputDeclarations += TEXT("// Valid pins: ") + FString::Join(RawBufferDataPins, TEXT(", ")) + TEXT("\n\n");
		}

		InputDeclarations += FString::Format(TEXT(
			"uint {0}_ReadNumValues();\n"
			"uint {0}_ReadValue(uint Index);\n"),
			{ bMultiPin ? PCGHLSLElement::PinDeclTemplateStr : RawBufferDataPins[0] });

		InputDeclarations += TEXT("\n");
	}

	if (!StaticMeshDataPins.IsEmpty())
	{
		InputDeclarations += TEXT("/*** INPUT STATIC MESH DATA FUNCTIONS ***/\n\n");

		const bool bMultiPin = StaticMeshDataPins.Num() > 1;

		if (bMultiPin)
		{
			InputDeclarations += TEXT("// Valid pins: ") + FString::Join(StaticMeshDataPins, TEXT(", ")) + TEXT("\n\n");
		}

		InputDeclarations += FString::Format(TEXT(
			"// Vertex functions\n"
			"int {0}_GetNumVertices(int DataIndex);\n"
			"void {0}_GetVertex(int DataIndex, int VertexIndex, out float3 OutPosition, out float3 OutNormal, out float3 OutTangent, out float3 OutBitangent);\n"
			"float4 {0}_GetVertexColor(int DataIndex, int VertexIndex);\n"
			"float2 {0}_GetVertexUVs(int DataIndex, int VertexIndex, int UVSet);\n"
			"\n"
			"// Triangle functions\n"
			"int {0}_GetNumTriangles(int DataIndex);\n"
			"void {0}_GetTriangleIndices(int DataIndex, int TriangleIndex, out int OutIndex0, out int OutIndex1, out int OutIndex2);\n"
			"void {0}_SampleTriangle(int DataIndex, int TriangleIndex, float3 BaryCoords, out float3 OutPosition, out float3 OutNormal, out float3 OutTangent, out float3 OutBitangent);\n"
			"float4 {0}_SampleTriangleColor(int DataIndex, int TriangleIndex, float3 BaryCoords);\n"
			"float2 {0}_SampleTriangleUVs(int DataIndex, int TriangleIndex, float3 BaryCoords, int UVSet);\n"
			"\n"
			"// Get bounds extents of the static mesh.\n"
			"float3 {0}_GetMeshBoundsExtents(int DataIndex);\n"),
			{ bMultiPin ? PCGHLSLElement::PinDeclTemplateStr : StaticMeshDataPins[0] });

		InputDeclarations += TEXT("\n");
	}

	InputDeclarations.TrimStartAndEndInline();
}

void UPCGCustomHLSLSettings::UpdateOutputDeclarations()
{
	OutputDeclarations.Reset();

	TArray<FString> DataCollectionDataPins;
	TArray<FString> PointDataPins;
	TArray<FString> TextureDataPins;
	TArray<FString> RawBufferDataPins;

	for (const FPCGPinProperties& Pin : OutputPinProperties())
	{
		if (PCGComputeHelpers::IsTypeAllowedInDataCollection(Pin.AllowedTypes))
		{
			DataCollectionDataPins.Add(Pin.Label.ToString());
		}

		if (!!(Pin.AllowedTypes & EPCGDataType::Point))
		{
			PointDataPins.Add(Pin.Label.ToString());
		}

		if (!!(Pin.AllowedTypes & EPCGDataType::BaseTexture))
		{
			TextureDataPins.Add(Pin.Label.ToString());
		}
	}

	if (!DataCollectionDataPins.IsEmpty())
	{
		OutputDeclarations += TEXT("/*** OUTPUT DATA FUNCTIONS ***/\n\n");

		const bool bMultiPin = DataCollectionDataPins.Num() > 1;

		if (bMultiPin)
		{
			OutputDeclarations += TEXT("// Valid pins: ") + FString::Join(DataCollectionDataPins, TEXT(", ")) + TEXT("\n\n");
		}

		OutputDeclarations += FString::Format(TEXT("void {0}_GetElementCountMultiplier();\n\n"),
			{ bMultiPin ? PCGHLSLElement::PinDeclTemplateStr : DataCollectionDataPins[0] });

		OutputDeclarations += FString::Format(TEXT(
			"// Valid types: bool, int, float, float2, float3, float4, Rotator (float3), Quat (float4), Transform (float4x4), StringKey (int), Name (uint2)\n"
			"\n"
			"void {0}_Set{type}(uint DataIndex, uint ElementIndex, 'AttributeName', {type} Value);\n"
			"\n"
			"// Example: {0}_SetFloat({0}_DataIndex, ElementIndex, 'MyFloatAttr', MyValue);\n"),
			{ bMultiPin ? PCGHLSLElement::PinDeclTemplateStr : DataCollectionDataPins[0] });

		OutputDeclarations += TEXT("\n");
	}

	if (!PointDataPins.IsEmpty())
	{
		OutputDeclarations += TEXT("/*** OUTPUT POINT DATA FUNCTIONS ***/\n\n");

		const bool bMultiPin = PointDataPins.Num() > 1;

		if (bMultiPin)
		{
			OutputDeclarations += TEXT("// Valid pins: ") + FString::Join(PointDataPins, TEXT(", ")) + TEXT("\n\n");
		}

		OutputDeclarations += FString::Format(TEXT(
			"void {0}_InitializePoint(uint DataIndex, uint ElementIndex);\n"
			"void {0}_CopyElementFrom_{input pin}(uint TargetDataIndex, uint TargetElementIndex, uint SourceDataIndex, uint SourceElementIndex);\n"
			"bool {0}_RemovePoint(uint DataIndex, uint ElementIndex);\n"
			"\n"
			"void {0}_SetPosition(uint DataIndex, uint ElementIndex, float3 Position);\n"
			"void {0}_SetRotation(uint DataIndex, uint ElementIndex, float4 Rotation);\n"
			"void {0}_SetScale(uint DataIndex, uint ElementIndex, float3 Scale);\n"
			"void {0}_SetBoundsMin(uint DataIndex, uint ElementIndex, float3 BoundsMin);\n"
			"void {0}_SetBoundsMax(uint DataIndex, uint ElementIndex, float3 BoundsMax);\n"
			"void {0}_SetColor(uint DataIndex, uint ElementIndex, float4 Color);\n"
			"void {0}_SetDensity(uint DataIndex, uint ElementIndex, float Density);\n"
			"void {0}_SetSeed(uint DataIndex, uint ElementIndex, int Seed);\n"
			"void {0}_SetSteepness(uint DataIndex, uint ElementIndex, float Steepness);\n"
			"void {0}_SetPointTransform(uint DataIndex, uint ElementIndex, float4x4 Transform);\n"),
			{ bMultiPin ? PCGHLSLElement::PinDeclTemplateStr : PointDataPins[0] });

		OutputDeclarations += TEXT("\n");
	}

	if (!TextureDataPins.IsEmpty())
	{
		OutputDeclarations += TEXT("/*** OUTPUT TEXTURE DATA FUNCTIONS ***/\n\n");

		const bool bMultiPin = TextureDataPins.Num() > 1;

		if (bMultiPin)
		{
			OutputDeclarations += TEXT("// Valid pins: ") + FString::Join(TextureDataPins, TEXT(", ")) + TEXT("\n\n");
		}

		OutputDeclarations += FString::Format(TEXT(
			"uint {0}_GetNumData();\n"
			"uint2 {0}_GetNumElements(uint DataIndex);\n"
			"void {0}_Store(uint DataIndex, uint2 ElementIndex, float4 Value);\n"),
			{ bMultiPin ? PCGHLSLElement::PinDeclTemplateStr : TextureDataPins[0] });

		OutputDeclarations += TEXT("\n");
	}

	if (!RawBufferDataPins.IsEmpty())
	{
		OutputDeclarations += TEXT("/*** OUTPUT BYTE ADDRESS BUFFER DATA FUNCTIONS ***/\n\n");

		const bool bMultiPin = RawBufferDataPins.Num() > 1;

		if (bMultiPin)
		{
			OutputDeclarations += TEXT("// Valid pins: ") + FString::Join(RawBufferDataPins, TEXT(", ")) + TEXT("\n\n");
		}

		OutputDeclarations += FString::Format(TEXT(
			"uint {0}_WriteValue(uint Index, uint Value);\n"),
			{ bMultiPin ? PCGHLSLElement::PinDeclTemplateStr : RawBufferDataPins[0] });

		OutputDeclarations += TEXT("\n");
	}

	OutputDeclarations.TrimStartAndEndInline();
}

void UPCGCustomHLSLSettings::UpdateHelperDeclarations()
{
	HelperDeclarations.Reset();

	// Helper functions category
	{
		HelperDeclarations += TEXT(
			"/*** HELPER FUNCTIONS ***/\n"
			"\n"
			"int3 GetNumThreads();\n"
			"uint GetThreadCountMultiplier();\n");

		// Get thread data - useful in all kernel types for secondary pins.
		{
			TArray<FString> DataCollectionPinNames;
			TArray<FString> TextureDataPinNames;

			for (const FPCGPinProperties& Properties : InputPinProperties())
			{
				if (PCGComputeHelpers::IsTypeAllowedInDataCollection(Properties.AllowedTypes))
				{
					DataCollectionPinNames.Add(Properties.Label.ToString());
				}

				if (!!(Properties.AllowedTypes & EPCGDataType::BaseTexture))
				{
					TextureDataPinNames.Add(Properties.Label.ToString());
				}
			}

			for (const FPCGPinProperties& Properties : OutputPinProperties())
			{
				if (PCGComputeHelpers::IsTypeAllowedInDataCollection(Properties.AllowedTypes))
				{
					DataCollectionPinNames.Add(Properties.Label.ToString());
				}

				if (!!(Properties.AllowedTypes & EPCGDataType::BaseTexture))
				{
					TextureDataPinNames.Add(Properties.Label.ToString());
				}
			}

			if (!DataCollectionPinNames.IsEmpty())
			{
				HelperDeclarations += TEXT("\n// Returns false if thread has no data to operate on.\n");

				const bool bMultiPin = DataCollectionPinNames.Num() > 1;

				if (bMultiPin)
				{
					HelperDeclarations += TEXT("// Valid pins: ") + FString::Join(DataCollectionPinNames, TEXT(", ")) + TEXT("\n");
				}

				HelperDeclarations += FString::Format(
					TEXT("bool {0}_GetThreadData(uint ThreadIndex, out uint OutDataIndex, out uint OutElementIndex);\n"),
					{ bMultiPin ? PCGHLSLElement::PinDeclTemplateStr : DataCollectionPinNames[0] });

				HelperDeclarations += TEXT("\n");
			}

			if (!TextureDataPinNames.IsEmpty())
			{
				HelperDeclarations += TEXT("\n// Returns false if thread has no data to operate on.\n");

				const bool bMultiPin = TextureDataPinNames.Num() > 1;

				if (bMultiPin)
				{
					HelperDeclarations += TEXT("// Valid pins: ") + FString::Join(TextureDataPinNames, TEXT(", ")) + TEXT("\n");
				}

				HelperDeclarations += FString::Format(
					TEXT("bool {0}_GetThreadData(uint ThreadIndex, out uint OutDataIndex, out uint2 OutElementIndex);\n"),
					{ bMultiPin ? PCGHLSLElement::PinDeclTemplateStr : TextureDataPinNames[0] });

				HelperDeclarations += TEXT("\n");
			}
		}

		HelperDeclarations += TEXT(
			"float3 GetComponentBoundsMin(); // World-space\n"
			"float3 GetComponentBoundsMax();\n"
			"uint GetSeed();\n"
			"\n"
			"float FRand(inout uint Seed); // Returns random float between 0 and 1.\n"
			"uint ComputeSeed(uint A, uint B);\n"
			"uint ComputeSeed(uint A, uint B, uint C);\n"
			"uint ComputeSeedFromPosition(float3 Position);\n"
			"\n"
			"// Returns the position of the Nth point in a 2D or 3D grid with the given constraints.\n"
			"float3 CreateGrid2D(uint ElementIndex, uint NumPoints, float3 Min, float3 Max);\n"
			"float3 CreateGrid2D(uint ElementIndex, uint NumPoints, uint NumRows, float3 Min, float3 Max);\n"
			"float3 CreateGrid3D(uint ElementIndex, uint NumPoints, float3 Min, float3 Max);\n"
			"float3 CreateGrid3D(uint ElementIndex, uint NumPoints, uint NumRows, uint NumCols, float3 Min, float3 Max);\n");

		if (bPrintShaderDebugValues)
		{
			HelperDeclarations += FString::Format(TEXT(
				"\n"
				"// Writes floats to the debug buffer array, which will be readback and logged in the console for inspection.\n"
				"void WriteDebugValue(uint Index, float Value); // Index in [0, {0}] (set from 'Debug Buffer Size' property)\n"),
				{ DebugBufferSize - 1 });
		}
	}

	HelperDeclarations.TrimStartAndEndInline();
}

void UPCGCustomHLSLSettings::UpdatePinSettings()
{
	// Setup input pins.
	for (int PinIndex = 0; PinIndex < InputPins.Num(); ++PinIndex)
	{
		FPCGPinProperties& Properties = InputPins[PinIndex];

		// Type Any is not allowed, default to Point
		if (Properties.AllowedTypes == EPCGDataType::Any)
		{
			Properties.AllowedTypes = EPCGDataType::Point;
		}

		// Allow kernel type to drive the first pin type.
		if (PinIndex == 0)
		{
			if (KernelType == EPCGKernelType::PointProcessor)
			{
				Properties.AllowedTypes = EPCGDataType::Point;
			}
			else if (KernelType == EPCGKernelType::TextureProcessor)
			{
				Properties.AllowedTypes = EPCGDataType::BaseTexture;

				// Texture kernels are single-data for now.
				Properties.bAllowMultipleData = false;
				Properties.bAllowEditMultipleData = false;
			}
		}

		if (!!(Properties.AllowedTypes & EPCGDataType::Landscape))
		{
			// Don't allow multiple data on this pin because we do not support a dynamic number of landscapes bound to a
			// compute kernel.
			Properties.bAllowMultipleData = false;

			Properties.bAllowEditMultipleData = false;
		}
		else
		{
			Properties.bAllowEditMultipleData = true;
		}

		// TODO: We have work to do to allow dynamic merging of data. Also we will likely inject Gather
		// nodes on the CPU side so that merging is handled CPU side where possible.
		Properties.SetAllowMultipleConnections(false);

		Properties.bAllowEditMultipleConnections = false;
	}

	// Setup output pins.
	for (int PinIndex = 0; PinIndex < OutputPins.Num(); ++PinIndex)
	{
		FPCGPinPropertiesGPU& Properties = OutputPins[PinIndex];

		// Type Any is not allowed, default to Point
		if (Properties.AllowedTypes == EPCGDataType::Any)
		{
			Properties.AllowedTypes = EPCGDataType::Point;
		}

		// Only allow editing the initialization mode if it's not driven by the kernel type.
		bool bInitModeDrivenByKernel = false;
		if (PinIndex == 0)
		{
			if (KernelType == EPCGKernelType::PointProcessor || KernelType == EPCGKernelType::PointGenerator)
			{
				bInitModeDrivenByKernel = true;
				Properties.AllowedTypes = EPCGDataType::Point;
			}
			else if (KernelType == EPCGKernelType::TextureProcessor || KernelType == EPCGKernelType::TextureGenerator)
			{
				bInitModeDrivenByKernel = true;
				Properties.AllowedTypes = EPCGDataType::BaseTexture;
			}

			Properties.bShowPropertiesGPU = !IsTextureKernel();
		}

		if (!!(Properties.AllowedTypes & EPCGDataType::BaseTexture))
		{
			// Texture outputs are single-data for now.
			Properties.bAllowMultipleData = false;
			Properties.bAllowEditMultipleData = false;
			Properties.PropertiesGPU.bShowTexturePinSettings = true;
		}
		else
		{
			Properties.PropertiesGPU.bShowTexturePinSettings = false;
		}

		Properties.PropertiesGPU.bAllowEditInitMode = !bInitModeDrivenByKernel;
		Properties.PropertiesGPU.bMultipleInitPins = Properties.PropertiesGPU.PinsToInititalizeFrom.Num() > 1;

		// Output pins should always allow multiple connections.
		// TODO this could be hoisted up somewhere in the future.
		Properties.bAllowEditMultipleConnections = false;

		Properties.bAllowEditMultipleData = true;
		Properties.PropertiesGPU.bAllowEditDataCount = true;
	}
}

void UPCGCustomHLSLSettings::UpdateAttributeKeys()
{
	// Make sure all the output attributes are up-to-date on their identifiers.
	bool bMarkedDirty = false;
	for (FPCGPinPropertiesGPU& OutputPin : OutputPins)
	{
		for (FPCGKernelAttributeKey& AttributeKey : OutputPin.PropertiesGPU.CreatedKernelAttributeKeys)
		{
			if (AttributeKey.UpdateIdentifierFromSelector() && !bMarkedDirty)
			{
				bMarkedDirty = true;
				MarkPackageDirty();
			}
		}
	}
}

void UPCGCustomHLSLSettings::OnComputeSourceModified(const UPCGComputeSource* InModifiedComputeSource)
{
	TArray<UComputeSource*> ComputeSourcesToVisit;
	TSet<UComputeSource*> VisitedComputeSources;

	ComputeSourcesToVisit.Push(KernelSourceOverride);
	ComputeSourcesToVisit.Append(AdditionalSources);

	// Visit the entire network of additional sources to see if our source depends on the modified compute source.
	bool bAnyMatch = false;

	while (!ComputeSourcesToVisit.IsEmpty())
	{
		if (UComputeSource* ComputeSource = ComputeSourcesToVisit.Pop())
		{
			if (ComputeSource == InModifiedComputeSource)
			{
				bAnyMatch = true;
				break;
			}

			VisitedComputeSources.Add(ComputeSource);

			for (UComputeSource* AdditionalSource : ComputeSource->AdditionalSources)
			{
				if (!VisitedComputeSources.Contains(AdditionalSource))
				{
					ComputeSourcesToVisit.Add(AdditionalSource);
				}
			}
		}
	}

	if (bAnyMatch)
	{
		// @todo_pcg: Revisit whether we can remove Structural from this (and other) source modifications.
		OnSettingsChangedDelegate.Broadcast(this, EPCGChangeType::ShaderSource | EPCGChangeType::Structural);
	}
}

TArray<FName> UPCGCustomHLSLSettings::GetInputPinNames() const
{
	TArray<FName> PinNames;

	for (const FPCGPinProperties& PinProps : InputPins)
	{
		PinNames.Add(PinProps.Label);
	}

	return PinNames;
}

TArray<FName> UPCGCustomHLSLSettings::GetInputPinNamesAndNone() const
{
	TArray<FName> PinNames = GetInputPinNames();
	PinNames.Insert(NAME_None, 0);

	return PinNames;
}
#endif // WITH_EDITOR

bool FPCGCustomHLSLElement::ExecuteInternal(FPCGContext* Context) const
{
	// This element does not support CPU execution and we are never supposed to land here.
	check(false);
	return true;
}

#undef LOCTEXT_NAMESPACE
