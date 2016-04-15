#pragma once

#include <string>
#include <glm/glm.hpp>

class InteractionVolume;
class Viewport;


class IWidget
{
public:
	IWidget();
	IWidget(const IWidget& cp);
	virtual ~IWidget();

	virtual void draw(const Viewport* vp) const = 0;

	virtual void beginMouseMove(const Viewport* vp, const glm::vec2& coords);
	virtual void updateMouseMove(const Viewport* vp, const glm::vec2& coords, float valueStep = 0.f);
	virtual void endMouseMove(const Viewport* vp, const glm::vec2& coords);


	void setInteractionVolume(InteractionVolume* v);

	void setMode(const std::string& mode);
	std::string getMode() const;

	inline bool isActive() const { return active; }

protected:
	bool				active;
	InteractionVolume*	volume;

	glm::mat4			initialVolumeMatrix;
	glm::vec3			initialVolumePosition;
	
	glm::vec2			initialMouseCoords;
	glm::vec2			currentMouseCoords;





	enum Mode
	{
		VIEW_RELATIVE,
		AXIS_X,
		AXIS_Y,
		AXIS_Z
	};

	Mode				mode;


	virtual void applyTransform(const Viewport* vp, float valueStep) = 0;
	
};

class TranslateWidget : public IWidget
{
public:
	virtual void draw(const Viewport* vp) const;


protected:
	virtual void applyTransform(const Viewport* vp, float valueStep);

};

class RotateWidget : public IWidget
{
public:
	virtual void draw(const Viewport* vp) const;


protected:
	virtual void applyTransform(const Viewport* vp, float valueStep);
};

class ScaleWidget : public IWidget
{
public:
	virtual void draw(const Viewport* vp) const;


protected:
	virtual void applyTransform(const Viewport* vp, float valueStep);
};



