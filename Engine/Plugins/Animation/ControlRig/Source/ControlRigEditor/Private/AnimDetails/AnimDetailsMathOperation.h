// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "UObject/NameTypes.h"

namespace UE::ControlRigEditor
{
	/** Math operators for a control proxy value. */
	enum class EAnimDetailsMathOperator : uint8
	{
		None,
		Add,
		Substract,
		Multiply,
		Divide
	};

	/** Defines a mathematical operation by an operator and a RHS value */
	template <typename NumericType>
	struct FAnimDetailsMathOperation
	{
		FAnimDetailsMathOperation(EAnimDetailsMathOperator InMathOperator, NumericType InRHSValue)
			: MathOperator(InMathOperator)
			, RHSValue(InRHSValue)
		{}

		const EAnimDetailsMathOperator MathOperator;
		const NumericType RHSValue;
	};

	namespace FAnimDetailsMathParser
	{
		/** 
		 * Extracts a math operation from a string. The result is optional and only set if a valid math operation could be parsed.
		 *
		 * Leading valid LHS values are accepted, but not parsed.
		 * 
		 * Currently supported operations, whereas x is an arbitrary number of numeric type:
		 * Add: =+x or +=x
		 * Substract: =-x or -=x
		 * Multiply: *x or =*x or *=x
		 * Divide: /x or =/x or /=x
		 */
		template <typename NumericType>
		TOptional<FAnimDetailsMathOperation<NumericType>> FromString(const FString& InString)
		{
			// Map of operator names with their related math operator
			static const TMap<FName, EAnimDetailsMathOperator> MathOperatorNameToMathOperatorMap =
			{
				{ "+=", EAnimDetailsMathOperator::Add },
				{ "=+", EAnimDetailsMathOperator::Add },

				{ "-=", EAnimDetailsMathOperator::Substract },
				{ "=-", EAnimDetailsMathOperator::Substract },

				{ "*", EAnimDetailsMathOperator::Multiply },
				{ "=*", EAnimDetailsMathOperator::Multiply },
				{ "*=", EAnimDetailsMathOperator::Multiply },

				{ "/", EAnimDetailsMathOperator::Divide },
				{ "=/", EAnimDetailsMathOperator::Divide },
				{ "/=", EAnimDetailsMathOperator::Divide },
			};

			TArray<FString> NoWhiteSpaceArr;
			InString.ParseIntoArrayWS(NoWhiteSpaceArr);

			FString TrimmedString;
			for (const FString& NoWhiteSpaceString : NoWhiteSpaceArr)
			{
				TrimmedString += NoWhiteSpaceString;
			}

			if (TrimmedString.IsEmpty())
			{
				return TOptional<FAnimDetailsMathOperation<NumericType>>();
			}

			// Remove any valid LHS value
			for (const TTuple<FName, EAnimDetailsMathOperator>& MathOperatorNameToMathOperatorPair : MathOperatorNameToMathOperatorMap)
			{
				const int32 OperatorIndex = TrimmedString.Find(MathOperatorNameToMathOperatorPair.Key.ToString());
				
				if (OperatorIndex == INDEX_NONE)
				{
					// The string does not contain this operator.
					continue;
				}
				else if (OperatorIndex == 0)
				{
					// Starts with an operator, there's no LHS value.
					break;
				}
				else
				{
					// Try chop the LHS value
					const FString LeadingNumberString = TrimmedString.Left(OperatorIndex + 1);
					NumericType LeadingNumber;
					if (LexTryParseString(LeadingNumber, *LeadingNumberString))
					{
						TrimmedString = TrimmedString.RightChop(OperatorIndex);
						break;
					}
					else
					{
						// There is no LHS Value but another leading string
						return TOptional<FAnimDetailsMathOperation<NumericType>>();
					}
				}
			}

			int32 OperatorSize = -1;
			const EAnimDetailsMathOperator* OperatorPtr = nullptr;
			
			// Find single char operators, e.g. '*'
			OperatorPtr = MathOperatorNameToMathOperatorMap.Find(*TrimmedString.Left(1));
			if (OperatorPtr)
			{
				OperatorSize = 1;
			}
			else
			{
				// Find two char size math operators, e.g. "+="
				OperatorPtr = MathOperatorNameToMathOperatorMap.Find(*TrimmedString.Left(2));
				OperatorSize = 2;
			}

			if (!OperatorPtr)
			{
				return TOptional<FAnimDetailsMathOperation<NumericType>>();
			}

			// Remove the math operator from the string and get the RHS value
			const FString TrimmedRHSValueString = TrimmedString.RightChop(OperatorSize);

			NumericType RHSValue;
			if (LexTryParseString<NumericType>(RHSValue, *TrimmedRHSValueString))
			{
				return FAnimDetailsMathOperation<NumericType>(*OperatorPtr, RHSValue);
			}

			return TOptional<FAnimDetailsMathOperation<NumericType>>();
		}
	}
}
