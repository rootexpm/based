#pragma once
#include <numbers>
#include <cstdint>
#include <cmath>
#include <algorithm>

class CVector
{
public:
	constexpr CVector operator+(const CVector& other) const noexcept
	{
		return { x + other.x, y + other.y, z + other.z };
	}

	constexpr CVector operator-(const CVector& other) const noexcept
	{
		return { x - other.x, y - other.y, z - other.z };
	}

	constexpr CVector operator*(const CVector& other) const noexcept
	{
		return { x * other.x, y * other.y, z * other.z };
	}

	constexpr CVector operator/(const CVector& other) const noexcept
	{
		return { x / other.x, y / other.y, z / other.z };
	}

	constexpr CVector Scale(float factor) const noexcept
	{
		return { x * factor, y * factor, z * factor };
	}

	constexpr CVector operator*(float scalar) const noexcept {
		return { x * scalar, y * scalar, z * scalar };
	}

	inline CVector ToAngle() const noexcept
	{
		return {
			std::atan2(-z, std::hypot(x, y)) * (180.0f / std::numbers::pi_v<float>),
			std::atan2(y, x) * (180.0f / std::numbers::pi_v<float>),
			0.0f 
		};
	}

	inline void Normalize() noexcept {
		while (y > 180.0f) y -= 360.0f;
		while (y < -180.0f) y += 360.0f;
		x = std::clamp(x, -89.0f, 89.0f);
	}

	inline float Length() const noexcept {
		return std::sqrt(x * x + y * y + z * z);
	}

	inline float Length2D() const noexcept {
		return std::sqrt(x * x + y * y);
	}

	float x{ }, y{ }, z{ };
};

__declspec(align(16)) class CVectorAligned : public CVector
{
public:
	constexpr CVectorAligned operator-(const CVectorAligned& other) const noexcept
	{
		return { x - other.x, y - other.y, z - other.z, w - other.w };
	}

	float w{ };
};
