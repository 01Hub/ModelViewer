#ifndef MESHPROPERTIES_H
#define MESHPROPERTIES_H

class SceneMesh;

#include <QObject>
#include "BoundingBox.h"

class RenderableMesh;
class MeshProperties : public QObject
{
	Q_OBJECT
public:
	explicit MeshProperties(SceneMesh* mesh, QObject* parent = nullptr);

	SceneMesh* mesh() const;
	void setMesh(SceneMesh* mesh);

	std::vector<float> meshPoints() const;

	float surfaceArea() const;

	float volume() const;

	float weight() const;

	void setDensity(const float& density);
	float density() const;

	QVector3D centerOfMass() const;

	BoundingBox boundingBox() const;

signals:

private:
	void calculateSurfaceAreaAndVolume();

private:
	SceneMesh* _mesh;
	std::vector<float> _meshPoints;
	float _surfaceArea;
	float _volume;
	float _weight;
	float _density;
	QVector3D _centerOfMass;
	BoundingBox _boundingBox;
};

#endif // MESHPROPERTIES_H
