#pragma once

#include <vector>

struct Threshold;

struct Histogram
{
	std::vector<unsigned int>		bins;


	// min and max count
	unsigned int		min, max;
	unsigned int		binSize;

	// the lowest and highest value in the histogram
	unsigned int		lowest, highest;


	void calculate(unsigned short bucketSize, const std::vector<unsigned short>& data);

	inline bool empty() const { return bins.empty(); }

	void reset();

	void print() const;

};