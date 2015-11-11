#include "BeadDetection.h"
#include "SpimStack.h"

#include <opencv2/imgproc.hpp>

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>

using namespace std;
using namespace glm;

void Hourglass::draw() const
{
	glPushMatrix();
	glTranslatef(centerAxis.x, centerAxis.y, 0.f);

	for (size_t i = 0; i < circles.size(); ++i)
	{

		glPushMatrix();
		glTranslatef(0, 0, circles[i].x);
		glScalef(circles[i].y, circles[i].y, 1.f);


		static vector<vec3> circle;
		if (circle.empty())
		{
			for (int a = 0; a < 36; ++a)
			{
				float angle = radians((float)a);
				circle.push_back(vec3(sin(angle), cos(angle), 0.f));			
			}
		}

		glVertexPointer(3, GL_FLOAT, 0, glm::value_ptr(circle[0]));
		glDrawArrays(GL_LINE_LOOP, 0, circle.size());


		glPopMatrix();

	}

	glPopMatrix();
}
