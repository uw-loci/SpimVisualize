#pragma once


struct Threshold
{
	float			min;
	float			max;

	float			mean;
	float			stdDeviation;

	inline float getSpread() const { return max - min; }
	inline float getRelativeValue(float v) const { return (v - min) / (max - min); }


	inline void set(float Min, float Max) { max = Max; min = Min; mean = (max - min) / 2; stdDeviation = mean - min; }
};