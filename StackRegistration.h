#pragma once


#include <vector>
#include <glm/glm.hpp>

class SpimStack;


class ReferencePoints
{
public:	
	void draw() const;


	/// Determines the closest pairs between point and rearranges this point cloud to match the reference one.
	/// This also resizes this point cloud so that both it and the reference point cloud are the same size.
	float determineClosestPairs(const ReferencePoints* reference);


		
	// aligns to
	float align(const ReferencePoints* reference, glm::mat4& deltaTransform);

	inline bool empty() const { return points.empty(); }

	inline void setPoints(const std::vector<glm::vec4>& pts) { points = pts; }


private:
	friend class SpimStack;

	std::vector<glm::vec4>		points;
};
