#pragma once

#include <glm/glm.hpp>
#include <boost/utility.hpp>

#include <vector>

class IStackTransformationSolver : boost::noncopyable
{
public:
	struct Solution
	{
		// transformation matrix proposed
		glm::mat4			matrix;

		// score -- calculated externally?
		double				score;

		unsigned long		id;

		inline bool operator < (const Solution& rhs) const { return score < rhs.score; }
	};


	virtual ~IStackTransformationSolver() {};

	virtual void createCandidateSolutions() = 0;
	virtual void recordCurrentScore(double score);

	bool nextSolution();
	void resetSolutions();
	
	const Solution& getCurrentSolution() const;
	const Solution& getBestSolution();

	bool hasValidCurrentSolution() const;
	
protected:
	std::vector<Solution>		solutions;
	int							currentSolution;

	
	IStackTransformationSolver();
	
	void assertValidCurrentSolution();

};


class UniformSamplingSolver : public IStackTransformationSolver
{
public:
	virtual void createCandidateSolutions();
 };