#include "Histogram.h"
#include "StackRegistration.h"

#include <cmath>
#include <algorithm>
#include <iostream>
#include <iomanip>

using namespace std;

void Histogram::reset()
{
	min = 0;
	max = 0;
	bins.clear();
}

void Histogram::calculate(unsigned short bucketSize, const vector<unsigned short>& data)
{
	const float bSize = (float)bucketSize / numeric_limits<unsigned short>::max();

	int number_of_buckets = (int)ceil(1.f / bSize); // requires <cmath>
	
	bins.clear();
	for (size_t i = 0; i < data.size(); ++i)
	{
		int bucket = (int)floor(data[i] / bSize);
		bins[bucket]++;
	}

	min = numeric_limits<unsigned int>::max();
	max = 0;
	for (size_t i = 0; i < bins.size(); ++i)
	{
		this->min = std::min(bins[i], min);
		this->max = std::max(bins[i], max);

	}
	
}


void Histogram::print() const
{

	cout << "[Histo]:\n";
	//cout << "     Below [<" << setw(5) << (int)t.min << "]: " << setw(7) << belowBin << " (" << setprecision(4) << (float)belowBin / COUNT << ")" << endl;

	for (unsigned int i = 0; i < bins.size(); ++i)
	{
		cout << setw(3) << i << "[" << setw(5) << i*binSize << " -> " << setw(5) << (i + 1)*binSize << "]: " << setw(7) << bins[i] << endl; // " (" << setprecision(4) << (float)bins[i] / COUNT << ")" << endl;
	}

	//cout << "     Above [>" << setw(5) << (int)t.max << "]: " << setw(7) << aboveBin << " (" << setprecision(4) << (float)aboveBin / COUNT << ")" << endl;


}
