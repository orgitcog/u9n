// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/HyprsenseRealtimeNode.h"

#include "Pipeline/Log.h"
#include "CoreUtils.h"

#include "UObject/Package.h"
#include "Math/TransformCalculus2D.h"

#include "NNE.h"
#include "NNETypes.h"
#include "NNEModelData.h"
#include "NNERuntimeGPU.h"
#include "NNERuntimeCPU.h"

#include "GuiToRawControlsUtils.h"
#include "OpenCVHelperLocal.h"

#ifdef USE_OPENCV
#include "PreOpenCVHeaders.h"
#include "OpenCVHelper.h"
#include "ThirdParty/OpenCV/include/opencv2/imgproc.hpp"
#include "PostOpenCVHeaders.h"
#endif

//#define PRINT_RESULTS

namespace UE::MetaHuman::Pipeline
{

static const TArray<FString> SolverControlNames = { "CTRL_L_brow_down.ty", "CTRL_R_brow_down.ty", "CTRL_L_brow_lateral.ty", "CTRL_R_brow_lateral.ty", "CTRL_L_brow_raiseIn.ty", "CTRL_R_brow_raiseIn.ty", "CTRL_L_brow_raiseOut.ty", "CTRL_R_brow_raiseOut.ty", "CTRL_L_ear_up.ty", "CTRL_R_ear_up.ty", "CTRL_L_eye_blink.ty", "CTRL_R_eye_blink.ty", "CTRL_L_eye_lidPress.ty", "CTRL_R_eye_lidPress.ty", "CTRL_L_eye_squintInner.ty", "CTRL_R_eye_squintInner.ty", "CTRL_L_eye_cheekRaise.ty", "CTRL_R_eye_cheekRaise.ty", "CTRL_L_eye_faceScrunch.ty", "CTRL_R_eye_faceScrunch.ty", "CTRL_L_eye_eyelidU.ty", "CTRL_R_eye_eyelidU.ty", "CTRL_L_eye_eyelidD.ty", "CTRL_R_eye_eyelidD.ty", "CTRL_L_eye.ty", "CTRL_R_eye.ty", "CTRL_L_eye.tx", "CTRL_R_eye.tx", "CTRL_L_eye_pupil.ty", "CTRL_R_eye_pupil.ty", "CTRL_C_eye_parallelLook.ty", "CTRL_L_eyelashes_tweakerIn.ty", "CTRL_R_eyelashes_tweakerIn.ty", "CTRL_L_eyelashes_tweakerOut.ty", "CTRL_R_eyelashes_tweakerOut.ty", "CTRL_L_nose.ty", "CTRL_R_nose.ty", "CTRL_L_nose.tx", "CTRL_R_nose.tx", "CTRL_L_nose_wrinkleUpper.ty", "CTRL_R_nose_wrinkleUpper.ty", "CTRL_L_nose_nasolabialDeepen.ty", "CTRL_R_nose_nasolabialDeepen.ty", "CTRL_L_mouth_suckBlow.ty", "CTRL_R_mouth_suckBlow.ty", "CTRL_L_mouth_lipsBlow.ty", "CTRL_R_mouth_lipsBlow.ty", "CTRL_C_mouth.ty", "CTRL_C_mouth.tx", "CTRL_L_mouth_upperLipRaise.ty", "CTRL_R_mouth_upperLipRaise.ty", "CTRL_L_mouth_lowerLipDepress.ty", "CTRL_R_mouth_lowerLipDepress.ty", "CTRL_L_mouth_cornerPull.ty", "CTRL_R_mouth_cornerPull.ty", "CTRL_L_mouth_stretch.ty", "CTRL_R_mouth_stretch.ty", "CTRL_L_mouth_stretchLipsClose.ty", "CTRL_R_mouth_stretchLipsClose.ty", "CTRL_L_mouth_dimple.ty", "CTRL_R_mouth_dimple.ty", "CTRL_L_mouth_cornerDepress.ty", "CTRL_R_mouth_cornerDepress.ty", "CTRL_L_mouth_pressU.ty", "CTRL_R_mouth_pressU.ty", "CTRL_L_mouth_pressD.ty", "CTRL_R_mouth_pressD.ty", "CTRL_L_mouth_purseU.ty", "CTRL_R_mouth_purseU.ty", "CTRL_L_mouth_purseD.ty", "CTRL_R_mouth_purseD.ty", "CTRL_L_mouth_towardsU.ty", "CTRL_R_mouth_towardsU.ty", "CTRL_L_mouth_towardsD.ty", "CTRL_R_mouth_towardsD.ty", "CTRL_L_mouth_funnelU.ty", "CTRL_R_mouth_funnelU.ty", "CTRL_L_mouth_funnelD.ty", "CTRL_R_mouth_funnelD.ty", "CTRL_L_mouth_lipsTogetherU.ty", "CTRL_R_mouth_lipsTogetherU.ty", "CTRL_L_mouth_lipsTogetherD.ty", "CTRL_R_mouth_lipsTogetherD.ty", "CTRL_L_mouth_lipBiteU.ty", "CTRL_R_mouth_lipBiteU.ty", "CTRL_L_mouth_lipBiteD.ty", "CTRL_R_mouth_lipBiteD.ty", "CTRL_L_mouth_tightenU.ty", "CTRL_R_mouth_tightenU.ty", "CTRL_L_mouth_tightenD.ty", "CTRL_R_mouth_tightenD.ty", "CTRL_L_mouth_lipsPressU.ty", "CTRL_R_mouth_lipsPressU.ty", "CTRL_L_mouth_sharpCornerPull.ty", "CTRL_R_mouth_sharpCornerPull.ty", "CTRL_C_mouth_stickyU.ty", "CTRL_L_mouth_stickyInnerU.ty", "CTRL_R_mouth_stickyInnerU.ty", "CTRL_L_mouth_stickyOuterU.ty", "CTRL_R_mouth_stickyOuterU.ty", "CTRL_C_mouth_stickyD.ty", "CTRL_L_mouth_stickyInnerD.ty", "CTRL_R_mouth_stickyInnerD.ty", "CTRL_L_mouth_stickyOuterD.ty", "CTRL_R_mouth_stickyOuterD.ty", "CTRL_L_mouth_lipSticky.ty", "CTRL_R_mouth_lipSticky.ty", "CTRL_L_mouth_pushPullU.ty", "CTRL_R_mouth_pushPullU.ty", "CTRL_L_mouth_pushPullD.ty", "CTRL_R_mouth_pushPullD.ty", "CTRL_L_mouth_thicknessU.ty", "CTRL_R_mouth_thicknessU.ty", "CTRL_L_mouth_thicknessD.ty", "CTRL_R_mouth_thicknessD.ty", "CTRL_L_mouth_thicknessInwardU.ty", "CTRL_R_mouth_thicknessInwardU.ty", "CTRL_L_mouth_thicknessInwardD.ty", "CTRL_R_mouth_thicknessInwardD.ty", "CTRL_L_mouth_cornerSharpnessU.ty", "CTRL_R_mouth_cornerSharpnessU.ty", "CTRL_L_mouth_cornerSharpnessD.ty", "CTRL_R_mouth_cornerSharpnessD.ty", "CTRL_L_mouth_lipsTowardsTeethU.ty", "CTRL_R_mouth_lipsTowardsTeethU.ty", "CTRL_L_mouth_lipsTowardsTeethD.ty", "CTRL_R_mouth_lipsTowardsTeethD.ty", "CTRL_C_mouth_lipShiftU.ty", "CTRL_C_mouth_lipShiftD.ty", "CTRL_L_mouth_lipsRollU.ty", "CTRL_R_mouth_lipsRollU.ty", "CTRL_L_mouth_lipsRollD.ty", "CTRL_R_mouth_lipsRollD.ty", "CTRL_L_mouth_corner.ty", "CTRL_L_mouth_corner.tx", "CTRL_R_mouth_corner.ty", "CTRL_R_mouth_corner.tx", "CTRL_C_tongue_inOut.ty", "CTRL_C_tongue_move.ty", "CTRL_C_tongue_move.tx", "CTRL_C_tongue_press.ty", "CTRL_C_tongue_wideNarrow.ty", "CTRL_C_tongue_bendTwist.ty", "CTRL_C_tongue_bendTwist.tx", "CTRL_C_tongue_roll.ty", "CTRL_C_tongue_tipMove.ty", "CTRL_C_tongue_tipMove.tx", "CTRL_C_tongue_thickThin.ty", "CTRL_C_jaw.ty", "CTRL_C_jaw.tx", "CTRL_C_jaw_fwdBack.ty", "CTRL_L_jaw_clench.ty", "CTRL_R_jaw_clench.ty", "CTRL_L_jaw_ChinRaiseU.ty", "CTRL_R_jaw_ChinRaiseU.ty", "CTRL_L_jaw_ChinRaiseD.ty", "CTRL_R_jaw_ChinRaiseD.ty", "CTRL_L_jaw_chinCompress.ty", "CTRL_R_jaw_chinCompress.ty", "CTRL_C_jaw_openExtreme.ty", "CTRL_L_neck_stretch.ty", "CTRL_R_neck_stretch.ty", "CTRL_C_neck_swallow.ty", "CTRL_L_neck_mastoidContract.ty", "CTRL_R_neck_mastoidContract.ty", "CTRL_neck_throatUpDown.ty", "CTRL_neck_digastricUpDown.ty", "CTRL_neck_throatExhaleInhale.ty", "CTRL_C_teethU.ty", "CTRL_C_teethU.tx", "CTRL_C_teeth_fwdBackU.ty", "CTRL_C_teethD.ty", "CTRL_C_teethD.tx", "CTRL_C_teeth_fwdBackD.ty" };

static TArray<float> UEImageToHSImage(int32 InWidth, int32 InHeight, const uint8* InData, bool bInNorm)
{
	TArray<float> Output;
	Output.SetNumUninitialized(InWidth * InHeight * 3);

	const int32 FullSize = InHeight * InWidth;
	const int32 TwiceFullSize = 2 * FullSize;
	int32 OutputIndex = 0;
	const float Sqrt2 = FMath::Sqrt(2.f);
	const float ImageMean = 127.0f;
	const float ImageStd = 128.0f;

	for (int32 Y = 0; Y < InHeight; ++Y)
	{
		for (int32 X = 0; X < InWidth; ++X, ++OutputIndex, InData += 4)
		{
			const float Blue = InData[0];
			const float Green = InData[1];
			const float Red = InData[2];

			if (bInNorm)
			{
				Output[OutputIndex] = (((Red / 255.f) - 0.5) * Sqrt2);
				Output[OutputIndex + FullSize] = (((Green / 255.f) - 0.5) * Sqrt2);
				Output[OutputIndex + TwiceFullSize] = (((Blue / 255.f) - 0.5) * Sqrt2);
			}
			else
			{
				Output[OutputIndex] = (Red - ImageMean) / ImageStd;
				Output[OutputIndex + FullSize] = (Green - ImageMean) / ImageStd;
				Output[OutputIndex + TwiceFullSize] = (Blue - ImageMean) / ImageStd;
			}
		}
	}

	return Output;
}

static TArray<uint8> HSImageToUEImage(int32 InWidth, int32 InHeight, const TArray<float>& InData, bool bInNorm)
{
	TArray<uint8> Output;
	Output.SetNumUninitialized(InWidth * InHeight * 4);

	const int32 FullSize = InHeight * InWidth;
	const int32 TwiceFullSize = 2 * FullSize;
	int32 InputIndex = 0;
	int32 OutputIndex = 0;
	const float Sqrt2 = FMath::Sqrt(2.f);

	for (int32 Y = 0; Y < InHeight; ++Y)
	{
		for (int32 X = 0; X < InWidth; ++X, OutputIndex += 4, ++InputIndex)
		{
			if (bInNorm)
			{
				Output[OutputIndex] = ((InData[InputIndex + TwiceFullSize] / Sqrt2) + 0.5) * 255;
				Output[OutputIndex + 1] = ((InData[InputIndex + FullSize] / Sqrt2) + 0.5) * 255;
				Output[OutputIndex + 2] = ((InData[InputIndex] / Sqrt2) + 0.5) * 255;
			}
			else
			{
				Output[OutputIndex] = (InData[InputIndex + TwiceFullSize] * 128) + 127;
				Output[OutputIndex + 1] = (InData[InputIndex + FullSize] * 128) + 127;
				Output[OutputIndex + 2] = (InData[InputIndex] * 128) + 127;
			}

			Output[OutputIndex + 3] = 255;
		}
	}

	return Output;
}

#ifdef USE_OPENCV
static cv::Mat EigenToCV(const Eigen::Matrix<float, 2, 3>& InMatrix)
{
	cv::Mat Matrix(2, 3, CV_64FC1);

	for (int32 Row = 0; Row < 2; ++Row)
	{
		for (int32 Col = 0; Col < 3; ++Col)
		{
			Matrix.at<double>(Row, Col) = InMatrix(Row, Col);
		}
	}

	return Matrix;
}
#endif

// Start of head pose estimation code.
// This code is provided by a team who work outside of UE. As such the code does not follow UE coding standards.
// The code may change in future and to ease integrating any changes we are leaving the code in its original form.
// This code is internal to this file.

//  265 vertices, excluding three joints (corresponding to the indices from 1 to 3: two eyes and facial_c)
//  from the 268 'joints' landmark set
constexpr int32_t num_skull_points = 265;

constexpr int32_t num_joint_landmark_points = 268;

const Eigen::Matrix3f coordinate_shifter{ { 1.0f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } };

//  Average skull landmarks
const float skull_mean_shape_in_cm[265][3] = {
	{ 0.0, 0.0, 0.0 },
	{ 0.0, 9.064771, 10.714114 },
	{ -0.35572574, 9.080679, 10.711132 },
	{ -0.7047408, 9.118496, 10.695984 },
	{ -1.0408206, 9.162449, 10.663597 },
	{ -1.3883348, 9.212189, 10.604985 },
	{ -1.7732228, 9.268808, 10.508484 },
	{ -2.1940017, 9.320387, 10.375083 },
	{ -2.6446958, 9.350742, 10.209621 },
	{ -3.1249084, 9.347136, 10.009478 },
	{ -3.629829, 9.3028755, 9.7717705 },
	{ -4.129295, 9.224873, 9.500156 },
	{ -4.5853233, 9.11475, 9.202723 },
	{ -4.9580355, 8.953342, 8.901176 },
	{ -5.220229, 8.727922, 8.622347 },
	{ -5.392555, 8.456596, 8.378587 },
	{ -5.5086617, 8.160261, 8.170922 },
	{ -5.6075563, 7.8437386, 7.988229 },
	{ -5.676009, 7.501348, 7.8198385 },
	{ -5.701, 7.1431437, 7.683574 },
	{ -5.7117567, 6.7793913, 7.6047297 },
	{ -5.7293377, 6.411813, 7.5676765 },
	{ -5.770344, 6.042312, 7.540431 },
	{ -5.808794, 5.6755276, 7.5106544 },
	{ -5.8818693, 5.3314953, 7.442479 },
	{ -5.8900604, 5.0997906, 7.540354 },
	{ -5.850459, 4.851129, 7.687504 },
	{ -5.7870994, 4.560954, 7.8344274 },
	{ -5.7324915, 4.2184925, 7.9301834 },
	{ -5.6815443, 3.870152, 7.9868712 },
	{ -5.6333046, 3.549659, 8.015254 },
	{ -5.5926313, 3.2649684, 8.019542 },
	{ -5.557212, 3.0152194, 8.00358 },
	{ -5.5168014, 2.8029242, 7.9714527 },
	{ -5.2935505, 2.6744585, 8.127856 },
	{ -5.0189786, 2.6061401, 8.257733 },
	{ -4.6955824, 2.5821397, 8.341583 },
	{ -4.372637, 2.5599995, 8.3238945 },
	{ -4.0780163, 2.6005778, 8.351778 },
	{ -3.88686, 2.5945566, 8.353683 },
	{ -3.6997824, 2.560552, 8.3556385 },
	{ -3.507236, 2.4456468, 8.290636 },
	{ -3.3274715, 2.2612472, 8.162617 },
	{ -3.1821842, 2.0699625, 8.058613 },
	{ -3.0623298, 1.8707128, 7.982608 },
	{ -2.9588294, 1.6626211, 7.9338894 },
	{ -2.878439, 1.4493685, 7.9030886 },
	{ -2.8294172, 1.2333735, 7.87984 },
	{ -2.7857738, 1.0165255, 7.844682 },
	{ -2.7170043, 0.81728494, 7.786527 },
	{ -2.6226134, 0.65815085, 7.7157774 },
	{ -2.5723257, 0.6221302, 7.963957 },
	{ -2.509824, 0.5852673, 8.236382 },
	{ -2.416068, 0.54535383, 8.547819 },
	{ -2.2740936, 0.49588245, 8.895996 },
	{ -2.0807695, 0.44075966, 9.254065 },
	{ -1.8551157, 0.3948689, 9.576664 },
	{ -1.6178403, 0.35759974, 9.844225 },
	{ -1.3822787, 0.32201475, 10.057026 },
	{ -1.1463567, 0.28440598, 10.218607 },
	{ -0.89775455, 0.24438202, 10.340034 },
	{ -0.6360675, 0.20680138, 10.427404 },
	{ -0.3779509, 0.18347979, 10.486902 },
	{ -0.12577611, 0.17622426, 10.518052 },
	{ 0.12577611, 0.17622426, 10.518052 },
	{ 0.3779509, 0.18347979, 10.486902 },
	{ 0.6360675, 0.20680138, 10.427404 },
	{ 0.89775455, 0.24438202, 10.340034 },
	{ 1.1463567, 0.28440598, 10.218607 },
	{ 1.3822787, 0.32201475, 10.057026 },
	{ 1.6178403, 0.35759974, 9.844225 },
	{ 1.8551157, 0.3948689, 9.576664 },
	{ 2.0807695, 0.44075966, 9.254065 },
	{ 2.2740936, 0.49588245, 8.895996 },
	{ 2.416068, 0.54535383, 8.547819 },
	{ 2.509824, 0.5852673, 8.236382 },
	{ 2.5723257, 0.6221302, 7.963957 },
	{ 2.6226134, 0.65815085, 7.7157774 },
	{ 2.7170043, 0.81728494, 7.786527 },
	{ 2.7857738, 1.0165255, 7.844682 },
	{ 2.8294172, 1.2333735, 7.87984 },
	{ 2.878439, 1.4493685, 7.9030886 },
	{ 2.9588294, 1.6626211, 7.9338894 },
	{ 3.0623298, 1.8707128, 7.982608 },
	{ 3.1821842, 2.0699625, 8.058613 },
	{ 3.3274715, 2.2612472, 8.162617 },
	{ 3.507236, 2.4456468, 8.290636 },
	{ 3.6997824, 2.560552, 8.3556385 },
	{ 3.88686, 2.5945566, 8.353683 },
	{ 4.0780163, 2.6005778, 8.351778 },
	{ 4.372637, 2.5599995, 8.3238945 },
	{ 4.6955824, 2.5821397, 8.341583 },
	{ 5.0189786, 2.6061401, 8.257733 },
	{ 5.2935505, 2.6744585, 8.127856 },
	{ 5.5168014, 2.8029242, 7.9714527 },
	{ 5.557212, 3.0152194, 8.00358 },
	{ 5.5926313, 3.2649684, 8.019542 },
	{ 5.6333046, 3.549659, 8.015254 },
	{ 5.6815443, 3.870152, 7.9868712 },
	{ 5.7324915, 4.2184925, 7.9301834 },
	{ 5.7870994, 4.560954, 7.8344274 },
	{ 5.850459, 4.851129, 7.687504 },
	{ 5.8900604, 5.0997906, 7.540354 },
	{ 5.8818693, 5.3314953, 7.442479 },
	{ 5.808794, 5.6755276, 7.5106544 },
	{ 5.770344, 6.042312, 7.540431 },
	{ 5.7293377, 6.411813, 7.5676765 },
	{ 5.7117567, 6.7793913, 7.6047297 },
	{ 5.701, 7.1431437, 7.683574 },
	{ 5.676009, 7.501348, 7.8198385 },
	{ 5.6075563, 7.8437386, 7.988229 },
	{ 5.5086617, 8.160261, 8.170922 },
	{ 5.392555, 8.456596, 8.378587 },
	{ 5.220229, 8.727922, 8.622347 },
	{ 4.9580355, 8.953342, 8.901176 },
	{ 4.5853233, 9.11475, 9.202723 },
	{ 4.129295, 9.224873, 9.500156 },
	{ 3.629829, 9.3028755, 9.7717705 },
	{ 3.1249084, 9.347136, 10.009478 },
	{ 2.6446958, 9.350742, 10.209621 },
	{ 2.1940017, 9.320387, 10.375083 },
	{ 1.7732228, 9.268808, 10.508484 },
	{ 1.3883348, 9.212189, 10.604985 },
	{ 1.0408206, 9.162449, 10.663597 },
	{ 0.7047408, 9.118496, 10.695984 },
	{ 0.35572574, 9.080679, 10.711132 },
	{ 0.0, 8.656193, 10.753616 },
	{ 0.0, 8.286388, 10.707211 },
	{ 0.0, 7.974279, 10.603137 },
	{ 0.0, 7.7060194, 10.492035 },
	{ 0.0, 7.465375, 10.414276 },
	{ 0.0, 7.2579193, 10.383489 },
	{ 0.0, 7.0787296, 10.409057 },
	{ 0.0, 6.8568115, 10.502897 },
	{ 0.0, 6.535432, 10.665105 },
	{ 0.0, 6.194317, 10.847094 },
	{ 0.0, 5.927672, 11.005412 },
	{ 0.0, 5.4399314, 11.316417 },
	{ -0.10788158, 5.4006495, 11.294765 },
	{ -0.23085795, 5.290925, 11.231158 },
	{ -0.36310542, 5.12276, 11.131464 },
	{ -0.4975595, 4.9097185, 11.007837 },
	{ -0.62657154, 4.6727533, 10.874516 },
	{ -0.7626407, 4.430338, 10.755653 },
	{ -0.8835659, 4.2053957, 10.662713 },
	{ -0.9774412, 4.0077066, 10.586503 },
	{ -1.0508511, 3.8341677, 10.52065 },
	{ -1.1167439, 3.6729665, 10.458325 },
	{ -1.1830264, 3.5105667, 10.393387 },
	{ -1.2421821, 3.327695, 10.328738 },
	{ -1.2816288, 3.1129715, 10.2699585 },
	{ -1.2914957, 2.891846, 10.224287 },
	{ -1.2845047, 2.6779797, 10.174585 },
	{ -1.184876, 2.4228315, 10.117454 },
	{ -0.8812735, 2.2135143, 10.191338 },
	{ -0.5612408, 2.134215, 10.306191 },
	{ -0.27084506, 2.0974174, 10.397005 },
	{ 0.0, 2.0887902, 10.434594 },
	{ 0.27084506, 2.0974174, 10.397005 },
	{ 0.5612408, 2.134215, 10.306191 },
	{ 0.8812735, 2.2135143, 10.191338 },
	{ 1.184876, 2.4228315, 10.117454 },
	{ 1.2845047, 2.6779797, 10.174585 },
	{ 1.2914957, 2.891846, 10.224287 },
	{ 1.2816288, 3.1129715, 10.2699585 },
	{ 1.2421821, 3.327695, 10.328738 },
	{ 1.1830264, 3.5105667, 10.393387 },
	{ 1.1167439, 3.6729665, 10.458325 },
	{ 1.0508511, 3.8341677, 10.52065 },
	{ 0.9774412, 4.0077066, 10.586503 },
	{ 0.8835659, 4.2053957, 10.662713 },
	{ 0.7626407, 4.430338, 10.755653 },
	{ 0.62657154, 4.6727533, 10.874516 },
	{ 0.4975595, 4.9097185, 11.007837 },
	{ 0.36310542, 5.12276, 11.131464 },
	{ 0.23085795, 5.290925, 11.231158 },
	{ 0.10788158, 5.4006495, 11.294765 },
	{ -5.001498, 7.7343926, 8.755064 },
	{ -4.8664846, 7.88568, 8.959026 },
	{ -4.673849, 8.028105, 9.15465 },
	{ -4.4140997, 8.160458, 9.340817 },
	{ -4.0832024, 8.277138, 9.516504 },
	{ -3.7156425, 8.369957, 9.67105 },
	{ -3.349185, 8.429964, 9.799297 },
	{ -2.9753819, 8.453596, 9.915215 },
	{ -2.5792737, 8.433409, 10.029807 },
	{ -2.1735454, 8.346458, 10.141714 },
	{ -1.7763654, 8.185146, 10.241987 },
	{ -1.4181951, 8.007441, 10.315553 },
	{ -1.1735082, 7.8066883, 10.277129 },
	{ -0.9743082, 7.6422005, 10.2764225 },
	{ -0.83730644, 7.408587, 10.238022 },
	{ -0.7437445, 7.1518135, 10.205219 },
	{ -0.678272, 6.907565, 10.2002125 },
	{ -0.6530571, 6.6508846, 10.200544 },
	{ -0.68997055, 6.3666496, 10.165792 },
	{ -0.7841004, 6.092394, 10.104666 },
	{ -0.9407326, 5.851326, 10.027961 },
	{ -1.1434646, 5.6361647, 9.955016 },
	{ -1.3559535, 5.4395056, 9.892996 },
	{ -1.5756254, 5.2666883, 9.837721 },
	{ -1.8114226, 5.118995, 9.780801 },
	{ -2.0749743, 4.993431, 9.709002 },
	{ -2.3753853, 4.887937, 9.615028 },
	{ -2.7019386, 4.7963057, 9.510266 },
	{ -3.0426493, 4.714306, 9.417286 },
	{ -3.390891, 4.6404657, 9.344476 },
	{ -3.7366953, 4.5790286, 9.286493 },
	{ -4.064962, 4.5603933, 9.206291 },
	{ -4.380662, 4.54904, 9.078661 },
	{ -4.660027, 4.6751533, 8.897423 },
	{ -4.89849, 4.854555, 8.6982765 },
	{ -5.077415, 5.08823, 8.513255 },
	{ -5.186071, 5.3796444, 8.369019 },
	{ -5.2440014, 5.718604, 8.258554 },
	{ -5.2729993, 6.085272, 8.179075 },
	{ -5.267421, 6.4395447, 8.156724 },
	{ -5.237665, 6.765045, 8.195181 },
	{ -5.1980534, 7.062338, 8.280062 },
	{ -5.15299, 7.329048, 8.402868 },
	{ -5.0920963, 7.555195, 8.562624 },
	{ 5.001498, 7.7343926, 8.755064 },
	{ 4.8664846, 7.88568, 8.959026 },
	{ 4.673849, 8.028105, 9.15465 },
	{ 4.4140997, 8.160458, 9.340817 },
	{ 4.0832024, 8.277138, 9.516504 },
	{ 3.7156425, 8.369957, 9.67105 },
	{ 3.349185, 8.429964, 9.799297 },
	{ 2.9753819, 8.453596, 9.915215 },
	{ 2.5792737, 8.433409, 10.029807 },
	{ 2.1735454, 8.346458, 10.141714 },
	{ 1.7763654, 8.185146, 10.241987 },
	{ 1.4181951, 8.007441, 10.315553 },
	{ 1.1735082, 7.8066883, 10.277129 },
	{ 0.9743082, 7.6422005, 10.2764225 },
	{ 0.83730644, 7.408587, 10.238022 },
	{ 0.7437445, 7.1518135, 10.205219 },
	{ 0.678272, 6.907565, 10.2002125 },
	{ 0.6530571, 6.6508846, 10.200544 },
	{ 0.68997055, 6.3666496, 10.165792 },
	{ 0.7841004, 6.092394, 10.104666 },
	{ 0.9407326, 5.851326, 10.027961 },
	{ 1.1434646, 5.6361647, 9.955016 },
	{ 1.3559535, 5.4395056, 9.892996 },
	{ 1.5756254, 5.2666883, 9.837721 },
	{ 1.8114226, 5.118995, 9.780801 },
	{ 2.0749743, 4.993431, 9.709002 },
	{ 2.3753853, 4.887937, 9.615028 },
	{ 2.7019386, 4.7963057, 9.510266 },
	{ 3.0426493, 4.714306, 9.417286 },
	{ 3.390891, 4.6404657, 9.344476 },
	{ 3.7366953, 4.5790286, 9.286493 },
	{ 4.064962, 4.5603933, 9.206291 },
	{ 4.380662, 4.54904, 9.078661 },
	{ 4.660027, 4.6751533, 8.897423 },
	{ 4.89849, 4.854555, 8.6982765 },
	{ 5.077415, 5.08823, 8.513255 },
	{ 5.186071, 5.3796444, 8.369019 },
	{ 5.2440014, 5.718604, 8.258554 },
	{ 5.2729993, 6.085272, 8.179075 },
	{ 5.267421, 6.4395447, 8.156724 },
	{ 5.237665, 6.765045, 8.195181 },
	{ 5.1980534, 7.062338, 8.280062 },
	{ 5.15299, 7.329048, 8.402868 },
	{ 5.0920963, 7.555195, 8.562624 },
};

const Eigen::Matrix3Xf mat_skull_mean_shape_in_cm = Eigen::Map<const Eigen::Matrix<float, num_skull_points, 3, Eigen::RowMajor>>(skull_mean_shape_in_cm[0]).transpose();


/**
 *  @brief estimate the head rotation,
 *  @param in_image_width   The image frame width in pixels
 *  @param in_image_height  The image frame height in pixels
 *  @param in_focal         The camera focal length in pixels
 *  @param in_joint_landmarks The 'joints' landmarks consisting of 268 points within the image coordinate space in pixels (x==0 for left, y==0 for top)
 *  @param in_head_rotation   The 9 floating-point values of the 'head_pose' output
 *  @param in_translation     The head translation value of the previous frame to accelerate the computation, put a zero vector if you are unsure.
 *  @param out_head_rotation  The refined value of the head rotation. We advise ignore this value, and use the neural net output instead.
 *  @param out_translation    The x, y, z coordinate in the centimeter unit in the 3D space.
 *                            +x for the left ear, +y for the head top, +z for the face front. So the z value should typically be negative. (smaller the further)
 *  @return The error metric in the squared sum of image-space coordinate difference values.
 */
static void estimate_head_pose(
	const float in_image_width,
	const float in_image_height,
	const float in_focal,
	const Eigen::Matrix2Xf& in_joint_landmarks,
	const Eigen::Matrix3f& in_head_rotation,
	const Eigen::Vector3f& in_translation,
	Eigen::Matrix3f& out_rotation,
	Eigen::Vector3f& out_translation)
{
	//  Negate the x coordinate for correct alignment
	Eigen::Matrix3f intrinsic{
		{ -in_focal, 0.0f, in_image_width * 0.5f },
		{ 0.0f, in_focal, in_image_height * 0.5f },
		{ 0.0f, 0.0, 1.0f }
	};

	Eigen::Matrix3f intrinsic_inv = intrinsic.inverse();

	Eigen::Matrix2Xf S(2, num_skull_points);
	for (int i = 0; i < num_skull_points; ++i)
	{
		int j = (i == 0) ? 0 : i + 3;
		S.col(i) = (intrinsic_inv * in_joint_landmarks.col(j).homogeneous()).segment(0, 2);
	}

	float f = 1.0f;
	Eigen::Matrix3f R = coordinate_shifter * in_head_rotation.transpose();
	Eigen::Vector3f t = in_translation;

	//  If not set, (zero) set it to a default value
	if (t[2] >= 0.0f)
	{
		const float z_init = -50.0f;
		t = { S(0, 0) * z_init, S(0, 1) * z_init, z_init };
	}

	const int num_iterations = 20;

	Eigen::VectorXf X_0[6];
	Eigen::VectorXf Y_0;

	Eigen::VectorXf X_1[6];
	Eigen::VectorXf Y_1;

	for (int it = 0; it < num_iterations; ++it)
	{

		Eigen::MatrixXf XTX_0 = Eigen::MatrixXf::Zero(6, 6);
		Eigen::VectorXf XTY_0 = Eigen::VectorXf::Zero(6);

		Eigen::MatrixXf XTX_1 = Eigen::MatrixXf::Zero(6, 6);
		Eigen::VectorXf XTY_1 = Eigen::VectorXf::Zero(6);

		Eigen::Matrix3Xf P = R * mat_skull_mean_shape_in_cm;

		X_0[0] = Eigen::VectorXf::Constant(num_skull_points, f);
		X_0[1] = Eigen::VectorXf::Constant(num_skull_points, 0.0f);
		X_0[2] = -S.row(0).array() + f * t[0] / t[2];
		X_0[3] = f * P.row(0);
		X_0[4] = f * P.row(2).array() + P.row(0).array() * S.row(0).array();
		X_0[5] = P.row(1).array() * S.row(0).array();

		Y_0 = S.row(0).array() * (P.row(2).array() + t[2]) - f * (P.row(0).array() + t[0]);

		X_1[0] = Eigen::VectorXf::Constant(num_skull_points, 0.0f);
		X_1[1] = Eigen::VectorXf::Constant(num_skull_points, f);
		X_1[2] = -S.row(1).array() + f * t[1] / t[2];
		X_1[3] = -f * P.row(0);
		X_1[4] = P.row(0).array() * S.row(1).array();
		X_1[5] = f * P.row(2).array() + P.row(1).array() * S.row(1).array();

		Y_1 = S.row(1).array() * (P.row(2).array() + t[2]) - f * (P.row(1).array() + t[1]);

		for (int i = 0; i < 6; ++i)
		{
			for (int j = 0; j < 6; ++j)
			{
				if (j < i)
				{
					XTX_0(i, j) = XTX_0(j, i);
					XTX_1(i, j) = XTX_1(j, i);
				}
				else
				{
					XTX_0(i, j) = X_0[i].dot(X_0[j]);
					XTX_1(i, j) = X_1[i].dot(X_1[j]);
				}
			}
			XTY_0[i] = X_0[i].dot(Y_0);
			XTY_1[i] = X_1[i].dot(Y_1);
		}

		Eigen::VectorXf output = (XTX_0 + XTX_1).inverse() * (XTY_0 + XTY_1);

		float dx = output[0];
		float dy = output[1];
		float dz = output[2];

		float da = output[3];
		float db = output[4];
		float dc = output[5];

		float tx = t[0];
		float ty = t[1];
		float tz = t[2];

		dx += dz * tx / tz;
		dy += dz * ty / tz;

		t += Eigen::Vector3f{ dx, dy, dz };

		Eigen::Matrix3f Rp{
			{ 1.0f, da, db },
			{ -da, 1.0f, dc },
			{ -db, -dc, 1.0f }
		};

		Eigen::JacobiSVD<Eigen::Matrix3f> svd(Rp * R, Eigen::ComputeFullU | Eigen::ComputeFullV);
		R = (svd.matrixU()) * (svd.matrixV().transpose());
	}

	out_rotation = (coordinate_shifter * R).transpose();
	out_translation = t;
}


//  A helper function for find_focal_length
static float _find_focal_length_worker(
	int in_image_width, int in_image_height,
	float in_focal,
	const std::vector<Eigen::Matrix2Xf>& in_joint_landmarks,
	const Eigen::Matrix3f& in_head_rotation)
{
	float result = 0.0f;

	for (const Eigen::Matrix2Xf& landmarks : in_joint_landmarks)
	{
		Eigen::Matrix3f rotation;
		Eigen::Vector3f translation;
		estimate_head_pose(in_image_width, in_image_height, in_focal, landmarks, in_head_rotation, { 0.0f, 0.0f, 0.0f }, rotation, translation);

		Eigen::Matrix3f intrinsic{
			{ -in_focal, 0.0f, in_image_width * 0.5f },
			{ 0.0f, in_focal, in_image_height * 0.5f },
			{ 0.0f, 0.0, 1.0f }
		};

		Eigen::MatrixXf out_head_pose(3, 4);

		out_head_pose.block(0, 0, 3, 3) = coordinate_shifter * rotation.transpose();
		out_head_pose.block(0, 3, 3, 1) = translation;

		Eigen::Matrix3Xf landmarks_inferred = intrinsic * out_head_pose * mat_skull_mean_shape_in_cm.colwise().homogeneous();
		float sum_squares = 0.0f;
		for (int i = 0; i < 1; ++i)
		{
//			int j = (i == 0) ? 0 : i + 3; // build waring fix
			int j = 0; // build warning fix
			sum_squares += (landmarks_inferred.col(i).hnormalized() - landmarks.col(j)).squaredNorm();
		}

		result += sum_squares;
	}

	return result;
}

/**
 *  @brief estimate the focal length from joint landmarks, using ternary search
 *  @param in_image_width   The image frame width in pixels
 *  @param in_image_height  The image frame height in pixels
 *  @param in_joint_landmarks The 'joints' landmarks consisting of 268 points within the image coordinate space in pixels (x==0 for left, y==0 for top)
 *  @param in_head_rotation   The 9 floating-point values of the 'head_pose' output
 *  @return estimated focal value
 */
static float find_focal_length(int in_image_width, int in_image_height,
	const std::vector<Eigen::Matrix2Xf>& in_joint_landmarks,
	const Eigen::Matrix3f& in_head_rotation)
{
	float diagonal = sqrtf(in_image_width * in_image_width + in_image_height * in_image_height);

	float focal_low = diagonal * (10.0 / 43.27); // mimics a 10mm wide angle lens
	float focal_high = diagonal * (100.0 / 43.27); // mimics a 100mm zoom lens

	constexpr int num_iterations = 30;

	for (int it = 0; it < num_iterations; ++it)
	{
		//  Use harmonic means
		float focal_a = 3.0f / (2.0f / focal_low + 1.0f / focal_high);
		float focal_b = 3.0f / (1.0f / focal_low + 2.0f / focal_high);

		float error_a = _find_focal_length_worker(in_image_width, in_image_height, focal_a, in_joint_landmarks, in_head_rotation);
		float error_b = _find_focal_length_worker(in_image_width, in_image_height, focal_b, in_joint_landmarks, in_head_rotation);

		if (error_a < error_b)
		{
			focal_high = focal_b;
		}
		else
		{
			focal_low = focal_a;
		}
	}

	return 2.0f / (1.0f / focal_low + 1.0f / focal_high);
}

// End of head pose estimation code



FHyprsenseRealtimeNode::FHyprsenseRealtimeNode(const FString& InName) : FNode("HyprsenseRealtimeNode", InName)
{
	check(SolverControlNames.Num() == 174);

	Pins.Add(FPin("UE Image In", EPinDirection::Input, EPinType::UE_Image));
	Pins.Add(FPin("Neutral Frame In", EPinDirection::Input, EPinType::Bool));
	Pins.Add(FPin("Animation Out", EPinDirection::Output, EPinType::Animation));
	Pins.Add(FPin("Confidence Out", EPinDirection::Output, EPinType::Float, 0));
	Pins.Add(FPin("Debug UE Image Out", EPinDirection::Output, EPinType::UE_Image));
	Pins.Add(FPin("State Out", EPinDirection::Output, EPinType::Int));
	Pins.Add(FPin("Focal Length Out", EPinDirection::Output, EPinType::Float, 1));
}

void FHyprsenseRealtimeNode::SetDebugImage(EHyprsenseRealtimeNodeDebugImage InDebugImage)
{
	FScopeLock Lock(&DebugImageMutex);

	DebugImage = InDebugImage;
}

EHyprsenseRealtimeNodeDebugImage FHyprsenseRealtimeNode::GetDebugImage()
{
	FScopeLock Lock(&DebugImageMutex);

	return DebugImage;
}

void FHyprsenseRealtimeNode::SetFocalLength(float InFocalLength)
{
	FScopeLock Lock(&FocalLengthMutex);

	FocalLength = InFocalLength;
}

float FHyprsenseRealtimeNode::GetFocalLength()
{
	FScopeLock Lock(&FocalLengthMutex);

	return FocalLength;
}

void FHyprsenseRealtimeNode::SetHeadStabilization(bool bInHeadStabilization)
{
	bHeadStabilization = bInHeadStabilization;
}

bool FHyprsenseRealtimeNode::GetHeadStabilization() const
{
	return bHeadStabilization;
}

bool FHyprsenseRealtimeNode::LoadModels()
{
	using namespace UE::NNE;

	// Where should the NNE model live! For now search in a number of plugins to find it.
	UNNEModelData* FaceDetectorModelData = LoadObject<UNNEModelData>(GetTransientPackage(), TEXT("/MetaHumanCoreTech/GenericTracker/FaceDetector.FaceDetector"));
	if (!FaceDetectorModelData)
	{
		UE_LOG(LogMetaHumanPipeline, Warning, TEXT("Failed to load face detector model"));
		return false;
	}
	
	UNNEModelData* HeadposeModelData = LoadObject<UNNEModelData>(GetTransientPackage(), TEXT("/MetaHumanCoreTech/RealtimeMono/landmark_tracker_v0_6.landmark_tracker_v0_6"));
	if (!HeadposeModelData)
	{
		UE_LOG(LogMetaHumanPipeline, Warning, TEXT("Failed to load headpose model"));
		return false;
	}

	UNNEModelData* SolverModelData = LoadObject<UNNEModelData>(GetTransientPackage(), TEXT("/MetaHumanCoreTech/RealtimeMono/generic_rig_solver_v0_5_pt_onnx_fp16.generic_rig_solver_v0_5_pt_onnx_fp16"));
	if (!SolverModelData)
	{
		UE_LOG(LogMetaHumanPipeline, Warning, TEXT("Failed to load solver model"));
		return false;
	}

	TWeakInterfacePtr<INNERuntimeGPU> Runtime = GetRuntime<INNERuntimeGPU>("NNERuntimeORTDml");
	if (!Runtime.IsValid())
	{
		UE_LOG(LogMetaHumanPipeline, Warning, TEXT("Failed to get runtime"));
		return false;
	}

	FaceDetector = Runtime->CreateModelGPU(FaceDetectorModelData)->CreateModelInstanceGPU();
	if (!FaceDetector)
	{
		UE_LOG(LogMetaHumanPipeline, Warning, TEXT("Failed to create face detector"));
		return false;
	}

	Headpose = Runtime->CreateModelGPU(HeadposeModelData)->CreateModelInstanceGPU();
	if (!Headpose)
	{
		UE_LOG(LogMetaHumanPipeline, Warning, TEXT("Failed to create head pose"));
		return false;
	}

	Solver = Runtime->CreateModelGPU(SolverModelData)->CreateModelInstanceGPU();
	if (!Solver)
	{
		UE_LOG(LogMetaHumanPipeline, Warning, TEXT("Failed to create solver"));
		return false;
	}

	FTensorShape TensorShape = FTensorShape::Make({ 1, 3, DetectorInputSizeY, DetectorInputSizeX });
	if (FaceDetector->SetInputTensorShapes({ TensorShape }) != UE::NNE::EResultStatus::Ok)
	{
		UE_LOG(LogMetaHumanPipeline, Warning, TEXT("Failed to set face detector input"));
		return false;
	}

	TensorShape = FTensorShape::Make({ 1, 3, HeadposeInputSizeY, HeadposeInputSizeX });
	if (Headpose->SetInputTensorShapes({ TensorShape }) != UE::NNE::EResultStatus::Ok)
	{
		UE_LOG(LogMetaHumanPipeline, Warning, TEXT("Failed to set headpose input"));
		return false;
	}

	TensorShape = FTensorShape::Make({ 1, 3, SolverInputSizeY, SolverInputSizeX });
	if (Solver->SetInputTensorShapes({ TensorShape }) != UE::NNE::EResultStatus::Ok)
	{
		UE_LOG(LogMetaHumanPipeline, Warning, TEXT("Failed to set solver input"));
		return false;
	}

	return true;
}

bool FHyprsenseRealtimeNode::Start(const TSharedPtr<FPipelineData>& InPipelineData)
{
	if (!FaceDetector || !Headpose || !Solver)
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToInitialize);
		InPipelineData->SetErrorNodeMessage(TEXT("Failed to initialize"));
		return false;
	}

	bFaceDetected = false;

	HeadTranslation = FVector::ZeroVector;

	return true;
}

bool FHyprsenseRealtimeNode::Process(const TSharedPtr<FPipelineData>& InPipelineData)
{
	const FUEImageDataType& Input = InPipelineData->GetData<FUEImageDataType>(Pins[0]);
	const bool bIsNeutralFrame = InPipelineData->GetData<bool>(Pins[1]);
	float FocalLengthCopy = GetFocalLength();
	if (FocalLengthCopy < 0)
	{
		// Assume a 60 degree field of view if no focal length is set.
		constexpr float Tan30Times2 = 0.5774f * 2.f;
		FocalLengthCopy = FMath::Sqrt((float) (Input.Width * Input.Width + Input.Height * Input.Height)) / Tan30Times2;
	}

	FFrameAnimationData AnimOut;
	FUEImageDataType DebugImageOut;
	EHyprsenseRealtimeNodeState State = EHyprsenseRealtimeNodeState::Unknown;
	TArray<UE::NNE::FTensorBindingCPU> Inputs, Outputs;
	bool bHaveFace = false;
	float FaceScore = 0;
	Matrix23f HeadposeTransform;
#ifdef USE_OPENCV
	cv::Mat HeadposeTransformCV, HeadposeTransformInvCV;
#endif

	const EHyprsenseRealtimeNodeDebugImage DebugImageCopy = GetDebugImage();

	if (DebugImageCopy == EHyprsenseRealtimeNodeDebugImage::Input)
	{
		DebugImageOut = Input;
	}

	if (bFaceDetected)
	{
		Matrix23f HeadposeTransformInv;
		HeadposeTransform = GetTransformFromPoints(TrackingPoints, FVector2D(HeadposeInputSizeX, HeadposeInputSizeY), false, HeadposeTransformInv);
		bHaveFace = true;

		if (DebugImageCopy == EHyprsenseRealtimeNodeDebugImage::FaceDetect)
		{
			DebugImageOut.Width = DetectorInputSizeX;
			DebugImageOut.Height = DetectorInputSizeY;
			DebugImageOut.Data.SetNumZeroed(DebugImageOut.Width * DebugImageOut.Height * 4);
		}

#ifdef USE_OPENCV
		HeadposeTransformCV = EigenToCV(HeadposeTransform);
		HeadposeTransformInvCV = EigenToCV(HeadposeTransformInv);
#endif
	}
	else
	{
		// Prepare image for face detector
		const Bbox DetectorBox = { 0, 0, 1.f, 1.f };
		const Matrix23f DetectorTransform = GetTransformFromBbox(DetectorBox, Input.Width, Input.Height, DetectorInputSizeX, 0.0f, false, PartType::FaceDetector);
		TArray<float> DetectorInputArray = WarpAffineBilinear(Input.Data.GetData(), Input.Width, Input.Height, DetectorTransform, DetectorInputSizeX, DetectorInputSizeY, true);

#ifdef USE_OPENCV
		const cv::Mat InputCV(Input.Height, Input.Width, CV_8UC4, (uchar*)Input.Data.GetData());
		cv::Mat DetectorInputCV;
		cv::resize(InputCV, DetectorInputCV, cv::Size(DetectorInputSizeX, DetectorInputSizeY));
		DetectorInputArray = UEImageToHSImage(DetectorInputSizeX, DetectorInputSizeY, DetectorInputCV.data, false);
#endif

		if (DebugImageCopy == EHyprsenseRealtimeNodeDebugImage::FaceDetect)
		{
			DebugImageOut.Width = DetectorInputSizeX;
			DebugImageOut.Height = DetectorInputSizeY;
			DebugImageOut.Data = HSImageToUEImage(DebugImageOut.Width, DebugImageOut.Height, DetectorInputArray, false);
		}

		// Prepare output of face detector
		const int32 DetectorOutSize = 4212;
		TArray<float> Scores, Boxes;
		Scores.SetNumUninitialized(1 * DetectorOutSize * 2);
		Boxes.SetNumUninitialized(1 * DetectorOutSize * 4);

		// Run face detector
		Inputs = { {(void*)DetectorInputArray.GetData(), DetectorInputArray.Num() * sizeof(float)} };
		Outputs = { {(void*)Scores.GetData(), Scores.Num() * sizeof(float)}, {(void*)Boxes.GetData(), Boxes.Num() * sizeof(float)} };

		if (FaceDetector->RunSync(Inputs, Outputs) != UE::NNE::IModelInstanceGPU::ERunSyncStatus::Ok)
		{
			InPipelineData->SetErrorNodeCode(ErrorCode::FailedToDetect);
			InPipelineData->SetErrorNodeMessage(TEXT("Failed to face detect"));
			return false;
		}

		const float IouThreshold = 0.45f;
		const float ProbThreshold = 0.3f;
		const int32 TopK = 10;

		// Calculate the most accurate face by score
		const TArray<Bbox> ResultBoxes = HardNMS(Scores, Boxes, IouThreshold, ProbThreshold, DetectorOutSize, TopK);
		if (ResultBoxes.IsEmpty())
		{
			UE_LOG(LogMetaHumanPipeline, Verbose, TEXT("No face detected"));
			State = EHyprsenseRealtimeNodeState::NoFace;
		}
		else
		{
			Bbox Face = ResultBoxes[0];

			bFaceDetected = true;
			bHaveFace = true;

			// Calculate image transform for headpose stage
			HeadposeTransform = GetTransformFromBbox(Face, Input.Width, Input.Height, HeadposeInputSizeX, 0.0f, false, PartType::SparseTracker);

			Face.X1 *= Input.Width;
			Face.X2 *= Input.Width;
			Face.Y1 *= Input.Height;
			Face.Y2 *= Input.Height;

#ifdef PRINT_RESULTS
			UE_LOG(LogTemp, Warning, TEXT("JGC FACE1 %f"), Face.X1);
			UE_LOG(LogTemp, Warning, TEXT("JGC FACE2 %f"), Face.Y1);
			UE_LOG(LogTemp, Warning, TEXT("JGC FACE3 %f"), Face.X2 - Face.X1);
			UE_LOG(LogTemp, Warning, TEXT("JGC FACE4 %f"), Face.Y2 - Face.Y1);
#endif

#ifdef USE_OPENCV
			HeadposeTransformInvCV = cv::Mat::eye(3, 3, CV_64FC1);

			const double W = Face.X2 - Face.X1;
			const double H = Face.Y2 - Face.Y1;
			const double CX = Face.X1 + 0.5 * W;
			const double CY = Face.Y1 + 0.5 * H;
			const double Size = FMath::Sqrt(double(W * W + H * H)) * 256.l / 192.l;
			const double Scale = HeadposeInputSizeX / Size;

			HeadposeTransformInvCV.at<double>(0, 0) = Scale;
			HeadposeTransformInvCV.at<double>(0, 2) = HeadposeInputSizeX / 2 - Scale * CX;
			HeadposeTransformInvCV.at<double>(1, 1) = Scale;
			HeadposeTransformInvCV.at<double>(1, 2) = HeadposeInputSizeY / 2 - Scale * CY;

			HeadposeTransformCV = HeadposeTransformInvCV.inv();
			HeadposeTransformInvCV = HeadposeTransformInvCV(cv::Rect(0, 0, 3, 2));
#endif
		}
	}

	if (bHaveFace)
	{
		// Prepare image for headpose
		TArray<float> HeadposeInputArray = WarpAffineBilinear(Input.Data.GetData(), Input.Width, Input.Height, HeadposeTransform, HeadposeInputSizeX, HeadposeInputSizeY, false);

#ifdef USE_OPENCV
		const cv::Mat InputCV(Input.Height, Input.Width, CV_8UC4, (uchar*)Input.Data.GetData());
		cv::Mat HeadposeInputCV;

		cv::warpAffine(InputCV, HeadposeInputCV, HeadposeTransformInvCV, cv::Size(HeadposeInputSizeX, HeadposeInputSizeY), cv::INTER_LANCZOS4);

		HeadposeInputArray = UEImageToHSImage(HeadposeInputSizeX, HeadposeInputSizeY, HeadposeInputCV.data, true);
#endif

		// Prepare output of headpose
		TArray<float> Points, Pose, Rigid;
		Points.SetNumUninitialized(1573 * 2);
		Pose.SetNumUninitialized(9);
		Rigid.SetNumUninitialized(268 * 2);

		Inputs = { {(void*)HeadposeInputArray.GetData(), HeadposeInputArray.Num() * sizeof(float)} };
		Outputs = { {(void*)Points.GetData(), Points.Num() * sizeof(float)}, {(void*)&FaceScore, sizeof(float)}, {(void*)Pose.GetData(), Pose.Num() * sizeof(float)}, {(void*)Rigid.GetData(), Rigid.Num() * sizeof(float)} };

		// Run head pose
		if (Headpose->RunSync(Inputs, Outputs) != UE::NNE::EResultStatus::Ok)
		{
			InPipelineData->SetErrorNodeCode(ErrorCode::FailedToTrack);
			InPipelineData->SetErrorNodeMessage(TEXT("Failed to track"));
			return false;
		}

		for (int32 PointIndex = 0; PointIndex < 268; ++PointIndex)
		{
			const Eigen::Vector3f Point((Rigid[PointIndex * 2] + 0.5) * HeadposeInputSizeX, (Rigid[PointIndex * 2 + 1] + 0.5) * HeadposeInputSizeY, 1.0f);
			const Eigen::Vector2f Transformed = HeadposeTransform * Point;

			Rigid[PointIndex * 2] = Transformed[0];
			Rigid[PointIndex * 2 + 1] = Transformed[1];
		}

		const Eigen::Matrix3f HeadPose = Eigen::Map<const Eigen::Matrix<float, 3, 3, Eigen::RowMajor>>(Pose.GetData()).transpose();
		const Eigen::Matrix2Xf JointLandmarks = Eigen::Map<const Eigen::Matrix<float, 268, 2, Eigen::RowMajor>>(Rigid.GetData()).transpose();

		if (bIsNeutralFrame)
		{
			FocalLengthCopy = find_focal_length(Input.Width, Input.Height, { JointLandmarks }, HeadPose);
			SetFocalLength(FocalLengthCopy);
		}

		if (FocalLengthCopy > 0)
		{
			Eigen::Vector3f PreviousTranslation;
			Eigen::Matrix3f NewRotation;
			Eigen::Vector3f NewTranslation;

			PreviousTranslation(0) = HeadTranslation.X;
			PreviousTranslation(1) = HeadTranslation.Y;
			PreviousTranslation(2) = HeadTranslation.Z;

			estimate_head_pose(Input.Width, Input.Height, FocalLengthCopy, JointLandmarks, HeadPose, PreviousTranslation, NewRotation, NewTranslation);

			HeadTranslation.X = NewTranslation(0);
			HeadTranslation.Y = NewTranslation(1);
			HeadTranslation.Z = NewTranslation(2);
		}

		if (FaceScore <= FaceScoreThreshold)
		{
			bFaceDetected = false;
			UE_LOG(LogMetaHumanPipeline, Verbose, TEXT("No face detected"));
			State = EHyprsenseRealtimeNodeState::NoFace;
		}
		else
		{
#ifdef PRINT_RESULTS
			UE_LOG(LogTemp, Warning, TEXT("JGC LANDMARKS1 %f"), Points[0]);
			UE_LOG(LogTemp, Warning, TEXT("JGC LANDMARKS2 %f"), Points[1]);
#endif

			// Tracking points in headpose image coords
			TrackingPoints.Reset();
			for (int32 Point = 0; Point < Points.Num(); Point += 2)
			{
				TrackingPoints.Add(FVector2D((Points[Point] + 0.5) * HeadposeInputSizeX, (Points[Point + 1] + 0.5) * HeadposeInputSizeY));
			}

#ifdef PRINT_RESULTS
			UE_LOG(LogTemp, Warning, TEXT("JGC LANDMARKS3 %f"), TrackingPoints[0].X);
			UE_LOG(LogTemp, Warning, TEXT("JGC LANDMARKS4 %f"), TrackingPoints[0].Y);
#endif

			if (DebugImageCopy == EHyprsenseRealtimeNodeDebugImage::Headpose)
			{
				DebugImageOut.Width = HeadposeInputSizeX;
				DebugImageOut.Height = HeadposeInputSizeY;
				DebugImageOut.Data = HSImageToUEImage(DebugImageOut.Width, DebugImageOut.Height, HeadposeInputArray, true);

				epic::core::BurnPointsIntoImage(TrackingPoints, DebugImageOut.Width, DebugImageOut.Height, DebugImageOut.Data, 0, 0, 255, 1);
			}

			// Tracking points in input image coords
			for (FVector2D& TrackingPoint : TrackingPoints)
			{
#ifdef USE_OPENCV
				cv::Mat Point(1, 1, CV_64FC3);
				Point.at<cv::Vec3d>(0, 0)[0] = TrackingPoint.X;
				Point.at<cv::Vec3d>(0, 0)[1] = TrackingPoint.Y;
				Point.at<cv::Vec3d>(0, 0)[2] = 1;

				cv::Mat Transformed;
				cv::transform(Point, Transformed, HeadposeTransformCV);

				TrackingPoint.X = Transformed.at<cv::Vec3d>(0, 0)[0];
				TrackingPoint.Y = Transformed.at<cv::Vec3d>(0, 0)[1];
#else
				const Eigen::Vector3f Point(TrackingPoint.X, TrackingPoint.Y, 1.0f);
				const Eigen::Vector2f Transformed = HeadposeTransform * Point;

				TrackingPoint.X = Transformed[0];
				TrackingPoint.Y = Transformed[1];
#endif
			}

#ifdef PRINT_RESULTS
			UE_LOG(LogTemp, Warning, TEXT("JGC LANDMARKS8 %f"), TrackingPoints[0].X);
			UE_LOG(LogTemp, Warning, TEXT("JGC LANDMARKS9 %f"), TrackingPoints[0].Y);
#endif

			if (DebugImageCopy == EHyprsenseRealtimeNodeDebugImage::Trackers)
			{
				DebugImageOut.Width = Input.Width;
				DebugImageOut.Height = Input.Height;
				DebugImageOut.Data = Input.Data;

				epic::core::BurnPointsIntoImage(TrackingPoints, DebugImageOut.Width, DebugImageOut.Height, DebugImageOut.Data, 0, 0, 255, 2);
			}

			// Prepare image for solver
			Matrix23f SolverTransformInv;
			const Matrix23f SolverTransform = GetTransformFromPoints(TrackingPoints, FVector2D(SolverInputSizeX, SolverInputSizeY), true, SolverTransformInv);

			TArray<float> SolverInputArray = WarpAffineBilinear(Input.Data.GetData(), Input.Width, Input.Height, SolverTransform, SolverInputSizeX, SolverInputSizeY, false);

#ifdef USE_OPENCV
			cv::Mat SolverInputCV;
			const cv::Mat SolverTransformInvCV = EigenToCV(SolverTransformInv);

			cv::warpAffine(InputCV, SolverInputCV, SolverTransformInvCV, cv::Size(SolverInputSizeX, SolverInputSizeY), cv::INTER_LANCZOS4);

			SolverInputArray = UEImageToHSImage(SolverInputSizeX, SolverInputSizeY, SolverInputCV.data, true);
#endif

			if (DebugImageCopy == EHyprsenseRealtimeNodeDebugImage::Solver)
			{
				DebugImageOut.Width = SolverInputSizeX;
				DebugImageOut.Height = SolverInputSizeY;
				DebugImageOut.Data = HSImageToUEImage(DebugImageOut.Width, DebugImageOut.Height, SolverInputArray, true);
			}

			// Prepare output of solver
			TArray<float> Controls;
			Controls.SetNumUninitialized(174);

			Inputs = { {(void*)SolverInputArray.GetData(), SolverInputArray.Num() * sizeof(float)} };
			Outputs = { {(void*)Controls.GetData(), Controls.Num() * sizeof(float)} };

			// Run solver
			if (Solver->RunSync(Inputs, Outputs) != UE::NNE::EResultStatus::Ok)
			{
				InPipelineData->SetErrorNodeCode(ErrorCode::FailedToSolve);
				InPipelineData->SetErrorNodeMessage(TEXT("Failed to solve"));
				return false;
			}

#ifdef PRINT_RESULTS
			UE_LOG(LogTemp, Warning, TEXT("JGC CONTROL1 %f"), Controls[0]);
			UE_LOG(LogTemp, Warning, TEXT("JGC CONTROL2 %f"), Controls[10]);
			UE_LOG(LogTemp, Warning, TEXT("JGC CONTROL3 %f"), Controls[30]);
#endif

			// Convert solver controls to raw controls
			TMap<FString, float> SolverControlMap;
			for (int32 Index = 0; Index < SolverControlNames.Num(); ++Index)
			{
				SolverControlMap.Add(SolverControlNames[Index], Controls[Index]);
			}

			// Fill in pipeline animation structure

			// The code below is largely a copy of that in FMetaHumanFaceTracker::GetTrackingState
			// The low-level mesh tracking produces a pose matrix in a similar manner to realtime, ie
			// OpenCV coordinate system and is based on the geometry of the DNA. 

			// The original rig is in Maya, ie Y up, X right, right-handed
			// By default this gets converted on import into UE, which is Z up, Y right, left-handed, such that it is the right way up and looking along 
			// the positive y axis .
			// So the first thing we need to do is to transform the rig in UE so that it looks the same orientation as the solver code sees it ie 
			// upside down, looking along the negative x axis (in UE). 
			// We do this using an initial offset transform, below, which is applied to the rig before the pose transform
			const FTransform Offset = FTransform(FRotator(0, 90, 180));

			// get the rotation and translation in OpenCV coordinate system
			FMatrix RotationOpenCV = FRotationMatrix::MakeFromXY(FVector(Pose[0], Pose[1], Pose[2]),
				FVector(Pose[3], Pose[4], Pose[5]));
			FVector TranslationOpenCV = FVector(0, 0, 0);

			// convert to UE coordinate system
			FRotator RotatorUE;
			FVector TranslationUE;
			FOpenCVHelperLocal::ConvertOpenCVToUnreal(RotationOpenCV, TranslationOpenCV, RotatorUE, TranslationUE);

			// Account for image having been rotated so it is upright when passed to headpose model.
			const FVector2d AnchorPt1 = TrackingPoints[838 + 56];
			const FVector2d AnchorPt2 = TrackingPoints[838 + 86];
			const double Angle = FMath::Atan2(AnchorPt2.Y - AnchorPt1.Y, AnchorPt2.X - AnchorPt1.X);
			const FTransform RotMat(FQuat(FVector(1, 0, 0), -Angle));
			FTransform RotatorUETransform(RotatorUE);
			RotatorUETransform *= RotMat;
			RotatorUE = RotatorUETransform.Rotator();

			//	Apply the landmark aware smoothing here.
			FTransform Transform = FTransform(RotatorUE, FVector(HeadTranslation.Y, HeadTranslation.Z, -HeadTranslation.X));
			if (bHeadStabilization && FocalLengthCopy > 0)
			{
				Transform = LandmarkAwareSmooth(TrackingPoints, Transform, FocalLengthCopy);
			}

			// apply the offset transform then the transform from the solver
			AnimOut.Pose = Offset * FTransform(Transform.Rotator(), TranslationUE);

			// Apply translation. The axis swapping here covers a multitude of transformations
			// such as OpenCV conversion, Maya offset, and maybe others!.
			// Taking the short cut of accumulating these here for speed ahead of playtesting.
			// This will need to be changed soon anyway to support offline processing where translation
			// need to be root bone relative not head bone relative as it is here.
			AnimOut.Pose.SetTranslation(Transform.GetTranslation());

			// End of code above copied from FMetaHumanFaceTracker::GetTrackingState

			AnimOut.AnimationData = GuiToRawControlsUtils::ConvertGuiToRawControls(SolverControlMap);

			// noseWrinkle is not output by the model, but it should be one.
			AnimOut.AnimationData["CTRL_expressions_noseWrinkleUpperL"] = 1;
			AnimOut.AnimationData["CTRL_expressions_noseWrinkleUpperR"] = 1;

			AnimOut.AnimationQuality = EFrameAnimationQuality::PostFiltered;
			check(AnimOut.AnimationData.Num() == 251);

			// An arbitrary indicator of subject being too far away is if head occupies <10% of image
			const double AnchorDist = FVector2D::Distance(AnchorPt1, AnchorPt2);
			State = AnchorDist / Input.Width > 0.1 ? EHyprsenseRealtimeNodeState::OK : EHyprsenseRealtimeNodeState::SubjectTooFar;
		}
	}
	
	if (State == EHyprsenseRealtimeNodeState::NoFace || bIsNeutralFrame)
	{
		PreviousTrackingPoints.Empty();
	}

	InPipelineData->SetData<FFrameAnimationData>(Pins[2], MoveTemp(AnimOut));
	InPipelineData->SetData<float>(Pins[3], FaceScore);
	InPipelineData->SetData<FUEImageDataType>(Pins[4], MoveTemp(DebugImageOut));
	InPipelineData->SetData<int32>(Pins[5], static_cast<int32>(State));
	InPipelineData->SetData<float>(Pins[6], FocalLengthCopy);

	return true;
}

FTransform FHyprsenseRealtimeNode::LandmarkAwareSmooth(const TArray<FVector2D>& InTrackingPoints, const FTransform& InTransform,
                                                       const float InFocalLength)
{
	if (PreviousTrackingPoints.Num() == 0)
	{
		PreviousTrackingPoints = InTrackingPoints;
		PreviousTransform = InTransform;
		return InTransform;
	}
    constexpr int32 NumGroups = 3;
	constexpr int32 LandmarkGroups[NumGroups][2] = { 
		{838, 894}, // Outer lip
		{894, 924},	// Left eye
		{924, 954}};  // Right eye

    //	Iterate over groups
	double MinGroupDistance = INFINITY;
	for (int32 GroupId = 0; GroupId < NumGroups; ++GroupId)
	{
		double SumDistance = 0.0f;
		const int32 RangeStart = LandmarkGroups[GroupId][0];
		const int32 RangeEnd = LandmarkGroups[GroupId][1];
		for (int32 Index = RangeStart; Index < RangeEnd; ++Index)
		{
			SumDistance += FVector2D::Distance(PreviousTrackingPoints[Index], InTrackingPoints[Index]);
		}
		const double AvgDistance = SumDistance / (RangeEnd - RangeStart);
		MinGroupDistance = FMath::Min(MinGroupDistance, AvgDistance);
	}
	
	const double MinGroupDistanceInCm = FMath::Abs(InTransform.GetTranslation().Y * MinGroupDistance / InFocalLength);
	const double SmoothFactor = MinGroupDistanceInCm / LandmarkAwareSmoothingThresholdInCm;
	
	//	TODO: this SmoothFactor could be stored somewhere to be used in the post-processing filters.
	if (SmoothFactor >= 1.0)
	{
		PreviousTrackingPoints = InTrackingPoints;
		PreviousTransform = InTransform;
		return PreviousTransform;
	}

	check(PreviousTrackingPoints.Num() == InTrackingPoints.Num());
	
    for (int32 Index = 0; Index < InTrackingPoints.Num(); ++Index)
	{
    	PreviousTrackingPoints[Index] = FMath::Lerp(PreviousTrackingPoints[Index], InTrackingPoints[Index], SmoothFactor);
	}

	const FVector Translation = FMath::Lerp(PreviousTransform.GetTranslation(), InTransform.GetTranslation(), SmoothFactor);
	const FQuat Rotation = FQuat::Slerp(PreviousTransform.GetRotation(), InTransform.GetRotation(), SmoothFactor);
	const FVector Scale = FMath::Lerp(PreviousTransform.GetScale3D(), InTransform.GetScale3D(), SmoothFactor);

	PreviousTransform = FTransform(Rotation, Translation, Scale);
	return PreviousTransform;
}

FHyprsenseRealtimeNode::Matrix23f FHyprsenseRealtimeNode::GetTransformFromPoints(const TArray<FVector2D>& InPoints, const FVector2D& InSize, bool bInIsStableBox, Matrix23f& OutTransformInv) const
{
	const FVector2d AnchorPt1 = InPoints[838 + 56];
	const FVector2d AnchorPt2 = InPoints[838 + 86];
	const double Angle = FMath::Atan2(AnchorPt2.Y - AnchorPt1.Y, AnchorPt2.X - AnchorPt1.X);

	const FTransform2d RotMat(FQuat2d(-Angle), FVector2D::ZeroVector);

	TArray<FVector2D> RotatedPoints;
	for (const FVector2D& Point : InPoints)
	{
		RotatedPoints.Add(RotMat.TransformPoint(Point));
	}

	double CX, CY, Scale;
	Matrix23f Transform;

	if (bInIsStableBox)
	{
#ifdef PRINT_RESULTS
		UE_LOG(LogTemp, Warning, TEXT("JGC ROT1 %f"), RotatedPoints[0].X);
		UE_LOG(LogTemp, Warning, TEXT("JGC ROT2 %f"), RotatedPoints[0].Y);
#endif

		const int32 AnchorIndex1 = 835;
		const int32 AnchorIndex2 = 837;
		const double PivotY = (RotatedPoints[AnchorIndex2].Y + RotatedPoints[AnchorIndex1].Y) * 0.5;
		const double LE = RotatedPoints[AnchorIndex1].X;
		const double RE = RotatedPoints[AnchorIndex2].X;
		const double Dist = RE - LE;
		const double XOffset = 0.08;
		const double YOffset = 0.83;
		double Height = 1.65;

		double XOff = int32(XOffset * Dist);
		double YOff = int32(YOffset * Dist);
		Height = int32(Height * Dist);

		const double MinX = LE - XOff;
		const double MinY = PivotY - YOff;
		const double MaxX = RE + XOff;
		const double MaxY = PivotY + Height;

		CX = (MinX + MaxX) * 0.5;
		CY = (MinY + MaxY) * 0.5;

		const double Width = MaxX - MinX;
		Height = MaxY - MinY;

		Scale = InSize.X / Width;

#ifdef PRINT_RESULTS
		UE_LOG(LogTemp, Warning, TEXT("JGC SCALE %f"), Scale);
#endif
	}
	else
	{
		double MinX = 0, MaxX = 0, MinY = 0, MaxY = 0;
		bool bIsFirstPoint = true;

		for (const FVector2D& RotatedPoint : RotatedPoints)
		{
			if (bIsFirstPoint || RotatedPoint.X < MinX)
			{
				MinX = RotatedPoint.X;
			}

			if (bIsFirstPoint || RotatedPoint.X > MaxX)
			{
				MaxX = RotatedPoint.X;
			}

			if (bIsFirstPoint || RotatedPoint.Y < MinY)
			{
				MinY = RotatedPoint.Y;
			}

			if (bIsFirstPoint || RotatedPoint.Y > MaxY)
			{
				MaxY = RotatedPoint.Y;
			}

			bIsFirstPoint = false;
		}

		CX = (MinX + MaxX) * 0.5;
		CY = (MinY + MaxY) * 0.5;

		const double Width = MaxX - MinX;
		const double Height = MaxY - MinY;

		const double OriginalImageSize = FMath::Sqrt(Width * Width + Height * Height) * 256.0l / 192.0l;

		Scale = InSize.X / OriginalImageSize;
	}

	const FTransform2d PosMat(Scale, FVector2D(InSize.X / 2 - Scale * CX, InSize.Y / 2 - Scale * CY));

	const FTransform2d TransformInvUE = RotMat.Concatenate(PosMat);
	const FMatrix44d TransformInvUE3D = TransformInvUE.To3DMatrix();
	const FMatrix44d TransformUE3D = TransformInvUE3D.Inverse();

	for (int32 I = 0; I < 2; ++I)
	{
		for (int32 J = 0; J < 2; ++J)
		{
			Transform(I, J) = TransformUE3D.M[J][I];
			OutTransformInv(I, J) = TransformInvUE3D.M[J][I];
		}
	}
	Transform(0, 2) = TransformUE3D.M[3][0];
	Transform(1, 2) = TransformUE3D.M[3][1];
	OutTransformInv(0, 2) = TransformInvUE3D.M[3][0];
	OutTransformInv(1, 2) = TransformInvUE3D.M[3][1];

	return Transform;
}

}