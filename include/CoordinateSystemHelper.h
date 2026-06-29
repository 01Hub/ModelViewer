#pragma once

#include "AssImpModelLoader.h"
#include "BoundingBox.h"
#include "Plane.h"
#include "RenderEnums.h"

#include <QMatrix4x4>
#include <QQuaternion>
#include <QVector3D>

namespace CoordinateSystemHelper
{
QQuaternion cameraUpAxisConventionRotation(bool cameraUpAxisZUp);
QVector3D transformVectorForCameraUpAxis(bool cameraUpAxisZUp, const QVector3D& vector);

void standardViewBasis(bool cameraUpAxisZUp,
                       ViewMode mode,
                       QVector3D& viewDir,
                       QVector3D& upDir,
                       QVector3D& rightDir);
QQuaternion standardViewRotation(bool cameraUpAxisZUp, ViewMode mode);

Plane::Orientation floorPlaneOrientation(bool cameraUpAxisZUp);
QVector3D currentWorldUpVector(bool cameraUpAxisZUp);
float coordinateAlongCurrentWorldUp(bool cameraUpAxisZUp, const QVector3D& point);
void setCoordinateAlongCurrentWorldUp(bool cameraUpAxisZUp, QVector3D& point, float value);

bool sceneUpAxisIsZUp(SceneUpAxis sceneUpAxis);
float groundPlaneScaleFactor(GroundMode groundMode, float floorSizeFactor);
float groundPlaneExtent(float floorSize, float floorSizeFactor, GroundMode groundMode);
float groundPlaneZ(const BoundingBox& boundingBox,
                   bool cameraUpAxisZUp,
                   float floorSize,
                   float floorOffsetPercent,
                   float depthBias);
}
