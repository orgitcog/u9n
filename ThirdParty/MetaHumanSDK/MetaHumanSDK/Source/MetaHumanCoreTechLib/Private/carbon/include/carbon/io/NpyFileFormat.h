// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Math.h>

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>

#if !defined(CARBON_NPY_NAMESPACE)
#define CARBON_NPY_NAMESPACE TITAN_NAMESPACE::npy
#endif

// Important: this code assumes the machine is using little endian ("<" in NumPy, as opposed to ">" for big endian)

CARBON_NAMESPACE_BEGIN(CARBON_NPY_NAMESPACE)

template <typename T> std::string NpyTypeName();

struct NPYHeader
{
    std::string m_dataType; // supported: "<i4" (32-bit int), "<f4" (32-bit float), "<f8" (64-bit double)
    std::vector<int> m_shape;
    bool m_fortranOrder;

    int DataTypeSize() const;

    template <typename T> bool IsType() { return m_dataType == NpyTypeName<T>(); }
};

void SaveNPYRaw(std::ostream& out, const NPYHeader& header, const uint8_t* data, size_t dataSize);

template <class T> void SaveNPY(std::ostream& out, const NPYHeader& header, const std::vector<T>& data)
{
    SaveNPYRaw(out, header, (const uint8_t*)data.data(), sizeof(T) * data.size());
}
template <class T> void SaveNPY(const std::string& filename, const NPYHeader& header, const std::vector<T>& data)
{
    std::ofstream out(filename, std::ios::binary);
    SaveNPY(out, header, data);
}

template <class T, int R, int C, int MatrixOrder>
void SaveMatrixAsNpy(std::ostream& out, const Eigen::Matrix<T, R, C, MatrixOrder>& matrix)
{
    NPYHeader vertices_header;
    vertices_header.m_dataType = NpyTypeName<T>();
    vertices_header.m_shape = { int(matrix.rows()), int(matrix.cols()) };
    vertices_header.m_fortranOrder = (MatrixOrder == Eigen::ColMajor) ? true : false;
    SaveNPYRaw(out, vertices_header, (const uint8_t*)matrix.data(), sizeof(T) * matrix.size());
}

template <class T, int R, int C, int MatrixOrder>
void SaveMatrixAsNpy(const std::string& filename, const Eigen::Matrix<T, R, C, MatrixOrder>& matrix)
{
    std::ofstream out(filename, std::ios::binary);
    SaveMatrixAsNpy(out, matrix);
}

void LoadNPYRawHeader(const std::vector<char>& htxt, NPYHeader& header);
void LoadNPYRawHeader(std::istream& in, NPYHeader& header);
void LoadNPYRawData(std::istream& in, const NPYHeader& header, std::vector<uint8_t>& data);
void LoadNPYRaw(std::istream& in, NPYHeader& header, std::vector<uint8_t>& data);

void LoadNPYRawHeader(FILE* pFile, NPYHeader& header);
void LoadNPYRawData(FILE* pFile, const NPYHeader& header, std::vector<uint8_t>& data);

template <class T, std::enable_if_t<!std::is_same_v<T, uint8_t>, bool> = true>
void LoadNPY(std::istream& in, NPYHeader& header, std::vector<T>& data)
{
    std::vector<uint8_t> rawData;
    LoadNPYRaw(in, header, rawData);
    if (!header.IsType<T>())
    {
        CARBON_CRITICAL("Incorrect type T: npy contains {}, but {} was requested.", header.m_dataType, NpyTypeName<T>());
    }
    data.resize(rawData.size() / sizeof(T));
    std::memcpy(data.data(), rawData.data(), rawData.size());
}

template <class T, std::enable_if_t<!std::is_same_v<T, uint8_t>, bool> = true>
void LoadNPY(const std::string& filename, NPYHeader& header, std::vector<T>& data)
{
    std::ifstream in(filename, std::ios::binary);
    LoadNPY(in, header, data);
}

template <class T, int R, int C, int MatrixOrder>
void LoadMatrixFromNpy(std::istream& in, Eigen::Matrix<T, R, C, MatrixOrder>& matrix)
{
    NPYHeader header;
    LoadNPYRawHeader(in, header);

    const int rows = header.m_shape[0];
    int cols = 1;
    if (header.m_shape.size() > 1)
    {
        cols = header.m_shape[1];
    }
    if constexpr (R >= 0)
    {
        if (rows != R)
        {
            CARBON_CRITICAL("Npy error: number of rows expected to be {}, but got {}", R, rows);
        }
    }
    if constexpr (C >= 0)
    {
        if (cols != C)
        {
            CARBON_CRITICAL("Npy error: number of cols expected to be {}, but got {}", C, cols);
        }
    }

    if (!header.IsType<T>())
    {
        CARBON_CRITICAL("Incorrect type T: npy contains {}, but {} was requested.", header.m_dataType, NpyTypeName<T>());
    }

    matrix.resize(rows, cols);

    if ((header.m_fortranOrder && MatrixOrder == Eigen::ColMajor) ||
        (!header.m_fortranOrder && MatrixOrder == Eigen::RowMajor))
    {
        in.read((char*)matrix.data(), rows * cols * sizeof(T));
    }
    else if (header.m_fortranOrder)
    {
        Eigen::Matrix<T, -1, -1> tmpMatrix(rows, cols);
        in.read((char*)tmpMatrix.data(), rows * cols * sizeof(T));
        matrix = tmpMatrix;
    }
    else
    {
        Eigen::Matrix<T, -1, -1, Eigen::RowMajor> tmpMatrix(rows, cols);
        in.read((char*)tmpMatrix.data(), rows * cols * sizeof(T));
        matrix = tmpMatrix;
    }
    if (!in) { CARBON_CRITICAL("Failed to read matrix from npy"); }
}


template <class T, int R, int C, int MatrixOrder>
void LoadMatrixFromNpy(FILE* pFile, Eigen::Matrix<T, R, C, MatrixOrder>& matrix)
{
    NPYHeader header;
    LoadNPYRawHeader(pFile, header);

    const int rows = header.m_shape[0];
    int cols = 1;
    if (header.m_shape.size() > 1)
    {
        cols = header.m_shape[1];
    }
    if constexpr (R >= 0)
    {
        if (rows != R)
        {
            CARBON_CRITICAL("Npy error: number of rows expected to be {}, but got {}", R, rows);
        }
    }
    if constexpr (C >= 0)
    {
        if (cols != C)
        {
            CARBON_CRITICAL("Npy error: number of cols expected to be {}, but got {}", C, cols);
        }
    }

    if (!header.IsType<T>())
    {
        CARBON_CRITICAL("Incorrect type T: npy contains {}, but {} was requested.", header.m_dataType, NpyTypeName<T>());
    }

    matrix.resize(rows, cols);

    if ((header.m_fortranOrder && MatrixOrder == Eigen::ColMajor) ||
        (!header.m_fortranOrder && MatrixOrder == Eigen::RowMajor))
    {
        if (fread((char*)matrix.data(), 1, rows * cols * sizeof(T), pFile) != (size_t)rows * cols * sizeof(T))
        {
            CARBON_CRITICAL("Failed to read matrix from npy");
        }
    }
    else
    {
        Eigen::Matrix<T, -1, -1, MatrixOrder == Eigen::RowMajor ? Eigen::ColMajor : Eigen::RowMajor> tmpMatrix(rows, cols);
        if (fread((char*)tmpMatrix.data(), 1, rows * cols * sizeof(T), pFile) != (size_t)rows * cols * sizeof(T))
        {
            CARBON_CRITICAL("Failed to read matrix from npy");
        }
        matrix = tmpMatrix;
    }
}

template <class T, int R, int C, int MatrixOrder>
void LoadMatrixFromNpy(const std::string& filename, Eigen::Matrix<T, R, C, MatrixOrder>& matrix)
{
    FILE* pFile = nullptr;
#ifdef _MSC_VER
    if (fopen_s(&pFile, filename.c_str(), "rb") == 0 && pFile != nullptr) {
#else
    pFile = fopen(filename.c_str(), "rb");
    if (pFile) {
#endif
        LoadMatrixFromNpy(pFile, matrix);
        fclose(pFile);
    }
}

CARBON_NAMESPACE_END(CARBON_NPY_NAMESPACE)
