// Copyright Epic Games, Inc. All Rights Reserved.
// SWIG interface file for DNA Python bindings

%module(directors="1") dna

%{
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// DNA library headers - only include what's needed
#include "dna/DataLayer.h"
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

// Only include the DataLayer enum which is safe to wrap
%include "dna/DataLayer.h"
