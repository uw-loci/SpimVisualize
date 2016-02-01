#ifndef TINYSTATS_INCLUDED
#define TINYSTATS_INCLUDED

#include <algorithm>
#include <vector>

template<typename T>
struct TinyStats
{
	T				sum;
	T				squaredSum;
	T				min, max;
	unsigned int	counter;


	TinyStats() : sum(0), squaredSum(0), min(std::numeric_limits<T>::max()), max(std::numeric_limits<T>::lowest()), counter(0)
	{
	}

	TinyStats(const TinyStats& cp) : sum(cp.sum), squaredSum(cp.squaredSum), min(cp.min), max(cp.max), counter(cp.counter)
	{
	}

	virtual ~TinyStats() {}

	virtual void reset(const T& min_ = std::numeric_limits<T>::max(), const T& max_ = -(FLT_MAX - 2.f))
	{
		counter = 0;
		sum = T(0);
		squaredSum = T(0);
		min = min_;
		max = max_;
	}
	
	virtual void add(const T& val)
	{
		sum += val;
		squaredSum += (val*val);

		if (counter == 0)
			min = max = val;
		else
		{
			min = std::min(min, val);
			max = std::max(max, val);
		}

		++counter;
	}

	virtual void add(const TinyStats& t)
	{
		sum += t.sum;
		squaredSum += t.squaredSum;
		counter += t.counter;

		if (counter > 0)
		{
			min = std::min(min, t.min);
			max = std::max(max, t.max);
		}
	}
	
	inline const TinyStats& operator += (const T& v)
	{
		add(v);
		return *this;
	}

	inline const TinyStats& operator += (const TinyStats& t)
	{
		add(t);
		return *this;
	}

	T getMean() const
	{
		if (counter == 0)
			return T(0);
		
		return sum * 1.f / (float)counter;
	}

	T getRMS() const
	{
		return std::sqrt( (double)squaredSum / (double)counter);
	}


};


template<typename T>
struct TinyHistory : public TinyStats < T >
{
	std::vector<T>		history;

	virtual void reset()
	{
		TinyStats<T>::reset();
		history.clear();
	}

	virtual void add(const T& val)
	{
		TinyStats<T>::add(val);
		history.push_back(val);
	}
	
	inline size_t size() const
	{
		return history.size();
	}

};

#endif
