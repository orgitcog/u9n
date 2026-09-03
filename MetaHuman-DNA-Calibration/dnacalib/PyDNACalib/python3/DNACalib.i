// Copyright Epic Games, Inc. All Rights Reserved.
// SWIG interface file for DNACalib Python bindings

%module(directors="1") dnacalib

%{
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// DNACalib library headers - only include what's needed
#include "dnacalib/Defs.h"
%}

// Include standard SWIG typemaps
%include <std_string.i>
%include <std_vector.i>
%include <stdint.i>

// Define vector templates
%template(StringVector) std::vector<std::string>;
%template(FloatVector) std::vector<float>;
%template(IntVector) std::vector<int>;
%template(UInt16Vector) std::vector<uint16_t>;
%template(UInt32Vector) std::vector<uint32_t>;

// Exception handling
%exception {
    try {
        $action
    } catch (const std::exception& e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        SWIG_fail;
    }
}

// Include the safe headers
%include "dnacalib/Defs.h"
