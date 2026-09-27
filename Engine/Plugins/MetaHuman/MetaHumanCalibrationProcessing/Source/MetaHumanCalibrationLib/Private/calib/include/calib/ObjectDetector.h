// Copyright Epic Games, Inc. All Rights Reserved.

/**
    Multiple-View-Geometry Toolbox - Object detector class

    Header contains the C++ implementation of the object detector.
 */
#pragma once

#include <carbon/Common.h>

#include <calib/Image.h>
#include <calib/Object.h>
#include <calib/Calibration.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::calib)

/**
    @brief
        Implementation of object detection algorithm which is an extension of basic
        checkerboard pattern detection, to handle detection of multiple patterns in
        one image, assuming that each pattern is unique. Implemented pipeline for
        detecting multiple chessboard patterns on image is as follows: First, we sort
        possible patterns, by dimensions, in decreasing order, from largest to smallest.
        Next, we apply standard algorithm for chessboard pattern detection aiming at
        detecting first pattern with largest dimension. In case of detection, we write
        detected points, and mask area of detected pattern on image. In next iteration,
        In the next iteration, the algorithm for checkerboard pattern detection is
        applied on masked image, and it aims at detecting second largest pattern.
        If the pattern is not detected, this iteration is skipped. The algorithm
        iterates to the last � smallest possible pattern. With this procedure, we avoid
        possible false detection of a small dimension pattern in a large one.

        For more information check our paper:
        https://drive.google.com/open?id=1a7NCwb24yb3ho099m2dzNpNGcx2b4v51
 */

class /*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/ObjectDetector
{
public:
    virtual ~ObjectDetector() = default;

    // --------------------------------------------------------------------
    // API
    // --------------------------------------------------------------------

    /**
        @brief Try to detect object.

        Try to detect specified object on specified image (or multiple images). Algorithm
        for detection is explained above.

        @param numThreads
            Number of threads to be employed to find patterns. Default parameter is 0,
            which runs max cores. Due to the IO bottleneck, max cores is defined to be 12,
            except if that exceeds CPU number of logical processors, which is then used.

        @return
            trust::Expected<std::vector<ObjectPlaneProjection*> > which, if valid, contains
            array of detected object plane projections.
     */

    virtual std::optional<std::vector<ObjectPlaneProjection*>> tryDetect() = 0;

    // --------------------------------------------------------------------
    // Constructors
    // --------------------------------------------------------------------

    /*!
        @brief Construct the object detector using input parameters.

        @param image
            Pointer to Image object of a image on which we want to detect calibration object.

        @param object
            Pointer to calibration Object object which we want to detect on image.

        @param type
            Which of 2 possible algorithms for pattern detection will be used.

        @warning
            Created object plane projection must always be deallocated using
            ObjectDetector::destructor.
     */

    static ObjectDetector* create(Image* image, Object* object, PatternDetect type);

    /*!
        @brief Construct the object detector using input parameters.

        @param images
            Array of Image objects on which we want to detect calibration object.

        @param object
            Pointer to calibration Object object which we want to detect on images.

        @param type
            Which of 2 possible algorithms for pattern detection will be used.

        @warning
            Created object plane projection must always be deallocated using
            ObjectDetector::destructor.
     */

    static ObjectDetector* create(const std::vector<Image*>& images, Object* object, PatternDetect type);

    // --------------------------------------------------------------------
    // Destructors
    // --------------------------------------------------------------------

    /**
        Destructor for object detectors created using this class.

        Except for manual memory management, common use-case of this function
        would be to provide a smart pointer with a valid destructor.
     */

    struct/*CALIB_API TODO this is the core module which is a static library, external APIs should be done separately. Is this correct?*/destructor
    {
        void operator()(ObjectDetector* detector);
    };
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE::calib)
