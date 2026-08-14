// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// Keep in sync with MegaLights::ETileType
#define TILE_MODE_SIMPLE_SHADING						0
#define TILE_MODE_COMPLEX_SHADING						1
#define TILE_MODE_SIMPLE_SHADING_RECT					2
#define TILE_MODE_COMPLEX_SHADING_RECT					3
#define TILE_MODE_SIMPLE_SHADING_RECT_TEXTURED			4
#define TILE_MODE_COMPLEX_SHADING_RECT_TEXTURED			5
#define TILE_MODE_EMPTY									6
#define TILE_MODE_MAX_LEGACY							7
// Substrate
#define TILE_MODE_SINGLE_SHADING						7
#define TILE_MODE_COMPLEX_SPECIAL_SHADING				8
#define TILE_MODE_SINGLE_SHADING_RECT					9
#define TILE_MODE_COMPLEX_SPECIAL_SHADING_RECT			10
#define TILE_MODE_SINGLE_SHADING_RECT_TEXTURED			11
#define TILE_MODE_COMPLEX_SPECIAL_SHADING_RECT_TEXTURED	12
// Total
#define TILE_MODE_MAX									13

// Keep in sync with MegaLights.cpp
#define TILE_SIZE					8

// Limited by PackLightSample and PackCandidateLightSample
#define MAX_LOCAL_LIGHT_NUM			65536
#define MAX_LOCAL_LIGHT_INDEX		(MAX_LOCAL_LIGHT_NUM - 1)
#define VISIBLE_LIGHT_HASH_SIZE		4 // 4 uints