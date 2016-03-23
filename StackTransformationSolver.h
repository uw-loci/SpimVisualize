#pragma once

#include <glm/glm.hpp>
#include <boost/utility.hpp>

#include <random>
#include <vector>

#include "TinyStats.h"

class Framebuffer;
class InteractionVolume;


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

	/// initializes a new run of the solver
	virtual void initialize(const InteractionVolume* v) = 0;
	/// resets all previous solutions
	virtual void resetSolution() = 0;
	/// returns true if another solution should be tested
	virtual bool nextSolution() = 0;

	/// records the image/occlusion score for the current solution
	virtual void recordCurrentScore(double score) = 0;

	/// returns the current solution to be tested
	virtual const Solution& getCurrentSolution() const = 0;
	/// returns the best solution found so far
	virtual const Solution& getBestSolution() = 0;
	
	inline const TinyHistory<double>& getHistory() const { return history; }
	inline void clearHistory() { history.history.clear(); }

protected:
	TinyHistory<double>			history;


	// creates a rotation matrix around the volumes' centroid around the y-axis with the specified angle
	static glm::mat4 createRotationMatrix(float radians, const InteractionVolume* v);

};

/// tests a uniform range of different transformations in TX,TY,TZ, RY
class UniformSamplingSolver : public IStackTransformationSolver
{
public:

	virtual void initialize(const InteractionVolume* v);
	virtual void resetSolution();
	virtual bool nextSolution();

	virtual void recordCurrentScore(double score);

	virtual const Solution& getCurrentSolution() const;
	virtual const Solution& getBestSolution();

protected:
	std::vector<Solution>		solutions;
	int							currentSolution;

	virtual void createCandidateSolutions(const InteractionVolume* v) = 0;

	bool hasValidCurrentSolution() const;
 };

class DXSolver : public UniformSamplingSolver
{
protected:
	virtual void createCandidateSolutions(const InteractionVolume* v);
};

class DYSolver : public UniformSamplingSolver
{
protected:
	virtual void createCandidateSolutions(const InteractionVolume* v);
};

class DZSolver : public UniformSamplingSolver
{
protected:
	virtual void createCandidateSolutions(const InteractionVolume* v);
};

// only checks rotation solutions 
class RYSolver : public UniformSamplingSolver
{
protected:
	virtual void createCandidateSolutions(const InteractionVolume* v);

};

class MultiDimensionalHillClimb : public IStackTransformationSolver
{
public:
	MultiDimensionalHillClimb();


	virtual void initialize(const InteractionVolume* v);
	virtual void resetSolution();
	virtual bool nextSolution();

	virtual void recordCurrentScore(double score);

	const Solution& getCurrentSolution() const;
	inline const Solution& getBestSolution() { return bestSolution; }

private:
	std::vector<Solution>		potentialSolutions;
	Solution					bestSolution;
	
	unsigned int				solutionCounter;
	std::mt19937				rng;

	const InteractionVolume*	currentVolume;
	void createPotentialSolutions();
};

class SimulatedAnnealingSolver : public IStackTransformationSolver
{
public:

	virtual void initialize(const InteractionVolume* v);
	virtual void resetSolution();
	virtual bool nextSolution();

	virtual void recordCurrentScore(double score);

	inline const Solution& getCurrentSolution() const { return currentSolution; }
	inline const Solution& getBestSolution() { return bestSolution; }

	inline double getTemp() const { return temp; }

private:
	Solution					currentSolution, bestSolution;
	double						temp, cooling;

	std::mt19937				rng;
	unsigned int				iteration;

	const InteractionVolume*	currentVolume;
	void modifyCurrentSolution();

};

class ParameterSpaceMapping : public UniformSamplingSolver
{
public:
	virtual void createCandidateSolutions(const InteractionVolume* v);

	virtual const Solution& getBestSolution();

	std::vector<glm::vec4> getSolutions() const;
};

