#pragma once

#include "cvector.h"

class CMatrix3x4
{
public:
	// Gives access like matrix[row][col]
	inline float* operator[](size_t index) noexcept {
		return data[index];
	}

	inline const float* operator[](size_t index) const noexcept {
		return data[index];
	}

	// Return origin vector (translation part)
	inline CVector Origin() const noexcept {
		return { data[0][3], data[1][3], data[2][3] };
	}

	float data[3][4];
};


class CMatrix4x4
{
public:
	float data[4][4];
};
