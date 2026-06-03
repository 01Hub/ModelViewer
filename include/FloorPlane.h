#pragma once

#include "Plane.h"

class FloorPlane : public Plane
{
public:
	FloorPlane(QOpenGLShaderProgram* prog, QVector3D center, float xsize, float ysize, int xdivs, int ydivs, float zlevel = 0.0f, float smax = 1.0f, float tmax = 1.0f);

	TriangleMesh* clone() override;
	void render() override;

protected:
	void setupTextures() override;
	void setupUniforms() override;
};
