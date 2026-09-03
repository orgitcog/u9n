// PythonDNACalibWrapper.h
// Deep Tree Echo - Python DNACalib Integration
// Copyright (c) 2025 Deep Tree Echo Project

#pragma once

#include "CoreMinimal.h"

/**
 * FPythonDNACalibWrapper
 * 
 * Wrapper class for MetaHuman DNA Calibration Python API.
 * Embeds Python interpreter and provides C++ interface to DNACalib functions.
 * 
 * Key Features:
 * - Embed Python interpreter in Unreal Engine
 * - Load DNACalib Python modules
 * - Provide C++ interface to Python DNACalib functions
 * - Handle Python-C++ data marshalling
 * 
 * Dependencies:
 * - Python 3.7 or 3.9 (matching Maya versions)
 * - MetaHuman-DNA-Calibration Python modules
 * - DNACalib library (PyDNA, PyDNACalib)
 * 
 * Usage:
 * ```cpp
 * TSharedPtr<FPythonDNACalibWrapper> Wrapper = MakeShared<FPythonDNACalibWrapper>();
 * if (Wrapper->Initialize("/path/to/MetaHuman-DNA-Calibration"))
 * {
 *     Wrapper->LoadDNA("/path/to/character.dna");
 *     TArray<FString> Joints = Wrapper->GetJointNames();
 *     Wrapper->SaveDNA("/path/to/modified.dna");
 * }
 * ```
 */
class UNREALECHO_API FPythonDNACalibWrapper
{
public:
    FPythonDNACalibWrapper();
    ~FPythonDNACalibWrapper();

    // ========================================
    // Initialization
    // ========================================

    /**
     * Initialize Python interpreter and load DNACalib modules
     * @param DNACalibPath - Path to MetaHuman-DNA-Calibration directory
     * @return True if successfully initialized
     */
    bool Initialize(const FString& DNACalibPath);

    /**
     * Shutdown Python interpreter
     */
    void Shutdown();

    /**
     * Check if wrapper is initialized
     * @return True if initialized
     */
    bool IsInitialized() const { return bInitialized; }

    // ========================================
    // DNA File Operations
    // ========================================

    /**
     * Load DNA file
     * @param DNAPath - Path to .dna file
     * @return True if successfully loaded
     */
    bool LoadDNA(const FString& DNAPath);

    /**
     * Save DNA file
     * @param DNAPath - Path to save .dna file
     * @return True if successfully saved
     */
    bool SaveDNA(const FString& DNAPath);

    /**
     * Get DNA database name (DHI or MH.4)
     * @return Database name string
     */
    FString GetDNADatabaseName();

    // ========================================
    // Rig Structure Access
    // ========================================

    /**
     * Get all joint names from DNA
     * @return Array of joint names
     */
    TArray<FString> GetJointNames();

    /**
     * Get all blend shape names from DNA
     * @return Array of blend shape names
     */
    TArray<FString> GetBlendShapeNames();

    /**
     * Get all mesh names from DNA
     * @return Array of mesh names
     */
    TArray<FString> GetMeshNames();

    /**
     * Get joint count
     * @return Number of joints in DNA
     */
    int32 GetJointCount();

    /**
     * Get blend shape count
     * @return Number of blend shapes in DNA
     */
    int32 GetBlendShapeCount();

    /**
     * Get mesh count
     * @return Number of meshes in DNA
     */
    int32 GetMeshCount();

    // ========================================
    // DNA Manipulation
    // ========================================

    /**
     * Rename joint in DNA
     * @param OldName - Current joint name
     * @param NewName - New joint name
     * @return True if successfully renamed
     */
    bool RenameJoint(const FString& OldName, const FString& NewName);

    /**
     * Remove joint from DNA
     * @param JointName - Name of joint to remove
     * @return True if successfully removed
     */
    bool RemoveJoint(const FString& JointName);

    /**
     * Rename mesh in DNA
     * @param OldName - Current mesh name
     * @param NewName - New mesh name
     * @return True if successfully renamed
     */
    bool RenameMesh(const FString& OldName, const FString& NewName);

    /**
     * Remove mesh from DNA
     * @param MeshName - Name of mesh to remove
     * @return True if successfully removed
     */
    bool RemoveMesh(const FString& MeshName);

    /**
     * Clear all blend shape data
     * @return True if successfully cleared
     */
    bool ClearBlendShapes();

    /**
     * Remove LOD from DNA
     * @param LODIndex - Index of LOD to remove
     * @return True if successfully removed
     */
    bool RemoveLOD(int32 LODIndex);

    /**
     * Rename blend shape channel in DNA
     * @param OldName - Current blend shape name
     * @param NewName - New blend shape name
     * @return True if successfully renamed
     */
    bool RenameBlendShape(const FString& OldName, const FString& NewName);

    /**
     * Remove blend shape channel from DNA
     * @param BlendShapeName - Name of blend shape to remove
     * @return True if successfully removed
     */
    bool RemoveBlendShape(const FString& BlendShapeName);

    /**
     * Modify blend shape deltas for a channel
     * @param BlendShapeName - Name of blend shape to modify
     * @param MeshName - Target mesh name
     * @param Deltas - Array of vertex position deltas
     * @return True if successfully modified
     */
    bool ModifyBlendShapeDeltas(const FString& BlendShapeName, const FString& MeshName, const TArray<FVector>& Deltas);

    /**
     * Get joint transform in neutral pose
     * @param JointName - Name of joint
     * @param OutTransform - Output transform
     * @return True if joint found
     */
    bool GetJointNeutralTransform(const FString& JointName, FTransform& OutTransform);

    /**
     * Get joint parent index
     * @param JointName - Name of joint
     * @return Parent joint index (-1 if root or not found)
     */
    int32 GetJointParentIndex(const FString& JointName);

    /**
     * Get blend shape target mesh name
     * @param BlendShapeName - Name of blend shape
     * @return Target mesh name
     */
    FString GetBlendShapeTargetMesh(const FString& BlendShapeName);

    /**
     * Get blend shape vertex count
     * @param BlendShapeName - Name of blend shape
     * @return Number of affected vertices
     */
    int32 GetBlendShapeVertexCount(const FString& BlendShapeName);

    // ========================================
    // Advanced Operations
    // ========================================

    /**
     * Execute arbitrary Python script
     * @param Script - Python script to execute
     * @return True if successfully executed
     */
    bool ExecutePythonScript(const FString& Script);

    /**
     * Call Python function with arguments
     * @param FunctionName - Name of function to call
     * @param Args - Array of string arguments
     * @return Result string from Python function
     */
    FString CallPythonFunction(const FString& FunctionName, const TArray<FString>& Args);

    /**
     * Get last Python error message
     * @return Error message string
     */
    FString GetLastError() const { return LastError; }

private:
    // Initialization state
    bool bInitialized;
    FString DNACalibRootPath;
    FString LastError;

    // Python interpreter state
    void* PythonInterpreter;
    void* PythonMainModule;
    void* PythonMainDict;

    // DNACalib modules
    void* DNAModule;
    void* DNACalibModule;

    // DNA reader/writer objects
    void* DNAReaderObject;
    void* DNAWriterObject;
    void* DNAStreamReaderObject;
    void* DNAStreamWriterObject;

    // Helper functions
    bool InitializePythonInterpreter();
    bool SetupPythonPath();
    bool LoadDNACalibModules();
    bool CreateDNAReader();
    void CleanupPythonObjects();

    // Python object helpers
    void* GetPythonObject(const FString& Name);
    bool SetPythonObject(const FString& Name, void* Object);
    void* CallPythonMethod(void* Object, const FString& MethodName, void* Args = nullptr);
    FString PythonObjectToString(void* Object);
    TArray<FString> PythonListToStringArray(void* List);
    int32 PythonObjectToInt(void* Object);

    // Error handling
    void SetLastError(const FString& Error);
    void LogPythonError();
};
