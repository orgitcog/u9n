// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningFrameAttribute.h"

#if UE_LEARNING_ISPC
#include "Learning.ispc.generated.h"
#endif

namespace UE::Learning
{
	namespace FrameAttribute::Private
	{
		static inline bool FindMinimum(
			int32& OutChannelIdx,
			int32& OutFrameIdx,
			float& OutMinimumValue,
			const TLearningArrayView<2, const float> Values)
		{
			OutChannelIdx = INDEX_NONE;
			OutFrameIdx = INDEX_NONE;
			OutMinimumValue = UE_MAX_FLT;

			const int32 ChannelNum = Values.Num<0>();
			const int32 FrameNum = Values.Num<1>();

			for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
			{
				for (int32 FrameIdx = 0; FrameIdx < FrameNum; FrameIdx++)
				{
					if (Values[ChannelIdx][FrameIdx] < OutMinimumValue)
					{
						OutChannelIdx = ChannelIdx;
						OutFrameIdx = FrameIdx;
						OutMinimumValue = Values[ChannelIdx][FrameIdx];
					}
				}
			}

			return OutChannelIdx != INDEX_NONE;
		}
		
		static inline bool FindMaximum(
			int32& OutChannelIdx,
			int32& OutFrameIdx,
			float& OutMaximumValue,
			const TLearningArrayView<2, const float> Values)
		{
			OutChannelIdx = INDEX_NONE;
			OutFrameIdx = INDEX_NONE;
			OutMaximumValue = -UE_MAX_FLT;

			const int32 ChannelNum = Values.Num<0>();
			const int32 FrameNum = Values.Num<1>();

			for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
			{
				for (int32 FrameIdx = 0; FrameIdx < FrameNum; FrameIdx++)
				{
					if (Values[ChannelIdx][FrameIdx] > OutMaximumValue)
					{
						OutChannelIdx = ChannelIdx;
						OutFrameIdx = FrameIdx;
						OutMaximumValue = Values[ChannelIdx][FrameIdx];
					}
				}
			}

			return OutChannelIdx != INDEX_NONE;
		}

		static inline void Add(TLearningArrayView<1, float> Out, const TLearningArrayView<1, const float> Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			check(Out.Num() == Lhs.Num());
			check(Out.Num() == Rhs.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = Lhs[ValueIdx] + Rhs[ValueIdx];
			}
		}

		static inline void Sub(TLearningArrayView<1, float> Out, const TLearningArrayView<1, const float> Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			check(Out.Num() == Lhs.Num());
			check(Out.Num() == Rhs.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = Lhs[ValueIdx] - Rhs[ValueIdx];
			}
		}

		static inline void Mul(TLearningArrayView<1, float> Out, const TLearningArrayView<1, const float> Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			check(Out.Num() == Lhs.Num());
			check(Out.Num() == Rhs.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = Lhs[ValueIdx] * Rhs[ValueIdx];
			}
		}

		static inline void Div(TLearningArrayView<1, float> Out, const TLearningArrayView<1, const float> Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			check(Out.Num() == Lhs.Num());
			check(Out.Num() == Rhs.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = Lhs[ValueIdx] / Rhs[ValueIdx];
			}
		}

		static inline void Dot(TLearningArrayView<1, float> InOut, const TLearningArrayView<1, const float> Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			check(InOut.Num() == Lhs.Num());
			check(InOut.Num() == Rhs.Num());
			const int32 ValueNum = InOut.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				InOut[ValueIdx] += Lhs[ValueIdx] * Rhs[ValueIdx];
			}
		}

		static inline void Neg(TLearningArrayView<1, float> Out, const TLearningArrayView<1, const float> In)
		{
			check(Out.Num() == In.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = -In[ValueIdx];
			}
		}

		static inline void Inv(TLearningArrayView<1, float> Out, const TLearningArrayView<1, const float> In)
		{
			check(Out.Num() == In.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = 1.0f / In[ValueIdx];
			}
		}

		static inline void Abs(TLearningArrayView<1, float> Out, const TLearningArrayView<1, const float> In)
		{
			check(Out.Num() == In.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = FMath::Abs(In[ValueIdx]);
			}
		}

		static inline void Log(TLearningArrayView<1, float> Out, const TLearningArrayView<1, const float> In)
		{
			check(Out.Num() == In.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = FMath::Loge(In[ValueIdx]);
			}
		}

		static inline void Exp(TLearningArrayView<1, float> Out, const TLearningArrayView<1, const float> In)
		{
			check(Out.Num() == In.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = FMath::Exp(In[ValueIdx]);
			}
		}

		static inline void Sqrt(TLearningArrayView<1, float> Out, const TLearningArrayView<1, const float> In)
		{
			check(Out.Num() == In.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = FMath::Sqrt(In[ValueIdx]);
			}
		}

		static inline void LengthSquared(TLearningArrayView<1, float> InOut, const TLearningArrayView<1, const float> In)
		{
			check(InOut.Num() == In.Num());
			const int32 ValueNum = InOut.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				InOut[ValueIdx] += In[ValueIdx] * In[ValueIdx];
			}
		}

		static inline void SqrtInplace(TLearningArrayView<1, float> InOut)
		{
			const int32 ValueNum = InOut.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				InOut[ValueIdx] = FMath::Sqrt(InOut[ValueIdx]);
			}
		}

		static inline void Normalize(TLearningArrayView<2, float> Out, const TLearningArrayView<2, const float> In)
		{
			check(Out.Num<0>() == In.Num<0>());
			check(Out.Num<1>() == In.Num<1>());

			const int32 ChannelNum = In.Num<0>();
			const int32 FrameNum = In.Num<1>();
			for (int32 FrameIdx = 0; FrameIdx < FrameNum; FrameIdx++)
			{
				float LengthSquared = 0.0f;
				for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
				{
					LengthSquared += In[ChannelIdx][FrameIdx] * In[ChannelIdx][FrameIdx];
				}

				const float Length = FMath::Sqrt(LengthSquared);

				for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
				{
					Out[ChannelIdx][FrameIdx] = In[ChannelIdx][FrameIdx] / Length;
				}
			}
		}

		static inline void AddConstant(TLearningArrayView<1, float> Out, const TLearningArrayView<1, const float> Lhs, const float Rhs)
		{
			check(Out.Num() == Lhs.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = Lhs[ValueIdx] + Rhs;
			}
		}

		static inline void SubConstant(TLearningArrayView<1, float> Out, const TLearningArrayView<1, const float> Lhs, const float Rhs)
		{
			check(Out.Num() == Lhs.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = Lhs[ValueIdx] - Rhs;
			}
		}

		static inline void MulConstant(TLearningArrayView<1, float> Out, const TLearningArrayView<1, const float> Lhs, const float Rhs)
		{
			check(Out.Num() == Lhs.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = Lhs[ValueIdx] * Rhs;
			}
		}

		static inline void DivConstant(TLearningArrayView<1, float> Out, const TLearningArrayView<1, const float> Lhs, const float Rhs)
		{
			check(Out.Num() == Lhs.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = Lhs[ValueIdx] / Rhs;
			}
		}

		static inline void ConstantAdd(TLearningArrayView<1, float> Out, const float Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			check(Out.Num() == Rhs.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = Lhs + Rhs[ValueIdx];
			}
		}

		static inline void ConstantSub(TLearningArrayView<1, float> Out, const float Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			check(Out.Num() == Rhs.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = Lhs - Rhs[ValueIdx];
			}
		}

		static inline void ConstantMul(TLearningArrayView<1, float> Out, const float Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			check(Out.Num() == Rhs.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = Lhs * Rhs[ValueIdx];
			}
		}

		static inline void ConstantDiv(TLearningArrayView<1, float> Out, const float Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			check(Out.Num() == Rhs.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = Lhs / Rhs[ValueIdx];
			}
		}

		static inline void LogicalAnd(TLearningArrayView<1, float> Out, const TLearningArrayView<1, const float> Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			check(Out.Num() == Lhs.Num());
			check(Out.Num() == Rhs.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = Lhs[ValueIdx] && Rhs[ValueIdx];
			}
		}

		static inline void LogicalOr(TLearningArrayView<1, float> Out, const TLearningArrayView<1, const float> Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			check(Out.Num() == Lhs.Num());
			check(Out.Num() == Rhs.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = Lhs[ValueIdx] || Rhs[ValueIdx];
			}
		}

		static inline void LogicalNot(TLearningArrayView<1, float> Out, const TLearningArrayView<1, const float> In)
		{
			check(Out.Num() == In.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = !In[ValueIdx];
			}
		}

		static inline void Gt(TLearningArrayView<1, float> Out, const TLearningArrayView<1, const float> Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			check(Out.Num() == Lhs.Num());
			check(Out.Num() == Rhs.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = Lhs[ValueIdx] > Rhs[ValueIdx];
			}
		}

		static inline void Ge(TLearningArrayView<1, float> Out, const TLearningArrayView<1, const float> Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			check(Out.Num() == Lhs.Num());
			check(Out.Num() == Rhs.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = Lhs[ValueIdx] >= Rhs[ValueIdx];
			}
		}

		static inline void Lt(TLearningArrayView<1, float> Out, const TLearningArrayView<1, const float> Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			check(Out.Num() == Lhs.Num());
			check(Out.Num() == Rhs.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = Lhs[ValueIdx] < Rhs[ValueIdx];
			}
		}

		static inline void Le(TLearningArrayView<1, float> Out, const TLearningArrayView<1, const float> Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			check(Out.Num() == Lhs.Num());
			check(Out.Num() == Rhs.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = Lhs[ValueIdx] <= Rhs[ValueIdx];
			}
		}

		static inline void Eq(TLearningArrayView<1, float> Out, const TLearningArrayView<1, const float> Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			check(Out.Num() == Lhs.Num());
			check(Out.Num() == Rhs.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = Lhs[ValueIdx] == Rhs[ValueIdx];
			}
		}

		static inline void Neq(TLearningArrayView<1, float> Out, const TLearningArrayView<1, const float> Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			check(Out.Num() == Lhs.Num());
			check(Out.Num() == Rhs.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = Lhs[ValueIdx] != Rhs[ValueIdx];
			}
		}

		static inline void GtConstant(TLearningArrayView<1, float> Out, const TLearningArrayView<1, const float> Lhs, const float Rhs)
		{
			check(Out.Num() == Lhs.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = Lhs[ValueIdx] > Rhs;
			}
		}

		static inline void GeConstant(TLearningArrayView<1, float> Out, const TLearningArrayView<1, const float> Lhs, const float Rhs)
		{
			check(Out.Num() == Lhs.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = Lhs[ValueIdx] >= Rhs;
			}
		}

		static inline void LtConstant(TLearningArrayView<1, float> Out, const TLearningArrayView<1, const float> Lhs, const float Rhs)
		{
			check(Out.Num() == Lhs.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = Lhs[ValueIdx] < Rhs;
			}
		}

		static inline void LeConstant(TLearningArrayView<1, float> Out, const TLearningArrayView<1, const float> Lhs, const float Rhs)
		{
			check(Out.Num() == Lhs.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = Lhs[ValueIdx] <= Rhs;
			}
		}

		static inline void EqConstant(TLearningArrayView<1, float> Out, const TLearningArrayView<1, const float> Lhs, const float Rhs)
		{
			check(Out.Num() == Lhs.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = Lhs[ValueIdx] == Rhs;
			}
		}

		static inline void NeqConstant(TLearningArrayView<1, float> Out, const TLearningArrayView<1, const float> Lhs, const float Rhs)
		{
			check(Out.Num() == Lhs.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = Lhs[ValueIdx] != Rhs;
			}
		}

		static inline void ConstantGt(TLearningArrayView<1, float> Out, const float Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			check(Out.Num() == Rhs.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = Lhs > Rhs[ValueIdx];
			}
		}

		static inline void ConstantGe(TLearningArrayView<1, float> Out, const float Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			check(Out.Num() == Rhs.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = Lhs >= Rhs[ValueIdx];
			}
		}

		static inline void ConstantLt(TLearningArrayView<1, float> Out, const float Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			check(Out.Num() == Rhs.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = Lhs < Rhs[ValueIdx];
			}
		}

		static inline void ConstantLe(TLearningArrayView<1, float> Out, const float Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			check(Out.Num() == Rhs.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = Lhs < Rhs[ValueIdx];
			}
		}

		static inline void ConstantEq(TLearningArrayView<1, float> Out, const float Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			check(Out.Num() == Rhs.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = Lhs == Rhs[ValueIdx];
			}
		}

		static inline void ConstantNeq(TLearningArrayView<1, float> Out, const float Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			check(Out.Num() == Rhs.Num());
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				Out[ValueIdx] = Lhs != Rhs[ValueIdx];
			}
		}

		static inline void FilterGaussian(TLearningArrayView<1, float> Out, const TLearningArrayView<1, const float> In, const float StdInFrames)
		{
			check(Out.Num() == In.Num());
			check(StdInFrames >= 0.0f);

#if UE_LEARNING_ISPC

			ispc::LearningFilterGaussian(Out.GetData(), In.GetData(), In.Num(), StdInFrames);

#else
			const int32 StdRange = FMath::RoundToInt(StdInFrames * 3.0f);

			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				float Total = 0.0f;
				Out[ValueIdx] = 0.0f;

				const int32 RangeMin = FMath::Max(ValueIdx - StdRange, 0);
				const int32 RangeMax = FMath::Min(ValueIdx + StdRange, ValueNum - 1);

				for (int32 Offset = -StdRange; Offset <= +StdRange; Offset++)
				{
					if (ValueIdx + Offset >= 0 && ValueIdx + Offset < ValueNum)
					{
						const float Weight = FMath::InvExpApprox(FMath::Square(Offset / StdInFrames));
						Out[ValueIdx] += Weight * In[ValueIdx + Offset];
						Total += Weight;
					}
				}

				Out[ValueIdx] = Out[ValueIdx] / Total;
			}
#endif
		}

		static inline void FilterMajorityVote(TLearningArrayView<1, float> Out, const TLearningArrayView<1, const float> In, const int32 FilterWidthInFrames)
		{
			check(Out.Num() == In.Num());
			check(FilterWidthInFrames >= 0.0f);

#if UE_LEARNING_ISPC

			ispc::LearningFilterMajorityVote(Out.GetData(), In.GetData(), In.Num(), FilterWidthInFrames);

#else
			const int32 ValueNum = Out.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				int32 Total = 0;

				const int32 RangeMin = FMath::Max(ValueIdx - FilterWidthInFrames, 0);
				const int32 RangeMax = FMath::Min(ValueIdx + FilterWidthInFrames, ValueNum - 1);

				for (int32 Offset = -FilterWidthInFrames; Offset <= +FilterWidthInFrames; Offset++)
				{
					if (ValueIdx + Offset >= 0 && ValueIdx + Offset < ValueNum)
					{
						Total += In[ValueIdx + Offset] ? 1 : -1;
					}
				}

				Out[ValueIdx] = Total > 0;
			}
#endif
		}

		static inline void MeanStd(float& OutMean, float& OutStd, const TLearningArrayView<1, const float> In)
		{
			const int32 Num = In.Num();

			OutMean = 0.0f;
			OutStd = 0.0f;
			for (int32 Idx = 0; Idx < Num; Idx++)
			{
				OutStd += (((float)Idx / Num) / (Idx + 1)) * FMath::Square(In[Idx] - OutMean);
				OutMean += (In[Idx] - OutMean) / (Idx + 1);
			}
			OutStd = FMath::Sqrt(OutStd);
		}

		static inline void LogMeanStd(float& OutMean, float& OutLogStd, const TLearningArrayView<1, const float> In)
		{
			const int32 Num = In.Num();

			OutMean = 0.0f;
			OutLogStd = 0.0f;
			for (int32 Idx = 0; Idx < Num; Idx++)
			{
				const float Value = FMath::Loge(FMath::Max(In[Idx], UE_SMALL_NUMBER));
				OutLogStd += (((float)Idx / Num) / (Idx + 1)) * FMath::Square(Value - OutMean);
				OutMean += (Value - OutMean) / (Idx + 1);
			}
			OutMean = FMath::Exp(OutMean);
			OutLogStd = FMath::Sqrt(OutLogStd);
		}
		
		static inline void QuatInv(TLearningArrayView<2, float> Out, const TLearningArrayView<2, const float> In)
		{
			check(Out.Num<0>() == 4);
			check(In.Num<0>() == 4);
			check(Out.Num<1>() == In.Num<1>());

			const int32 ValueNum = Out.Num<1>();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				const FQuat4f InValue = FQuat4f(In[0][ValueIdx], In[1][ValueIdx], In[2][ValueIdx], In[3][ValueIdx]);
				const FQuat4f OutValue = InValue.Inverse();
				Out[0][ValueIdx] = OutValue.X;
				Out[1][ValueIdx] = OutValue.Y;
				Out[2][ValueIdx] = OutValue.Z;
				Out[3][ValueIdx] = OutValue.W;
			}
		}

		static inline void QuatAbs(TLearningArrayView<2, float> Out, const TLearningArrayView<2, const float> In)
		{
			check(Out.Num<0>() == 4);
			check(In.Num<0>() == 4);
			check(Out.Num<1>() == In.Num<1>());

			const int32 ValueNum = Out.Num<1>();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				const FQuat4f InValue = FQuat4f(In[0][ValueIdx], In[1][ValueIdx], In[2][ValueIdx], In[3][ValueIdx]);
				const FQuat4f OutValue = InValue.GetShortestArcWith(FQuat4f::Identity);
				Out[0][ValueIdx] = OutValue.X;
				Out[1][ValueIdx] = OutValue.Y;
				Out[2][ValueIdx] = OutValue.Z;
				Out[3][ValueIdx] = OutValue.W;
			}
		}

		static inline void QuatToRotationVector(TLearningArrayView<2, float> Out, const TLearningArrayView<2, const float> In)
		{
			check(Out.Num<0>() == 3);
			check(In.Num<0>() == 4);
			check(Out.Num<1>() == In.Num<1>());

			const int32 ValueNum = Out.Num<1>();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				const FQuat4f InValue = FQuat4f(In[0][ValueIdx], In[1][ValueIdx], In[2][ValueIdx], In[3][ValueIdx]);
				const FVector3f OutValue = InValue.ToRotationVector();
				Out[0][ValueIdx] = OutValue.X;
				Out[1][ValueIdx] = OutValue.Y;
				Out[2][ValueIdx] = OutValue.Z;
			}
		}

		static inline void QuatFromRotationVector(TLearningArrayView<2, float> Out, const TLearningArrayView<2, const float> In)
		{
			check(Out.Num<0>() == 4);
			check(In.Num<0>() == 3);
			check(Out.Num<1>() == In.Num<1>());

			const int32 ValueNum = Out.Num<1>();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				const FVector3f InValue = FVector3f(In[0][ValueIdx], In[1][ValueIdx], In[2][ValueIdx]);
				const FQuat4f OutValue = FQuat4f::MakeFromRotationVector(InValue);
				Out[0][ValueIdx] = OutValue.X;
				Out[1][ValueIdx] = OutValue.Y;
				Out[2][ValueIdx] = OutValue.Z;
				Out[3][ValueIdx] = OutValue.W;
			}
		}

		static inline void QuatMul(
			TLearningArrayView<1, float> OutX,
			TLearningArrayView<1, float> OutY,
			TLearningArrayView<1, float> OutZ,
			TLearningArrayView<1, float> OutW,
			const TLearningArrayView<1, const float> LhsX,
			const TLearningArrayView<1, const float> LhsY,
			const TLearningArrayView<1, const float> LhsZ,
			const TLearningArrayView<1, const float> LhsW,
			const TLearningArrayView<1, const float> RhsX,
			const TLearningArrayView<1, const float> RhsY,
			const TLearningArrayView<1, const float> RhsZ,
			const TLearningArrayView<1, const float> RhsW)
		{
			const int32 ValueNum = OutX.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				const FQuat4f Lhs = FQuat4f(LhsX[ValueIdx], LhsY[ValueIdx], LhsZ[ValueIdx], LhsW[ValueIdx]);
				const FQuat4f Rhs = FQuat4f(RhsX[ValueIdx], RhsY[ValueIdx], RhsZ[ValueIdx], RhsW[ValueIdx]);
				const FQuat4f Out = Lhs * Rhs;
				OutX[ValueIdx] = Out.X;
				OutY[ValueIdx] = Out.Y;
				OutZ[ValueIdx] = Out.Z;
				OutW[ValueIdx] = Out.W;
			}
		}

		static inline void QuatMulInv(
			TLearningArrayView<1, float> OutX,
			TLearningArrayView<1, float> OutY,
			TLearningArrayView<1, float> OutZ,
			TLearningArrayView<1, float> OutW,
			const TLearningArrayView<1, const float> LhsX,
			const TLearningArrayView<1, const float> LhsY,
			const TLearningArrayView<1, const float> LhsZ,
			const TLearningArrayView<1, const float> LhsW,
			const TLearningArrayView<1, const float> RhsX,
			const TLearningArrayView<1, const float> RhsY,
			const TLearningArrayView<1, const float> RhsZ,
			const TLearningArrayView<1, const float> RhsW)
		{
			const int32 ValueNum = OutX.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				const FQuat4f Lhs = FQuat4f(LhsX[ValueIdx], LhsY[ValueIdx], LhsZ[ValueIdx], LhsW[ValueIdx]);
				const FQuat4f Rhs = FQuat4f(RhsX[ValueIdx], RhsY[ValueIdx], RhsZ[ValueIdx], RhsW[ValueIdx]);
				const FQuat4f Out = Lhs * Rhs.Inverse();
				OutX[ValueIdx] = Out.X;
				OutY[ValueIdx] = Out.Y;
				OutZ[ValueIdx] = Out.Z;
				OutW[ValueIdx] = Out.W;
			}
		}

		static inline void QuatInvMul(
			TLearningArrayView<1, float> OutX,
			TLearningArrayView<1, float> OutY,
			TLearningArrayView<1, float> OutZ,
			TLearningArrayView<1, float> OutW,
			const TLearningArrayView<1, const float> LhsX,
			const TLearningArrayView<1, const float> LhsY,
			const TLearningArrayView<1, const float> LhsZ,
			const TLearningArrayView<1, const float> LhsW,
			const TLearningArrayView<1, const float> RhsX,
			const TLearningArrayView<1, const float> RhsY,
			const TLearningArrayView<1, const float> RhsZ,
			const TLearningArrayView<1, const float> RhsW)
		{
			const int32 ValueNum = OutX.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				const FQuat4f Lhs = FQuat4f(LhsX[ValueIdx], LhsY[ValueIdx], LhsZ[ValueIdx], LhsW[ValueIdx]);
				const FQuat4f Rhs = FQuat4f(RhsX[ValueIdx], RhsY[ValueIdx], RhsZ[ValueIdx], RhsW[ValueIdx]);
				const FQuat4f Out = Lhs.Inverse() * Rhs;
				OutX[ValueIdx] = Out.X;
				OutY[ValueIdx] = Out.Y;
				OutZ[ValueIdx] = Out.Z;
				OutW[ValueIdx] = Out.W;
			}
		}

		static inline void QuatRotate(
			TLearningArrayView<1, float> OutX,
			TLearningArrayView<1, float> OutY,
			TLearningArrayView<1, float> OutZ,
			const TLearningArrayView<1, const float> LhsX,
			const TLearningArrayView<1, const float> LhsY,
			const TLearningArrayView<1, const float> LhsZ,
			const TLearningArrayView<1, const float> LhsW,
			const TLearningArrayView<1, const float> RhsX,
			const TLearningArrayView<1, const float> RhsY,
			const TLearningArrayView<1, const float> RhsZ)
		{
			const int32 ValueNum = OutX.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				const FQuat4f Lhs = FQuat4f(LhsX[ValueIdx], LhsY[ValueIdx], LhsZ[ValueIdx], LhsW[ValueIdx]);
				const FVector3f Rhs = FVector3f(RhsX[ValueIdx], RhsY[ValueIdx], RhsZ[ValueIdx]);
				const FVector3f Out = Lhs.RotateVector(Rhs);
				OutX[ValueIdx] = Out.X;
				OutY[ValueIdx] = Out.Y;
				OutZ[ValueIdx] = Out.Z;
			}
		}

		static inline void QuatUnrotate(
			TLearningArrayView<1, float> OutX,
			TLearningArrayView<1, float> OutY,
			TLearningArrayView<1, float> OutZ,
			const TLearningArrayView<1, const float> LhsX,
			const TLearningArrayView<1, const float> LhsY,
			const TLearningArrayView<1, const float> LhsZ,
			const TLearningArrayView<1, const float> LhsW,
			const TLearningArrayView<1, const float> RhsX,
			const TLearningArrayView<1, const float> RhsY,
			const TLearningArrayView<1, const float> RhsZ)
		{
			const int32 ValueNum = OutX.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				const FQuat4f Lhs = FQuat4f(LhsX[ValueIdx], LhsY[ValueIdx], LhsZ[ValueIdx], LhsW[ValueIdx]);
				const FVector3f Rhs = FVector3f(RhsX[ValueIdx], RhsY[ValueIdx], RhsZ[ValueIdx]);
				const FVector3f Out = Lhs.UnrotateVector(Rhs);
				OutX[ValueIdx] = Out.X;
				OutY[ValueIdx] = Out.Y;
				OutZ[ValueIdx] = Out.Z;
			}
		}

		static inline void QuatBetween(
			TLearningArrayView<1, float> OutX,
			TLearningArrayView<1, float> OutY,
			TLearningArrayView<1, float> OutZ,
			TLearningArrayView<1, float> OutW,
			const TLearningArrayView<1, const float> LhsX,
			const TLearningArrayView<1, const float> LhsY,
			const TLearningArrayView<1, const float> LhsZ,
			const TLearningArrayView<1, const float> RhsX,
			const TLearningArrayView<1, const float> RhsY,
			const TLearningArrayView<1, const float> RhsZ)
		{
			const int32 ValueNum = OutX.Num();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				const FVector3f Lhs = FVector3f(LhsX[ValueIdx], LhsY[ValueIdx], LhsZ[ValueIdx]);
				const FVector3f Rhs = FVector3f(RhsX[ValueIdx], RhsY[ValueIdx], RhsZ[ValueIdx]);
				const FQuat4f Out = FQuat4f::FindBetween(Lhs, Rhs);
				OutX[ValueIdx] = Out.X;
				OutY[ValueIdx] = Out.Y;
				OutZ[ValueIdx] = Out.Z;
				OutW[ValueIdx] = Out.W;
			}
		}

		static inline void QuatMulConstant(TLearningArrayView<2, float> Out, const TLearningArrayView<2, const float> Lhs, const FQuat4f Rhs)
		{
			check(Out.Num<0>() == 4);
			check(Lhs.Num<0>() == 4);
			check(Out.Num<1>() == Lhs.Num<1>());

			const int32 ValueNum = Out.Num<1>();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				const FQuat4f LhsValue = FQuat4f(Lhs[0][ValueIdx], Lhs[1][ValueIdx], Lhs[2][ValueIdx], Lhs[3][ValueIdx]);
				const FQuat4f OutValue = LhsValue * Rhs;
				Out[0][ValueIdx] = OutValue.X;
				Out[1][ValueIdx] = OutValue.Y;
				Out[2][ValueIdx] = OutValue.Z;
				Out[3][ValueIdx] = OutValue.W;
			}
		}

		static inline void QuatInvMulConstant(TLearningArrayView<2, float> Out, const TLearningArrayView<2, const float> Lhs, const FQuat4f Rhs)
		{
			check(Out.Num<0>() == 4);
			check(Lhs.Num<0>() == 4);
			check(Out.Num<1>() == Lhs.Num<1>());

			const int32 ValueNum = Out.Num<1>();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				const FQuat4f LhsValue = FQuat4f(Lhs[0][ValueIdx], Lhs[1][ValueIdx], Lhs[2][ValueIdx], Lhs[3][ValueIdx]);
				const FQuat4f OutValue = LhsValue.Inverse() * Rhs;
				Out[0][ValueIdx] = OutValue.X;
				Out[1][ValueIdx] = OutValue.Y;
				Out[2][ValueIdx] = OutValue.Z;
				Out[3][ValueIdx] = OutValue.W;
			}
		}

		static inline void QuatMulInvConstant(TLearningArrayView<2, float> Out, const TLearningArrayView<2, const float> Lhs, const FQuat4f Rhs)
		{
			check(Out.Num<0>() == 4);
			check(Lhs.Num<0>() == 4);
			check(Out.Num<1>() == Lhs.Num<1>());

			const int32 ValueNum = Out.Num<1>();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				const FQuat4f LhsValue = FQuat4f(Lhs[0][ValueIdx], Lhs[1][ValueIdx], Lhs[2][ValueIdx], Lhs[3][ValueIdx]);
				const FQuat4f OutValue = LhsValue * Rhs.Inverse();
				Out[0][ValueIdx] = OutValue.X;
				Out[1][ValueIdx] = OutValue.Y;
				Out[2][ValueIdx] = OutValue.Z;
				Out[3][ValueIdx] = OutValue.W;
			}
		}

		static inline void QuatRotateConstant(TLearningArrayView<2, float> Out, const TLearningArrayView<2, const float> Lhs, const FVector3f Rhs)
		{
			check(Out.Num<0>() == 3);
			check(Lhs.Num<0>() == 4);
			check(Out.Num<1>() == Lhs.Num<1>());

			const int32 ValueNum = Out.Num<1>();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				const FQuat4f LhsValue = FQuat4f(Lhs[0][ValueIdx], Lhs[1][ValueIdx], Lhs[2][ValueIdx], Lhs[3][ValueIdx]);
				const FVector3f OutValue = LhsValue.RotateVector(Rhs);
				Out[0][ValueIdx] = OutValue.X;
				Out[1][ValueIdx] = OutValue.Y;
				Out[2][ValueIdx] = OutValue.Z;
			}
		}

		static inline void QuatUnrotateConstant(TLearningArrayView<2, float> Out, const TLearningArrayView<2, const float> Lhs, const FVector3f Rhs)
		{
			check(Out.Num<0>() == 3);
			check(Lhs.Num<0>() == 4);
			check(Out.Num<1>() == Lhs.Num<1>());

			const int32 ValueNum = Out.Num<1>();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				const FQuat4f LhsValue = FQuat4f(Lhs[0][ValueIdx], Lhs[1][ValueIdx], Lhs[2][ValueIdx], Lhs[3][ValueIdx]);
				const FVector3f OutValue = LhsValue.RotateVector(Rhs);
				Out[0][ValueIdx] = OutValue.X;
				Out[1][ValueIdx] = OutValue.Y;
				Out[2][ValueIdx] = OutValue.Z;
			}
		}

		static inline void QuatBetweenConstant(TLearningArrayView<2, float> Out, const TLearningArrayView<2, const float> Lhs, const FVector3f Rhs)
		{
			check(Out.Num<0>() == 4);
			check(Lhs.Num<0>() == 3);
			check(Out.Num<1>() == Lhs.Num<1>());

			const int32 ValueNum = Out.Num<1>();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				const FVector3f LhsValue = FVector3f(Lhs[0][ValueIdx], Lhs[1][ValueIdx], Lhs[2][ValueIdx]);
				const FQuat4f OutValue = FQuat4f::FindBetween(LhsValue, Rhs);
				Out[0][ValueIdx] = OutValue.X;
				Out[1][ValueIdx] = OutValue.Y;
				Out[2][ValueIdx] = OutValue.Z;
				Out[3][ValueIdx] = OutValue.W;
			}
		}

		static inline void QuatConstantMul(TLearningArrayView<2, float> Out, const FQuat4f Lhs, const TLearningArrayView<2, const float> Rhs)
		{
			check(Out.Num<0>() == 4);
			check(Rhs.Num<0>() == 4);
			check(Out.Num<1>() == Rhs.Num<1>());

			const int32 ValueNum = Out.Num<1>();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				const FQuat4f RhsValue = FQuat4f(Rhs[0][ValueIdx], Rhs[1][ValueIdx], Rhs[2][ValueIdx], Rhs[3][ValueIdx]);
				const FQuat4f OutValue = Lhs * RhsValue;
				Out[0][ValueIdx] = OutValue.X;
				Out[1][ValueIdx] = OutValue.Y;
				Out[2][ValueIdx] = OutValue.Z;
				Out[3][ValueIdx] = OutValue.W;
			}
		}

		static inline void QuatConstantInvMul(TLearningArrayView<2, float> Out, const FQuat4f Lhs, const TLearningArrayView<2, const float> Rhs)
		{
			check(Out.Num<0>() == 4);
			check(Rhs.Num<0>() == 4);
			check(Out.Num<1>() == Rhs.Num<1>());

			const int32 ValueNum = Out.Num<1>();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				const FQuat4f RhsValue = FQuat4f(Rhs[0][ValueIdx], Rhs[1][ValueIdx], Rhs[2][ValueIdx], Rhs[3][ValueIdx]);
				const FQuat4f OutValue = Lhs.Inverse() * RhsValue;
				Out[0][ValueIdx] = OutValue.X;
				Out[1][ValueIdx] = OutValue.Y;
				Out[2][ValueIdx] = OutValue.Z;
				Out[3][ValueIdx] = OutValue.W;
			}
		}

		static inline void QuatConstantMulInv(TLearningArrayView<2, float> Out, const FQuat4f Lhs, const TLearningArrayView<2, const float> Rhs)
		{
			check(Out.Num<0>() == 4);
			check(Rhs.Num<0>() == 4);
			check(Out.Num<1>() == Rhs.Num<1>());

			const int32 ValueNum = Out.Num<1>();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				const FQuat4f RhsValue = FQuat4f(Rhs[0][ValueIdx], Rhs[1][ValueIdx], Rhs[2][ValueIdx], Rhs[3][ValueIdx]);
				const FQuat4f OutValue = Lhs * RhsValue.Inverse();
				Out[0][ValueIdx] = OutValue.X;
				Out[1][ValueIdx] = OutValue.Y;
				Out[2][ValueIdx] = OutValue.Z;
				Out[3][ValueIdx] = OutValue.W;
			}
		}

		static inline void QuatConstantRotate(TLearningArrayView<2, float> Out, const FQuat4f Lhs, const TLearningArrayView<2, const float> Rhs)
		{
			check(Out.Num<0>() == 3);
			check(Rhs.Num<0>() == 3);
			check(Out.Num<1>() == Rhs.Num<1>());

			const int32 ValueNum = Out.Num<1>();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				const FVector3f RhsValue = FVector3f(Rhs[0][ValueIdx], Rhs[1][ValueIdx], Rhs[2][ValueIdx]);
				const FVector3f OutValue = Lhs.RotateVector(RhsValue);
				Out[0][ValueIdx] = OutValue.X;
				Out[1][ValueIdx] = OutValue.Y;
				Out[2][ValueIdx] = OutValue.Z;
			}
		}

		static inline void QuatConstantUnrotate(TLearningArrayView<2, float> Out, const FQuat4f Lhs, const TLearningArrayView<2, const float> Rhs)
		{
			check(Out.Num<0>() == 3);
			check(Rhs.Num<0>() == 3);
			check(Out.Num<1>() == Rhs.Num<1>());

			const int32 ValueNum = Out.Num<1>();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				const FVector3f RhsValue = FVector3f(Rhs[0][ValueIdx], Rhs[1][ValueIdx], Rhs[2][ValueIdx]);
				const FVector3f OutValue = Lhs.UnrotateVector(RhsValue);
				Out[0][ValueIdx] = OutValue.X;
				Out[1][ValueIdx] = OutValue.Y;
				Out[2][ValueIdx] = OutValue.Z;
			}
		}

		static inline void QuatConstantBetween(TLearningArrayView<2, float> Out, const FVector3f Lhs, const TLearningArrayView<2, const float> Rhs)
		{
			check(Out.Num<0>() == 4);
			check(Rhs.Num<0>() == 3);
			check(Out.Num<1>() == Rhs.Num<1>());

			const int32 ValueNum = Out.Num<1>();
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				const FVector3f RhsValue = FVector3f(Rhs[0][ValueIdx], Rhs[1][ValueIdx], Rhs[2][ValueIdx]);
				const FQuat4f OutValue = FQuat4f::FindBetween(Lhs, RhsValue);
				Out[0][ValueIdx] = OutValue.X;
				Out[1][ValueIdx] = OutValue.Y;
				Out[2][ValueIdx] = OutValue.Z;
				Out[3][ValueIdx] = OutValue.W;
			}
		}

		// We need these custom Vector4 functions because the defaults for the FVector4f class will treat it like a Vector3

		static inline float Vector4Length(const FVector4f X)
		{
			return FMath::Sqrt(X.X * X.X + X.Y * X.Y + X.Z * X.Z + X.W * X.W);
		}

		static inline FVector4f Vector4Normalize(const FVector4f X)
		{
			return X / Vector4Length(X);
		}

		static inline FVector4f DominantEigenVector(
			const FMatrix44f& A,
			const FVector4f V0,
			const int32 Iterations,
			const float Epsilon)
		{
			// Initial Guess at Eigen Vector & Value
			FVector4f V = V0;
			float EV = (A.TransformFVector4(V) / V).X;

			for (int32 Iteration = 0; Iteration < Iterations; Iteration++)
			{
				// Power Iteration
				const FVector4f Av = A.TransformFVector4(V);

				// Next Guess at Eigen Vector & Value
				const FVector4f VNew = Vector4Normalize(Av);
				const float EVNew = (A.TransformFVector4(VNew) / VNew).X;

				// Break if converged
				if (FMath::Abs(EV - EVNew) < Epsilon)
				{
					break;
				}

				// Update best guess
				V = VNew;
				EV = EVNew;
			}

			return V;
		}

		static inline void QuatMeanStd(FQuat4f& OutMean, FVector3f& OutStd, const TLearningArrayView<2, const float> In)
		{
			check(In.Num<0>() == 4);

			const int32 Num = In.Num<1>();

			FMatrix44f Accum;
			FPlatformMemory::Memzero(&Accum, sizeof(FMatrix44f));

			for (int64 Idx = 0; Idx < Num; Idx++)
			{
				const FQuat4f Q = FQuat4f(In[0][Idx], In[1][Idx], In[2][Idx], In[3][Idx]);

				Accum.M[0][0] += ((Q.X * Q.X) - Accum.M[0][0]) / (Idx + 1);
				Accum.M[0][1] += ((Q.X * Q.Y) - Accum.M[0][1]) / (Idx + 1);
				Accum.M[0][2] += ((Q.X * Q.Z) - Accum.M[0][2]) / (Idx + 1);
				Accum.M[0][3] += ((Q.X * Q.W) - Accum.M[0][3]) / (Idx + 1);

				Accum.M[1][0] += ((Q.Y * Q.X) - Accum.M[1][0]) / (Idx + 1);
				Accum.M[1][1] += ((Q.Y * Q.Y) - Accum.M[1][1]) / (Idx + 1);
				Accum.M[1][2] += ((Q.Y * Q.Z) - Accum.M[1][2]) / (Idx + 1);
				Accum.M[1][3] += ((Q.Y * Q.W) - Accum.M[1][3]) / (Idx + 1);

				Accum.M[2][0] += ((Q.Z * Q.X) - Accum.M[2][0]) / (Idx + 1);
				Accum.M[2][1] += ((Q.Z * Q.Y) - Accum.M[2][1]) / (Idx + 1);
				Accum.M[2][2] += ((Q.Z * Q.Z) - Accum.M[2][2]) / (Idx + 1);
				Accum.M[2][3] += ((Q.Z * Q.W) - Accum.M[2][3]) / (Idx + 1);

				Accum.M[3][0] += ((Q.W * Q.X) - Accum.M[3][0]) / (Idx + 1);
				Accum.M[3][1] += ((Q.W * Q.Y) - Accum.M[3][1]) / (Idx + 1);
				Accum.M[3][2] += ((Q.W * Q.Z) - Accum.M[3][2]) / (Idx + 1);
				Accum.M[3][3] += ((Q.W * Q.W) - Accum.M[3][3]) / (Idx + 1);
			}

			const FVector4f AverageQuat = DominantEigenVector(Accum, FVector4f(0.0f, 0.0f, 0.0f, 1.0f), 128, 0.0f);
			OutMean = FQuat4f(AverageQuat.X, AverageQuat.Y, AverageQuat.Z, AverageQuat.W);
			check(OutMean.IsNormalized());

			OutStd = FVector3f::ZeroVector;
			for (int64 Idx = 0; Idx < Num; Idx++)
			{
				const FQuat4f Q = FQuat4f(In[0][Idx], In[1][Idx], In[2][Idx], In[3][Idx]);
				const FQuat4f Diff = (Q * OutMean.Inverse()).GetShortestArcWith(FQuat4f::Identity);
				OutStd += FMath::Square(Diff.ToRotationVector()) / Num;
			}
			OutStd.X = FMath::Sqrt(OutStd.X);
			OutStd.Y = FMath::Sqrt(OutStd.Y);
			OutStd.Z = FMath::Sqrt(OutStd.Z);
		}

	}

	void FFrameAttribute::Check() const
	{
		FrameRangeSet.Check();
		check(AttributeData.Num<1>() == FrameRangeSet.GetTotalFrameNum());
	}

	void FFrameAttribute::Empty() { FrameRangeSet.Empty(); AttributeData.Empty(); }
	bool FFrameAttribute::IsEmpty() const { return FrameRangeSet.IsEmpty(); }

	const FFrameRangeSet& FFrameAttribute::GetFrameRangeSet() const { return FrameRangeSet; }
	int32 FFrameAttribute::GetTotalFrameNum() const { return AttributeData.Num<1>(); }
	int32 FFrameAttribute::GetTotalRangeNum() const { return FrameRangeSet.GetTotalRangeNum(); }
	int32 FFrameAttribute::GetChannelNum() const { return AttributeData.Num<0>(); }

	TLearningArrayView<2, const float> FFrameAttribute::GetAttributeData() const { return AttributeData; }

	TLearningArrayView<1, const float> FFrameAttribute::GetChannelAttributeData(const int32 ChannelIdx) const { return AttributeData[ChannelIdx]; }

	const float& FFrameAttribute::GetChannelAttributeDataAtFrame(const int32 ChannelIdx, const int32 RangeFrameIdx) const { return AttributeData[ChannelIdx][RangeFrameIdx]; }

	TLearningArrayView<1, const float> FFrameAttribute::GetChannelEntryRangeAttributeData(const int32 ChannelIdx, const int32 EntryIdx, const int32 RangeIdx) const
	{
		return AttributeData[ChannelIdx].Slice(FrameRangeSet.GetEntryRangeOffset(EntryIdx, RangeIdx), FrameRangeSet.GetEntryRangeLength(EntryIdx, RangeIdx));
	}

	TLearningArrayView<1, const float> FFrameAttribute::GetChannelRangeAttributeData(const int32 ChannelIdx, const int32 RangeOffset, const int32 RangeLength) const
	{
		return AttributeData[ChannelIdx].Slice(RangeOffset, RangeLength);
	}

	TLearningArrayView<2, float> FFrameAttribute::GetAttributeData() { return AttributeData; }

	TLearningArrayView<1, float> FFrameAttribute::GetChannelAttributeData(const int32 ChannelIdx) { return AttributeData[ChannelIdx]; }

	float& FFrameAttribute::GetChannelAttributeDataAtFrame(const int32 ChannelIdx, const int32 RangeFrameIdx) { return AttributeData[ChannelIdx][RangeFrameIdx]; }

	TLearningArrayView<1, float> FFrameAttribute::GetChannelEntryRangeAttributeData(const int32 ChannelIdx, const int32 EntryIdx, const int32 RangeIdx)
	{
		return AttributeData[ChannelIdx].Slice(FrameRangeSet.GetEntryRangeOffset(EntryIdx, RangeIdx), FrameRangeSet.GetEntryRangeLength(EntryIdx, RangeIdx));
	}

	TLearningArrayView<1, float> FFrameAttribute::GetChannelRangeAttributeData(const int32 ChannelIdx, const int32 RangeOffset, const int32 RangeLength)
	{
		return AttributeData[ChannelIdx].Slice(RangeOffset, RangeLength);
	}

	namespace FrameAttribute
	{
		void Intersection(FFrameAttribute& OutFrameAttribute, const FFrameAttribute& FrameAttribute, const FFrameRangeSet& FrameRangeSet)
		{
			if (FrameRangeSet::Equal(FrameAttribute.FrameRangeSet, FrameRangeSet))
			{
				OutFrameAttribute = FrameAttribute;
				return;
			}

			// Perform Intersection and get offsets
			TLearningArray<1, int32> LhsOffsets;
			TLearningArray<1, int32> RhsOffsets;
			LhsOffsets.SetNumUninitialized({ FrameAttribute.FrameRangeSet.GetTotalRangeNum() + FrameRangeSet.GetTotalRangeNum() });
			RhsOffsets.SetNumUninitialized({ FrameAttribute.FrameRangeSet.GetTotalRangeNum() + FrameRangeSet.GetTotalRangeNum() });

			const int32 OutTotalRangeNum = UE::Learning::FrameRangeSet::IntersectionWithOffsets(
				OutFrameAttribute.FrameRangeSet,
				LhsOffsets,
				RhsOffsets,
				FrameAttribute.FrameRangeSet,
				FrameRangeSet);

			// Resize back to correct size
			LhsOffsets.SetNumUninitialized({ OutTotalRangeNum });
			RhsOffsets.SetNumUninitialized({ OutTotalRangeNum });

			const int32 ChannelNum = FrameAttribute.GetChannelNum();
			const int32 TotalFrameNum = OutFrameAttribute.FrameRangeSet.GetTotalFrameNum();

			OutFrameAttribute.AttributeData.SetNumUninitialized({ ChannelNum, TotalFrameNum });

			for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
			{
				for (int32 RangeIdx = 0; RangeIdx < OutTotalRangeNum; RangeIdx++)
				{
					const int32 OutOffset = OutFrameAttribute.FrameRangeSet.GetAllRangeOffsets()[RangeIdx];
					const int32 LhsOffset = LhsOffsets[RangeIdx];
					const int32 Length = OutFrameAttribute.FrameRangeSet.GetAllRangeLengths()[RangeIdx];

					Array::Copy(
						OutFrameAttribute.GetChannelRangeAttributeData(ChannelIdx, OutOffset, Length),
						FrameAttribute.GetChannelRangeAttributeData(ChannelIdx, LhsOffset, Length));
				}
			}
		}

		void NonZeroFrameRangeSet(FFrameRangeSet& OutFrameRangeSet, const FFrameAttribute& FrameAttribute, const int32 ChannelIdx)
		{
			check(ChannelIdx >= 0 && ChannelIdx < FrameAttribute.GetChannelNum());

			const int32 EntryNum = FrameAttribute.FrameRangeSet.GetEntryNum();

			OutFrameRangeSet.Empty();

			TArray<int32> AddedRangeStarts;
			TArray<int32> AddedRangeLengths;

			int32 RangeOffset = 0;
			for (int32 EntryIdx = 0; EntryIdx < EntryNum; EntryIdx++)
			{
				const int32 RangeNum = FrameAttribute.FrameRangeSet.GetEntryRangeNum(EntryIdx);
				const int32 Sequence = FrameAttribute.FrameRangeSet.GetEntrySequence(EntryIdx);

				AddedRangeStarts.Reset();
				AddedRangeLengths.Reset();

				for (int32 RangeIdx = 0; RangeIdx < RangeNum; RangeIdx++)
				{
					const int32 FrameNum = FrameAttribute.FrameRangeSet.GetEntryRangeLength(EntryIdx, RangeIdx);
					const int32 StartFrame = FrameAttribute.FrameRangeSet.GetEntryRangeStart(EntryIdx, RangeIdx);
					const int32 FrameOffset = FrameAttribute.FrameRangeSet.GetEntryRangeOffset(EntryIdx, RangeIdx);

					int32 StartFrameIndex = INDEX_NONE;

					for (int32 FrameIdx = 0; FrameIdx < FrameNum; FrameIdx++)
					{
						const float FrameValue = FrameAttribute.GetChannelAttributeDataAtFrame(ChannelIdx, FrameOffset + FrameIdx);

						if (StartFrameIndex == INDEX_NONE && FrameValue == 1.0f)
						{
							StartFrameIndex = FrameIdx;
						}
						else if (StartFrameIndex == INDEX_NONE && FrameValue == 0.0f) {}
						else if (StartFrameIndex != INDEX_NONE && FrameValue == 1.0f) {}
						else if (StartFrameIndex != INDEX_NONE && FrameValue == 0.0f)
						{
							check(FrameIdx - StartFrameIndex > 0);
							AddedRangeStarts.Add(StartFrame + StartFrameIndex);
							AddedRangeLengths.Add(StartFrame + FrameIdx - StartFrameIndex);
							StartFrameIndex = INDEX_NONE;
						}
					}

					if (StartFrameIndex != INDEX_NONE)
					{
						check(FrameNum - StartFrameIndex > 0);
						AddedRangeStarts.Add(StartFrame + StartFrameIndex);
						AddedRangeLengths.Add(StartFrame + FrameNum - StartFrameIndex);
					}
				}

				OutFrameRangeSet.AddEntry(Sequence, AddedRangeStarts, AddedRangeLengths);
			}
		}

		void ReduceOp(
			const FFrameAttribute& In,
			const ReduceOpFunction Op)
		{
			Op(In, In.FrameRangeSet.GetAllRangeOffsets(), In.FrameRangeSet.GetAllRangeLengths());
		}

		void NullaryOp(
			FFrameAttribute& Out,
			const int32 OutChannelNum,
			const FFrameRangeSet& FrameRangeSet,
			const NullaryOpFunction Op)
		{
			Out.FrameRangeSet = FrameRangeSet;
			Out.AttributeData.SetNumUninitialized({ OutChannelNum, FrameRangeSet.GetTotalFrameNum() });

			Op(Out, Out.FrameRangeSet.GetAllRangeOffsets(), Out.FrameRangeSet.GetAllRangeLengths());
		}

		void UnaryOp(
			FFrameAttribute& Out,
			const int32 OutChannelNum,
			const FFrameAttribute& In,
			const UnaryOpFunction Op)
		{
			Out.FrameRangeSet = In.FrameRangeSet;
			Out.AttributeData.SetNumUninitialized({ OutChannelNum, In.FrameRangeSet.GetTotalFrameNum() });

			Op(Out, In, Out.FrameRangeSet.GetAllRangeOffsets(), Out.FrameRangeSet.GetAllRangeLengths());
		}

		void BinaryOp(
			FFrameAttribute& Out,
			const int32 OutChannelNum,
			const FFrameAttribute& Lhs,
			const FFrameAttribute& Rhs,
			const BinaryOpFunction Op)
		{
			// Fast Path for when FrameRangeSets are Equal

			if (FrameRangeSet::Equal(Lhs.FrameRangeSet, Rhs.FrameRangeSet))
			{
				Out.FrameRangeSet = Lhs.FrameRangeSet;
				Out.AttributeData.SetNumUninitialized({ OutChannelNum, Lhs.GetTotalFrameNum() });

				Op(Out, Lhs, Rhs, 
					Out.FrameRangeSet.GetAllRangeOffsets(),
					Lhs.FrameRangeSet.GetAllRangeOffsets(),
					Rhs.FrameRangeSet.GetAllRangeOffsets(),
					Out.FrameRangeSet.GetAllRangeLengths());

				return;
			}

			// Slow Path for when FrameRangeSets are not equal and we need to compute the intersection

			TLearningArray<1, int32> LhsRangeOffsets;
			TLearningArray<1, int32> RhsRangeOffsets;
			LhsRangeOffsets.SetNumUninitialized({ Lhs.FrameRangeSet.GetTotalRangeNum() + Rhs.FrameRangeSet.GetTotalRangeNum() });
			RhsRangeOffsets.SetNumUninitialized({ Lhs.FrameRangeSet.GetTotalRangeNum() + Rhs.FrameRangeSet.GetTotalRangeNum() });

			const int32 OutTotalRangeNum = UE::Learning::FrameRangeSet::IntersectionWithOffsets(
				Out.FrameRangeSet,
				LhsRangeOffsets,
				RhsRangeOffsets,
				Lhs.FrameRangeSet,
				Rhs.FrameRangeSet);

			// Resize back to correct size
			LhsRangeOffsets.SetNumUninitialized({ OutTotalRangeNum });
			RhsRangeOffsets.SetNumUninitialized({ OutTotalRangeNum });

			// Allocate Attribute Data
			Out.AttributeData.SetNumUninitialized({ OutChannelNum, Out.FrameRangeSet.GetTotalFrameNum() });

			Op(Out, Lhs, Rhs,
				Out.FrameRangeSet.GetAllRangeOffsets(),
				LhsRangeOffsets,
				RhsRangeOffsets,
				Out.FrameRangeSet.GetAllRangeLengths());
		}

		void NaryOp(
			FFrameAttribute& Out,
			const int32 OutChannelNum,
			const TArrayView<const ConstFrameAttributePtr> Inputs,
			const NaryOpFunction Op)
		{
			if (Inputs.Num() == 0)
			{
				Out.Empty();
				Op(Out, {}, {}, {}, {});
			}

			// Check All Equal

			const int32 InputNum = Inputs.Num();

			bool bAllEqual = true;
			for (int32 InputIdx = 1; InputIdx < InputNum; InputIdx++)
			{
				bAllEqual &= FrameRangeSet::Equal(Inputs[0]->FrameRangeSet, Inputs[InputIdx]->FrameRangeSet);
			}

			if (bAllEqual)
			{
				Out.FrameRangeSet = Inputs[0]->FrameRangeSet;
				Out.AttributeData.SetNumUninitialized({ OutChannelNum, Inputs[0]->FrameRangeSet.GetTotalFrameNum() });

				TArray<TLearningArrayView<1, const int32>, TInlineAllocator<8>> InputRangeOffsetsViews;
				InputRangeOffsetsViews.SetNumUninitialized(InputNum);
				for (int32 InputIdx = 0; InputIdx < InputNum; InputIdx++)
				{
					InputRangeOffsetsViews[InputIdx] = Inputs[InputIdx]->FrameRangeSet.GetAllRangeOffsets();
				}

				Op(Out, Inputs,
					Out.FrameRangeSet.GetAllRangeOffsets(),
					InputRangeOffsetsViews,
					Out.FrameRangeSet.GetAllRangeLengths());
			}
			else
			{
				// Currently we don't support the case where things are not equal.

				checkNoEntry();
				Out.Empty();
				return;
			}
		}

		bool FindMinimum(
			int32& OutChannelIdx,
			int32& OutFrameIdx,
			float& OutMinimumValue,
			const FFrameAttribute& In)
		{
			return Private::FindMinimum(OutChannelIdx, OutFrameIdx, OutMinimumValue, In.GetAttributeData());
		}

		bool FindMaximum(
			int32& OutChannelIdx,
			int32& OutFrameIdx,
			float& OutMaximumValue,
			const FFrameAttribute& In)
		{
			return Private::FindMaximum(OutChannelIdx, OutFrameIdx, OutMaximumValue, In.GetAttributeData());
		}

		void Zeros(FFrameAttribute& Out, const FFrameRangeSet& FrameRangeSet, const int32 ChannelNum)
		{
			Out.FrameRangeSet = FrameRangeSet;
			Out.AttributeData.SetNumUninitialized({ ChannelNum, FrameRangeSet.GetTotalFrameNum() });
			Array::Zero(Out.AttributeData);
		}

		void Ones(FFrameAttribute& Out, const FFrameRangeSet& FrameRangeSet, const int32 ChannelNum)
		{
			Out.FrameRangeSet = FrameRangeSet;
			Out.AttributeData.SetNumUninitialized({ ChannelNum, FrameRangeSet.GetTotalFrameNum() });
			Array::Set(Out.AttributeData, 1.0f);
		}

		void Fill(FFrameAttribute& Out, const FFrameRangeSet& FrameRangeSet, const TLearningArrayView<1, const float> Values)
		{
			const int32 ChannelNum = Values.Num();

			Out.FrameRangeSet = FrameRangeSet;
			Out.AttributeData.SetNumUninitialized({ ChannelNum, FrameRangeSet.GetTotalFrameNum() });
			for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
			{
				Array::Set(Out.AttributeData[ChannelIdx], Values[ChannelIdx]);
			}
		}

		void Add(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs)
		{
			check(Lhs.GetChannelNum() == Rhs.GetChannelNum());

			BinaryOp(Out, Lhs.GetChannelNum(), Lhs, Rhs, [](
				FFrameAttribute& Out,
				const FFrameAttribute& Lhs,
				const FFrameAttribute& Rhs,
				const TLearningArrayView<1, const int32> OutRangeOffsets,
				const TLearningArrayView<1, const int32> LhsRangeOffsets,
				const TLearningArrayView<1, const int32> RhsRangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();
					const int32 RangeNum = RangeLengths.Num();
					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						for (int32 RangeIdx = 0; RangeIdx < RangeNum; RangeIdx++)
						{
							Private::Add(
								Out.GetChannelRangeAttributeData(ChannelIdx, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
								Lhs.GetChannelRangeAttributeData(ChannelIdx, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
								Rhs.GetChannelRangeAttributeData(ChannelIdx, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]));
						}
					}
				});
		}

		void Sub(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs)
		{
			check(Lhs.GetChannelNum() == Rhs.GetChannelNum());

			BinaryOp(Out, Lhs.GetChannelNum(), Lhs, Rhs, [](
				FFrameAttribute& Out,
				const FFrameAttribute& Lhs,
				const FFrameAttribute& Rhs,
				const TLearningArrayView<1, const int32> OutRangeOffsets,
				const TLearningArrayView<1, const int32> LhsRangeOffsets,
				const TLearningArrayView<1, const int32> RhsRangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();
					const int32 RangeNum = RangeLengths.Num();
					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						for (int32 RangeIdx = 0; RangeIdx < RangeNum; RangeIdx++)
						{
							Private::Sub(
								Out.GetChannelRangeAttributeData(ChannelIdx, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
								Lhs.GetChannelRangeAttributeData(ChannelIdx, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
								Rhs.GetChannelRangeAttributeData(ChannelIdx, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]));
						}
					}
				});
		}

		void Mul(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs)
		{
			check(Lhs.GetChannelNum() == Rhs.GetChannelNum());

			BinaryOp(Out, Lhs.GetChannelNum(), Lhs, Rhs, [](
				FFrameAttribute& Out,
				const FFrameAttribute& Lhs,
				const FFrameAttribute& Rhs,
				const TLearningArrayView<1, const int32> OutRangeOffsets,
				const TLearningArrayView<1, const int32> LhsRangeOffsets,
				const TLearningArrayView<1, const int32> RhsRangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();
					const int32 RangeNum = RangeLengths.Num();
					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						for (int32 RangeIdx = 0; RangeIdx < RangeNum; RangeIdx++)
						{
							Private::Mul(
								Out.GetChannelRangeAttributeData(ChannelIdx, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
								Lhs.GetChannelRangeAttributeData(ChannelIdx, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
								Rhs.GetChannelRangeAttributeData(ChannelIdx, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]));
						}
					}
				});
		}

		void Div(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs)
		{
			check(Lhs.GetChannelNum() == Rhs.GetChannelNum());

			BinaryOp(Out, Lhs.GetChannelNum(), Lhs, Rhs, [](
				FFrameAttribute& Out,
				const FFrameAttribute& Lhs,
				const FFrameAttribute& Rhs,
				const TLearningArrayView<1, const int32> OutRangeOffsets,
				const TLearningArrayView<1, const int32> LhsRangeOffsets,
				const TLearningArrayView<1, const int32> RhsRangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();
					const int32 RangeNum = RangeLengths.Num();
					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						for (int32 RangeIdx = 0; RangeIdx < RangeNum; RangeIdx++)
						{
							Private::Div(
								Out.GetChannelRangeAttributeData(ChannelIdx, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
								Lhs.GetChannelRangeAttributeData(ChannelIdx, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
								Rhs.GetChannelRangeAttributeData(ChannelIdx, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]));
						}
					}
				});
		}

		void Dot(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs)
		{
			check(Lhs.GetChannelNum() == Rhs.GetChannelNum());

			BinaryOp(Out, 1, Lhs, Rhs, [](
				FFrameAttribute& Out,
				const FFrameAttribute& Lhs,
				const FFrameAttribute& Rhs,
				const TLearningArrayView<1, const int32> OutRangeOffsets,
				const TLearningArrayView<1, const int32> LhsRangeOffsets,
				const TLearningArrayView<1, const int32> RhsRangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					Array::Zero(Out.GetAttributeData());

					const int32 ChannelNum = Out.GetChannelNum();
					const int32 RangeNum = RangeLengths.Num();

					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						for (int32 RangeIdx = 0; RangeIdx < RangeNum; RangeIdx++)
						{
							Private::Dot(
								Out.GetChannelRangeAttributeData(0, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
								Lhs.GetChannelRangeAttributeData(ChannelIdx, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
								Rhs.GetChannelRangeAttributeData(ChannelIdx, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]));
						}
					}
				});
		}

		void Neg(FFrameAttribute& Out, const FFrameAttribute& In)
		{
			UnaryOp(Out, In.GetChannelNum(), In, [](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();

					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						Private::Neg(
							Out.GetChannelAttributeData(ChannelIdx),
							In.GetChannelAttributeData(ChannelIdx));
					}
				});
		}

		void Inv(FFrameAttribute& Out, const FFrameAttribute& In)
		{
			UnaryOp(Out, In.GetChannelNum(), In, [](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();

					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						Private::Inv(
							Out.GetChannelAttributeData(ChannelIdx),
							In.GetChannelAttributeData(ChannelIdx));
					}
				});
		}

		void Abs(FFrameAttribute& Out, const FFrameAttribute& In)
		{
			UnaryOp(Out, In.GetChannelNum(), In, [](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();

					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						Private::Abs(
							Out.GetChannelAttributeData(ChannelIdx),
							In.GetChannelAttributeData(ChannelIdx));
					}
				});
		}

		void Log(FFrameAttribute& Out, const FFrameAttribute& In)
		{
			UnaryOp(Out, In.GetChannelNum(), In, [](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();

					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						Private::Log(
							Out.GetChannelAttributeData(ChannelIdx),
							In.GetChannelAttributeData(ChannelIdx));
					}
				});
		}

		void Exp(FFrameAttribute& Out, const FFrameAttribute& In)
		{
			UnaryOp(Out, In.GetChannelNum(), In, [](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();

					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						Private::Exp(
							Out.GetChannelAttributeData(ChannelIdx),
							In.GetChannelAttributeData(ChannelIdx));
					}
				});
		}

		void Length(FFrameAttribute& Out, const FFrameAttribute& In)
		{
			UnaryOp(Out, 1, In, [](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();

					Array::Zero(Out.GetAttributeData());

					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						Private::LengthSquared(
							Out.GetChannelAttributeData(0),
							In.GetChannelAttributeData(ChannelIdx));
					}

					Private::SqrtInplace(Out.GetChannelAttributeData(0));
				});
		}

		void Normalize(FFrameAttribute& Out, const FFrameAttribute& In)
		{
			UnaryOp(Out, In.GetChannelNum(), In, [](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					Private::Normalize(Out.GetAttributeData(), In.GetAttributeData());

				});
		}

		void Index(FFrameAttribute& Out, const FFrameAttribute& In, const int32 ChannelIdx)
		{
			check(ChannelIdx >= 0);
			check(ChannelIdx < In.GetChannelNum());

			UnaryOp(Out, 1, In, [ChannelIdx](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					Array::Copy(Out.GetChannelAttributeData(0), In.GetChannelAttributeData(ChannelIdx));

				});
		}

		void AddConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			UnaryOp(Out, Lhs.GetChannelNum(), Lhs, [Rhs](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();

					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						Private::AddConstant(
							Out.GetChannelAttributeData(ChannelIdx),
							In.GetChannelAttributeData(ChannelIdx),
							Rhs[ChannelIdx]);
					}
				});
		}

		void SubConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			UnaryOp(Out, Lhs.GetChannelNum(), Lhs, [Rhs](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();

					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						Private::SubConstant(
							Out.GetChannelAttributeData(ChannelIdx),
							In.GetChannelAttributeData(ChannelIdx),
							Rhs[ChannelIdx]);
					}
				});
		}

		void MulConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			UnaryOp(Out, Lhs.GetChannelNum(), Lhs, [Rhs](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();

					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						Private::MulConstant(
							Out.GetChannelAttributeData(ChannelIdx),
							In.GetChannelAttributeData(ChannelIdx),
							Rhs[ChannelIdx]);
					}
				});
		}

		void DivConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			UnaryOp(Out, Lhs.GetChannelNum(), Lhs, [Rhs](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();

					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						Private::DivConstant(
							Out.GetChannelAttributeData(ChannelIdx),
							In.GetChannelAttributeData(ChannelIdx),
							Rhs[ChannelIdx]);
					}
				});
		}

		void ConstantAdd(FFrameAttribute& Out, const TLearningArrayView<1, const float> Lhs, const FFrameAttribute& Rhs)
		{
			UnaryOp(Out, Rhs.GetChannelNum(), Rhs, [Lhs](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();

					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						Private::ConstantAdd(
							Out.GetChannelAttributeData(ChannelIdx),
							Lhs[ChannelIdx],
							In.GetChannelAttributeData(ChannelIdx));
					}
				});
		}

		void ConstantSub(FFrameAttribute& Out, const TLearningArrayView<1, const float> Lhs, const FFrameAttribute& Rhs)
		{
			UnaryOp(Out, Rhs.GetChannelNum(), Rhs, [Lhs](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();

					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						Private::ConstantSub(
							Out.GetChannelAttributeData(ChannelIdx),
							Lhs[ChannelIdx],
							In.GetChannelAttributeData(ChannelIdx));
					}
				});
		}

		void ConstantMul(FFrameAttribute& Out, const TLearningArrayView<1, const float> Lhs, const FFrameAttribute& Rhs)
		{
			UnaryOp(Out, Rhs.GetChannelNum(), Rhs, [Lhs](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();

					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						Private::ConstantMul(
							Out.GetChannelAttributeData(ChannelIdx),
							Lhs[ChannelIdx],
							In.GetChannelAttributeData(ChannelIdx));
					}
				});
		}

		void ConstantDiv(FFrameAttribute& Out, const TLearningArrayView<1, const float> Lhs, const FFrameAttribute& Rhs)
		{
			UnaryOp(Out, Rhs.GetChannelNum(), Rhs, [Lhs](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();

					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						Private::ConstantDiv(
							Out.GetChannelAttributeData(ChannelIdx),
							Lhs[ChannelIdx],
							In.GetChannelAttributeData(ChannelIdx));
					}
				});
		}

		void Sum(FFrameAttribute& Out, const TArrayView<const ConstFrameAttributePtr> Inputs)
		{
			const int32 InputNum = Inputs.Num();

			if (InputNum == 0)
			{
				Out.Empty();
			}
			else if (InputNum == 1)
			{
				Out = *Inputs[0];
			}
			else 
			{
				FFrameAttribute Tmp = *Inputs[0];

				for (int32 InputIdx = 1; InputIdx < InputNum; InputIdx++)
				{
					Add(Out, Tmp, *Inputs[InputIdx]);
					Tmp = Out;
				}
			}
		}

		void Prod(FFrameAttribute& Out, const TArrayView<const ConstFrameAttributePtr> Inputs)
		{
			const int32 InputNum = Inputs.Num();

			if (InputNum == 0)
			{
				Out.Empty();
			}
			else if (InputNum == 1)
			{
				Out = *Inputs[0];
			}
			else
			{
				FFrameAttribute Tmp = *Inputs[0];

				for (int32 InputIdx = 1; InputIdx < InputNum; InputIdx++)
				{
					Mul(Out, Tmp, *Inputs[InputIdx]);
					Tmp = Out;
				}
			}
		}

		void LogicalAnd(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs)
		{
			check(Lhs.GetChannelNum() == Rhs.GetChannelNum());

			BinaryOp(Out, Lhs.GetChannelNum(), Lhs, Rhs, [](
				FFrameAttribute& Out,
				const FFrameAttribute& Lhs,
				const FFrameAttribute& Rhs,
				const TLearningArrayView<1, const int32> OutRangeOffsets,
				const TLearningArrayView<1, const int32> LhsRangeOffsets,
				const TLearningArrayView<1, const int32> RhsRangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();
					const int32 RangeNum = RangeLengths.Num();
					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						for (int32 RangeIdx = 0; RangeIdx < RangeNum; RangeIdx++)
						{
							Private::LogicalAnd(
								Out.GetChannelRangeAttributeData(ChannelIdx, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
								Lhs.GetChannelRangeAttributeData(ChannelIdx, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
								Rhs.GetChannelRangeAttributeData(ChannelIdx, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]));
						}
					}
				});
		}

		void LogicalOr(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs)
		{
			check(Lhs.GetChannelNum() == Rhs.GetChannelNum());

			BinaryOp(Out, Lhs.GetChannelNum(), Lhs, Rhs, [](
				FFrameAttribute& Out,
				const FFrameAttribute& Lhs,
				const FFrameAttribute& Rhs,
				const TLearningArrayView<1, const int32> OutRangeOffsets,
				const TLearningArrayView<1, const int32> LhsRangeOffsets,
				const TLearningArrayView<1, const int32> RhsRangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();
					const int32 RangeNum = RangeLengths.Num();
					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						for (int32 RangeIdx = 0; RangeIdx < RangeNum; RangeIdx++)
						{
							Private::LogicalOr(
								Out.GetChannelRangeAttributeData(ChannelIdx, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
								Lhs.GetChannelRangeAttributeData(ChannelIdx, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
								Rhs.GetChannelRangeAttributeData(ChannelIdx, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]));
						}
					}
				});
		}

		void LogicalNot(FFrameAttribute& Out, const FFrameAttribute& In)
		{
			UnaryOp(Out, In.GetChannelNum(), In, [](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();

					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						Private::LogicalNot(
							Out.GetChannelAttributeData(ChannelIdx),
							In.GetChannelAttributeData(ChannelIdx));
					}
				});
		}

		void Gt(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs)
		{
			check(Lhs.GetChannelNum() == Rhs.GetChannelNum());

			BinaryOp(Out, Lhs.GetChannelNum(), Lhs, Rhs, [](
				FFrameAttribute& Out,
				const FFrameAttribute& Lhs,
				const FFrameAttribute& Rhs,
				const TLearningArrayView<1, const int32> OutRangeOffsets,
				const TLearningArrayView<1, const int32> LhsRangeOffsets,
				const TLearningArrayView<1, const int32> RhsRangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();
					const int32 RangeNum = RangeLengths.Num();
					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						for (int32 RangeIdx = 0; RangeIdx < RangeNum; RangeIdx++)
						{
							Private::Gt(
								Out.GetChannelRangeAttributeData(ChannelIdx, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
								Lhs.GetChannelRangeAttributeData(ChannelIdx, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
								Rhs.GetChannelRangeAttributeData(ChannelIdx, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]));
						}
					}
				});
		}

		void Ge(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs)
		{
			check(Lhs.GetChannelNum() == Rhs.GetChannelNum());

			BinaryOp(Out, Lhs.GetChannelNum(), Lhs, Rhs, [](
				FFrameAttribute& Out,
				const FFrameAttribute& Lhs,
				const FFrameAttribute& Rhs,
				const TLearningArrayView<1, const int32> OutRangeOffsets,
				const TLearningArrayView<1, const int32> LhsRangeOffsets,
				const TLearningArrayView<1, const int32> RhsRangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();
					const int32 RangeNum = RangeLengths.Num();
					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						for (int32 RangeIdx = 0; RangeIdx < RangeNum; RangeIdx++)
						{
							Private::Ge(
								Out.GetChannelRangeAttributeData(ChannelIdx, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
								Lhs.GetChannelRangeAttributeData(ChannelIdx, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
								Rhs.GetChannelRangeAttributeData(ChannelIdx, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]));
						}
					}
				});
		}

		void Lt(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs)
		{
			check(Lhs.GetChannelNum() == Rhs.GetChannelNum());

			BinaryOp(Out, Lhs.GetChannelNum(), Lhs, Rhs, [](
				FFrameAttribute& Out,
				const FFrameAttribute& Lhs,
				const FFrameAttribute& Rhs,
				const TLearningArrayView<1, const int32> OutRangeOffsets,
				const TLearningArrayView<1, const int32> LhsRangeOffsets,
				const TLearningArrayView<1, const int32> RhsRangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();
					const int32 RangeNum = RangeLengths.Num();
					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						for (int32 RangeIdx = 0; RangeIdx < RangeNum; RangeIdx++)
						{
							Private::Lt(
								Out.GetChannelRangeAttributeData(ChannelIdx, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
								Lhs.GetChannelRangeAttributeData(ChannelIdx, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
								Rhs.GetChannelRangeAttributeData(ChannelIdx, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]));
						}
					}
				});
		}

		void Le(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs)
		{
			check(Lhs.GetChannelNum() == Rhs.GetChannelNum());

			BinaryOp(Out, Lhs.GetChannelNum(), Lhs, Rhs, [](
				FFrameAttribute& Out,
				const FFrameAttribute& Lhs,
				const FFrameAttribute& Rhs,
				const TLearningArrayView<1, const int32> OutRangeOffsets,
				const TLearningArrayView<1, const int32> LhsRangeOffsets,
				const TLearningArrayView<1, const int32> RhsRangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();
					const int32 RangeNum = RangeLengths.Num();
					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						for (int32 RangeIdx = 0; RangeIdx < RangeNum; RangeIdx++)
						{
							Private::Le(
								Out.GetChannelRangeAttributeData(ChannelIdx, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
								Lhs.GetChannelRangeAttributeData(ChannelIdx, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
								Rhs.GetChannelRangeAttributeData(ChannelIdx, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]));
						}
					}
				});
		}

		void Eq(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs)
		{
			check(Lhs.GetChannelNum() == Rhs.GetChannelNum());

			BinaryOp(Out, Lhs.GetChannelNum(), Lhs, Rhs, [](
				FFrameAttribute& Out,
				const FFrameAttribute& Lhs,
				const FFrameAttribute& Rhs,
				const TLearningArrayView<1, const int32> OutRangeOffsets,
				const TLearningArrayView<1, const int32> LhsRangeOffsets,
				const TLearningArrayView<1, const int32> RhsRangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();
					const int32 RangeNum = RangeLengths.Num();
					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						for (int32 RangeIdx = 0; RangeIdx < RangeNum; RangeIdx++)
						{
							Private::Eq(
								Out.GetChannelRangeAttributeData(ChannelIdx, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
								Lhs.GetChannelRangeAttributeData(ChannelIdx, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
								Rhs.GetChannelRangeAttributeData(ChannelIdx, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]));
						}
					}
				});
		}

		void Neq(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs)
		{
			check(Lhs.GetChannelNum() == Rhs.GetChannelNum());

			BinaryOp(Out, Lhs.GetChannelNum(), Lhs, Rhs, [](
				FFrameAttribute& Out,
				const FFrameAttribute& Lhs,
				const FFrameAttribute& Rhs,
				const TLearningArrayView<1, const int32> OutRangeOffsets,
				const TLearningArrayView<1, const int32> LhsRangeOffsets,
				const TLearningArrayView<1, const int32> RhsRangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();
					const int32 RangeNum = RangeLengths.Num();
					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						for (int32 RangeIdx = 0; RangeIdx < RangeNum; RangeIdx++)
						{
							Private::Neq(
								Out.GetChannelRangeAttributeData(ChannelIdx, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
								Lhs.GetChannelRangeAttributeData(ChannelIdx, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
								Rhs.GetChannelRangeAttributeData(ChannelIdx, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]));
						}
					}
				});
		}

		void GtConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			UnaryOp(Out, Lhs.GetChannelNum(), Lhs, [Rhs](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();

					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						Private::GtConstant(
							Out.GetChannelAttributeData(ChannelIdx),
							In.GetChannelAttributeData(ChannelIdx),
							Rhs[ChannelIdx]);
					}
				});
		}

		void GeConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			UnaryOp(Out, Lhs.GetChannelNum(), Lhs, [Rhs](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();

					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						Private::GeConstant(
							Out.GetChannelAttributeData(ChannelIdx),
							In.GetChannelAttributeData(ChannelIdx),
							Rhs[ChannelIdx]);
					}
				});
		}

		void LtConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			UnaryOp(Out, Lhs.GetChannelNum(), Lhs, [Rhs](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();

					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						Private::LtConstant(
							Out.GetChannelAttributeData(ChannelIdx),
							In.GetChannelAttributeData(ChannelIdx),
							Rhs[ChannelIdx]);
					}
				});
		}

		void LeConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			UnaryOp(Out, Lhs.GetChannelNum(), Lhs, [Rhs](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();

					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						Private::LeConstant(
							Out.GetChannelAttributeData(ChannelIdx),
							In.GetChannelAttributeData(ChannelIdx),
							Rhs[ChannelIdx]);
					}
				});
		}

		void EqConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			UnaryOp(Out, Lhs.GetChannelNum(), Lhs, [Rhs](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();

					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						Private::EqConstant(
							Out.GetChannelAttributeData(ChannelIdx),
							In.GetChannelAttributeData(ChannelIdx),
							Rhs[ChannelIdx]);
					}
				});
		}

		void NeqConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const TLearningArrayView<1, const float> Rhs)
		{
			UnaryOp(Out, Lhs.GetChannelNum(), Lhs, [Rhs](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();

					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						Private::NeqConstant(
							Out.GetChannelAttributeData(ChannelIdx),
							In.GetChannelAttributeData(ChannelIdx),
							Rhs[ChannelIdx]);
					}
				});
		}

		void ConstantGt(FFrameAttribute& Out, const TLearningArrayView<1, const float> Lhs, const FFrameAttribute& Rhs)
		{
			UnaryOp(Out, Rhs.GetChannelNum(), Rhs, [Lhs](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();

					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						Private::ConstantGt(
							Out.GetChannelAttributeData(ChannelIdx),
							Lhs[ChannelIdx],
							In.GetChannelAttributeData(ChannelIdx));
					}
				});
		}

		void ConstantGe(FFrameAttribute& Out, const TLearningArrayView<1, const float> Lhs, const FFrameAttribute& Rhs)
		{
			UnaryOp(Out, Rhs.GetChannelNum(), Rhs, [Lhs](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();

					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						Private::ConstantGe(
							Out.GetChannelAttributeData(ChannelIdx),
							Lhs[ChannelIdx],
							In.GetChannelAttributeData(ChannelIdx));
					}
				});
		}

		void ConstantLt(FFrameAttribute& Out, const TLearningArrayView<1, const float> Lhs, const FFrameAttribute& Rhs)
		{
			UnaryOp(Out, Rhs.GetChannelNum(), Rhs, [Lhs](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();

					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						Private::ConstantLt(
							Out.GetChannelAttributeData(ChannelIdx),
							Lhs[ChannelIdx],
							In.GetChannelAttributeData(ChannelIdx));
					}
				});
		}

		void ConstantLe(FFrameAttribute& Out, const TLearningArrayView<1, const float> Lhs, const FFrameAttribute& Rhs)
		{
			UnaryOp(Out, Rhs.GetChannelNum(), Rhs, [Lhs](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();

					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						Private::ConstantLe(
							Out.GetChannelAttributeData(ChannelIdx),
							Lhs[ChannelIdx],
							In.GetChannelAttributeData(ChannelIdx));
					}
				});
		}

		void ConstantEq(FFrameAttribute& Out, const TLearningArrayView<1, const float> Lhs, const FFrameAttribute& Rhs)
		{
			UnaryOp(Out, Rhs.GetChannelNum(), Rhs, [Lhs](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();

					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						Private::ConstantEq(
							Out.GetChannelAttributeData(ChannelIdx),
							Lhs[ChannelIdx],
							In.GetChannelAttributeData(ChannelIdx));
					}
				});
		}

		void ConstantNeq(FFrameAttribute& Out, const TLearningArrayView<1, const float> Lhs, const FFrameAttribute& Rhs)
		{
			UnaryOp(Out, Rhs.GetChannelNum(), Rhs, [Lhs](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();

					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						Private::ConstantNeq(
							Out.GetChannelAttributeData(ChannelIdx),
							Lhs[ChannelIdx],
							In.GetChannelAttributeData(ChannelIdx));
					}
				});
		}

		void FilterGaussian(FFrameAttribute& Out, const FFrameAttribute& In, const float StdInFrames)
		{
			UnaryOp(Out, In.GetChannelNum(), In, [StdInFrames](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();
					const int32 RangeNum = RangeLengths.Num();
					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						for (int32 RangeIdx = 0; RangeIdx < RangeNum; RangeIdx++)
						{
							Private::FilterGaussian(
								Out.GetChannelRangeAttributeData(ChannelIdx, RangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
								In.GetChannelRangeAttributeData(ChannelIdx, RangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
								StdInFrames);
						}
					}
				});
		}

		void FilterMajorityVote(FFrameAttribute& Out, const FFrameAttribute& In, const int32 FilterWidthFrames)
		{
			UnaryOp(Out, In.GetChannelNum(), In, [FilterWidthFrames](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = Out.GetChannelNum();
					const int32 RangeNum = RangeLengths.Num();
					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						for (int32 RangeIdx = 0; RangeIdx < RangeNum; RangeIdx++)
						{
							Private::FilterMajorityVote(
								Out.GetChannelRangeAttributeData(ChannelIdx, RangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
								In.GetChannelRangeAttributeData(ChannelIdx, RangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
								FilterWidthFrames);
						}
					}
				});
		}

		void MeanStd(TLearningArrayView<1, float> OutMean, TLearningArrayView<1, float> OutStd, const FFrameAttribute& In)
		{
			ReduceOp(In, [&OutMean, &OutStd](
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = In.GetChannelNum();

					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						Private::MeanStd(OutMean[ChannelIdx], OutStd[ChannelIdx], In.GetChannelAttributeData(ChannelIdx));
					}
				});
		}

		void LogMeanStd(TLearningArrayView<1, float> OutMean, TLearningArrayView<1, float> OutLogStd, const FFrameAttribute& In)
		{
			ReduceOp(In, [&OutMean, &OutLogStd](
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 ChannelNum = In.GetChannelNum();

					for (int32 ChannelIdx = 0; ChannelIdx < ChannelNum; ChannelIdx++)
					{
						Private::LogMeanStd(OutMean[ChannelIdx], OutLogStd[ChannelIdx], In.GetChannelAttributeData(ChannelIdx));
					}
				});
		}

		void QuatMul(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs)
		{
			check(Lhs.GetChannelNum() == 4);
			check(Rhs.GetChannelNum() == 4);

			BinaryOp(Out, 4, Lhs, Rhs, [](
				FFrameAttribute& Out,
				const FFrameAttribute& Lhs,
				const FFrameAttribute& Rhs,
				const TLearningArrayView<1, const int32> OutRangeOffsets,
				const TLearningArrayView<1, const int32> LhsRangeOffsets,
				const TLearningArrayView<1, const int32> RhsRangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 RangeNum = RangeLengths.Num();

					for (int32 RangeIdx = 0; RangeIdx < RangeNum; RangeIdx++)
					{
						Private::QuatMul(
							Out.GetChannelRangeAttributeData(0, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Out.GetChannelRangeAttributeData(1, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Out.GetChannelRangeAttributeData(2, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Out.GetChannelRangeAttributeData(3, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Lhs.GetChannelRangeAttributeData(0, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Lhs.GetChannelRangeAttributeData(1, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Lhs.GetChannelRangeAttributeData(2, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Lhs.GetChannelRangeAttributeData(3, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Rhs.GetChannelRangeAttributeData(0, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Rhs.GetChannelRangeAttributeData(1, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Rhs.GetChannelRangeAttributeData(2, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Rhs.GetChannelRangeAttributeData(3, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]));
					}
				});
		}

		void QuatDiv(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs)
		{
			QuatMulInv(Out, Lhs, Rhs);
		}

		void QuatInv(FFrameAttribute& Out, const FFrameAttribute& In)
		{
			check(In.GetChannelNum() == 4);

			UnaryOp(Out, 4, In, [](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					Private::QuatInv(Out.GetAttributeData(), In.GetAttributeData());
				});
		}

		void QuatAbs(FFrameAttribute& Out, const FFrameAttribute& In)
		{
			check(In.GetChannelNum() == 4);

			UnaryOp(Out, 4, In, [](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					Private::QuatAbs(Out.GetAttributeData(), In.GetAttributeData());
				});
		}

		void QuatToRotationVector(FFrameAttribute& Out, const FFrameAttribute& In)
		{
			check(In.GetChannelNum() == 4);

			UnaryOp(Out, 3, In, [](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {
				
					Private::QuatToRotationVector(Out.GetAttributeData(), In.GetAttributeData());
				});
		}

		void QuatFromRotationVector(FFrameAttribute& Out, const FFrameAttribute& In)
		{
			check(In.GetChannelNum() == 3);

			UnaryOp(Out, 4, In, [](
				FFrameAttribute& Out,
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {
				
					Private::QuatFromRotationVector(Out.GetAttributeData(),	In.GetAttributeData());
				});
		}

		void QuatInvMul(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs)
		{
			check(Lhs.GetChannelNum() == 4);
			check(Rhs.GetChannelNum() == 4);

			BinaryOp(Out, 4, Lhs, Rhs, [](
				FFrameAttribute& Out,
				const FFrameAttribute& Lhs,
				const FFrameAttribute& Rhs,
				const TLearningArrayView<1, const int32> OutRangeOffsets,
				const TLearningArrayView<1, const int32> LhsRangeOffsets,
				const TLearningArrayView<1, const int32> RhsRangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 RangeNum = RangeLengths.Num();

					for (int32 RangeIdx = 0; RangeIdx < RangeNum; RangeIdx++)
					{
						Private::QuatInvMul(
							Out.GetChannelRangeAttributeData(0, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Out.GetChannelRangeAttributeData(1, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Out.GetChannelRangeAttributeData(2, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Out.GetChannelRangeAttributeData(3, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Lhs.GetChannelRangeAttributeData(0, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Lhs.GetChannelRangeAttributeData(1, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Lhs.GetChannelRangeAttributeData(2, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Lhs.GetChannelRangeAttributeData(3, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Rhs.GetChannelRangeAttributeData(0, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Rhs.GetChannelRangeAttributeData(1, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Rhs.GetChannelRangeAttributeData(2, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Rhs.GetChannelRangeAttributeData(3, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]));
					}
				});
		}

		void QuatMulInv(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs)
		{
			check(Lhs.GetChannelNum() == 4);
			check(Rhs.GetChannelNum() == 4);

			BinaryOp(Out, 4, Lhs, Rhs, [](
				FFrameAttribute& Out,
				const FFrameAttribute& Lhs,
				const FFrameAttribute& Rhs,
				const TLearningArrayView<1, const int32> OutRangeOffsets,
				const TLearningArrayView<1, const int32> LhsRangeOffsets,
				const TLearningArrayView<1, const int32> RhsRangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 RangeNum = RangeLengths.Num();

					for (int32 RangeIdx = 0; RangeIdx < RangeNum; RangeIdx++)
					{
						Private::QuatMulInv(
							Out.GetChannelRangeAttributeData(0, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Out.GetChannelRangeAttributeData(1, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Out.GetChannelRangeAttributeData(2, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Out.GetChannelRangeAttributeData(3, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Lhs.GetChannelRangeAttributeData(0, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Lhs.GetChannelRangeAttributeData(1, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Lhs.GetChannelRangeAttributeData(2, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Lhs.GetChannelRangeAttributeData(3, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Rhs.GetChannelRangeAttributeData(0, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Rhs.GetChannelRangeAttributeData(1, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Rhs.GetChannelRangeAttributeData(2, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Rhs.GetChannelRangeAttributeData(3, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]));
					}
				});
		}

		void QuatRotate(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs)
		{
			check(Lhs.GetChannelNum() == 4);
			check(Rhs.GetChannelNum() == 3);

			BinaryOp(Out, 3, Lhs, Rhs, [](
				FFrameAttribute& Out,
				const FFrameAttribute& Lhs,
				const FFrameAttribute& Rhs,
				const TLearningArrayView<1, const int32> OutRangeOffsets,
				const TLearningArrayView<1, const int32> LhsRangeOffsets,
				const TLearningArrayView<1, const int32> RhsRangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 RangeNum = RangeLengths.Num();

					for (int32 RangeIdx = 0; RangeIdx < RangeNum; RangeIdx++)
					{
						Private::QuatRotate(
							Out.GetChannelRangeAttributeData(0, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Out.GetChannelRangeAttributeData(1, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Out.GetChannelRangeAttributeData(2, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Lhs.GetChannelRangeAttributeData(0, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Lhs.GetChannelRangeAttributeData(1, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Lhs.GetChannelRangeAttributeData(2, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Lhs.GetChannelRangeAttributeData(3, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Rhs.GetChannelRangeAttributeData(0, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Rhs.GetChannelRangeAttributeData(1, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Rhs.GetChannelRangeAttributeData(2, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]));
					}
				});
		}

		void QuatUnrotate(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs)
		{
			check(Lhs.GetChannelNum() == 4);
			check(Rhs.GetChannelNum() == 3);

			BinaryOp(Out, 3, Lhs, Rhs, [](
				FFrameAttribute& Out,
				const FFrameAttribute& Lhs,
				const FFrameAttribute& Rhs,
				const TLearningArrayView<1, const int32> OutRangeOffsets,
				const TLearningArrayView<1, const int32> LhsRangeOffsets,
				const TLearningArrayView<1, const int32> RhsRangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 RangeNum = RangeLengths.Num();

					for (int32 RangeIdx = 0; RangeIdx < RangeNum; RangeIdx++)
					{
						Private::QuatUnrotate(
							Out.GetChannelRangeAttributeData(0, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Out.GetChannelRangeAttributeData(1, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Out.GetChannelRangeAttributeData(2, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Lhs.GetChannelRangeAttributeData(0, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Lhs.GetChannelRangeAttributeData(1, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Lhs.GetChannelRangeAttributeData(2, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Lhs.GetChannelRangeAttributeData(3, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Rhs.GetChannelRangeAttributeData(0, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Rhs.GetChannelRangeAttributeData(1, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Rhs.GetChannelRangeAttributeData(2, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]));
					}
				});
		}

		void QuatBetween(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs)
		{
			check(Lhs.GetChannelNum() == 3);
			check(Rhs.GetChannelNum() == 3);

			BinaryOp(Out, 4, Lhs, Rhs, [](
				FFrameAttribute& Out,
				const FFrameAttribute& Lhs,
				const FFrameAttribute& Rhs,
				const TLearningArrayView<1, const int32> OutRangeOffsets,
				const TLearningArrayView<1, const int32> LhsRangeOffsets,
				const TLearningArrayView<1, const int32> RhsRangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					const int32 RangeNum = RangeLengths.Num();

					for (int32 RangeIdx = 0; RangeIdx < RangeNum; RangeIdx++)
					{
						Private::QuatBetween(
							Out.GetChannelRangeAttributeData(0, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Out.GetChannelRangeAttributeData(1, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Out.GetChannelRangeAttributeData(2, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Out.GetChannelRangeAttributeData(3, OutRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Lhs.GetChannelRangeAttributeData(0, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Lhs.GetChannelRangeAttributeData(1, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Lhs.GetChannelRangeAttributeData(2, LhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Rhs.GetChannelRangeAttributeData(0, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Rhs.GetChannelRangeAttributeData(1, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]),
							Rhs.GetChannelRangeAttributeData(2, RhsRangeOffsets[RangeIdx], RangeLengths[RangeIdx]));
					}
				});
		}

		void QuatMulConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FQuat4f Rhs)
		{
			check(Lhs.GetChannelNum() == 4);

			UnaryOp(Out, 4, Lhs, [Rhs](
				FFrameAttribute& Out,
				const FFrameAttribute& Lhs,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					Private::QuatMulConstant(
						Out.GetAttributeData(),
						Lhs.GetAttributeData(),
						Rhs);
				});
		}

		void QuatDivConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FQuat4f Rhs)
		{
			QuatMulInvConstant(Out, Lhs, Rhs);
		}

		void QuatInvMulConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FQuat4f Rhs)
		{
			check(Lhs.GetChannelNum() == 4);

			UnaryOp(Out, 4, Lhs, [Rhs](
				FFrameAttribute& Out,
				const FFrameAttribute& Lhs,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					Private::QuatInvMulConstant(
						Out.GetAttributeData(),
						Lhs.GetAttributeData(),
						Rhs);
				});
		}

		void QuatMulInvConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FQuat4f Rhs)
		{
			check(Lhs.GetChannelNum() == 4);

			UnaryOp(Out, 4, Lhs, [Rhs](
				FFrameAttribute& Out,
				const FFrameAttribute& Lhs,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					Private::QuatMulInvConstant(
						Out.GetAttributeData(),
						Lhs.GetAttributeData(),
						Rhs);
				});
		}

		void QuatRotateConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FVector3f Rhs)
		{
			check(Lhs.GetChannelNum() == 4);

			UnaryOp(Out, 3, Lhs, [Rhs](
				FFrameAttribute& Out,
				const FFrameAttribute& Lhs,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					Private::QuatRotateConstant(
						Out.GetAttributeData(),
						Lhs.GetAttributeData(),
						Rhs);
				});
		}

		void QuatUnrotateConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FVector3f Rhs)
		{
			check(Lhs.GetChannelNum() == 4);

			UnaryOp(Out, 3, Lhs, [Rhs](
				FFrameAttribute& Out,
				const FFrameAttribute& Lhs,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					Private::QuatUnrotateConstant(
						Out.GetAttributeData(),
						Lhs.GetAttributeData(),
						Rhs);
				});
		}

		void QuatBetweenConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FVector3f Rhs)
		{
			check(Lhs.GetChannelNum() == 3);

			UnaryOp(Out, 4, Lhs, [Rhs](
				FFrameAttribute& Out,
				const FFrameAttribute& Lhs,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					Private::QuatBetweenConstant(
						Out.GetAttributeData(),
						Lhs.GetAttributeData(),
						Rhs);
				});
		}

		void QuatConstantMul(FFrameAttribute& Out, const FQuat4f Lhs, const FFrameAttribute& Rhs)
		{
			check(Rhs.GetChannelNum() == 4);

			UnaryOp(Out, 4, Rhs, [Lhs](
				FFrameAttribute& Out,
				const FFrameAttribute& Rhs,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					Private::QuatConstantMul(
						Out.GetAttributeData(),
						Lhs,
						Rhs.GetAttributeData());
				});
		}

		void QuatConstantDiv(FFrameAttribute& Out, const FQuat4f Lhs, const FFrameAttribute& Rhs)
		{
			QuatConstantMulInv(Out, Lhs, Rhs);
		}

		void QuatConstantInvMul(FFrameAttribute& Out, const FQuat4f Lhs, const FFrameAttribute& Rhs)
		{
			check(Rhs.GetChannelNum() == 4);

			UnaryOp(Out, 4, Rhs, [Lhs](
				FFrameAttribute& Out,
				const FFrameAttribute& Rhs,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					Private::QuatConstantInvMul(
						Out.GetAttributeData(),
						Lhs,
						Rhs.GetAttributeData());
				});
		}

		void QuatConstantMulInv(FFrameAttribute& Out, const FQuat4f Lhs, const FFrameAttribute& Rhs)
		{
			check(Rhs.GetChannelNum() == 4);

			UnaryOp(Out, 4, Rhs, [Lhs](
				FFrameAttribute& Out,
				const FFrameAttribute& Rhs,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					Private::QuatConstantMulInv(
						Out.GetAttributeData(),
						Lhs,
						Rhs.GetAttributeData());
				});
		}

		void QuatConstantRotate(FFrameAttribute& Out, const FQuat4f Lhs, const FFrameAttribute& Rhs)
		{
			check(Rhs.GetChannelNum() == 3);

			UnaryOp(Out, 3, Rhs, [Lhs](
				FFrameAttribute& Out,
				const FFrameAttribute& Rhs,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					Private::QuatConstantRotate(
						Out.GetAttributeData(),
						Lhs,
						Rhs.GetAttributeData());
				});
		}

		void QuatConstantUnrotate(FFrameAttribute& Out, const FQuat4f Lhs, const FFrameAttribute& Rhs)
		{
			check(Rhs.GetChannelNum() == 3);

			UnaryOp(Out, 3, Rhs, [Lhs](
				FFrameAttribute& Out,
				const FFrameAttribute& Rhs,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					Private::QuatConstantUnrotate(
						Out.GetAttributeData(),
						Lhs,
						Rhs.GetAttributeData());
				});
		}

		void QuatConstantBetween(FFrameAttribute& Out, const FVector3f Lhs, const FFrameAttribute& Rhs)
		{
			check(Rhs.GetChannelNum() == 3);

			UnaryOp(Out, 4, Rhs, [Lhs](
				FFrameAttribute& Out,
				const FFrameAttribute& Rhs,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					Private::QuatConstantBetween(
						Out.GetAttributeData(),
						Lhs,
						Rhs.GetAttributeData());
				});
		}

		void QuatMeanStd(FQuat4f& OutMean, FVector3f& OutStd, const FFrameAttribute& In)
		{
			check(In.GetChannelNum() == 4);

			ReduceOp(In, [&OutMean, &OutStd](
				const FFrameAttribute& In,
				const TLearningArrayView<1, const int32> RangeOffsets,
				const TLearningArrayView<1, const int32> RangeLengths) {

					Private::QuatMeanStd(OutMean, OutStd, In.GetAttributeData());
				});
		}

	}
}
