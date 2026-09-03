// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanPipelineMediaPlayerNode.h"

#include "Async/ParallelFor.h"

namespace UE::MetaHuman::Pipeline
{

FString FMediaPlayerNode::BundleURL = TEXT("bundle://");

FMediaPlayerNode::FMediaPlayerNode(const FString& InTypeName, const FString& InName) : FNode(InTypeName, InName)
{
	Pins.Add(FPin("UE Image Out", EPinDirection::Output, EPinType::UE_Image));
	Pins.Add(FPin("Audio Out", EPinDirection::Output, EPinType::Audio));
	Pins.Add(FPin("UE Image Sample Time Out", EPinDirection::Output, EPinType::QualifiedFrameTime, 0));
	Pins.Add(FPin("Audio Sample Time Out", EPinDirection::Output, EPinType::QualifiedFrameTime, 1));
	Pins.Add(FPin("Dropped Frame Out", EPinDirection::Output, EPinType::Bool));
	Pins.Add(FPin("UE Image Sample Time Source Out", EPinDirection::Output, EPinType::Int, 0));
	Pins.Add(FPin("Audio Sample Time Source Out", EPinDirection::Output, EPinType::Int, 1));
}

static void RGBfromYUV(double& R, double& G, double& B, double Y, double U, double V)
{
	Y -= 16;
	U -= 128;
	V -= 128;
	R = 1.164 * Y + 1.596 * V;
	G = 1.164 * Y - 0.392 * U - 0.813 * V;
	B = 1.164 * Y + 2.017 * U;

	if (R < 0) R = 0;
	if (G < 0) G = 0;
	if (B < 0) B = 0;
	if (R > 255) R = 255;
	if (G > 255) G = 255;
	if (B > 255) B = 255;
}

void FMediaPlayerNode::ConvertSample(const FIntPoint& InRes, const int32 InStride, const EMediaTextureSampleFormat InFormat, const uint8* InVideoSampleData, FUEImageDataType &OutImage) const
{
	OutImage.Width = InRes.X;
	OutImage.Height = InRes.Y;
	OutImage.Data.SetNumUninitialized(OutImage.Width * OutImage.Height * 4);

	if (InFormat == EMediaTextureSampleFormat::CharNV12)
	{
		ParallelFor(InRes.Y, [&](int32 Y)
		{
			const uint8* SampleLumData = InVideoSampleData;
			SampleLumData += Y * InStride;

			const uint8* SampleUVData = InVideoSampleData;
			SampleUVData += (InRes.Y * InStride) + (Y / 2 * InStride / 2 * 2);

			uint8* RGBData = OutImage.Data.GetData();
			RGBData += Y * InRes.X * 4;

			for (int32 X = 0; X < InRes.X; ++X)
			{
				double R, G, B;

				RGBfromYUV(R, G, B, SampleLumData[0], SampleUVData[0], SampleUVData[1]);

				RGBData[0] = B;
				RGBData[1] = G;
				RGBData[2] = R;
				RGBData[3] = 255;
				RGBData += 4;

				++SampleLumData;
				if (X % 2 == 1) SampleUVData += 2;
			}
		});
	}
	else if (InFormat == EMediaTextureSampleFormat::CharYUY2)
	{
		ParallelFor(InRes.Y, [&](int32 Y)
		{
			const uint8* SampleData = InVideoSampleData;
			SampleData += Y * InStride;

			uint8* RGBData = OutImage.Data.GetData();
			RGBData += Y * InRes.X * 4;

			for (int32 X = 0; X < InRes.X; X += 2)
			{
				double R, G, B;

				RGBfromYUV(R, G, B, SampleData[0], SampleData[1], SampleData[3]);

				RGBData[0] = B;
				RGBData[1] = G;
				RGBData[2] = R;
				RGBData[3] = 255;
				RGBData += 4;

				RGBfromYUV(R, G, B, SampleData[2], SampleData[1], SampleData[3]);

				RGBData[0] = B;
				RGBData[1] = G;
				RGBData[2] = R;
				RGBData[3] = 255;
				RGBData += 4;

				SampleData += 4;
			}
		});
	}
	else if (InFormat == EMediaTextureSampleFormat::CharUYVY)
	{
		ParallelFor(InRes.Y, [&](int32 Y)
		{
			const uint8* SampleData = InVideoSampleData;
			SampleData += Y * InStride;

			uint8* RGBData = OutImage.Data.GetData();
			RGBData += Y * InRes.X * 4;

			for (int32 X = 0; X < InRes.X; X += 2)
			{
				double R, G, B;

				RGBfromYUV(R, G, B, SampleData[1], SampleData[0], SampleData[2]);

				RGBData[0] = B;
				RGBData[1] = G;
				RGBData[2] = R;
				RGBData[3] = 255;
				RGBData += 4;

				RGBfromYUV(R, G, B, SampleData[3], SampleData[0], SampleData[2]);

				RGBData[0] = B;
				RGBData[1] = G;
				RGBData[2] = R;
				RGBData[3] = 255;
				RGBData += 4;

				SampleData += 4;
			}
		});
	}
	else if (InFormat == EMediaTextureSampleFormat::CharBGRA)
	{
		if (InStride == InRes.X * 4)
		{
			FMemory::Memcpy(OutImage.Data.GetData(), InVideoSampleData, OutImage.Data.Num());
		}
		else
		{
			ParallelFor(InRes.Y, [&](int32 Y)
			{
				const uint8* SampleData = InVideoSampleData;
				SampleData += Y * InStride;

				uint8* RGBData = OutImage.Data.GetData();
				RGBData += Y * InRes.X * 4;

				FMemory::Memcpy(RGBData, SampleData, InRes.X * 4);
			});
		}
	}
	else if (InFormat == EMediaTextureSampleFormat::YUVv210)
	{
		ParallelFor(InRes.Y, [&](int32 Y)
		{
			const uint8* SampleData = InVideoSampleData;
			SampleData += Y * InStride;

			uint8* RGBData = OutImage.Data.GetData();
			RGBData += Y * InRes.X * 4;

			for (int32 X = 0; X < InRes.X; X += 6)
			{
				// Sample is 128 bits. Thats 12 values where each is 10 bits. Those 12
				// values (UYVY x 3) make 6 pixels.
				// 10 bit values are downsized to 8 bit.
				// See https://wiki.multimedia.cx/index.php/V210

				double R, G, B;
				uint32* SampleData32 = (uint32*) SampleData;

				uint8 U8 = (SampleData32[0] >> 2);
				uint8 Y8 = (SampleData32[0] >> 12);
				uint8 V8 = (SampleData32[0] >> 22);

				RGBfromYUV(R, G, B, Y8, U8, V8);

				RGBData[0] = B;
				RGBData[1] = G;
				RGBData[2] = R;
				RGBData[3] = 255;
				RGBData += 4;

				Y8 = (SampleData32[1] >> 2);

				RGBfromYUV(R, G, B, Y8, U8, V8);

				RGBData[0] = B;
				RGBData[1] = G;
				RGBData[2] = R;
				RGBData[3] = 255;
				RGBData += 4;

				U8 = (SampleData32[1] >> 12);
				Y8 = (SampleData32[1] >> 22);
				V8 = (SampleData32[2] >> 2);

				RGBfromYUV(R, G, B, Y8, U8, V8);

				RGBData[0] = B;
				RGBData[1] = G;
				RGBData[2] = R;
				RGBData[3] = 255;
				RGBData += 4;

				Y8 = (SampleData32[2] >> 12);

				RGBfromYUV(R, G, B, Y8, U8, V8);

				RGBData[0] = B;
				RGBData[1] = G;
				RGBData[2] = R;
				RGBData[3] = 255;
				RGBData += 4;

				U8 = (SampleData32[2] >> 22);
				Y8 = (SampleData32[3] >> 2);
				V8 = (SampleData32[3] >> 12);

				RGBfromYUV(R, G, B, Y8, U8, V8);

				RGBData[0] = B;
				RGBData[1] = G;
				RGBData[2] = R;
				RGBData[3] = 255;
				RGBData += 4;

				Y8 = (SampleData32[3] >> 22);

				RGBfromYUV(R, G, B, Y8, U8, V8);

				RGBData[0] = B;
				RGBData[1] = G;
				RGBData[2] = R;
				RGBData[3] = 255;
				RGBData += 4;

				SampleData += 16; // 128 bits
			}
		});
	}
	else
	{
		check(false);
	}
}

}