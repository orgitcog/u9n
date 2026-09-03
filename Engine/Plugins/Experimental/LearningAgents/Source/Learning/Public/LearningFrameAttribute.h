// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningArray.h"
#include "LearningFrameRangeSet.h"

#include "Math/Vector.h"
#include "Math/Quat.h"

namespace UE::Learning
{

	/**
	 * A FFrameAttribute represents a attribute associated with every frame in a FFrameRangeSet. An attribute is made up of multiple "channels" 
	 * such as the X, Y, Z components of a location.
	 * 
	 * The data for the attribute is stored in one large flat array of shape (ChannelNum, TotalFrameNum). This means the data is stored in a way
	 * designed for SoA access by default. Some helper functions are provided for accessing slices of this data according to various range properties.
	 * 
	 * Also provided are many "operators" which allow for batched operations on attributes to create new attributes such as adding or subtracting 
	 * attribute values. This operations are very efficient as they are computed across all frames in the attribute in batch and can be optimized 
	 * using ISPC. If you perform a binary operation on two channels with different frames in the range sets then it will construct a new channel
	 * which is the intersection of those two inputs.
	 */
	struct LEARNING_API FFrameAttribute
	{
	public:

		/** Check if the FrameAttribute is well-formed. */
		void Check() const;

		/** True if the FrameAttribute is Empty, otherwise false */
		bool IsEmpty() const;

		/** Empties the FrameAttribute */
		void Empty();

		/** Gets the internal FrameRangeSet associated to this attribute */
		const FFrameRangeSet& GetFrameRangeSet() const;

		/** Gets the total number of frames for this attribute */
		int32 GetTotalFrameNum() const;

		/** Gets the total number of ranges for this attribute */
		int32 GetTotalRangeNum() const;

		/** Gets the number of channels in this attribute */
		int32 GetChannelNum() const;

		/** Gets a view of the complete large array of attribute data stored as (ChannelNum, TotalFrameNum) */
		TLearningArrayView<2, const float> GetAttributeData() const;

		/** Gets a view of the complete attribute data for a single channel */
		TLearningArrayView<1, const float> GetChannelAttributeData(const int32 ChannelIdx) const;

		/** Gets the attribute value for a given channel and frame index */
		const float& GetChannelAttributeDataAtFrame(const int32 ChannelIdx, const int32 FrameIdx) const;

		/** Gets the attribute data associated with a single channel, and range */
		TLearningArrayView<1, const float> GetChannelEntryRangeAttributeData(const int32 ChannelIdx, const int32 EntryIdx, const int32 RangeIdx) const;

		/** Gets the attribute data associated with range offset and length */
		TLearningArrayView<1, const float> GetChannelRangeAttributeData(const int32 ChannelIdx, const int32 RangeOffset, const int32 RangeLength) const;

		/** Gets a view of the complete large array of attribute data stored as (ChannelNum, TotalFrameNum) */
		TLearningArrayView<2, float> GetAttributeData();

		/** Gets a view of the complete attribute data for a single channel */
		TLearningArrayView<1, float> GetChannelAttributeData(const int32 ChannelIdx);

		/** Gets the attribute value for a given channel and frame index */
		float& GetChannelAttributeDataAtFrame(const int32 ChannelIdx, const int32 FrameIdx);

		/** Gets the attribute data associated with a single channel, and range */
		TLearningArrayView<1, float> GetChannelEntryRangeAttributeData(const int32 ChannelIdx, const int32 EntryIdx, const int32 RangeIdx);

		/** Gets the attribute data associated with range offset and length */
		TLearningArrayView<1, float> GetChannelRangeAttributeData(const int32 ChannelIdx, const int32 RangeOffset, const int32 RangeLength);

	public:

		/** The internal associate FrameRangeSet */
		FFrameRangeSet FrameRangeSet;

		/** The large flat array of attribute data of shape (ChannelNum, TotalFrameNum) */
		TLearningArray<2, float> AttributeData;
	};

	namespace FrameAttribute
	{
		/** Reduce op function. Takes as input a single frame attribute and a set of ranges and lengths for that attribute. */
		using ReduceOpFunction = TFunctionRef<void(
			const FFrameAttribute& In,
			const TLearningArrayView<1, const int32> RangeOffsets,
			const TLearningArrayView<1, const int32> RangeLengths)>;

		/** Nullary op function. Produces as output a single frame attribute given a set of ranges and lengths for that attribute. */
		using NullaryOpFunction = TFunctionRef<void(
			FFrameAttribute& Out,
			const TLearningArrayView<1, const int32> RangeOffsets,
			const TLearningArrayView<1, const int32> RangeLengths)>;

		/** Unary op function. Takes a single frame attribute as input and produces as output a single frame attribute. */
		using UnaryOpFunction = TFunctionRef<void(
			FFrameAttribute& Out,
			const FFrameAttribute& In,
			const TLearningArrayView<1, const int32> RangeOffsets,
			const TLearningArrayView<1, const int32> RangeLengths)>;

		/** Binary op function. Takes a two frame attributes as input and produces as output a single frame attribute. */
		using BinaryOpFunction = TFunctionRef<void(
			FFrameAttribute& Out,
			const FFrameAttribute& Lhs,
			const FFrameAttribute& Rhs,
			const TLearningArrayView<1, const int32> OutRangeOffsets,
			const TLearningArrayView<1, const int32> LhsRangeOffsets,
			const TLearningArrayView<1, const int32> RhsRangeOffsets,
			const TLearningArrayView<1, const int32> RangeLengths)>;

		/** Convenience type to make the definition and usage of NaryOp easier */
		using ConstFrameAttributePtr = const FFrameAttribute*;

		/** Nary op function. Takes a multiple frame attributes as input and produces as output a single frame attribute. */
		using NaryOpFunction = TFunctionRef<void(
			FFrameAttribute& Out,
			const TArrayView<const ConstFrameAttributePtr> In,
			const TLearningArrayView<1, const int32> OutRangeOffsets,
			const TArrayView<const TLearningArrayView<1, const int32>> InRangeOffsets,
			const TLearningArrayView<1, const int32> RangeLengths)>;

		/** Computes the intersection of a frame attribute and frame range set */
		LEARNING_API void Intersection(FFrameAttribute& OutFrameAttribute, const FFrameAttribute& FrameAttribute, const FFrameRangeSet& FrameRangeSet);

		/** Computes the frame range set where the given channel is non-zero in the frame attribute. */
		LEARNING_API void NonZeroFrameRangeSet(FFrameRangeSet& OutFrameRangeSet, const FFrameAttribute& FrameAttribute, const int32 ChannelIdx);

		/** Computes a reduction on a frame attribute */
		LEARNING_API void ReduceOp(
			const FFrameAttribute& In,
			const ReduceOpFunction Op);

		/** Creates a frame attribute from zero arguments and the given op */
		LEARNING_API void NullaryOp(
			FFrameAttribute& Out,
			const int32 OutChannelNum,
			const FFrameRangeSet& FrameRangeSet,
			const NullaryOpFunction Op);

		/** Creates a frame attribute from another and the given op */
		LEARNING_API void UnaryOp(
			FFrameAttribute& Out,
			const int32 OutChannelNum,
			const FFrameAttribute& In,
			const UnaryOpFunction Op);

		/** Creates a frame attribute from two others and the given op. Performs an intersection of the Lhs and Rhs if they do not match. */
		LEARNING_API void BinaryOp(
			FFrameAttribute& Out,
			const int32 OutChannelNum,
			const FFrameAttribute& Lhs,
			const FFrameAttribute& Rhs,
			const BinaryOpFunction Op);

		/** Creates a frame attribute from multiple others and the given op. */
		LEARNING_API void NaryOp(
			FFrameAttribute& Out,
			const int32 OutChannelNum,
			const TArrayView<const ConstFrameAttributePtr> Inputs,
			const NaryOpFunction Op);

		/** Find the channel and frame with the smallest value */
		LEARNING_API bool FindMinimum(
			int32& OutChannelIdx,
			int32& OutFrameIdx,
			float& OutMinimumValue, 
			const FFrameAttribute& In);

		/** Find the channel and frame with the largest value */
		LEARNING_API bool FindMaximum(
			int32& OutChannelIdx,
			int32& OutFrameIdx,
			float& OutMaximumValue, 
			const FFrameAttribute& In);

		/** Create a frame attribute made up of zeros */
		LEARNING_API void Zeros(FFrameAttribute& Out, const FFrameRangeSet& FrameRangeSet, const int32 ChannelNum);

		/** Create a frame attribute made up of ones */
		LEARNING_API void Ones(FFrameAttribute& Out, const FFrameRangeSet& FrameRangeSet, const int32 ChannelNum);

		/** Create a frame attribute filled with the given value at each frame */
		LEARNING_API void Fill(FFrameAttribute& Out, const FFrameRangeSet& FrameRangeSet, const TLearningArrayView<1, const float> Values);

		/** Add two frame attributes. Channel numbers must match. */
		LEARNING_API void Add(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs);

		/** Subtract two frame attributes. Channel numbers must match. */
		LEARNING_API void Sub(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs);

		/** Multiply two frame attributes. Channel numbers must match. */
		LEARNING_API void Mul(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs);

		/** Divide two frame attributes. Channel numbers must match. */
		LEARNING_API void Div(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs);

		/** Compute the dot product of two frame attributes. Channel numbers must match. */
		LEARNING_API void Dot(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs);

		/** Negate a frame attribute. */
		LEARNING_API void Neg(FFrameAttribute& Out, const FFrameAttribute& In);

		/** Invert a frame attribute (compute 1/x). */
		LEARNING_API void Inv(FFrameAttribute& Out, const FFrameAttribute& In);

		/** Compute the absolute value of a frame attribute. */
		LEARNING_API void Abs(FFrameAttribute& Out, const FFrameAttribute& In);

		/** Compute the log of a frame attribute. */
		LEARNING_API void Log(FFrameAttribute& Out, const FFrameAttribute& In);

		/** Compute the exp of a frame attribute. */
		LEARNING_API void Exp(FFrameAttribute& Out, const FFrameAttribute& In);

		/** Compute the length of a frame attribute over the channels dimension. */
		LEARNING_API void Length(FFrameAttribute& Out, const FFrameAttribute& In);

		/** Normalize a frame attribute over the channels dimension. */
		LEARNING_API void Normalize(FFrameAttribute& Out, const FFrameAttribute& In);

		/** Extract a single channel from frame attribute. */
		LEARNING_API void Index(FFrameAttribute& Out, const FFrameAttribute& In, const int32 ChannelIdx);

		/** Add a constant value to a frame attribute. Channel number must match the size of Rhs. */
		LEARNING_API void AddConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const TLearningArrayView<1, const float> Rhs);

		/** Subtract a constant value to a frame attribute. Channel number must match the size of Rhs. */
		LEARNING_API void SubConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const TLearningArrayView<1, const float> Rhs);

		/** Multiply a frame attribute by a constant value. Channel number must match the size of Rhs. */
		LEARNING_API void MulConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const TLearningArrayView<1, const float> Rhs);

		/** Divide a frame attribute by a constant value. Channel number must match the size of Rhs. */
		LEARNING_API void DivConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const TLearningArrayView<1, const float> Rhs);

		/** Add a constant value to a frame attribute. Channel number must match the size of Lhs. */
		LEARNING_API void ConstantAdd(FFrameAttribute& Out, const TLearningArrayView<1, const float> Lhs, const FFrameAttribute& Rhs);

		/** Subtract a constant value to a frame attribute. Channel number must match the size of Lhs. */
		LEARNING_API void ConstantSub(FFrameAttribute& Out, const TLearningArrayView<1, const float> Lhs, const FFrameAttribute& Rhs);

		/** Multiply a frame attribute by a constant value. Channel number must match the size of Lhs. */
		LEARNING_API void ConstantMul(FFrameAttribute& Out, const TLearningArrayView<1, const float> Lhs, const FFrameAttribute& Rhs);

		/** Divide a frame attribute by a constant value. Channel number must match the size of Lhs. */
		LEARNING_API void ConstantDiv(FFrameAttribute& Out, const TLearningArrayView<1, const float> Lhs, const FFrameAttribute& Rhs);

		/** Compute the sum of an array of frame attributes. Channel numbers must match. */
		LEARNING_API void Sum(FFrameAttribute& Out, const TArrayView<const ConstFrameAttributePtr> Inputs);

		/** Compute the product of an array of frame attributes. Channel numbers must match. */
		LEARNING_API void Prod(FFrameAttribute& Out, const TArrayView<const ConstFrameAttributePtr> Inputs);

		/** Computes the logical and of two frame attributes. Channel numbers must match. */
		LEARNING_API void LogicalAnd(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs);

		/** Computes the logical or of two frame attributes. Channel numbers must match. */
		LEARNING_API void LogicalOr(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs);

		/** Computes the logical not of a frame attribute. */
		LEARNING_API void LogicalNot(FFrameAttribute& Out, const FFrameAttribute& In);

		/** Computes if the values in one frame attribute are greater than another. Channel numbers must match. */
		LEARNING_API void Gt(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs);

		/** Computes if the values in one frame attribute are greater than or equal to another. Channel numbers must match. */
		LEARNING_API void Ge(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs);

		/** Computes if the values in one frame attribute are less than another. Channel numbers must match. */
		LEARNING_API void Lt(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs);

		/** Computes if the values in one frame attribute are less than or equal to another. Channel numbers must match. */
		LEARNING_API void Le(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs);

		/** Computes if the values in one frame attribute are equal to another. Channel numbers must match. */
		LEARNING_API void Eq(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs);

		/** Computes if the values in one frame attribute are not equal to another. Channel numbers must match. */
		LEARNING_API void Neq(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs);

		/** Computes if the values in a frame attribute are greater than a constant. Channel number must match the size of Rhs. */
		LEARNING_API void GtConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const TLearningArrayView<1, const float> Rhs);

		/** Computes if the values in a frame attribute are greater than or equal to a constant. Channel number must match the size of Rhs. */
		LEARNING_API void GeConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const TLearningArrayView<1, const float> Rhs);

		/** Computes if the values in a frame attribute are less than a constant. Channel number must match the size of Rhs. */
		LEARNING_API void LtConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const TLearningArrayView<1, const float> Rhs);

		/** Computes if the values in a frame attribute are greater than or equal to a constant. Channel number must match the size of Rhs. */
		LEARNING_API void LeConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const TLearningArrayView<1, const float> Rhs);
		
		/** Computes if the values in a frame attribute are equal to a constant. Channel number must match the size of Rhs. */
		LEARNING_API void EqConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const TLearningArrayView<1, const float> Rhs);

		/** Computes if the values in a frame attribute are not equal to a constant. Channel number must match the size of Rhs. */
		LEARNING_API void NeqConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const TLearningArrayView<1, const float> Rhs);

		/** Computes if the values in a frame attribute are greater than a constant. Channel number must match the size of Lhs. */
		LEARNING_API void ConstantGt(FFrameAttribute& Out, const TLearningArrayView<1, const float> Lhs, const FFrameAttribute& Rhs);
		
		/** Computes if the values in a frame attribute are greater than or equal to a constant. Channel number must match the size of Lhs. */
		LEARNING_API void ConstantGe(FFrameAttribute& Out, const TLearningArrayView<1, const float> Lhs, const FFrameAttribute& Rhs);
		
		/** Computes if the values in a frame attribute are less than a constant. Channel number must match the size of Lhs. */
		LEARNING_API void ConstantLt(FFrameAttribute& Out, const TLearningArrayView<1, const float> Lhs, const FFrameAttribute& Rhs);

		/** Computes if the values in a frame attribute are greater than or equal to a constant. Channel number must match the size of Lhs. */
		LEARNING_API void ConstantLe(FFrameAttribute& Out, const TLearningArrayView<1, const float> Lhs, const FFrameAttribute& Rhs);

		/** Computes if the values in a frame attribute are equal to a constant. Channel number must match the size of Lhs. */
		LEARNING_API void ConstantEq(FFrameAttribute& Out, const TLearningArrayView<1, const float> Lhs, const FFrameAttribute& Rhs);
		
		/** Computes if the values in a frame attribute are not equal to a constant. Channel number must match the size of Lhs. */
		LEARNING_API void ConstantNeq(FFrameAttribute& Out, const TLearningArrayView<1, const float> Lhs, const FFrameAttribute& Rhs);

		/** Applies a Gaussian smoothing filter to the ranges of values in the frame attribute. */
		LEARNING_API void FilterGaussian(FFrameAttribute& Out, const FFrameAttribute& In, const float StdInFrames);

		/** Applies a Majority Vote filter to the ranges of values in the frame attribute. */
		LEARNING_API void FilterMajorityVote(FFrameAttribute& Out, const FFrameAttribute& In, const int32 FilterWidthFrames);

		/** Computes the mean and std across all frames. OutMean and OutStd should be size of the number of channels */
		LEARNING_API void MeanStd(TLearningArrayView<1, float> OutMean, TLearningArrayView<1, float> OutStd, const FFrameAttribute& In);

		/** Computes the mean and log std across all frames in the log space. OutMean and OutLogStd should be size of the number of channels */
		LEARNING_API void LogMeanStd(TLearningArrayView<1, float> OutMean, TLearningArrayView<1, float> OutLogStd, const FFrameAttribute& In);

		/** Computes the quaternion multiplication of two frame attributes. Both arguments are assumed to have 4 channels. */
		LEARNING_API void QuatMul(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs);

		/** Computes the quaternion division of two frame attributes. Both arguments are assumed to have 4 channels. */
		LEARNING_API void QuatDiv(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs);

		/** Compute the quaternion inverse of a frame attribute. Argument is assumed to have 4 channels. */
		LEARNING_API void QuatInv(FFrameAttribute& Out, const FFrameAttribute& In);

		/** Compute the quaternion closest to the identity quaternion for a frame attribute. Argument is assumed to have 4 channels. */
		LEARNING_API void QuatAbs(FFrameAttribute& Out, const FFrameAttribute& In);

		/** Compute a rotation vector from a quaternion. Argument is assumed to have 4 channels. */
		LEARNING_API void QuatToRotationVector(FFrameAttribute& Out, const FFrameAttribute& In);

		/** Compute a quaternion from a rotation vector. Argument is assumed to have 3 channels. */
		LEARNING_API void QuatFromRotationVector(FFrameAttribute& Out, const FFrameAttribute& In);

		/** Computes the quaternion inverse multiplication of two frame attributes. Both arguments are assumed to have 4 channels. */
		LEARNING_API void QuatInvMul(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs);

		/** Computes the quaternion multiplication inverse of two frame attributes. Both arguments are assumed to have 4 channels. */
		LEARNING_API void QuatMulInv(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs);

		/** Computes the rotation of the Rhs by the Lhs quaternion. Lhs is assumed to have 4 channels, and Rhs is assumed to have 3. */
		LEARNING_API void QuatRotate(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs);

		/** Computes the inverse rotation of the Rhs by the Lhs quaternion. Lhs is assumed to have 4 channels, and Rhs is assumed to have 3. */
		LEARNING_API void QuatUnrotate(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs);

		/** Computes the shortest rotation between two vectors. Both arguments are assumed to have 3 channels. */
		LEARNING_API void QuatBetween(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FFrameAttribute& Rhs);

		/** Computes the quaternion multiplication of a frame attribute by the given quaternion. Lhs is assumed to have 4 channels. */
		LEARNING_API void QuatMulConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FQuat4f Rhs);

		/** Computes the quaternion division of a frame attribute by the given quaternion. Lhs is assumed to have 4 channels. */
		LEARNING_API void QuatDivConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FQuat4f Rhs);

		/** Computes the quaternion inverse multiplication of a frame attribute by the given quaternion. Lhs is assumed to have 4 channels. */
		LEARNING_API void QuatInvMulConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FQuat4f Rhs);

		/** Computes the quaternion multiplication inverse of a frame attribute by the given quaternion. Lhs is assumed to have 4 channels. */
		LEARNING_API void QuatMulInvConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FQuat4f Rhs);

		/** Computes the quaternion rotation of the given vector by a frame attribute. Lhs is assumed to have 4 channels. */
		LEARNING_API void QuatRotateConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FVector3f Rhs);

		/** Computes the quaternion inverse rotation of the given vector by a frame attribute. Lhs is assumed to have 4 channels. */
		LEARNING_API void QuatUnrotateConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FVector3f Rhs);

		/** Computes the quaternion between the given frame attribute and vector. Lhs is assumed to have 3 channels. */
		LEARNING_API void QuatBetweenConstant(FFrameAttribute& Out, const FFrameAttribute& Lhs, const FVector3f Rhs);

		/** Computes the quaternion multiplication of a frame attribute by the given quaternion. Rhs is assumed to have 4 channels. */
		LEARNING_API void QuatConstantMul(FFrameAttribute& Out, const FQuat4f Lhs, const FFrameAttribute& Rhs);

		/** Computes the quaternion division of a frame attribute by the given quaternion. Rhs is assumed to have 4 channels. */
		LEARNING_API void QuatConstantDiv(FFrameAttribute& Out, const FQuat4f Lhs, const FFrameAttribute& Rhs);
		
		/** Computes the quaternion inverse multiplication of a frame attribute by the given quaternion. Rhs is assumed to have 4 channels. */
		LEARNING_API void QuatConstantInvMul(FFrameAttribute& Out, const FQuat4f Lhs, const FFrameAttribute& Rhs);
		
		/** Computes the quaternion multiplication inverse of a frame attribute by the given quaternion. Rhs is assumed to have 4 channels. */
		LEARNING_API void QuatConstantMulInv(FFrameAttribute& Out, const FQuat4f Lhs, const FFrameAttribute& Rhs);
		
		/** Computes the quaternion rotation of the given frame attribute. Rhs is assumed to have 3 channels. */
		LEARNING_API void QuatConstantRotate(FFrameAttribute& Out, const FQuat4f Lhs, const FFrameAttribute& Rhs);

		/** Computes the quaternion inverse rotation of the given frame attribute. Rhs is assumed to have 3 channels. */
		LEARNING_API void QuatConstantUnrotate(FFrameAttribute& Out, const FQuat4f Lhs, const FFrameAttribute& Rhs);

		/** Computes the quaternion between the given frame attribute and vector. Rhs is assumed to have 3 channels. */
		LEARNING_API void QuatConstantBetween(FFrameAttribute& Out, const FVector3f Lhs, const FFrameAttribute& Rhs);

		/** Computes the quaternion mean and std across all frames. */
		LEARNING_API void QuatMeanStd(FQuat4f& OutMean, FVector3f& OutStd, const FFrameAttribute& In);
	}
}