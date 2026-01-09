// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class FDNACalibDNAReader;

/**
	@brief UE Interface for DNACalib Command wrappers.
*/
class IDNACalibCommand
{
public:
	virtual ~IDNACalibCommand() = default;
	virtual void Run(FDNACalibDNAReader* Output) = 0;

};
