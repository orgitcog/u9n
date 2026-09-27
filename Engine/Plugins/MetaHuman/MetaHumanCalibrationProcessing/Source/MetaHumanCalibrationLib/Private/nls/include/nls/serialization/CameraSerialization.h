// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/io/JsonIO.h>
#include <carbon/io/Utils.h>
#include <nls/geometry/MetaShapeCamera.h>
#include <fstream>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * Reads camera calibration information from our own camera json format and converts it to MetaShapeCameras.
 */
template <class T>
bool ReadMetaShapeCamerasFromJsonFile(const std::string& filename, std::vector<MetaShapeCamera<T>>& cameras)
{
    JsonElement json = ReadJson(ReadFile(filename));
    if (!json.IsArray()) {
        LOG_ERROR("json camera file should contains an array as top level");
        return false;
    }

    std::vector<MetaShapeCamera<T>> newCameras;
    int cameraCounter = 0;
    for (int i = 0; i < static_cast<int>(json.Size()); ++i) {
        if (!json[i].Contains("metadata")) {
            LOG_ERROR("json camera file should contain an array of objects that have a metadata field");
            return false;
        }

        if (json[i]["metadata"]["type"].Get<std::string>() == "camera") {
            // we have a camera type
            MetaShapeCamera<T> camera;
            camera.SetCameraID(cameraCounter);
            cameraCounter += 1;

            camera.SetLabel(json[i]["metadata"]["name"].Get<std::string>());
            camera.SetWidth(json[i]["image_size_x"].Get<int>());
            camera.SetHeight(json[i]["image_size_y"].Get<int>());
            Eigen::Matrix<T, 3, 3> intrinsics = Eigen::Matrix<T, 3, 3>::Identity();
            if (json[i].Contains("fx") && json[i].Contains("fy") && json[i].Contains("cx") && json[i].Contains("cy")) {
                intrinsics(0, 0) = json[i]["fy"].Get<T>();
                intrinsics(1, 1) = json[i]["fy"].Get<T>();
                intrinsics(0, 2) = json[i]["cx"].Get<T>();
                intrinsics(1, 2) = json[i]["cy"].Get<T>();
                camera.SetIntrinsics(intrinsics);
            } else {
                LOG_ERROR("camera calibration is missing one of fx, fy, cx, cy");
                return false;
            }

            Eigen::Matrix<T, 4, 4> extrinsics;
            for (int c = 0; c < 4; ++c) {
                for (int r = 0; r < 4; ++r) {
                    extrinsics(r, c) = json[i]["transform"][4 * r + c].Get<T>();
                }
            }

            camera.SetExtrinsics(extrinsics);

            if (json[i]["distortion_model"].Get<std::string>() == "opencv") {
                auto valueOrNull = [](const JsonElement& j, const char* label) { return j.Contains(label) ? j[label].Get<T>() : T(0); };
                const T k1 = valueOrNull(json[i], "k1");
                const T k2 = valueOrNull(json[i], "k2");
                const T k3 = valueOrNull(json[i], "k3");
                const T k4 = valueOrNull(json[i], "k4");
                const T k5 = valueOrNull(json[i], "k5");
                const T k6 = valueOrNull(json[i], "k6");
                const T p1 = valueOrNull(json[i], "p1");
                const T p2 = valueOrNull(json[i], "p2");
                const T b1 = (json[i]["fx"].Get<T>() - json[i]["fy"].Get<T>());
                const T b2 = valueOrNull(json[i], "s");
                if ( k5 != T(0) || k6 != T(0)) {
                    LOG_ERROR("metashape camera does not support k5, and k6 parameter");
                    return false;
                }
                if (k4 != T(0)) {
                    camera.SetRadialDistortion(Eigen::Vector<T, 4>(k1, k2, k3, k4));
                }
                else {
                    camera.SetRadialDistortion(Eigen::Vector<T, 4>(k1, k2, k3, T(0)));
                }
                camera.SetTangentialDistortion(Eigen::Vector<T, 4>(p2, p1, T(0), T(0))); // note that metashape camera has swapped tangential distortion compared to opencv
                camera.SetSkew(Eigen::Vector<T, 2>(T(b1), T(b2)));
            } else {
                LOG_ERROR("no valid distortion model defined");
                return false;
            }

            newCameras.push_back(camera);
        }
    }

    cameras.swap(newCameras);
    return true;
}


/**
 * Writes camera calibration information into our own camera json format
 */
template <class T>
bool WriteMetaShapeCamerasToJsonFile(const std::string& filename, const std::vector<MetaShapeCamera<T>>& cameras)
{
    JsonElement json(JsonElement::JsonType::Array);
    for (const MetaShapeCamera<T>& camera : cameras) {

        JsonElement jsonMeta(JsonElement::JsonType::Object);
        jsonMeta.Insert("type", JsonElement("camera"));
        jsonMeta.Insert("version", JsonElement(0));
        jsonMeta.Insert("name", JsonElement(camera.Label()));
        jsonMeta.Insert("camera", JsonElement(camera.Label()));

        JsonElement jsonCamera(JsonElement::JsonType::Object);
        jsonCamera.Insert("metadata", std::move(jsonMeta));

        jsonCamera.Insert("image_size_x", JsonElement(camera.Width()));
        jsonCamera.Insert("image_size_y", JsonElement(camera.Height()));

        if (camera.Skew().squaredNorm() > 0) {
            jsonCamera.Insert("fx", JsonElement(camera.Intrinsics()(0, 0) + camera.Skew()[0]));
            jsonCamera.Insert("fy", JsonElement(camera.Intrinsics()(1, 1)));
            jsonCamera.Insert("s", JsonElement(camera.Skew()[1]));
        }
        else {
            jsonCamera.Insert("fx", JsonElement(camera.Intrinsics()(0, 0)));
            jsonCamera.Insert("fy", JsonElement(camera.Intrinsics()(1, 1)));
        }
        jsonCamera.Insert("cx", JsonElement(camera.Intrinsics()(0,2)));
        jsonCamera.Insert("cy", JsonElement(camera.Intrinsics()(1,2)));

        if (camera.Intrinsics()(0, 1) != T(0)) {
            LOG_ERROR("failed to write camera parameters as intrinsics skew is not supported");
            return false;
        }

        jsonCamera.Insert("distortion_model", JsonElement("opencv"));
        jsonCamera.Insert("k1", JsonElement(camera.RadialDistortion()[0]));
        jsonCamera.Insert("k2", JsonElement(camera.RadialDistortion()[1]));
        jsonCamera.Insert("k3", JsonElement(camera.RadialDistortion()[2]));

        if (camera.RadialDistortion()[3] != T(0)) {
            jsonCamera.Insert("k4", JsonElement(camera.RadialDistortion()[3]));
        }

        jsonCamera.Insert("p1", JsonElement(camera.TangentialDistortion()[1])); // note swapped tangential distortion between opencv and metashape
        jsonCamera.Insert("p2", JsonElement(camera.TangentialDistortion()[0]));
        if (camera.TangentialDistortion()[2] != T(0) || camera.TangentialDistortion()[3] != T(0)) {
            LOG_ERROR("failed to write camera parameters as extended tangential distortion is not supported");
            return false;
        }

        JsonElement jsonTransform(JsonElement::JsonType::Array);
        const Eigen::Matrix<T, 4, 4> transform = camera.Extrinsics().Matrix();
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                jsonTransform.Append(JsonElement(transform(r, c)));
            }
        }
        jsonCamera.Insert("transform", std::move(jsonTransform));
        json.Append(std::move(jsonCamera));
    }

    WriteFile(filename, WriteJson(json, /*tabs=*/1));

    return true;
}

template<class T>
void writeXmp(const std::string &filename,
              const std::string &calibrationPrior,
              const int group,
              T rcFocalLength,
              T principalPointU,
              T principalPointV,
              T skew,
              T aspectRatio,
              const Eigen::Vector<T, 4> &radialDistortion,
              const Eigen::Vector<T, 4> &tangentialDistortion,
              const Eigen::Matrix3<T> &rotation,
              const Eigen::Vector3<T> &translation)
{
    std::ofstream file;
    file.open(filename);

    auto position = -rotation.transpose() * translation;

    file << "<x:xmpmeta xmlns:x=\"adobe:ns:meta / \">" << std::endl;
    file << "  ";
    file << "<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">" << std::endl;
    file << "    ";
    file << "<rdf:Description xcr:Version=\"3\" xcr:PosePrior=\"" << calibrationPrior << "\""
         << " xcr:DistortionPrior=\"" << calibrationPrior << "\""
         << " xcr:Coordinates=\"absolute\"" << std::endl;
    file << "       ";
    file << "xcr:DistortionModel=\"brown3t2\"" << std::endl;
    file << "       ";
    file << "xcr:FocalLength35mm=\"" << std::setprecision(8) << rcFocalLength << "\" xcr:Skew=\"" << skew << "\"" << std::endl;
    file << "       ";
    file << "xcr:AspectRatio=\"" << std::setprecision(8) << aspectRatio << "\" xcr:PrincipalPointU=\"" << std::setprecision(8) << principalPointU
         << "\"" << std::endl;
    file << "       ";
    file << "xcr:PrincipalPointV=\"" << std::setprecision(8) << principalPointV << "\" xcr:CalibrationPrior=\"" << calibrationPrior << "\""
         << std::endl;
    file << "       ";
    file << "xcr:CalibrationGroup=\"" << group << "\" xcr:DistortionGroup=\"" << group << "\""
         << " xcr:LockedPoseGroup=\"" << group << "\""
         << " xcr:InTexturing=\"" << group << "\"" << std::endl;
    file << "       ";
    file << "xcr:InMeshing=\"" << group << "\" xmlns:xcr=\"http://www.capturingreality.com/ns/xcr/1.1#\">" << std::endl;
    file << "      ";
    file << "<xcr:Rotation>";
    for (int j = 0; j < rotation.rows(); j++)
    {
        for (int k = 0; k < rotation.cols(); k++)
        {
            if ((j == 2) && (k == 2))
            {
                file << std::setprecision(8) << std::to_string(rotation(j, k));
            }
            else
            {
                file << std::setprecision(8) << rotation(j, k) << " ";
            }
        }
    }
    file << "</xcr:Rotation>" << std::endl;
    file << "      ";
    file << "<xcr:Position>";
    for (int j = 0; j < position.size(); j++)
    {
        if (j == 2)
        {
            file << std::setprecision(8) << std::to_string(position(j));
        }
        else
        {
            file << std::setprecision(8) << position(j) << " ";
        }
    }
    file << "</xcr:Position>" << std::endl;
    file << "      ";
    file << "<xcr:DistortionCoeficients>";
    file << radialDistortion(0) << " " << radialDistortion(1) << " " << radialDistortion(2) << " " << radialDistortion(3) << " "
         << tangentialDistortion(0) << " " << tangentialDistortion(1) << "</xcr:DistortionCoeficients>" << std::endl;
    file << "    ";
    file << "</rdf:Description>" << std::endl;
    file << "  ";
    file << "</rdf:RDF>" << std::endl;
    file << "</x:xmpmeta>" << std::endl;

    file.close();
}

/**
 * Writes camera calibration information into xmp format
 */
template<class T>
bool WriteMetaShapeCamerasToXmpFolder(const std::string& folderPath, const std::vector<MetaShapeCamera<T>>& cameras, int type)
{
    std::string calibrationPrior = "exact";

    if (type == 0)
    {
        calibrationPrior = "initial";
    }

    const int group = 1;
    const T rcSensorWidth = 36.0;

    for (const auto &camera : cameras)
    {
        T f = camera.Intrinsics()(0, 0) + camera.Skew()(0);

        T cameraImageWidth = (T)(std::max(camera.Width(), camera.Height()));
        T rcPixelSize = rcSensorWidth / cameraImageWidth;
        T rcFocalLength = f * rcPixelSize;
        T aspectRatio = camera.Intrinsics()(1, 1) / f;

        T principalPointU = (camera.Intrinsics()(0, 2) - (T)camera.Width() / T(2)) / cameraImageWidth;
        T principalPointV = (camera.Intrinsics()(1, 2) - (T)camera.Height() / T(2)) / cameraImageWidth;
        const T skew = T(camera.Skew()(1) / cameraImageWidth);
        Eigen::Matrix3<T> rotation = camera.Extrinsics().Linear();
        Eigen::Vector3<T> translation = camera.Extrinsics().Translation();
        auto radialDistortion = camera.RadialDistortion();
        auto tangentialDistortion = camera.TangentialDistortion();
        std::string filename = folderPath + camera.Label() + ".xmp";
        writeXmp<T>(filename,
                    calibrationPrior,
                    group,
                    rcFocalLength,
                    principalPointU,
                    principalPointV,
                    skew,
                    aspectRatio,
                    radialDistortion,
                    tangentialDistortion,
                    rotation,
                    translation);
    }

    return true;
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
