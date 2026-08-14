#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// VisionSystem — DTE's Visual Cortex
//
// Multi-stage visual processing pipeline:
//   1. Frame Capture (from MLAdapterSensor_Camera or screen capture)
//   2. Preprocessing (resize, normalize, color space)
//   3. Feature Extraction (CNN/ViT via NPU or spatial pooling)
//   4. Object Detection (YOLO-style bounding boxes)
//   5. Semantic Segmentation (pixel-level classification)
//   6. Temporal Integration (frame differencing, optical flow)
//   7. Saliency Map (attention-guided visual focus)
//
// The vision system feeds into the Echo Reservoir, which naturally
// handles temporal integration through its recurrent dynamics.
//
// NPU Integration:
//   - Visual encoder model loaded via LlamaCoprocessorDriver (ONNX)
//   - Feature vectors written to MMIO registers
//   - DMA transfer for bulk frame data
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include <Eigen/Dense>

/** Visual feature type */
enum class EVisualFeatureType : uint8
{
    SPATIAL_POOL,    // Simple spatial average pooling (fast, no model)
    EDGE_DETECT,     // Sobel edge detection (fast, structural)
    COLOR_HISTOGRAM, // Color distribution features
    CNN_FEATURES,    // CNN backbone features (requires NPU)
    VIT_FEATURES,    // Vision Transformer features (requires NPU)
    HYBRID           // Combination of multiple feature types
};

/** Detected object in the visual field */
struct FDetectedObject
{
    /** Object class name */
    FString ClassName;

    /** Bounding box (normalized 0-1) */
    float X = 0.0f, Y = 0.0f, W = 0.0f, H = 0.0f;

    /** Confidence score */
    float Confidence = 0.0f;

    /** Distance estimate (from depth or size) */
    float EstimatedDistance = 0.0f;

    /** Whether this object is a threat */
    bool bIsThreat = false;

    /** Whether this object is interactable */
    bool bIsInteractable = false;
};

/** Saliency point — where DTE is "looking" */
struct FSaliencyPoint
{
    float X = 0.5f; // Normalized screen position
    float Y = 0.5f;
    float Strength = 0.0f;
    FString Reason; // Why this point is salient
};

/**
 * VisionSystem — DTE's visual processing pipeline.
 *
 * Processes raw pixel data from the game viewport into features
 * suitable for the Echo Reservoir. Implements a biologically-inspired
 * hierarchy:
 *
 * ┌─────────────────────────────────────────────────────────┐
 * │                    Raw Pixels (RGBA)                    │
 * │                    160x120x4 = 76,800                   │
 * └────────────────────────┬────────────────────────────────┘
 *                          ▼
 * ┌─────────────────────────────────────────────────────────┐
 * │              Preprocessing (V1 - Primary)               │
 * │  Resize → Normalize → Grayscale + Color channels       │
 * └────────────────────────┬────────────────────────────────┘
 *                          ▼
 * ┌─────────────────────────────────────────────────────────┐
 * │              Edge Detection (V1 - Orientation)          │
 * │  Sobel X/Y → Edge magnitude → Orientation histogram    │
 * └────────────────────────┬────────────────────────────────┘
 *                          ▼
 * ┌─────────────────────────────────────────────────────────┐
 * │              Spatial Pooling (V2 - Complex)             │
 * │  Grid cells → Average pooling → Feature vector         │
 * └────────────────────────┬────────────────────────────────┘
 *                          ▼
 * ┌─────────────────────────────────────────────────────────┐
 * │              Temporal Differencing (MT - Motion)        │
 * │  Frame[t] - Frame[t-1] → Motion energy map             │
 * └────────────────────────┬────────────────────────────────┘
 *                          ▼
 * ┌─────────────────────────────────────────────────────────┐
 * │              Saliency (Attention - FEF)                 │
 * │  Edge + Motion + Color → Saliency map → Focus point    │
 * └────────────────────────┬────────────────────────────────┘
 *                          ▼
 * ┌─────────────────────────────────────────────────────────┐
 * │              Feature Vector (IT - Recognition)          │
 * │  Concatenate: spatial + edge + color + motion + saliency│
 * │  Output: 256D feature vector → Echo Reservoir           │
 * └─────────────────────────────────────────────────────────┘
 */
class FVisionSystem
{
public:
    FVisionSystem() = default;

    /**
     * Initialize the vision system.
     */
    void Initialize(int32 InputWidth = 160, int32 InputHeight = 120,
                    int32 OutputDim = 256,
                    EVisualFeatureType FeatureType = EVisualFeatureType::HYBRID)
    {
        Width = InputWidth;
        Height = InputHeight;
        OutDim = OutputDim;
        FeatType = FeatureType;

        // Allocate previous frame buffer for temporal differencing
        PrevFrame.SetNumZeroed(Width * Height);

        // Compute grid dimensions for spatial pooling
        GridW = FMath::Max(1, (int32)FMath::Sqrt((float)OutDim / 4.0f));
        GridH = GridW;

        bInitialized = true;
    }

    /**
     * Process a raw pixel frame into a feature vector.
     *
     * @param Pixels - Raw RGBA pixel data (Width * Height * 4 floats)
     * @return Feature vector suitable for Echo Reservoir input
     */
    Eigen::VectorXf ProcessFrame(const TArray<float>& Pixels)
    {
        check(bInitialized);

        Eigen::VectorXf Features = Eigen::VectorXf::Zero(OutDim);
        int32 Offset = 0;

        // === Stage 1: Grayscale conversion ===
        TArray<float> Gray;
        Gray.SetNumUninitialized(Width * Height);
        for (int32 i = 0; i < Width * Height && i * 4 + 2 < Pixels.Num(); ++i)
        {
            Gray[i] = 0.299f * Pixels[i*4] + 0.587f * Pixels[i*4+1] + 0.114f * Pixels[i*4+2];
        }

        // === Stage 2: Spatial pooling features ===
        int32 SpatialDim = GridW * GridH;
        if (SpatialDim > 0 && Offset + SpatialDim <= OutDim)
        {
            int32 CellW = FMath::Max(1, Width / GridW);
            int32 CellH = FMath::Max(1, Height / GridH);

            for (int32 gy = 0; gy < GridH; ++gy)
            {
                for (int32 gx = 0; gx < GridW; ++gx)
                {
                    float Sum = 0.0f;
                    int32 Count = 0;
                    for (int32 py = gy * CellH; py < FMath::Min((gy+1)*CellH, Height); ++py)
                    {
                        for (int32 px = gx * CellW; px < FMath::Min((gx+1)*CellW, Width); ++px)
                        {
                            Sum += Gray[py * Width + px];
                            Count++;
                        }
                    }
                    Features(Offset++) = (Count > 0) ? Sum / Count : 0.0f;
                }
            }
        }

        // === Stage 3: Edge detection (Sobel) ===
        int32 EdgeDim = FMath::Min(SpatialDim, OutDim - Offset);
        if (EdgeDim > 0)
        {
            TArray<float> EdgeMag;
            EdgeMag.SetNumZeroed(Width * Height);
            ComputeSobelEdges(Gray, EdgeMag);

            // Pool edges into grid
            int32 CellW = FMath::Max(1, Width / GridW);
            int32 CellH = FMath::Max(1, Height / GridH);
            for (int32 gy = 0; gy < GridH && Offset < OutDim; ++gy)
            {
                for (int32 gx = 0; gx < GridW && Offset < OutDim; ++gx)
                {
                    float Sum = 0.0f;
                    int32 Count = 0;
                    for (int32 py = gy * CellH; py < FMath::Min((gy+1)*CellH, Height); ++py)
                    {
                        for (int32 px = gx * CellW; px < FMath::Min((gx+1)*CellW, Width); ++px)
                        {
                            Sum += EdgeMag[py * Width + px];
                            Count++;
                        }
                    }
                    Features(Offset++) = (Count > 0) ? Sum / Count : 0.0f;
                }
            }
        }

        // === Stage 4: Color histogram features ===
        int32 ColorDim = FMath::Min(24, OutDim - Offset); // 8 bins x 3 channels
        if (ColorDim > 0)
        {
            TArray<float> ColorHist;
            ColorHist.SetNumZeroed(ColorDim);
            ComputeColorHistogram(Pixels, ColorHist, ColorDim);
            for (int32 i = 0; i < ColorDim && Offset < OutDim; ++i)
                Features(Offset++) = ColorHist[i];
        }

        // === Stage 5: Temporal differencing (motion energy) ===
        int32 MotionDim = FMath::Min(SpatialDim, OutDim - Offset);
        if (MotionDim > 0 && PrevFrame.Num() == Gray.Num())
        {
            TArray<float> MotionEnergy;
            MotionEnergy.SetNumZeroed(Width * Height);
            for (int32 i = 0; i < Width * Height; ++i)
                MotionEnergy[i] = FMath::Abs(Gray[i] - PrevFrame[i]);

            // Pool motion into grid
            int32 CellW = FMath::Max(1, Width / GridW);
            int32 CellH = FMath::Max(1, Height / GridH);
            for (int32 gy = 0; gy < GridH && Offset < OutDim; ++gy)
            {
                for (int32 gx = 0; gx < GridW && Offset < OutDim; ++gx)
                {
                    float Sum = 0.0f;
                    int32 Count = 0;
                    for (int32 py = gy * CellH; py < FMath::Min((gy+1)*CellH, Height); ++py)
                    {
                        for (int32 px = gx * CellW; px < FMath::Min((gx+1)*CellW, Width); ++px)
                        {
                            Sum += MotionEnergy[py * Width + px];
                            Count++;
                        }
                    }
                    Features(Offset++) = (Count > 0) ? Sum / Count : 0.0f;
                }
            }
        }

        // === Stage 6: Saliency computation ===
        ComputeSaliency(Gray);

        // Fill remaining dimensions with saliency features
        if (Offset < OutDim)
        {
            Features(Offset++) = CurrentSaliency.X;
            if (Offset < OutDim) Features(Offset++) = CurrentSaliency.Y;
            if (Offset < OutDim) Features(Offset++) = CurrentSaliency.Strength;
        }

        // Store current frame for next temporal differencing
        PrevFrame = Gray;

        return Features;
    }

    /** Get current saliency point */
    const FSaliencyPoint& GetSaliency() const { return CurrentSaliency; }

    /** Get detected objects (if object detection is enabled) */
    const TArray<FDetectedObject>& GetDetectedObjects() const { return DetectedObjects; }

    bool IsInitialized() const { return bInitialized; }

private:
    void ComputeSobelEdges(const TArray<float>& Gray, TArray<float>& EdgeMag) const
    {
        for (int32 y = 1; y < Height - 1; ++y)
        {
            for (int32 x = 1; x < Width - 1; ++x)
            {
                // Sobel X kernel
                float Gx = -Gray[(y-1)*Width+(x-1)] + Gray[(y-1)*Width+(x+1)]
                          -2*Gray[y*Width+(x-1)]     + 2*Gray[y*Width+(x+1)]
                          -Gray[(y+1)*Width+(x-1)]   + Gray[(y+1)*Width+(x+1)];

                // Sobel Y kernel
                float Gy = -Gray[(y-1)*Width+(x-1)] - 2*Gray[(y-1)*Width+x] - Gray[(y-1)*Width+(x+1)]
                          +Gray[(y+1)*Width+(x-1)]  + 2*Gray[(y+1)*Width+x] + Gray[(y+1)*Width+(x+1)];

                EdgeMag[y * Width + x] = FMath::Sqrt(Gx*Gx + Gy*Gy);
            }
        }
    }

    void ComputeColorHistogram(const TArray<float>& Pixels, TArray<float>& Hist, int32 NumBins) const
    {
        int32 BinsPerChannel = NumBins / 3;
        if (BinsPerChannel < 1) BinsPerChannel = 1;

        for (int32 i = 0; i < Width * Height && i * 4 + 2 < Pixels.Num(); ++i)
        {
            for (int32 c = 0; c < 3; ++c)
            {
                float Val = FMath::Clamp(Pixels[i*4+c], 0.0f, 1.0f);
                int32 Bin = FMath::Min((int32)(Val * BinsPerChannel), BinsPerChannel - 1);
                int32 Idx = c * BinsPerChannel + Bin;
                if (Idx < Hist.Num())
                    Hist[Idx] += 1.0f / (Width * Height);
            }
        }
    }

    void ComputeSaliency(const TArray<float>& Gray)
    {
        // Simple center-surround saliency
        float MaxSaliency = 0.0f;
        int32 MaxX = Width / 2, MaxY = Height / 2;

        int32 Step = 4; // Check every 4th pixel for speed
        for (int32 y = 4; y < Height - 4; y += Step)
        {
            for (int32 x = 4; x < Width - 4; x += Step)
            {
                float Center = Gray[y * Width + x];
                float Surround = 0.0f;
                int32 Count = 0;
                for (int32 dy = -4; dy <= 4; dy += 2)
                {
                    for (int32 dx = -4; dx <= 4; dx += 2)
                    {
                        if (dx == 0 && dy == 0) continue;
                        Surround += Gray[(y+dy) * Width + (x+dx)];
                        Count++;
                    }
                }
                Surround /= Count;
                float S = FMath::Abs(Center - Surround);
                if (S > MaxSaliency)
                {
                    MaxSaliency = S;
                    MaxX = x;
                    MaxY = y;
                }
            }
        }

        CurrentSaliency.X = (float)MaxX / Width;
        CurrentSaliency.Y = (float)MaxY / Height;
        CurrentSaliency.Strength = MaxSaliency;
    }

    int32 Width = 160;
    int32 Height = 120;
    int32 OutDim = 256;
    int32 GridW = 8;
    int32 GridH = 8;
    EVisualFeatureType FeatType = EVisualFeatureType::HYBRID;

    TArray<float> PrevFrame;
    FSaliencyPoint CurrentSaliency;
    TArray<FDetectedObject> DetectedObjects;

    bool bInitialized = false;
};
