// Copyright Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialIRModuleBuilder.h"

#if WITH_EDITOR

#include "Materials/MaterialIRValueAnalyzer.h"
#include "Materials/MaterialIRModule.h"
#include "Materials/MaterialIRDebug.h"
#include "Materials/MaterialIR.h"
#include "Materials/MaterialIRTypes.h"
#include "Materials/MaterialIREmitter.h"
#include "Materials/MaterialIRInternal.h"
#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/Material.h"
#include "MaterialDomain.h"
#include "MaterialExpressionIO.h"
#include "MaterialShared.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialInsights.h"
#include "Engine/Texture.h"
#include "Misc/FileHelper.h"

static TAutoConsoleVariable<int32> CVarMaterialIRDebugDumpLevel(
	TEXT("r.Material.Translator.DebugDump"),
	0,
	TEXT("Whether the material translator should dump debug information about the translated module IR.\n")
	TEXT("0 (Default): No debug dump generated.\n")
	TEXT("1: Dump the material IR instructions to readable a human readable textual representation (to '{SavedDir}/Materials/IRDump.txt').\n")
	TEXT("2: Everything above, plus dump the 'Uses' graph in Graphviz Dot syntax (to '{SavedDir}/Materials/IRDumpUseGraph.dot').\n"),
	ECVF_RenderThreadSafe);

struct FAnalysisContext
{
	UMaterialExpressionMaterialFunctionCall* Call{};
	TSet<UMaterialExpression*> BuiltExpressions{};
	TArray<UMaterialExpression*> ExpressionStack{};
	TMap<const FExpressionInput*, MIR::FValue*> InputValues;
	TMap<const FExpressionOutput*, MIR::FValue*> OutputValues;

	MIR::FValue* GetInputValue(const FExpressionInput* Input)
	{
		MIR::FValue** Value = InputValues.Find(Input);
		return Value ? *Value : nullptr;
	}

	void SetInputValue(const FExpressionInput* Input, MIR::FValue* Value)
	{
		InputValues.Add(Input, Value);
	}

	MIR::FValue* GetOutputValue(const FExpressionOutput* Output)
	{
		MIR::FValue** Value = OutputValues.Find(Output);
		return Value ? *Value : nullptr;
	}
	
	void SetOutputValue(const FExpressionOutput* Output, MIR::FValue* Value)
	{
		OutputValues.Add(Output, Value);
	}
};

struct FMaterialIRModuleBuilderImpl
{
	FMaterialIRModuleBuilder* Builder;
	FMaterialIRModule* Module;
	MIR::FEmitter* Emitter;
	TArray<FAnalysisContext> AnalysisContextStack;
	FMaterialIRValueAnalyzer ValueAnalyzer;

	void Step_Initialize()
	{
		Module->Empty();
		Module->ShaderPlatform = Builder->ShaderPlatform;
		
		Emitter->Initialize();
		AnalysisContextStack.Emplace();
	}

	void Step_GenerateOutputInstructions()
	{
		// The normal input is read back from the value set in the material attribute.
		// For this reason, the normal attribute is evaluated and set first, ensuring that
		// other inputs can read its value.
		PrepareSingleMaterialAttribute(MP_Normal);

		// Then prepare all the other material attributes.
		for (int32 Index = 0; MIR::Internal::NextMaterialAttributeInput(Builder->Material, Index); ++Index)
		{
			if (Index != MP_Normal)
			{
				PrepareSingleMaterialAttribute((EMaterialProperty)Index);
			}
		}
	}

	void PrepareSingleMaterialAttribute(EMaterialProperty Property)
	{
		FMaterialInputDescription Input;
		Builder->Material->GetExpressionInputDescription(Property, Input);

		MIR::FSetMaterialOutput* Output = Emitter->SetMaterialOutput(Property, nullptr);

		if (Input.bUseConstant)
		{
			Output->Arg = Emitter->ConstantFromShaderValue(Input.ConstantValue);
		}
		else if (!Input.Input->IsConnected())
		{
			Output->Arg = MIR::Internal::CreateMaterialAttributeDefaultValue(*Emitter, Builder->Material, Property);
		}
		else
		{
			AnalysisContextStack.Last().ExpressionStack.Add(Input.Input->Expression);
		}
	}

	void Step_BuildMaterialExpressionsToIRGraph()
	{
		while (true)
		{
			FAnalysisContext& Context = AnalysisContextStack.Last();

			if (!Context.ExpressionStack.IsEmpty())
			{
				// Some expression is on the expression stack of this context. Analyze it. This will
				// have the effect of either building the expression or pushing its other expression
				// dependencies onto the stack.
				BuildTopMaterialExpression();
			}
			else if (Context.Call)
			{
				// There are no more expressions to analyze on the stack, this analysis context is complete.
				// Context.Call isn't null so this context is for a function call, which has now been fully analyzed.
				// Pop the callee context from the stack and resume analyzing the parent context (the caller).
				PopFunctionCall();
			}
			else
			{
				// No other expressions on the stack to evaluate, nor this is a function
				// call context but the root context. Nothing left to do so simply quit.
				break;
			}
		}
	}

	void BuildTopMaterialExpression()
	{
		FAnalysisContext& CurrContext = AnalysisContextStack.Last();
		Emitter->Expression = CurrContext.ExpressionStack.Last();

		// If expression is clean, nothing to be done.
		if (CurrContext.BuiltExpressions.Contains(Emitter->Expression))
		{
			CurrContext.ExpressionStack.Pop(EAllowShrinking::No);
			return;
		}

		// Push to the expression stack all dependencies that still need to be analyzed.
		for (FExpressionInputIterator It{ Emitter->Expression }; It; ++It)
		{
			// Ignore disconnected inputs and connected expressions already built.
			if (!It->IsConnected() || CurrContext.BuiltExpressions.Contains(It->Expression))
			{
				continue;
			}

			CurrContext.ExpressionStack.Push(It->Expression);
		}

		// If on top of the stack there's a different expression, we have a dependency to analyze first.
		if (CurrContext.ExpressionStack.Last() != Emitter->Expression)
		{
			return;
		}

		// Take the top expression out of the stack as ready for analysis. Also mark it as built.
		CurrContext.ExpressionStack.Pop();
		CurrContext.BuiltExpressions.Add(Emitter->Expression);

		// Flow the value into this expression's inputs from their connected outputs.
		for (FExpressionInputIterator It{ Emitter->Expression}; It; ++It)
		{
			// Fetch the value flowing through connected output.
			FExpressionOutput* ConnectedOutput = It->GetConnectedOutput();
			if (ConnectedOutput)
			{
				MIR::FValue** ValuePtr = CurrContext.OutputValues.Find(ConnectedOutput);
				if (ValuePtr)
				{
					// ...and flow it into this input.
					CurrContext.InputValues.Add(It.Input, *ValuePtr);
				}
			}
		}

		if (auto Call = Cast<UMaterialExpressionMaterialFunctionCall>(Emitter->Expression))
		{
			// Function calls are handled internally as they manipulate the analysis context stack.
			PushFunctionCall(Call);
		}
		else
		{
			// Invoke the expression build function. This will perform semantic analysis, error reporting and
			// emit IR values for its outputs (which will flow into connected expressions inputs).
			Emitter->Expression->Build(*Emitter);

			// Populate the insight information about this expression pins.
			AddExpressionConnectionInsights(Emitter->Expression);
		}
	}

	void PushFunctionCall(UMaterialExpressionMaterialFunctionCall* Call)
	{
		FMemMark Mark(FMemStack::Get());
		TArrayView<MIR::FValue*> CallInputValues = MakeTemporaryArray<MIR::FValue*>(Mark, Call->FunctionInputs.Num());

		// Make sure each function input is connected and has a value. If so, cache the values flowing into this
		// funcion call inside the auxiliary value array.
		for (int i = 0; i < Call->FunctionInputs.Num(); ++i)
		{
			FFunctionExpressionInput& FunctionInput = Call->FunctionInputs[i];
			MIR::FValue* Value = Emitter->Input(Call->GetInput(i));
			if (Value)
			{
				const MIR::FType* Type = MIR::FType::FromMaterialValueType(FunctionInput.ExpressionInput->GetInputValueType(0));
				CallInputValues[i] = Emitter->Cast(Value, Type);
			}
		}

		// If some error occurred (e.g. some function input wasn't linked in) early out.
		if (Emitter->CurrentExpressionHasErrors())
		{
			return;
		}

		// Push a new analysis context on the stack dedicated to this function call.
		AnalysisContextStack.Emplace();
		FAnalysisContext& ParentContext = AnalysisContextStack[AnalysisContextStack.Num() - 2];
		FAnalysisContext& NewContext = AnalysisContextStack[AnalysisContextStack.Num() - 1];

		// Set the function call. When the expressions stack in this new context is empty, this
		// will be used to wire all values flowing inside the function outputs to the function call outputs.
		NewContext.Call = Call;

		// Forward values flowing into call inputs to called function inputs
		for (int i = 0; i < Call->FunctionInputs.Num(); ++i)
		{
			FFunctionExpressionInput& FunctionInput = Call->FunctionInputs[i];

			// Bind the value flowing into the function call input to the function input
			// expression (inside the function) in the new context.
			NewContext.SetOutputValue(FunctionInput.ExpressionInput->GetOutput(0), CallInputValues[i]);

			// Mark the function input as built.
			NewContext.BuiltExpressions.Add(FunctionInput.ExpressionInput.Get());
		}

		// Finally push the function outputs to the expression evaluation stack in the new context.
		for (FFunctionExpressionOutput& FunctionOutput : Call->FunctionOutputs)
		{
			NewContext.ExpressionStack.Push(FunctionOutput.ExpressionOutput.Get());
		}
	}

	void PopFunctionCall()
	{
		// Pull the values flowing into the function outputs out of the current
		// context and flow them into the Call outputs in the parent context so that
		// analysis can continue from the call expression.
		FAnalysisContext& ParentContext = AnalysisContextStack[AnalysisContextStack.Num() - 2];
		FAnalysisContext& CurrContext = AnalysisContextStack[AnalysisContextStack.Num() - 1];
		UMaterialExpressionMaterialFunctionCall* Call = CurrContext.Call;

		for (int i = 0; i < Call->FunctionOutputs.Num(); ++i)
		{
			FFunctionExpressionOutput& FunctionOutput = Call->FunctionOutputs[i];

			// Get the value flowing into the function output inside the function in the current context.
			MIR::FValue* Value = Emitter->Input(FunctionOutput.ExpressionOutput->GetInput(0));

			// Get the function output type.
			const MIR::FType* OutputType = MIR::FType::FromMaterialValueType(FunctionOutput.ExpressionOutput->GetOutputValueType(0));

			// Cast the value to the expected output type. This may fail (value will be poison). 
			Value = Emitter->Cast(Value, OutputType);

			// And flow it to the relative function *call* output in the parent context.
			ParentContext.SetOutputValue(Call->GetOutput(i), Value);
		}

		// Finally pop this context (the function call) to return to the caller.
		AnalysisContextStack.Pop();

		// Populate the insight information about this expression pins.
		AddExpressionConnectionInsights(Call);
	}

	void Step_FlowValuesIntoMaterialOutputs()
	{
		FAnalysisContext& Context = AnalysisContextStack.Last();

		for (uint32 StageIndex = 0; StageIndex < MIR::NumStages; ++StageIndex)
		{
			for (MIR::FSetMaterialOutput* Output : Module->Outputs[StageIndex])
			{
				FMaterialInputDescription Input;
				ensure(Builder->Material->GetExpressionInputDescription(Output->Property, Input));

				if (!Output->Arg)
				{
					MIR::FValue** ValuePtr = Context.OutputValues.Find(Input.Input->GetConnectedOutput());
					check(ValuePtr && *ValuePtr);

					MIR::Internal::BindValueToExpressionInput(this, Input.Input, *ValuePtr);

					const MIR::FType* OutputArgType = MIR::FType::FromShaderType(Input.Type);
					Output->Arg = Emitter->Cast(*ValuePtr, OutputArgType);
				}

				// Push this connection insight
				check(Output->Arg);
				PushConnectionInsight(Builder->Material, (int)Output->Property, Input.Input->Expression, Input.Input->OutputIndex, Output->Arg->Type);
			}
		}
	}

	void Step_AnalyzeIRGraph()
	{
		TArray<MIR::FValue*> ValueStack{};

		for (uint32 StageIndex = 0; StageIndex < MIR::NumStages; ++StageIndex)
		{
			MIR::EStage CurrentStage = (MIR::EStage)StageIndex;

			// Clear the value stack but preserve its allocated memory.
			ValueStack.Empty(ValueStack.Max());

			// Push each output in the current stage to the value stack.
			for (MIR::FSetMaterialOutput* Output : Module->Outputs[StageIndex])
			{
				ValueStack.Push(Output);
			}

			// Process until the value stack is empty.
			while (!ValueStack.IsEmpty())
			{
				MIR::FValue* Value = ValueStack.Last();
				
				// Module building should have interrupted before if poison values were generated.
				check(!Value->IsPoison());

				// If this instruction has already been analyzed for this stage, nothing else is left to do for it. Continue.
				if (Value->IsAnalyzed(CurrentStage))
				{
					ValueStack.Pop();
					continue;
				}

				// Before analyzing this value, make sure all used values are analyzed first.
				for (MIR::FValue* Use : Value->GetUsesForStage(CurrentStage))
				{
					if (Use && !Use->IsAnalyzed(CurrentStage))
					{
						ValueStack.Push(Use);
					}
				}

				// If any other value has been pushed to the stack, it means we have a dependency to analyze first.
				if (ValueStack.Last() != Value)
				{
					continue;
				}

				// All dependencies of this value has been analyzed, we can proceed analyzing this value now.
				ValueStack.Pop();

				// Go through each use instruction and increment its counter of users (this instruction).
				for (MIR::FValue* Use : Value->GetUsesForStage(CurrentStage))
				{
					// If this used value is an instruction, update its counter of users (in current stage).
					if (MIR::FInstruction* UseInstr = MIR::AsInstruction(Use))
					{
						UseInstr->NumUsers[StageIndex] += 1;
					}
				}

				// If this is the first time this value is analyzed, let the analyzer process it.
				// Note that individual value processing is independent from the stage it runs on so we can perform it only once.
				if ((Value->Flags & MIR::EValueFlags::AnalyzedInAnyStageMask) == MIR::EValueFlags::None)
				{
					ValueAnalyzer.Analyze(Value);
				}

				ValueAnalyzer.PropagateStateInStage(Value, CurrentStage);

				// Mark the used instruction as analyzed for this stage.
				Value->Flags |= MIR::EValueFlags(1 << StageIndex);
			}
		}
	}

	void Step_ConsolidateEnvironmentDefines()
	{
		// Keep defines if a combined condition is met. Otherwise, remove them from the environemnt defines set.
		auto KeepDefineConditionally = [this](FName Name, bool bConditionToKeepDefine) -> void
			{
				if (!bConditionToKeepDefine)
				{
					ValueAnalyzer.EnvironmentDefines.Remove(Name);
				}
			};

		KeepDefineConditionally(TEXT("USES_PER_INSTANCE_CUSTOM_DATA"), ValueAnalyzer.Material->bUsedWithInstancedStaticMeshes);
		KeepDefineConditionally(TEXT("NEEDS_PER_INSTANCE_RANDOM_PS"), ValueAnalyzer.Material->bUsedWithInstancedStaticMeshes);
		KeepDefineConditionally(TEXT("USES_PER_INSTANCE_FADE_AMOUNT"), ValueAnalyzer.Material->bUsedWithInstancedStaticMeshes);

		// Move final environemnt defines from analyzer into output module
		Module->EnvironmentDefines = MoveTemp(ValueAnalyzer.EnvironmentDefines);
	}

	void Step_AnalyzeBuiltinDefines()
	{
		// Match various defines against the material configuration
		if (Module->EnvironmentDefines.Contains(TEXT("MIR.SceneDepth")))
		{
			if (ValueAnalyzer.Material->MaterialDomain != MD_PostProcess && !IsTranslucentBlendMode(ValueAnalyzer.Material->BlendMode))
			{
				Module->AddError(nullptr, TEXT("Only transparent or postprocess materials can read from scene depth."));
			}
		}

		// Remove all environment defines that have the "MIR." prefix as they are not meant to propagate into the set of compiler environment defines.
		TCHAR DefineMIRPrefix[5] = {};
		for (auto Iter = Module->EnvironmentDefines.CreateIterator(); Iter; ++Iter)
		{
			Iter->ToStringTruncate(DefineMIRPrefix, UE_ARRAY_COUNT(DefineMIRPrefix));
			if (FCString::Strncmp(DefineMIRPrefix, TEXT("MIR."), UE_ARRAY_COUNT(DefineMIRPrefix)) == 0)
			{
				Iter.RemoveCurrent();
			}
		}
	}

	void Step_LinkInstructions()
	{
		TArray<MIR::FInstruction*> InstructionStack{};

		for (uint32 StageIndex = 0; StageIndex < MIR::NumStages; ++StageIndex)
		{
			// This function walks the instruction graph and puts each instruction into the inner most possible block.
			InstructionStack.Empty(InstructionStack.Max());

			// Initialize each output's block in this stage and add it to the instruction stack.
			for (MIR::FSetMaterialOutput* Output : Module->Outputs[StageIndex])
			{
				Output->Block[StageIndex] = Module->RootBlock[StageIndex];

				InstructionStack.Add(Output);
			}

			while (!InstructionStack.IsEmpty())
			{
				MIR::FInstruction* Instr = InstructionStack.Pop();

				// Push the instruction to its block in reverse order (push front)
				Instr->Next[StageIndex] = Instr->Block[StageIndex] ->Instructions;
				Instr->Block[StageIndex]->Instructions = Instr;

				TConstArrayView<MIR::FValue*> Uses = Instr->GetUsesForStage((MIR::EStage)StageIndex);
				for (int32 UseIndex = 0; UseIndex < Uses.Num(); ++UseIndex)
				{
					MIR::FValue* Use = Uses[UseIndex];
					MIR::FInstruction* UseInstr = MIR::AsInstruction(Use);
					if (!UseInstr)
					{
						continue;
					}

					// Get the block into which the dependency instruction should go.
					MIR::FBlock* TargetBlock = Instr->GetDesiredBlockForUse((MIR::EStage)StageIndex, UseIndex);

					// Update dependency's block to be a child of current instruction's block.
					if (TargetBlock != Instr->Block[StageIndex])
					{
						TargetBlock->Parent = Instr->Block[StageIndex];
						TargetBlock->Level = Instr->Block[StageIndex]->Level + 1;
					}

					// Set the dependency's block to the common block betwen its current block and this one.
					UseInstr->Block[StageIndex] = UseInstr->Block[StageIndex]
						? UseInstr->Block[StageIndex]->FindCommonParentWith(TargetBlock)
						: TargetBlock;

					// Increase the number of times this dependency instruction has been considered.
					// When all of its users have processed, we can carry on visiting this instruction.
					++UseInstr->NumProcessedUsers[StageIndex];
					check(UseInstr->NumProcessedUsers[StageIndex] <= UseInstr->NumUsers[StageIndex]);

					// If all dependants have been processed, we can carry the processing from this dependency.
					if (UseInstr->NumProcessedUsers[StageIndex] == UseInstr->NumUsers[StageIndex])
					{
						InstructionStack.Push(UseInstr);
					}
				}
			}
		}
	}

	void Step_Finalize()
	{
		FMaterialCompilationOutput& CompilationOutput = Module->CompilationOutput;
		CompilationOutput.NumUsedUVScalars = Module->Statistics.NumPixelTexCoords * 2;
	}
	
	/* Other functions */

	//
	void AddExpressionConnectionInsights(UMaterialExpression* Expression)
	{
		if (!Builder->TargetInsights)
		{
			return;
		}

		// Update expression inputs insight.
		for (FExpressionInputIterator It{ Expression }; It; ++It)
		{
			if (MIR::FValue* Value = MIR::Internal::FetchValueFromExpressionInput(this, It.Input))
			{
				PushConnectionInsight(Expression, It.Index, It->Expression, It->OutputIndex, Value->Type);
			}
		}
	}
	
	//
	void PushConnectionInsight(const UObject* InputObject, int InputIndex, const UMaterialExpression* OutputExpression, int OutputIndex, const MIR::FType* Type)
	{
		if (!Builder->TargetInsights || !Type || Type->IsPoison())
		{
			return;
		}

		FMaterialInsights::FConnectionInsight Insight {
			.InputObject = InputObject,
			.OutputExpression = OutputExpression,
			.InputIndex = InputIndex,
			.OutputIndex = OutputIndex,
			.ValueType = Type->ToValueType(),
		};
		
		Builder->TargetInsights->ConnectionInsights.Push(Insight);
	}
};

bool FMaterialIRModuleBuilder::Build(FMaterialIRModule* TargetModule)
{
	FMaterialIRModuleBuilderImpl Impl{ this, TargetModule };

	// Setup the emitter
	MIR::FEmitter Emitter;
	Emitter.BuilderImpl = &Impl;
	Emitter.Material = Material;
	Emitter.Module = TargetModule;
	Emitter.StaticParameterSet = &StaticParameters;

	// Setup the Builder implementation
	Impl.Emitter = &Emitter;
	Impl.ValueAnalyzer.Setup(Material, TargetModule, &TargetModule->CompilationOutput, TargetInsights);
	
	Impl.Step_Initialize();
	Impl.Step_GenerateOutputInstructions();
	Impl.Step_BuildMaterialExpressionsToIRGraph();

	if (!TargetModule->IsValid())
	{
		return false;
	}

	Impl.Step_FlowValuesIntoMaterialOutputs();
	Impl.Step_AnalyzeIRGraph();
	Impl.Step_ConsolidateEnvironmentDefines();
	Impl.Step_AnalyzeBuiltinDefines();
	Impl.Step_LinkInstructions();
	Impl.Step_Finalize();

	check(Material->MaterialInsight.IsValid());
	Material->MaterialInsight.Get()->IRString = MIR::DebugDumpIR(Material->GetFullName(), *TargetModule);

	// Dump debugging information if requested 
	switch (CVarMaterialIRDebugDumpLevel.GetValueOnGameThread())
	{
		case 2: MIR::DebugDumpIRUseGraph(*TargetModule); // fallthrough
		case 1:
		{
			// Save the dump to file
			FString FilePath = FPaths::Combine(FPaths::ProjectSavedDir(), "Materials", TEXT("IRDump.txt"));
			FFileHelper::SaveStringToFile(Material->MaterialInsight.Get()->IRString, *FilePath);
			// fallthrough
		}
	}

	return TargetModule->IsValid();
}

namespace MIR::Internal {

FValue* FetchValueFromExpressionInput(FMaterialIRModuleBuilderImpl* Builder, const FExpressionInput* Input)
{
	return Builder->AnalysisContextStack.Last().GetInputValue(Input);
}

void BindValueToExpressionInput(FMaterialIRModuleBuilderImpl* Builder, const FExpressionInput* Input, FValue* Value)
{
	Builder->AnalysisContextStack.Last().SetInputValue(Input, Value);
}

void BindValueToExpressionOutput(FMaterialIRModuleBuilderImpl* Builder, const FExpressionOutput* Output, FValue* Value)
{
	Builder->AnalysisContextStack.Last().SetOutputValue(Output, Value);
}

} // namespace MIR::Internal

#endif // #if WITH_EDITOR
