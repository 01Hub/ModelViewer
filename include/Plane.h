#pragma once

#include "RenderableMesh.h"

class Plane : public TriangleMesh
{
public:
	enum class Orientation
	{
		XY_ZNormal,
		XZ_YNormal
	};

	Plane(QOpenGLShaderProgram* prog, QVector3D center, float xsize, float ysize, int xdivs, int ydivs, float zlevel = 0.0f, float smax = 1.0f, float tmax = 1.0f, Orientation orientation = Orientation::XY_ZNormal);
	void setPlane(QOpenGLShaderProgram* prog, QVector3D center, float xsize, float ysize, int xdivs, int ydivs, float zlevel = 0.0f, float smax = 1.0f, float tmax = 1.0f, Orientation orientation = Orientation::XY_ZNormal);

	virtual TriangleMesh* clone();
private:
	void buildMesh(QVector3D center, float xsize, float ysize, int xdivs, int ydivs, float zlevel, float smax, float tmax, Orientation orientation);

protected:
	QVector3D	_center;
	float		_xSize;
	float		_ySize;
	int			_xDivs;
	int			_yDivs;
	float		_zLevel;
	Orientation _orientation;
};
