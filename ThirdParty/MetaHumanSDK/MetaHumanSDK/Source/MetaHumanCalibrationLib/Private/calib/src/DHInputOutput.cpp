// Copyright Epic Games, Inc. All Rights Reserved.

#include <calib/DHInputOutput.h>
#include <carbon/data/CameraModelOpenCV.h>
#include <carbon/io/CameraIO.h>
#include <nls/serialization/CameraSerialization.h>

#include <fstream>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::calib)

template <class T>
void writeXmp(const std::string& filename,
              const std::string& calibrationPrior,
              const int group,
              T rcFocalLength,
              T principalPointU,
              T principalPointV,
              T skew,
              T aspectRatio,
              const Eigen::Vector<T, 4>& radialDistortion,
              const Eigen::Vector<T, 4>& tangentialDistortion,
              const Eigen::Matrix3<T>& rotation,
              const Eigen::Vector3<T>& translation)
{
    std::ofstream file;
    file.open(filename);

    auto position = -rotation.transpose() * translation;

    file << "<x:xmpmeta xmlns:x=\"adobe:ns:meta / \">" << std::endl;
    file << "  ";
    file << "<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">" << std::endl;
    file << "    ";
    file << "<rdf:Description xcr:Version=\"3\" xcr:PosePrior=\"" << calibrationPrior << "\"" << " xcr:DistortionPrior=\"" <<
    calibrationPrior << "\"" << " xcr:Coordinates=\"absolute\"" << std::endl;
    file << "       ";
    file << "xcr:DistortionModel=\"brown3t2\"" << std::endl;
    file << "       ";
    file << "xcr:FocalLength35mm=\"" << std::setprecision(8) << rcFocalLength << "\" xcr:Skew=\"" << skew << "\"" <<
    std::endl;
    file << "       ";
    file << "xcr:AspectRatio=\"" << std::setprecision(8) << aspectRatio << "\" xcr:PrincipalPointU=\"" <<
    std::setprecision(8) << principalPointU << "\"" << std::endl;
    file << "       ";
    file << "xcr:PrincipalPointV=\"" << std::setprecision(8) << principalPointV << "\" xcr:CalibrationPrior=\"" <<
    calibrationPrior << "\"" << std::endl;
    file << "       ";
    file << "xcr:CalibrationGroup=\"" << group << "\" xcr:DistortionGroup=\"" << group << "\"" << " xcr:LockedPoseGroup=\"" <<
    group << "\"" << " xcr:InTexturing=\"" << group << "\"" << std::endl;
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
    file << radialDistortion(0) << " " << radialDistortion(1) << " " << radialDistortion(2) << " " << radialDistortion(3) <<
    " " << tangentialDistortion(0) << " " << tangentialDistortion(1) <<
    "</xcr:DistortionCoeficients>" << std::endl;
    file << "    ";
    file << "</rdf:Description>" << std::endl;
    file << "  ";
    file << "</rdf:RDF>" << std::endl;
    file << "</x:xmpmeta>" << std::endl;

    file.close();
}

template void writeXmp(const std::string& filename,
                       const std::string& calibrationPrior,
                       const int group,
                       float rcFocalLength,
                       float rcCx,
                       float rcCy,
                       float rcSkew,
                       float aspectRatio,
                       const Eigen::Vector<float, 4>& radial,
                       const Eigen::Vector<float, 4>& tangential,
                       const Eigen::Matrix3<float>& rotation,
                       const Eigen::Vector3<float>& translation);

template void writeXmp(const std::string& filename,
                       const std::string& calibrationPrior,
                       const int group,
                       double rcFocalLength,
                       double rcCx,
                       double rcCy,
                       double rcSkew,
                       double aspectRatio,
                       const Eigen::Vector<double, 4>& radial,
                       const Eigen::Vector<double, 4>& tangential,
                       const Eigen::Matrix3<double>& rotation,
                       const Eigen::Vector3<double>& translation);

template <class T>
void writeCamerasRealityCapture(const std::string& path, const std::vector<TITAN_NAMESPACE::MetaShapeCamera<T>>& cameras, int type)
{
    std::string calibrationPrior = "exact";

    if (type == 0)
    {
        calibrationPrior = "initial";
    }

    const int group = 1;
    const T rcSensorWidth = 36.0;

    for (const auto& camera : cameras)
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
                         std::string filename = path + camera.Label() + ".xmp";
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
}

template void writeCamerasRealityCapture(const std::string& path, const std::vector<TITAN_NAMESPACE::MetaShapeCamera<double>>& cameras, int type);
template void writeCamerasRealityCapture(const std::string& path, const std::vector<TITAN_NAMESPACE::MetaShapeCamera<float>>& cameras, int type);

template <class T>
void writeCameraRealityCapture(const std::string& path, const TITAN_NAMESPACE::MetaShapeCamera<T>& camera, int type)
{
    std::string calibrationPrior = "exact";

    if (type == 0)
    {
        calibrationPrior = "initial";
    }

    const int group = 1;
    const T rcSensorWidth = 36.0;

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
                     writeXmp<T>(path,
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

template void writeCameraRealityCapture(const std::string& path, const TITAN_NAMESPACE::MetaShapeCamera<double>& cameras, int type);
template void writeCameraRealityCapture(const std::string& path, const TITAN_NAMESPACE::MetaShapeCamera<float>& cameras, int type);

template <class T>
std::vector<Camera*> loadCamerasJson(const std::string& path)
{
    std::vector<TITAN_NAMESPACE::CameraModelOpenCV<T>> carbonCameras = TITAN_NAMESPACE::ReadOpenCvModelJson<T>(path);
    std::vector<Camera*> calibCameras;

    for (auto carbonCamera : carbonCameras)
    {
        std::string cameraLabel = carbonCamera.GetLabel();
        std::string modelLabel = carbonCamera.GetModel();

        std::optional<Camera*> maybeCamera = Camera::create(cameraLabel.c_str());
        Camera* camera = maybeCamera.value();
        std::optional<CameraModel*> model = CameraModel::create(modelLabel.c_str(),
                                                                carbonCamera.GetWidth(),
                                                                carbonCamera.GetHeight());
        CameraModel* calibModel = model.value();

        calibModel->setDistortionParams(carbonCamera.GetDistortionParams().template cast<real_t>());
        calibModel->setIntrinsicMatrix(carbonCamera.GetIntrinsics().template cast<real_t>());
        camera->setCameraModel(*calibModel);

        const auto cam = carbonCamera.GetExtrinsics();
        Eigen::MatrixX<real_t> cameraTransform = Eigen::MatrixX<real_t>::Identity(4, 4);
        cameraTransform.block<3, 4>(0, 0) = cam.template cast<real_t>();

        camera->setWorldPosition(cameraTransform);
        calibCameras.push_back(camera);
    }
    return calibCameras;
}

template std::vector<Camera*> loadCamerasJson<float>(const std::string& path);
template std::vector<Camera*> loadCamerasJson<double>(const std::string& path);

template <class T>
std::vector<Camera*> loadCamerasXml(const std::string& path)
{
    std::vector<TITAN_NAMESPACE::CameraModelOpenCV<T>> carbonCameras = TITAN_NAMESPACE::ReadOpenCvModelXml<T>(path);
    std::vector<Camera*> calibCameras;

    for (auto carbonCamera : carbonCameras)
    {
        std::string cameraLabel = carbonCamera.GetLabel();
        std::string modelLabel = carbonCamera.GetModel();

        std::optional<Camera*> maybeCamera = Camera::create(cameraLabel.c_str());
        Camera* camera = maybeCamera.value();

        std::optional<CameraModel*> model = CameraModel::create(modelLabel.c_str(),
                                                                carbonCamera.GetWidth(),
                                                                carbonCamera.GetHeight());
        CameraModel* calibModel = model.value();
        calibModel->setDistortionParams(carbonCamera.GetDistortionParams().template cast<real_t>());
        calibModel->setIntrinsicMatrix(carbonCamera.GetIntrinsics().template cast<real_t>());

        camera->setCameraModel(*calibModel);

        Eigen::MatrixX<T> cam = carbonCamera.GetExtrinsics();
        Eigen::MatrixX<real_t> cameraTransform = Eigen::MatrixX<real_t>::Identity(4, 4);
        cameraTransform.block<3, 4>(0, 0) = cam.template cast<real_t>();

        camera->setWorldPosition(cameraTransform);
        calibCameras.push_back(camera);
    }
    return calibCameras;
}

template std::vector<Camera*> loadCamerasXml<float>(const std::string& path);
template std::vector<Camera*> loadCamerasXml<double>(const std::string& path);

template <class T>
void writeCamerasJson(const std::string& path, const std::vector<Camera*>& cameras, bool setOriginInFirstCamera)
{
    std::vector<TITAN_NAMESPACE::CameraModelOpenCV<T>> carbonCameras;

    for (const auto& calibCamera : cameras)
    {
        Eigen::Matrix<T, 4, 4> transform44 = calibCamera->getWorldPosition().inverse().template cast<T>();
        if (setOriginInFirstCamera)
        {
            transform44 = transform44 * cameras[0]->getWorldPosition().template cast<T>();
        }
        Eigen::Matrix<T, 3, 4> transform = transform44.template block<3, 4>(0, 0);

        const Eigen::Matrix3<T> K = calibCamera->getCameraModel()->getIntrinsicMatrix().template cast<T>();
        const Eigen::Vector<T, 5> D = calibCamera->getCameraModel()->getDistortionParams().template cast<T>();

        const int cameraWidth = static_cast<int>(calibCamera->getCameraModel()->getFrameWidth());
        const int cameraHeight = static_cast<int>(calibCamera->getCameraModel()->getFrameHeight());
        const std::string cameraLabel = calibCamera->getTag();
        const std::string sensorLabel = calibCamera->getCameraModel()->getTag();

        TITAN_NAMESPACE::CameraModelOpenCV<T> cameraToWrite;

        cameraToWrite.SetIntrinsics(K);
        cameraToWrite.SetExtrinsics(transform);
        cameraToWrite.SetHeight(cameraHeight);
        cameraToWrite.SetWidth(cameraWidth);
        cameraToWrite.SetLabel(cameraLabel);
        cameraToWrite.SetModel(sensorLabel);
        cameraToWrite.SetDistortionParams(D);

        carbonCameras.push_back(cameraToWrite);
    }
    TITAN_NAMESPACE::WriteOpenCvModelJson<T>(path, carbonCameras);
}

template void writeCamerasJson<float>(const std::string& path, const std::vector<Camera*>& camera, bool setOriginInFirstCameras);
template void writeCamerasJson<double>(const std::string& path, const std::vector<Camera*>& cameras, bool setOriginInFirstCamera);

template <class T>
void writeCamerasXml(const std::string& path, const std::vector<Camera*>& cameras, bool setOriginInFirstCamera)
{
    std::vector<TITAN_NAMESPACE::CameraModelOpenCV<T>> carbonCameras;
    for (const auto& calibCamera : cameras)
    {
        TITAN_NAMESPACE::CameraModelOpenCV<T> cameraToWrite;

        Eigen::Matrix<T, 4, 4> transform44 = calibCamera->getWorldPosition().inverse().template cast<T>();
        if (setOriginInFirstCamera)
        {
            transform44 = transform44 * cameras[0]->getWorldPosition().template cast<T>();
        }
        Eigen::Matrix<T, 3, 4> transform = transform44.template block<3, 4>(0, 0);
        const Eigen::Matrix3<T> K = calibCamera->getCameraModel()->getIntrinsicMatrix().template cast<T>();
        const Eigen::VectorX<T> D = calibCamera->getCameraModel()->getDistortionParams().template cast<T>();

        const int cameraWidth = static_cast<int>(calibCamera->getCameraModel()->getFrameWidth());
        const int cameraHeight = static_cast<int>(calibCamera->getCameraModel()->getFrameHeight());
        const std::string cameraLabel = calibCamera->getTag();
        const std::string sensorLabel = calibCamera->getCameraModel()->getTag();

        cameraToWrite.SetIntrinsics(K);
        cameraToWrite.SetExtrinsics(transform);
        cameraToWrite.SetHeight(cameraHeight);
        cameraToWrite.SetWidth(cameraWidth);
        cameraToWrite.SetLabel(cameraLabel);
        cameraToWrite.SetModel(sensorLabel);
        cameraToWrite.SetDistortionParams(D);

        carbonCameras.push_back(cameraToWrite);
    }
    TITAN_NAMESPACE::WriteOpenCvModelXml<T>(path, carbonCameras);
}

template void writeCamerasXml<float>(const std::string& path, const std::vector<Camera*>& cameras, bool setOriginInFirstCamera);
template void writeCamerasXml<double>(const std::string& path, const std::vector<Camera*>& cameras, bool setOriginInFirstCamera);

CARBON_NAMESPACE_END(TITAN_NAMESPACE::calib)
