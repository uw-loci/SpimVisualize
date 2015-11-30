#pragma once

#include <glm/glm.hpp>


#undef near
#undef far

struct AABB;

class ICamera
{
public:
	virtual ~ICamera() {};

	virtual void setup() const = 0;

	virtual void getMVP(glm::mat4& mvp) const = 0;
	virtual void getMatrices(glm::mat4& proj, glm::mat4& view) const = 0;

	virtual void zoom(float d) = 0;
	virtual void rotate(float dt, float dp) = 0;
	virtual void pan(float dx, float dy) = 0;

	virtual glm::vec3 getPosition() const = 0;
	virtual glm::vec3 getViewDirection() const = 0;

	// calculates the planar movement from the given 2D vector which is assumed to be on the near plane of the camera
	virtual glm::vec3 calculatePlanarMovement(const glm::vec2& delta) const = 0;

	virtual void maximizeView(const AABB& bbox) = 0;

	float		aspect, near, far;
	glm::vec3	target, up;





};


class OrbitCamera : public ICamera
{
public:
	OrbitCamera();

	void setup() const;
	void jitter(float v);

	void getMVP(glm::mat4& mvp) const;
	void getMVPdouble(glm::dmat4& mvp) const;
	void getMatrices(glm::mat4& proj, glm::mat4& view) const;
	void getDoubleMatrices(glm::dmat4& proj, glm::dmat4& view) const;

	void zoom(float delta);
	void rotate(float deltaTheta, float deltaPhi);
	void pan(float deltaX, float deltaY);

	float		fovy;
	float		radius;

	inline glm::vec3 getPosition() const { return target + getOffset(); }
	inline void setTarget(const glm::vec3& t) { target = t; }

	inline glm::vec3 getViewDirection() const { return -getOffset(); }

	virtual glm::vec3 calculatePlanarMovement(const glm::vec2& delta) const;

	void maximizeView(const AABB& bbox);

	inline float getTheta() const { return theta; }
	inline float getPhi() const { return phi; }

private:
	float	theta, phi;

	glm::vec3 getOffset() const;

};

class OrthoCamera: public ICamera
{
public:
	OrthoCamera(const glm::vec3& viewDir, const glm::vec3& up, float z=1000.f);
	virtual void setup() const;

	virtual void getMVP(glm::mat4& mvp) const;
	virtual void getMatrices(glm::mat4& proj, glm::mat4& view) const;

	virtual void zoom(float d);
	virtual void rotate(float dt, float dp) {};
	virtual void pan(float dx, float dy);

	virtual glm::vec3 calculatePlanarMovement(const glm::vec2& delta) const;

	inline glm::vec3 getPosition() const { return target - viewDirection; } ;
	inline glm::vec3 getViewDirection() const {	return viewDirection;}

	void maximizeView(const AABB& bbox);

	float			zoomFactor;

protected:
	glm::vec3		viewDirection;
	

};


// an orthographic camera with projection in [0..1] in the XY plane
class UnitCamera : public ICamera
{
public:
	UnitCamera();

	virtual void setup() const;
	virtual void getMVP(glm::mat4& mvp) const;
	virtual void getMatrices(glm::mat4& proj, glm::mat4& view) const;

	virtual void zoom(float d) {};
	virtual void rotate(float dt, float dp) {};
	virtual void pan(float dx, float dy) { target += glm::vec3(dx, dy, 0.f); };

	virtual glm::vec3 calculatePlanarMovement(const glm::vec2& delta) const { return glm::vec3(delta, 0.f); }

	inline glm::vec3 getPosition() const { return target; };
	inline glm::vec3 getViewDirection() const { return glm::vec3(0,0,-1.f); }

	void maximizeView(const AABB& bbox);

};