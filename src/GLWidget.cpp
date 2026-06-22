
#include "ClippingPlanesEditor.h"
#include "ExplodedViewPanel.h"
#include "AssemblyRelationGraph.h"
#include "ExplodedViewManager.h"
#include "SceneGraph.h"
#include "Cone.h"
#include "Cube.h"
#include "FloorPlane.h"
#include "GltfCameraData.h"
#include "GLWidget.h"
#include <QtMath>
#include "SelectionManager.h"
#include "TransformGizmo.h"
#include "LanguageManager.h"
#include "MainWindow.h"
#include "MaterialVariantsPanel.h"
#include "ModelViewer.h"
#include "SceneTreeWidget.h"
#include "AnimationsPanel.h"
#include "ModelViewerApplication.h"
#include "PathUtils.h"
#include "Plane.h"
#include "Point.h"
#include "Sphere.h"
#include "ViewCubeMesh.h"
#include "stb_image.h"
#include "TangentGenerator.h"
#include "TextRenderer.h"
#include "Utils.h"
#include <algorithm>
#include <iostream>
#include <QCryptographicHash>
#include <QOpenGLContext>
#include <QElapsedTimer>
#include <QMessageBox>
#include <QPainter>
#include <QSettings>
#include <QStyleFactory>
#include <QThread>
#include <QTreeView>
#include <QDebug>


constexpr auto MAX_MODEL_SIZE_BYTES = 52428800; // bytes

namespace
{
constexpr float kDefaultFloorOffsetPercent = 0.0f;
constexpr float kExplodedManualTransformTolerance = 1.0e-8f;

struct ViewCubeStyle
{
	QVector3D baseFaceColor = QVector3D(0.92f, 0.92f, 0.92f);
	QVector3D primaryFaceColor = QVector3D(0.90f, 0.76f, 0.10f);
	QVector3D hoverFaceColor = QVector3D(0.74f, 0.96f, 0.18f);
	QColor labelTextColor = QColor(60, 60, 60, 235);
	float baseAmbient = 0.45f;
	float baseDiffuse = 0.55f;
	float primaryAmbient = 0.38f;
	float primaryDiffuse = 0.62f;
	float hoverAmbient = 0.75f;
	float hoverDiffuse = 0.25f;
	float perspectiveScale = 0.90f;
	float orthographicScale = 1.12f;
	float orthographicHalfHeight = 1.2f;
	float eyeDistance = 3.6f;
	int minViewportSize = 96;
	int maxViewportSize = 160;
	int viewportPadding = 18;
	int labelTextureSize = 256;
	int labelFontPixelSize = 76;
	float labelFaceOffset = 0.501f;
	float labelFaceScale = 0.68f;
};

bool transformStatesNearlyEqual(const TransformState& a, const TransformState& b)
{
	if ((a.translation - b.translation).lengthSquared() > kExplodedManualTransformTolerance)
		return false;
	if ((a.scale - b.scale).lengthSquared() > kExplodedManualTransformTolerance)
		return false;

	if (a.hasExactRotation || b.hasExactRotation)
	{
		const QQuaternion identityRotation(1.0f, 0.0f, 0.0f, 0.0f);
		const QQuaternion aq = a.hasExactRotation ? a.rotationQuat.normalized() : identityRotation;
		const QQuaternion bq = b.hasExactRotation ? b.rotationQuat.normalized() : identityRotation;
		return std::abs(QQuaternion::dotProduct(aq, bq)) >= (1.0f - 1.0e-4f);
	}

	return (a.rotation - b.rotation).lengthSquared() <= kExplodedManualTransformTolerance;
}

TransformState explodedViewTransformState(const TriangleMesh* mesh)
{
	if (!mesh)
		return {};

	return TransformState(
		mesh->getExplodedViewTranslation(),
		mesh->getExplodedViewRotation(),
		mesh->getExplodedViewScaling(),
		mesh->getExplodedViewRotationQuaternion());
}

QMatrix4x4 explodedViewTransformMatrix(const TriangleMesh* mesh)
{
	return mesh ? mesh->getExplodedViewTransformation() : QMatrix4x4();
}

void applyExplodedViewTransformState(TriangleMesh* mesh, const TransformState& state, bool fast)
{
	if (!mesh)
		return;

	if (fast)
	{
		mesh->setExplodedViewTranslationFast(state.translation);
		if (state.hasExactRotation)
			mesh->setExplodedViewRotationQuaternionFast(state.rotationQuat, state.rotation);
		else
			mesh->setExplodedViewRotationFast(state.rotation);
		mesh->setExplodedViewScalingFast(state.scale);
		return;
	}

	mesh->setExplodedViewTranslation(state.translation);
	if (state.hasExactRotation)
		mesh->setExplodedViewRotationQuaternion(state.rotationQuat, state.rotation);
	else
		mesh->setExplodedViewRotation(state.rotation);
	mesh->setExplodedViewScaling(state.scale);
}

const ViewCubeStyle kViewCubeStyle;


bool convertPixelToRay(const QPoint& pixel, const QRect& viewport, int widgetHeight,
	const QMatrix4x4& view, const QMatrix4x4& projection,
	QVector3D& orig, QVector3D& dir)
{
	if (viewport.width() <= 0 || viewport.height() <= 0)
	{
		orig = QVector3D(0, 0, 0);
		dir = QVector3D(0, 0, 0);
		return false;
	}

	const int yInverted = widgetHeight - pixel.y() - 1;
	const float ndcX = (2.0f * (pixel.x() - viewport.x())) / viewport.width() - 1.0f;
	const float ndcY = (2.0f * (yInverted - viewport.y())) / viewport.height() - 1.0f;

	const QVector4D nearNdc(ndcX, ndcY, -1.0f, 1.0f);
	const QVector4D farNdc(ndcX, ndcY, 1.0f, 1.0f);
	const QMatrix4x4 inv = (projection * view).inverted();

	QVector4D nearWorld = inv * nearNdc;
	QVector4D farWorld = inv * farNdc;
	if (qFuzzyIsNull(nearWorld.w()) || qFuzzyIsNull(farWorld.w()))
	{
		orig = QVector3D(0, 0, 0);
		dir = QVector3D(0, 0, 0);
		return false;
	}

	nearWorld /= nearWorld.w();
	farWorld /= farWorld.w();
	orig = nearWorld.toVector3D();
	const QVector3D rawDir = farWorld.toVector3D() - orig;
	dir = rawDir.isNull() ? QVector3D(0, 0, 0) : rawDir.normalized();
	return !dir.isNull();
}

SceneNode* findSceneNodeByAiChildPath(SceneNode* aiRootNode, const QVector<int>& aiChildPath)
{
	if (!aiRootNode)
		return nullptr;

	SceneNode* current = aiRootNode;
	for (int childIndex : aiChildPath)
	{
		if (childIndex < 0 || childIndex >= current->children.size())
			return nullptr;
		current = current->children.at(childIndex);
	}

	return current;
}

bool intersectRayPlane(const QVector3D& rayOrigin, const QVector3D& rayDir,
	const QVector3D& planePoint, const QVector3D& planeNormal,
	QVector3D& outPoint)
{
	const float denom = QVector3D::dotProduct(rayDir, planeNormal);
	if (std::abs(denom) <= 1.0e-6f)
		return false;

	const float t = QVector3D::dotProduct(planePoint - rayOrigin, planeNormal) / denom;
	if (t < 0.0f)
		return false;

	outPoint = rayOrigin + rayDir * t;
	return true;
}

QVector3D rotatePointAroundAxis(const QVector3D& point, const QVector3D& pivot,
	const QVector3D& axis, float angleDegrees)
{
	QMatrix4x4 rot;
	rot.setToIdentity();
	rot.translate(pivot);
	rot.rotate(angleDegrees, axis);
	rot.translate(-pivot);
	return rot.map(point);
}

QMatrix4x4 buildMeshRotationMatrix(const QVector3D& rotation)
{
	QMatrix4x4 matrix;
	matrix.setToIdentity();
	matrix.rotate(rotation.x(), QVector3D(1.0f, 0.0f, 0.0f));
	matrix.rotate(rotation.y(), QVector3D(0.0f, 1.0f, 0.0f));
	matrix.rotate(rotation.z(), QVector3D(0.0f, 0.0f, 1.0f));
	return matrix;
}

QQuaternion quaternionFromMeshEuler(const QVector3D& rotation)
{
	const QMatrix4x4 matrix = buildMeshRotationMatrix(rotation);
	return QQuaternion::fromRotationMatrix(matrix.toGenericMatrix<3, 3>()).normalized();
}

float normalizeDegrees180(float degrees)
{
	float normalized = std::fmod(degrees + 180.0f, 360.0f);
	if (normalized < 0.0f)
		normalized += 360.0f;
	normalized -= 180.0f;

	if (std::abs(normalized) < 1.0e-4f)
		return 0.0f;
	if (std::abs(normalized - 180.0f) < 1.0e-4f || std::abs(normalized + 180.0f) < 1.0e-4f)
		return 180.0f;

	return normalized;
}

QVector3D normalizeEulerDegrees(const QVector3D& rotation)
{
	return QVector3D(
		normalizeDegrees180(rotation.x()),
		normalizeDegrees180(rotation.y()),
		normalizeDegrees180(rotation.z()));
}

float eulerCanonicalScore(const QVector3D& rotation)
{
	return std::abs(rotation.x()) + std::abs(rotation.y()) + std::abs(rotation.z());
}

QVector3D canonicalizeMeshEuler(const QVector3D& rotation)
{
	const QVector3D primary = normalizeEulerDegrees(rotation);
	const QVector3D alternate = normalizeEulerDegrees(
		QVector3D(rotation.x() + 180.0f, 180.0f - rotation.y(), rotation.z() + 180.0f));

	if (eulerCanonicalScore(alternate) + 1.0e-4f < eulerCanonicalScore(primary))
		return alternate;

	return primary;
}

QVector3D rotationMatrixToMeshEuler(const QMatrix4x4& matrix)
{
	const float m00 = matrix(0, 0);
	const float m01 = matrix(0, 1);
	const float m02 = matrix(0, 2);
	const float m10 = matrix(1, 0);
	const float m11 = matrix(1, 1);
	const float m12 = matrix(1, 2);
	const float m22 = matrix(2, 2);

	const float yRadians = std::asin(std::clamp(m02, -1.0f, 1.0f));
	const float cosY = std::cos(yRadians);

	float xRadians = 0.0f;
	float zRadians = 0.0f;
	if (std::abs(cosY) > 1.0e-6f)
	{
		xRadians = std::atan2(-m12, m22);
		zRadians = std::atan2(-m01, m00);
	}
	else
	{
		xRadians = std::atan2(m10, m11);
		zRadians = 0.0f;
	}

	return canonicalizeMeshEuler(QVector3D(
		qRadiansToDegrees(xRadians),
		qRadiansToDegrees(yRadians),
		qRadiansToDegrees(zRadians)));
}

QVector3D extractMeshRotationFromMatrix(const QMatrix4x4& matrix)
{
	QVector3D col0(matrix(0, 0), matrix(1, 0), matrix(2, 0));
	QVector3D col1(matrix(0, 1), matrix(1, 1), matrix(2, 1));
	QVector3D col2(matrix(0, 2), matrix(1, 2), matrix(2, 2));

	const float sx = col0.length();
	const float sy = col1.length();
	const float sz = col2.length();

	if (sx > 1.0e-8f) col0 /= sx;
	if (sy > 1.0e-8f) col1 /= sy;
	if (sz > 1.0e-8f) col2 /= sz;

	QMatrix4x4 rotationOnly;
	rotationOnly.setToIdentity();
	rotationOnly(0, 0) = col0.x(); rotationOnly(1, 0) = col0.y(); rotationOnly(2, 0) = col0.z();
	rotationOnly(0, 1) = col1.x(); rotationOnly(1, 1) = col1.y(); rotationOnly(2, 1) = col1.z();
	rotationOnly(0, 2) = col2.x(); rotationOnly(1, 2) = col2.y(); rotationOnly(2, 2) = col2.z();
	return rotationMatrixToMeshEuler(rotationOnly);
}

struct ViewCubeLabelFace
{
	QString text;
	QVector3D right;
	QVector3D up;
	QVector3D normal;
};

CornerAxisPosition normalizeCornerAxisPosition(CornerAxisPosition position)
{
	switch (position)
	{
	case CornerAxisPosition::TOP_LEFT:
	case CornerAxisPosition::TOP_RIGHT:
	case CornerAxisPosition::BOTTOM_LEFT:
	case CornerAxisPosition::BOTTOM_RIGHT:
		return position;
	default:
		return CornerAxisPosition::TOP_RIGHT;
	}
}

float computeFloorDepthBias(float workspaceExtent, float floorSize)
{
	const float extentBias = std::max(workspaceExtent, 0.0f) * 1.0e-5f;
	const float floorFallbackBias = std::max(floorSize, 0.0f) * 1.0e-8f;
	return std::clamp(std::max(extentBias, floorFallbackBias), 1.0e-6f, 1.0e-4f);
}

QMatrix4x4 aiToQMatrix(const aiMatrix4x4& m)
{
	QMatrix4x4 out;
	out.setRow(0, QVector4D(m.a1, m.a2, m.a3, m.a4));
	out.setRow(1, QVector4D(m.b1, m.b2, m.b3, m.b4));
	out.setRow(2, QVector4D(m.c1, m.c2, m.c3, m.c4));
	out.setRow(3, QVector4D(m.d1, m.d2, m.d3, m.d4));
	return out;
}

GLWidget::RuntimeNodeTransform decomposeNodeTransform(const aiMatrix4x4& matrix)
{
	aiVector3D scaling;
	aiQuaternion rotation;
	aiVector3D position;
	matrix.Decompose(scaling, rotation, position);

	GLWidget::RuntimeNodeTransform result;
	result.translation = QVector3D(position.x, position.y, position.z);
	result.rotation = QQuaternion(rotation.w, rotation.x, rotation.y, rotation.z).normalized();
	result.scale = QVector3D(scaling.x, scaling.y, scaling.z);
	return result;
}

QMatrix4x4 composeNodeTransform(const GLWidget::RuntimeNodeTransform& tr)
{
	QMatrix4x4 matrix;
	matrix.translate(tr.translation);
	matrix.rotate(tr.rotation);
	matrix.scale(tr.scale);
	return matrix;
}

QUuid resolveRuntimeNodeUuid(const GLWidget::RuntimeAnimationFileState& runtime,
                             int targetNodeIndex,
                             const QString& fallbackNodeName = QString())
{
	if (targetNodeIndex >= 0 && runtime.nodeUuidByIndex.contains(targetNodeIndex))
		return runtime.nodeUuidByIndex.value(targetNodeIndex);

	if (!fallbackNodeName.isEmpty())
		return runtime.nodeUuidByName.value(fallbackNodeName, QUuid());

	return QUuid();
}

QVector3D sampleVec3Keys(const QVector<GltfAnimationVec3Key>& keys, double timeSeconds, const QVector3D& fallback)
{
	if (keys.isEmpty())
		return fallback;
	if (timeSeconds <= keys.front().timeSeconds)
		return keys.front().value;
	if (timeSeconds >= keys.back().timeSeconds)
		return keys.back().value;

	for (int i = 1; i < keys.size(); ++i)
	{
		if (timeSeconds <= keys[i].timeSeconds)
		{
			const double start = keys[i - 1].timeSeconds;
			const double end = keys[i].timeSeconds;
			const float t = end > start ? static_cast<float>((timeSeconds - start) / (end - start)) : 0.0f;
			return keys[i - 1].value * (1.0f - t) + keys[i].value * t;
		}
	}
	return keys.back().value;
}

QVector2D sampleVec2Keys(const QVector<GltfAnimationVec2Key>& keys, double timeSeconds, const QVector2D& fallback)
{
	if (keys.isEmpty())
		return fallback;
	if (timeSeconds <= keys.front().timeSeconds)
		return keys.front().value;
	if (timeSeconds >= keys.back().timeSeconds)
		return keys.back().value;

	for (int i = 1; i < keys.size(); ++i)
	{
		if (timeSeconds <= keys[i].timeSeconds)
		{
			const double start = keys[i - 1].timeSeconds;
			const double end = keys[i].timeSeconds;
			const float t = end > start ? static_cast<float>((timeSeconds - start) / (end - start)) : 0.0f;
			return keys[i - 1].value * (1.0f - t) + keys[i].value * t;
		}
	}
	return keys.back().value;
}

float sampleFloatKeys(const QVector<GltfAnimationFloatKey>& keys, double timeSeconds, float fallback)
{
	if (keys.isEmpty())
		return fallback;
	if (timeSeconds <= keys.front().timeSeconds)
		return keys.front().value;
	if (timeSeconds >= keys.back().timeSeconds)
		return keys.back().value;

	for (int i = 1; i < keys.size(); ++i)
	{
		if (timeSeconds <= keys[i].timeSeconds)
		{
			const double start = keys[i - 1].timeSeconds;
			const double end = keys[i].timeSeconds;
			const float t = end > start ? static_cast<float>((timeSeconds - start) / (end - start)) : 0.0f;
			return keys[i - 1].value * (1.0f - t) + keys[i].value * t;
		}
	}
	return keys.back().value;
}

QVector4D sampleVec4Keys(const QVector<GltfAnimationVec4Key>& keys, double timeSeconds, const QVector4D& fallback)
{
	if (keys.isEmpty())
		return fallback;
	if (timeSeconds <= keys.front().timeSeconds)
		return keys.front().value;
	if (timeSeconds >= keys.back().timeSeconds)
		return keys.back().value;

	for (int i = 1; i < keys.size(); ++i)
	{
		if (timeSeconds <= keys[i].timeSeconds)
		{
			const double start = keys[i - 1].timeSeconds;
			const double end = keys[i].timeSeconds;
			const float t = end > start ? static_cast<float>((timeSeconds - start) / (end - start)) : 0.0f;
			return keys[i - 1].value * (1.0f - t) + keys[i].value * t;
		}
	}
	return keys.back().value;
}

bool sampleBoolKeys(const QVector<GltfAnimationBoolKey>& keys, double timeSeconds, bool fallback)
{
	if (keys.isEmpty())
		return fallback;
	if (timeSeconds <= keys.front().timeSeconds)
		return keys.front().value;
	if (timeSeconds >= keys.back().timeSeconds)
		return keys.back().value;

	bool result = keys.front().value;
	for (int i = 1; i < keys.size(); ++i)
	{
		if (timeSeconds < keys[i].timeSeconds)
			return result;
		result = keys[i].value;
	}
	return result;
}

QVector<float> sampleWeightKeys(const QVector<GltfAnimationWeightsKey>& keys, double timeSeconds, const QVector<float>& fallback)
{
	if (keys.isEmpty())
		return fallback;
	if (timeSeconds <= keys.front().timeSeconds)
		return keys.front().values;
	if (timeSeconds >= keys.back().timeSeconds)
		return keys.back().values;

	for (int i = 1; i < keys.size(); ++i)
	{
		if (timeSeconds <= keys[i].timeSeconds)
		{
			const double start = keys[i - 1].timeSeconds;
			const double end = keys[i].timeSeconds;
			const float t = end > start ? static_cast<float>((timeSeconds - start) / (end - start)) : 0.0f;
			const int count = std::max(keys[i - 1].values.size(), keys[i].values.size());
			QVector<float> result(count, 0.0f);
			for (int weightIndex = 0; weightIndex < count; ++weightIndex)
			{
				const float startValue = weightIndex < keys[i - 1].values.size()
					? keys[i - 1].values[weightIndex]
					: 0.0f;
				const float endValue = weightIndex < keys[i].values.size()
					? keys[i].values[weightIndex]
					: startValue;
				result[weightIndex] = startValue * (1.0f - t) + endValue * t;
			}
			return result;
		}
	}
	return keys.back().values;
}

std::array<ViewCubeLabelFace, 6> buildViewCubeLabelFaces(const QString& top,
                                                         const QString& front,
                                                         const QString& left,
                                                         const QString& bottom,
                                                         const QString& rear,
                                                         const QString& right,
                                                         const QQuaternion& axisRotation)
{
	// Viewer convention is Z-up:
	// Top    = +Z, Bottom = -Z, Front = -Y, Rear = +Y, Left = -X, Right = +X.
	std::array<ViewCubeLabelFace, 6> faces = {{
		{ top,    QVector3D( 1.0f,  0.0f, 0.0f), QVector3D( 0.0f,  1.0f, 0.0f),         QVector3D( 0.0f,  0.0f, 1.0f) },
		{ bottom, QVector3D( 1.0f,  0.0f, 0.0f), QVector3D( 0.0f, -1.0f, 0.0f),         QVector3D( 0.0f,  0.0f,-1.0f) },
		{ front,  QVector3D( 1.0f,  0.0f, 0.0f), QVector3D( 0.0f,  0.0f, 1.0f),         QVector3D( 0.0f, -1.0f, 0.0f) },
		{ rear,   QVector3D(-1.0f,  0.0f, 0.0f), QVector3D( 0.0f,  0.0f, 1.0f),         QVector3D( 0.0f,  1.0f, 0.0f) },
		{ left,   QVector3D( 0.0f, -1.0f, 0.0f), QVector3D( 0.0f,  0.0f, 1.0f),         QVector3D(-1.0f,  0.0f, 0.0f) },
		{ right,  QVector3D( 0.0f,  1.0f, 0.0f), QVector3D( 0.0f,  0.0f, 1.0f),         QVector3D( 1.0f,  0.0f, 0.0f) }
	}};

	for (ViewCubeLabelFace& face : faces)
	{
		face.right = axisRotation.rotatedVector(face.right).normalized();
		face.up = axisRotation.rotatedVector(face.up).normalized();
		face.normal = axisRotation.rotatedVector(face.normal).normalized();
	}

	return faces;
}

void applyTexturePointerValue(GLMaterial& material,
	GltfAnimationTextureTarget textureTarget,
	GltfAnimationPointerProperty property,
	const QVector2D& vec2Value,
	float scalarValue)
{
	auto applyVec2 = [&](auto setOffset, auto setScale) {
		switch (property)
		{
		case GltfAnimationPointerProperty::Offset:
			(material.*setOffset)(vec2Value);
			break;
		case GltfAnimationPointerProperty::Scale:
			(material.*setScale)(vec2Value);
			break;
		default:
			break;
		}
	};

	auto applyRotation = [&](auto setRotation) {
		if (property == GltfAnimationPointerProperty::Rotation)
			(material.*setRotation)(scalarValue);
	};

	switch (textureTarget)
	{
	case GltfAnimationTextureTarget::Albedo:
		applyVec2(&GLMaterial::setAlbedoTexOffset, &GLMaterial::setAlbedoTexScale);
		applyRotation(&GLMaterial::setAlbedoTexRotation);
		break;
	case GltfAnimationTextureTarget::Metallic:
		applyVec2(&GLMaterial::setMetallicTexOffset, &GLMaterial::setMetallicTexScale);
		applyRotation(&GLMaterial::setMetallicTexRotation);
		break;
	case GltfAnimationTextureTarget::Roughness:
		applyVec2(&GLMaterial::setRoughnessTexOffset, &GLMaterial::setRoughnessTexScale);
		applyRotation(&GLMaterial::setRoughnessTexRotation);
		break;
	case GltfAnimationTextureTarget::MetallicRoughness:
		applyVec2(&GLMaterial::setMetallicTexOffset, &GLMaterial::setMetallicTexScale);
		applyRotation(&GLMaterial::setMetallicTexRotation);
		applyVec2(&GLMaterial::setRoughnessTexOffset, &GLMaterial::setRoughnessTexScale);
		applyRotation(&GLMaterial::setRoughnessTexRotation);
		break;
	case GltfAnimationTextureTarget::Normal:
		applyVec2(&GLMaterial::setNormalTexOffset, &GLMaterial::setNormalTexScale);
		applyRotation(&GLMaterial::setNormalTexRotation);
		break;
	case GltfAnimationTextureTarget::Occlusion:
		applyVec2(&GLMaterial::setOcclusionTexOffset, &GLMaterial::setOcclusionTexScale);
		applyRotation(&GLMaterial::setOcclusionTexRotation);
		break;
	case GltfAnimationTextureTarget::Emissive:
		applyVec2(&GLMaterial::setEmissiveTexOffset, &GLMaterial::setEmissiveTexScale);
		applyRotation(&GLMaterial::setEmissiveTexRotation);
		break;
	case GltfAnimationTextureTarget::Transmission:
		applyVec2(&GLMaterial::setTransmissionTexOffset, &GLMaterial::setTransmissionTexScale);
		applyRotation(&GLMaterial::setTransmissionTexRotation);
		break;
	case GltfAnimationTextureTarget::Thickness:
		applyVec2(&GLMaterial::setThicknessTexOffset, &GLMaterial::setThicknessTexScale);
		applyRotation(&GLMaterial::setThicknessTexRotation);
		break;
	case GltfAnimationTextureTarget::IOR:
		applyVec2(&GLMaterial::setIORTexOffset, &GLMaterial::setIORTexScale);
		applyRotation(&GLMaterial::setIORTexRotation);
		break;
	case GltfAnimationTextureTarget::SheenColor:
		applyVec2(&GLMaterial::setSheenColorTexOffset, &GLMaterial::setSheenColorTexScale);
		applyRotation(&GLMaterial::setSheenColorTexRotation);
		break;
	case GltfAnimationTextureTarget::SheenRoughness:
		applyVec2(&GLMaterial::setSheenRoughnessTexOffset, &GLMaterial::setSheenRoughnessTexScale);
		applyRotation(&GLMaterial::setSheenRoughnessTexRotation);
		break;
	case GltfAnimationTextureTarget::Clearcoat:
		applyVec2(&GLMaterial::setClearcoatColorTexOffset, &GLMaterial::setClearcoatColorTexScale);
		applyRotation(&GLMaterial::setClearcoatColorTexRotation);
		break;
	case GltfAnimationTextureTarget::ClearcoatRoughness:
		applyVec2(&GLMaterial::setClearcoatRoughnessTexOffset, &GLMaterial::setClearcoatRoughnessTexScale);
		applyRotation(&GLMaterial::setClearcoatRoughnessTexRotation);
		break;
	case GltfAnimationTextureTarget::ClearcoatNormal:
		applyVec2(&GLMaterial::setClearcoatNormalTexOffset, &GLMaterial::setClearcoatNormalTexScale);
		applyRotation(&GLMaterial::setClearcoatNormalTexRotation);
		break;
	case GltfAnimationTextureTarget::Iridescence:
		applyVec2(&GLMaterial::setIridescenceTexOffset, &GLMaterial::setIridescenceTexScale);
		applyRotation(&GLMaterial::setIridescenceTexRotation);
		break;
	case GltfAnimationTextureTarget::IridescenceThickness:
		applyVec2(&GLMaterial::setIridescenceThicknessTexOffset, &GLMaterial::setIridescenceThicknessTexScale);
		applyRotation(&GLMaterial::setIridescenceThicknessTexRotation);
		break;
	case GltfAnimationTextureTarget::SpecularFactor:
		applyVec2(&GLMaterial::setSpecularFactorTexOffset, &GLMaterial::setSpecularFactorTexScale);
		applyRotation(&GLMaterial::setSpecularFactorTexRotation);
		break;
	case GltfAnimationTextureTarget::SpecularColor:
		applyVec2(&GLMaterial::setSpecularColorTexOffset, &GLMaterial::setSpecularColorTexScale);
		applyRotation(&GLMaterial::setSpecularColorTexRotation);
		break;
	case GltfAnimationTextureTarget::Anisotropy:
		applyVec2(&GLMaterial::setAnisotropyTexOffset, &GLMaterial::setAnisotropyTexScale);
		applyRotation(&GLMaterial::setAnisotropyTexRotation);
		break;
	case GltfAnimationTextureTarget::DiffuseTransmission:
		applyVec2(&GLMaterial::setDiffuseTransmissionTexOffset, &GLMaterial::setDiffuseTransmissionTexScale);
		applyRotation(&GLMaterial::setDiffuseTransmissionTexRotation);
		break;
	case GltfAnimationTextureTarget::DiffuseTransmissionColor:
		applyVec2(&GLMaterial::setDiffuseTransmissionColorTexOffset, &GLMaterial::setDiffuseTransmissionColorTexScale);
		applyRotation(&GLMaterial::setDiffuseTransmissionColorTexRotation);
		break;
	case GltfAnimationTextureTarget::Diffuse:
		applyVec2(&GLMaterial::setDiffuseTexOffset, &GLMaterial::setDiffuseTexScale);
		applyRotation(&GLMaterial::setDiffuseTexRotation);
		break;
	case GltfAnimationTextureTarget::SpecularGlossiness:
		applyVec2(&GLMaterial::setSpecularGlossinessTexOffset, &GLMaterial::setSpecularGlossinessTexScale);
		applyRotation(&GLMaterial::setSpecularGlossinessTexRotation);
		break;
	default:
		break;
	}
}

void applyMaterialFactorPointerValue(GLMaterial& material,
    GltfAnimationPointerProperty property,
    const QVector4D& vec4Value)
{
    if (property != GltfAnimationPointerProperty::BaseColorFactor)
        return;

    const QVector3D color(vec4Value.x(), vec4Value.y(), vec4Value.z());
    material.setAlbedoColor(color);
    material.setDiffuse(color);
    material.setOpacity(vec4Value.w());
}

QQuaternion sampleQuatKeys(const QVector<GltfAnimationQuatKey>& keys, double timeSeconds, const QQuaternion& fallback)
{
	if (keys.isEmpty())
		return fallback;
	if (timeSeconds <= keys.front().timeSeconds)
		return keys.front().value;
	if (timeSeconds >= keys.back().timeSeconds)
		return keys.back().value;

	for (int i = 1; i < keys.size(); ++i)
	{
		if (timeSeconds <= keys[i].timeSeconds)
		{
			const double start = keys[i - 1].timeSeconds;
			const double end = keys[i].timeSeconds;
			const float t = end > start ? static_cast<float>((timeSeconds - start) / (end - start)) : 0.0f;
			return QQuaternion::slerp(keys[i - 1].value, keys[i].value, t).normalized();
		}
	}
	return keys.back().value;
}
}

GLWidget::GLWidget(QWidget* parent, const char* /*name*/) : QOpenGLWidget(parent),
_bgShader(nullptr),
_bgSplitShader(nullptr),
_fgShader(nullptr),
_fgFlatShader(nullptr),
_axisShader(nullptr),
_vertexNormalShader(nullptr),
_faceNormalShader(nullptr),
_shadowMappingShader(nullptr),
_skyBoxShader(nullptr),
_gridShader(nullptr),
_irradianceShader(nullptr),
_prefilterShader(nullptr),
_brdfShader(nullptr),
_lightCubeShader(nullptr),
_viewCubeShader(nullptr),
_viewCubeLabelShader(nullptr),
_clippingPlaneShader(nullptr),
_clippedMeshShader(nullptr),
_selectionShader(nullptr),
_debugShader(nullptr),
_textShader(nullptr),
_textRenderer(nullptr),
_axisTextRenderer(nullptr),
_clippingPlanesEditor(nullptr),
_explodedViewPanel(nullptr),
_explodedViewManager(new ExplodedViewManager()),
_clippingPlaneXY(nullptr),
_clippingPlaneYZ(nullptr),
_clippingPlaneZX(nullptr),
_floorPlane(nullptr),
	_gridPlane(nullptr),
	_skyBox(nullptr),
	_axisCone(nullptr),
	_transformGizmo(nullptr),
	_lightCube(nullptr),
	_assimpModelLoader(nullptr)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);  // Enable mouseMoveEvent for hover highlighting

    _viewer = static_cast<ModelViewer*>(parent);
	_transformGizmo = new TransformGizmo(this);


	// Setup the view toolbar
	_viewToolbar = new ViewToolbar(this);
	_viewToolbar->reposition(width(), height());

	connect(_viewToolbar, &ViewToolbar::zoomViewRequested, this, [this]() {
		setZoomingActive(true);
		});

	connect(_viewToolbar, &ViewToolbar::panViewRequested, this, [this]() {
		setPanningActive(true);
		});

	connect(_viewToolbar, &ViewToolbar::rotateViewRequested, this, [this]() {
		setRotationActive(true);
		});

	 connect(_viewToolbar, &ViewToolbar::cameraModeSelected, this, [this](const QString& type) {
		if (type == "Orbit") setCameraMode(GLCamera::CameraMode::Orbit);
		else if (type == "Fly") setCameraMode(GLCamera::CameraMode::Fly);
		else if (type == "First Person") setCameraMode(GLCamera::CameraMode::FirstPerson);
		});

	connect(_viewToolbar, &ViewToolbar::cameraUpAxisToggled, this, [this](bool zUp) {
		setCameraUpAxisZUp(zUp, false);
	});

	 connect(_viewToolbar, &ViewToolbar::viewSelected, this, [this](const QString& view) {
        if (view == "Top") setViewMode(ViewMode::TOP);
        else if (view == "Front") setViewMode(ViewMode::FRONT);
        else if (view == "Left") setViewMode(ViewMode::LEFT);
        else if (view == "Bottom") setViewMode(ViewMode::BOTTOM);
        else if (view == "Rear") setViewMode(ViewMode::BACK);
        else if (view == "Right") setViewMode(ViewMode::RIGHT);
    });

	 connect(_viewToolbar, &ViewToolbar::axonometricSelected, this, [this](const QString& type) {
		 if (type == "Isometric") setViewMode(ViewMode::ISOMETRIC);
		 else if (type == "Dimetric") setViewMode(ViewMode::DIMETRIC);
		 else if (type == "Trimetric") setViewMode(ViewMode::TRIMETRIC);
		 });

	 connect(_viewToolbar, &ViewToolbar::displayModeSelected, this, [this](const QString& type) {
		 if (type == "Realistic") setDisplayMode(DisplayMode::REALSHADED);
		 else if (type == "Shaded") setDisplayMode(DisplayMode::SHADED);
		 else if (type == "Wireframe") setDisplayMode(DisplayMode::WIREFRAME);
		 else if (type == "WireShaded") setDisplayMode(DisplayMode::WIRESHADED);
		 else if (type == "FlatShaded") setDisplayMode(DisplayMode::FLATSHADED);
		 });
	 connect(this, &GLWidget::displayModeChanged, _viewer, &ModelViewer::onDisplayModeChanged);

	 connect(_viewToolbar, &ViewToolbar::fitToViewRequested, this, &GLWidget::fitAll);
	 
	 connect(_viewToolbar, &ViewToolbar::windowZoomRequested, this, &GLWidget::beginWindowZoom);

	 connect(_viewToolbar, &ViewToolbar::projectionToggled, this, [this](bool ortho) {
		 setProjection(ortho ? ViewProjection::ORTHOGRAPHIC : ViewProjection::PERSPECTIVE);
		 fitAll();
		 update();
		 });

	 connect(_viewToolbar, &ViewToolbar::multiViewToggled, this, [this](bool enabled) {
		 setMultiView(enabled);
		 if (enabled)
			 setViewMode(ViewMode::ISOMETRIC);
		 fitAll();
		 update();
		 });

	 connect(_viewToolbar, &ViewToolbar::sectionViewToggled, this, [this](bool enabled) {
		 showClippingPlaneEditor(enabled);
		 });

	 connect(_viewToolbar, &ViewToolbar::explodedViewToggled, this, [this](bool enabled) {
		 showExplodedViewPanel(enabled);
		 });

	 connect(_viewToolbar, &ViewToolbar::swapVisibleToggled, this, [this](bool enabled) {
		 swapVisible(enabled);
		 });

	 connect(_viewToolbar, &ViewToolbar::axisDisplayToggled, this, [this](bool enabled) {
		 showAxis(enabled);
		 });

	 connect(this, &GLWidget::visibleSwapped, _viewToolbar, &ViewToolbar::setSwapVisibleChecked);

	loadBgColorSettings();

	_quadVAO = 0;

	_viewBoundingSphereDia = 200.0f;
	_viewRange = _viewBoundingSphereDia;
	_FOV = 45.0f;
	_currentViewRange = 1.0f;
	_viewMode = ViewMode::ISOMETRIC;
	_projection = ViewProjection::ORTHOGRAPHIC;
	_previousProjection = GLCamera::ProjectionType::ORTHOGRAPHIC;

	_autoFitViewOnUpdate = true;
	_selectionHighlighting = true;

	_primaryCamera = new GLCamera(width(), height(), _viewRange, _FOV);
	_primaryCamera->setView(GLCamera::ViewProjection::SE_ISOMETRIC_VIEW);

	_orthoViewsCamera = new GLCamera(width(), height(), _viewRange, _FOV);
	_orthoViewsCamera->setView(GLCamera::ViewProjection::SE_ISOMETRIC_VIEW);

	{
		QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
		setCameraUpAxisZUp(settings.value("comboCameraUpAxis", 0).toInt() == 0);
	}

	_currentRotation = QQuaternion::fromRotationMatrix(_primaryCamera->getViewMatrix().toGenericMatrix<3, 3>());
	_currentTranslation = _primaryCamera->getPosition();
	_currentViewRange = _viewRange;

	// Create SelectionManager (dependency injection of camera reference)
	_selectionManager = new SelectionManager(
		this,
		_primaryCamera,
		_meshStore,
		_displayedObjectsIds,
		_hiddenObjectsIds,
		_visibleSwapped,
		this);  // Parent for Qt memory management

	// Connect SelectionManager signals to GLWidget update
	connect(_selectionManager, &SelectionManager::hoverChanged,
			this, [this](int) { update(); });
	connect(_selectionManager, &SelectionManager::selectionChanged,
			this, [this](const QList<int>& selectedIds) {
				if (selectedIds.isEmpty()) {
					// Viewport empty-space click: nothing was hit.
					// Clear the tracked selection, then let setListRow(-1) handle
					// deselecting the tree widget and broadcasting to panels.
					_selectedIDs.clear();
					emit singleSelectionDone(-1);
					update();
					return;
				}
				// Click select APPENDS to selection (multi-select by default)
				// Add the selected mesh(es) if not already there
				for (int id : selectedIds) {
					if (!_selectedIDs.contains(id)) {
						_selectedIDs.append(id);
					}
				}
				// Forward to external panels (e.g. TextureDebugPanel) BEFORE
				// singleSelectionDone so the panel sees the "raw" click state
				// first.  singleSelectionDone triggers setListRow, which may
				// toggle-deselect the mesh and call broadcastSelectionChanged({})
				// — that final broadcast is the authoritative state the panel
				// should end up in.  If we emitted selectionChanged AFTER
				// singleSelectionDone, the toggle-deselect clear would be
				// immediately overwritten by this "raw" [meshId] emission.
				emit selectionChanged(selectedIds);
				// Emit singleSelectionDone only for actual single clicks.
				// This triggers setListRow, which handles toggle-deselect and
				// multi-select bookkeeping, and ultimately calls
				// broadcastSelectionChanged with the authoritative final list.
				if (selectedIds.count() == 1) {
					emit singleSelectionDone(selectedIds.first());
				}
				update();
			});

	// Load hover highlight mode from saved settings
	QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
	int modeIndex = settings.value("comboBoxHoverHighlightMode", static_cast<int>(HoverHighlightMode::Disabled)).toInt();
	if (modeIndex >= 0 && modeIndex <= 2) {
		_selectionManager->setHoverHighlightMode(static_cast<HoverHighlightMode>(modeIndex));
	}

	_slerpStep = 0.0f;
	_slerpFrac = 0.05f;

	_modelNum = 6;

	_defaultLightColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	_ambientLight = { 0.12f, 0.12f, 0.12f, 1.0f };
	_diffuseLight = _defaultLightColor;
	_specularLight = _defaultLightColor;

	_lightPosition = { 25.0f, 25.0f, 50.0f };
	_lightOffsetX = 0.0f;
	_lightOffsetY = 0.0f;
	_lightOffsetZ = 0.0f;

	_displayMode = DisplayMode::SHADED;
	_renderingMode = RenderingMode::ADS_BLINN_PHONG;

	_multiViewActive = false;

	_showAxis = true;

	_windowZoomActive = false;

    _rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
    _rubberBand->setStyle(QStyleFactory::create("Fusion"));


	_viewZooming = false;
	_viewPanning = false;
	_viewRotating = false;

	_modelName = "Model";

	_clipYZEnabled = false;
	_clipZXEnabled = false;
	_clipXYEnabled = false;

	_clipXFlipped = false;
	_clipYFlipped = false;
	_clipZFlipped = false;
	_clippingPlaneXY = nullptr;
	_clippingPlaneYZ = nullptr;
	_clippingPlaneZX = nullptr;

	_cappingEnabled = false;
	_cappingTexture = 0;

	_showVertexNormals = false;
	_showFaceNormals = false;

	_envMapEnabled = false;
	_shadowsEnabled = false;
	_selfShadowsEnabled = false;
	_reflectionsEnabled = false;
	_floorSize = 10.0f;
	_floorSizeFactor = 5.0f;
	_floorPlaneZ = -0.5f;
	_groundMode = GroundMode::None;
	_floorTextureDisplayed = true;
	_floorTexRepeatS = _floorTexRepeatT = 1;
	_floorOffsetPercent = kDefaultFloorOffsetPercent / 100.0f;

	// Floor texture
	if (!_texBuffer.load(PathUtils::getDataDirectory() + "/" + "textures/envmap/floor/Grey-White-Checkered-Squares1800x1800.jpg"))
	{ // Load first image from file
		qWarning("GLWidget::loadFloor - Could not read image file, using single-color instead.");
		QImage dummy(128, 128, QImage::Format_ARGB32);
		dummy.fill(Qt::white);
		_floorTexImage = dummy;
	}
	else
	{
		_floorTexImage = convertToGLFormat(_texBuffer);
	}

	_skyBoxEnabled = false;
	_skyBoxBlurPercent = 0;
	_skyBoxFOV = 45.0f;
	_skyBoxZRotation = 0.0f;
	_skyBoxTextureHDRI = false;
	_gammaCorrection = false;
	_screenGamma = 2.2f;
	_hdrToneMapping = false;
	_envMapExposure = 1.0f;
	_iblExposure = 1.0f;
	_toneMappingMode = HDRToneMapMode::KhronosPbrNeutral;

	_lowResEnabled = false;	
	_showLights = false;
	_useDefaultLights = true;
	_usePunctualLights = true;
	_useIBL = true;

	_shadowWidth = 1024 * 4;
	_shadowHeight = 1024 * 4;

	_environmentMap = 0;
	_shadowMap = 0;
	_shadowMapFBO = 0;
	_irradianceMap = 0;
	_prefilterMap = 0;
	_brdfLUTTexture = 0;
	_charlieLUTTexture = 0;
	_sheenELUTTexture = 0;

	_selectionFBO = 0;
	_selectionRBO = 0;
	_selectionDBO = 0;

	_quadVBO = 0;

	_rubberBandRadius = 1.0f;
	_rubberBandZoomRatio = 0.5f;

	_scaleFrac = 1.0f;

	_clipXCoeff = 0.0f;
	_clipYCoeff = 0.0f;
	_clipZCoeff = 0.0f;

	_clipDX = 0.0f;
	_clipDY = 0.0f;
	_clipDZ = 0.0f;

	_xTran = 0.0f;
	_yTran = 0.0f;
	_zTran = 0.0f;

	_xRot = 0.0f;
	_yRot = 0.0f;
	_zRot = 0.0f;

	_xScale = 1.0f;
	_yScale = 1.0f;
	_zScale = 1.0f;

	_displayedObjectsMemSize = 0;
	_visibleSwapped = false;

	_keyboardNavTimer = new QTimer(this);
	connect(_keyboardNavTimer, &QTimer::timeout, this, &GLWidget::performKeyboardNav);
	_keyboardNavTimer->start(15);

	_animateViewTimer = new QTimer(this);
	_animateViewTimer->setTimerType(Qt::PreciseTimer);
	connect(_animateViewTimer, &QTimer::timeout, this, &GLWidget::animateViewChange);
	connect(this, &GLWidget::rotationsSet, this, &GLWidget::stopAnimations);

	_animateFitAllTimer = new QTimer(this);
	_animateFitAllTimer->setTimerType(Qt::PreciseTimer);
	connect(_animateFitAllTimer, &QTimer::timeout, this, &GLWidget::animateFitAll);
	connect(this, &GLWidget::zoomAndPanSet, this, &GLWidget::stopAnimations);

	_animateWindowZoomTimer = new QTimer(this);
	_animateWindowZoomTimer->setTimerType(Qt::PreciseTimer);
	connect(_animateWindowZoomTimer, &QTimer::timeout, this, &GLWidget::animateWindowZoom);
	connect(this, &GLWidget::zoomAndPanSet, this, &GLWidget::stopAnimations);

	_animateCenterScreenTimer = new QTimer(this);
	_animateCenterScreenTimer->setTimerType(Qt::PreciseTimer);
	connect(_animateCenterScreenTimer, &QTimer::timeout, this, &GLWidget::animateCenterScreen);
	connect(this, &GLWidget::zoomAndPanSet, this, &GLWidget::stopAnimations);

	_inertiaTimer = new QTimer(this);
	_inertiaTimer->setInterval(16); // ~60 FPS
	connect(_inertiaTimer, &QTimer::timeout, this, &GLWidget::onInertiaTimer);

	_animationTimer = new QTimer(this);
	_animationTimer->setInterval(16);
	connect(_animationTimer, &QTimer::timeout, this, &GLWidget::onAnimationTick);

	_editorLayout = new QVBoxLayout(this);
	_upperLayout = new QFormLayout();
	_upperLayout->setFormAlignment(Qt::AlignTop | Qt::AlignLeft);
	_upperLayout->setRowWrapPolicy(QFormLayout::DontWrapRows);
	_upperLayout->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
	_editorLayout->addItem(_upperLayout);

	_editorLayout->addStretch(height());

	_lowerLayout = new QFormLayout();
	_editorLayout->addItem(_lowerLayout);
	_lowerLayout->setFormAlignment(Qt::AlignBottom | Qt::AlignRight);
	_lowerLayout->setRowWrapPolicy(QFormLayout::DontWrapRows);
	_lowerLayout->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);

	int toolbarHeight = _viewToolbar->height();
	_lowerLayout->setContentsMargins(0, 0, 0, toolbarHeight);

	_clippingPlanesEditor = new ClippingPlanesEditor(this);
	_lowerLayout->addWidget(_clippingPlanesEditor);
	connect(this, &GLWidget::backgroundColorChanged,
	        _clippingPlanesEditor, &ClippingPlanesEditor::applyBackgroundTheme);
	_clippingPlanesEditor->hide();

	_explodedViewPanel = new ExplodedViewPanel(this);
	_lowerLayout->addWidget(_explodedViewPanel);
	connect(this, &GLWidget::backgroundColorChanged,
	        _explodedViewPanel, &ExplodedViewPanel::applyBackgroundTheme);
	_explodedViewPanel->hide();
	connect(_explodedViewPanel, &ExplodedViewPanel::explosionParametersChanged,
	        this, &GLWidget::updateExplosion);
	updateOverlayEditorTheme();

	//_displayedObjectsIds.push_back(0);

	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, &GLWidget::customContextMenuRequested, this, &GLWidget::showContextMenu);

	_selectRect = new QRubberBand(QRubberBand::Rectangle, this);

	retranslateUI();

	connect(&LanguageManager::instance(), &LanguageManager::languageChanged, this, [this]() {		
		retranslateUI();  // if needed
		});
}

GLWidget::~GLWidget()
{
	if (_animationTimer)
	{
		_animationTimer->stop();
		disconnect(_animationTimer, nullptr, this, nullptr);
	}
	_animationPlaying = false;
	_activeAnimationFile.clear();
	_activeAnimationClip = -1;
	_runtimeAnimationsByFile.clear();

	_viewToolbar = nullptr;

	if (_textRenderer)
		delete _textRenderer;
	if (_axisTextRenderer)
		delete _axisTextRenderer;

	for (auto a : _meshStore)
	{
		delete a;
	}
	if (_primaryCamera)
		delete _primaryCamera;
	if (_orthoViewsCamera)
		delete _orthoViewsCamera;

	if (_globalScene)
	{
		SceneUtils::deleteScene(_globalScene);
		_globalScene = nullptr;
	}

	if (_assimpModelLoader)
		delete _assimpModelLoader;

	// ===== CRITICAL: Ensure context is current before GL calls =====
	if (context() && context()->isValid())
	{
		makeCurrent();

		cleanUpShaders();

		// Delete textures
		glDeleteTextures(1, &_environmentMap);
		glDeleteTextures(1, &_shadowMap);
		glDeleteTextures(1, &_irradianceMap);
		glDeleteTextures(1, &_prefilterMap);
		glDeleteTextures(1, &_sheenPrefilterMap);
		glDeleteTextures(1, &_studioEnvironmentMap);
		glDeleteTextures(1, &_studioIrradianceMap);
		glDeleteTextures(1, &_studioPrefilterMap);
		glDeleteTextures(1, &_studioSheenPrefilterMap);
		glDeleteTextures(1, &_outdoorEnvironmentMap);
		glDeleteTextures(1, &_outdoorIrradianceMap);
		glDeleteTextures(1, &_outdoorPrefilterMap);
		glDeleteTextures(1, &_outdoorSheenPrefilterMap);
		glDeleteTextures(1, &_officeEnvironmentMap);
		glDeleteTextures(1, &_officeIrradianceMap);
		glDeleteTextures(1, &_officePrefilterMap);
		glDeleteTextures(1, &_officeSheenPrefilterMap);
		glDeleteTextures(1, &_brdfLUTTexture);
		glDeleteTextures(1, &_charlieLUTTexture);
		glDeleteTextures(1, &_sheenELUTTexture);
		glDeleteTextures(1, &_cappingTexture);
		for (GLuint& labelTexture : _viewCubeLabelTextures)
		{
			if (labelTexture != 0)
			{
				glDeleteTextures(1, &labelTexture);
				labelTexture = 0;
			}
		}

		// Delete framebuffers and renderbuffers
		if (_skyboxFBO != 0)
			glDeleteFramebuffers(1, &_skyboxFBO);
		if (_shadowMapFBO != 0)
			glDeleteFramebuffers(1, &_shadowMapFBO);
		if (_selectionFBO != 0)
			glDeleteFramebuffers(1, &_selectionFBO);

		glDeleteRenderbuffers(1, &_skyboxDepthBuffer);
		if (_selectionRBO != 0)
			glDeleteRenderbuffers(1, &_selectionRBO);
		if (_selectionDBO != 0)
			glDeleteRenderbuffers(1, &_selectionDBO);

		// Destroy Qt wrapper types
		_axisVBO.destroy();
		_axisVAO.destroy();
		_bgSplitVBO.destroy();
		_bgSplitVAO.destroy();
		_bgVAO.destroy();

		// Delete fullscreen quad
		if (_fsTriVAO != 0)
		{
			glDeleteBuffers(1, &_fsTriVBO);
			glDeleteVertexArrays(1, &_fsTriVAO);
			_fsTriVAO = 0;
			_fsTriVBO = 0;
		}

		// Delete quad mesh
		if (_quadVAO != 0)
		{
			glDeleteBuffers(1, &_quadVBO);
			glDeleteVertexArrays(1, &_quadVAO);
			_quadVAO = 0;
			_quadVBO = 0;
		}

		// Delete conversion cube
		if (_conversionCubeVAO != 0)
		{
			glDeleteBuffers(1, &_conversionCubeVBO);
			glDeleteVertexArrays(1, &_conversionCubeVAO);
			_conversionCubeVAO = 0;
			_conversionCubeVBO = 0;
		}
		if (_viewCubeLabelVAO != 0)
		{
			glDeleteBuffers(1, &_viewCubeLabelVBO);
			glDeleteVertexArrays(1, &_viewCubeLabelVAO);
			_viewCubeLabelVAO = 0;
			_viewCubeLabelVBO = 0;
		}

		// Delete scene objects
		if (_clippingPlaneXY)
			delete _clippingPlaneXY;
		if (_clippingPlaneYZ)
			delete _clippingPlaneYZ;
		if (_clippingPlaneZX)
			delete _clippingPlaneZX;
		if (_floorPlane)
			delete _floorPlane;
		if (_gridPlane)
			delete _gridPlane;
		if (_axisCone)
			delete _axisCone;
		if (_transformGizmo)
		{
			delete _transformGizmo;
			_transformGizmo = nullptr;
		}
		if (_viewCube)
			delete _viewCube;
		if (_skyBox)
			delete _skyBox;
		if (_lightCube)
			delete _lightCube;

		if (glLights)
		{
			glLights->cleanup();
		}

		cleanupTransmissionBuffer();
		cleanupSSSBuffer();

		doneCurrent();  // Release context

		qInfo() << "GLWidget::~GLWidget - OpenGL resources cleaned up successfully.";
	}
	else
	{
		qWarning() << "GLWidget::~GLWidget - No valid OpenGL context for cleanup.";
	}
}

void GLWidget::retranslateUI()
{
	// Axis labels
	_labelAxisX = tr("X");
	_labelAxisY = tr("Y");
	_labelAxisZ = tr("Z");

	// View labels
	_labelTop = tr("Top");
	_labelFront = tr("Front");
	_labelLeft = tr("Left");
	_labelIsometric = tr("Isometric");
	_labelDimetric = tr("Dimetric");
	_labelTrimetric = tr("Trimetric");

	// Mesh count label
	_labelNumMeshes = tr("No of Meshes: %1");
}

void GLWidget::cleanUpShaders()
{	
}

void GLWidget::moveToRecycleBin(const QUuid& uuid, int originalIndex)
{
	// Find mesh in _meshStore
	TriangleMesh* mesh = nullptr;
	int index = -1;

	for (size_t i = 0; i < _meshStore.size(); ++i)
	{
		if (_meshStore[i]->uuid() == uuid)
		{
			mesh = _meshStore[i];
			index = static_cast<int>(i);
			break;
		}
	}

	if (!mesh)
	{
		qWarning() << "GLWidget::moveToRecycleBin - Mesh not found:" << uuid;
		return;
	}

	// Remove from _meshStore (but don't delete)
	_meshStore.erase(_meshStore.begin() + index);

	// Also remove from displayed/hidden lists
	auto it = std::find(_displayedObjectsIds.begin(),
		_displayedObjectsIds.end(), index);
	if (it != _displayedObjectsIds.end())
		_displayedObjectsIds.erase(it);

	it = std::find(_hiddenObjectsIds.begin(),
		_hiddenObjectsIds.end(), index);
	if (it != _hiddenObjectsIds.end())
		_hiddenObjectsIds.erase(it);

	// Adjust indices in displayed/hidden lists
	// All indices > removed index need to be decremented
	for (int& id : _displayedObjectsIds)
	{
		if (id > index)
			id--;
	}
	for (int& id : _hiddenObjectsIds)
	{
		if (id > index)
			id--;
	}

	// Add to recycle bin
	RecycleBinEntry entry;
	entry.mesh = mesh;
	entry.originalIndex = originalIndex;
	entry.deletedAt = QDateTime::currentDateTime();

	_recycleBin[uuid] = entry;

	qDebug() << "Moved mesh to recycle bin:" << mesh->getName()
		<< "uuid:" << uuid;
}

bool GLWidget::restoreFromRecycleBin(const QUuid& uuid)
{
	if (!_recycleBin.contains(uuid))
	{
		qWarning() << "GLWidget::restoreFromRecycleBin - Mesh not in bin:" << uuid;
		return false;
	}

	RecycleBinEntry entry = _recycleBin.take(uuid);
	TriangleMesh* mesh = entry.mesh;

	// Restore at the ORIGINAL index so _meshStore keeps the SceneGraph DFS
	// order.  The export path pairs the export scene's meshes with the
	// runtime mesh list by order — appending at the end scrambles material
	// and texture assignment on any export after a delete + undo.
	int insertIndex = entry.originalIndex;
	if (insertIndex < 0 || insertIndex > static_cast<int>(_meshStore.size()))
		insertIndex = static_cast<int>(_meshStore.size());
	_meshStore.insert(_meshStore.begin() + insertIndex, mesh);

	// Shift existing display/hidden ids at or above the insertion point,
	// then add the restored mesh to the displayed set (kept sorted —
	// setDisplayList relies on sorted ids for std::set_difference).
	for (int& id : _displayedObjectsIds)
	{
		if (id >= insertIndex)
			id++;
	}
	for (int& id : _hiddenObjectsIds)
	{
		if (id >= insertIndex)
			id++;
	}
	_displayedObjectsIds.push_back(insertIndex);
	std::sort(_displayedObjectsIds.begin(), _displayedObjectsIds.end());

	qDebug() << "Restored mesh from recycle bin:" << mesh->getName()
		<< "uuid:" << uuid << "at index:" << insertIndex;

	return true;
}

void GLWidget::permanentlyDeleteFromBin(const QUuid& uuid)
{
	if (!_recycleBin.contains(uuid))
		return;

	RecycleBinEntry entry = _recycleBin.take(uuid);
	TriangleMesh* mesh = entry.mesh;

	qDebug() << "Permanently deleting mesh:" << mesh->getName()
		<< "uuid:" << uuid;

	// Actually destroy the mesh
	delete mesh;
}

bool GLWidget::isInRecycleBin(const QUuid& uuid) const
{
	return _recycleBin.contains(uuid);
}

QVector<QUuid> GLWidget::getRecycleBinUuids() const
{
	return _recycleBin.keys().toVector();
}

TriangleMesh* GLWidget::getMeshByUuid(const QUuid& uuid) const
{
	// Check in _meshStore first
	for (TriangleMesh* mesh : _meshStore)
	{
		if (mesh->uuid() == uuid)
			return mesh;
	}

	// Check in recycle bin
	if (_recycleBin.contains(uuid))
		return _recycleBin[uuid].mesh;

	return nullptr;
}

TriangleMesh* GLWidget::getMeshByIndex(int index) const
{
	return getMeshByUuid(getUuidByIndex(index));
}

int GLWidget::getIndexByUuid(const QUuid& uuid) const
{
	for (size_t i = 0; i < _meshStore.size(); ++i)
	{
		if (_meshStore[i]->uuid() == uuid)
			return static_cast<int>(i);
	}

	return -1;  // Not found or in recycle bin
}

QUuid GLWidget::getUuidByIndex(int index) const
{
	if (index >= 0 && index < static_cast<int>(_meshStore.size()))
		return _meshStore[index]->uuid();

	return QUuid();  // Invalid
}

void GLWidget::initializeGL()
{
	_openGLInitialized = false;

	if (!QOpenGLContext::currentContext())
	{
		qCritical() << "GLWidget::initializeGL: no current OpenGL context — skipping initialisation";
		return;
	}

	if (!initializeOpenGLFunctions())
	{
		qCritical() << "GLWidget::initializeGL: failed to resolve OpenGL 4.5 Core functions — skipping initialisation";
		return;
	}

	int maxSamples = 0;
	glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);	
	ModelViewerApplication::setSupportedMSAASamples(maxSamples);

	GLfloat maxAniso = 0.0f;
	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
	ModelViewerApplication::setSupportedAnisotropicFilteringLevel(maxAniso);

	// Sheen is part of the guaranteed 0..31 budget, so its LUTs live on fixed
	// units 8/9 instead of using the older overflow/fallback layout.
	
	QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
	// Set Anisotropic Filtering Level
	int anIsoVals[] = { 1, 2, 4, 8, 16, 32 };
	_anisotropicFilteringLevel = anIsoVals[settings.value("anisotropyComboBox", 4).toInt()];

	_userShowAxisOverride = settings.value("showCenterTrihedronCheckBox", true).toBool();
	_userShowCornerAxisOverride = settings.value("showCornerTrihedronCheckBox", true).toBool();
	_cornerAxisPosition = normalizeCornerAxisPosition(static_cast<CornerAxisPosition>(settings.value("comboBoxCornerTrihedronPosition", 1).toInt()));
	_showViewCubeOverride = settings.value("showViewCubeCheckBox", true).toBool() && (_cornerAxisPosition != CornerAxisPosition::BOTTOM_RIGHT);
		
	makeCurrent();

	createShaderPrograms();
	createFullscreenTriangle();

	qRegisterMetaType<AssImpMeshDataBatch>("AssImpMeshDataBatch");
	qRegisterMetaType<const aiScene*>("const aiScene*");
	qRegisterMetaType<std::vector<GPULight>>("std::vector<GPULight>");

	if (!_ktx2Loader.initializeOpenGL())
	{
		qWarning() << "GLWidget::initializeGL - Failed to initialize KTX2 loader";
	}
	_gpuCapabilities = KTX2Loader::detectGPUCapabilities();

	_assimpModelLoader = new AssImpModelLoader();
	_assimpModelLoader->setImageTextureUploader(
		[this](GLMaterial::Texture& texture, const QImage& image) -> unsigned int {
			TextureSamplerSettings samplers{ texture.wrapS, texture.wrapT, texture.minFilter, texture.magFilter };
			if (QThread::currentThread() != this->thread())
			{
				unsigned int result = 0;
				const QImage imageCopy = image;
				QMetaObject::invokeMethod(this, [this, &result, imageCopy, samplers]() {
					result = uploadDecodedTextureImage(imageCopy, samplers);
					}, Qt::BlockingQueuedConnection);
				return result;
			}
			return uploadDecodedTextureImage(image, samplers);
		});
	_assimpModelLoader->setKtx2TextureUploader(
		[this](const QString& path, const std::string& mapType, GLMaterial::Texture& texture) -> unsigned int {
			TextureSamplerSettings samplers{ texture.wrapS, texture.wrapT, texture.minFilter, texture.magFilter };
			if (QThread::currentThread() != this->thread())
			{
				unsigned int result = 0;
				const QString pathCopy = path;
				const std::string mapTypeCopy = mapType;
				QMetaObject::invokeMethod(this, [this, &result, pathCopy, mapTypeCopy, samplers]() {
					result = uploadKtx2TextureImage(pathCopy, mapTypeCopy, samplers);
					}, Qt::BlockingQueuedConnection);
				return result;
			}
			return uploadKtx2TextureImage(path, mapType, samplers);
		});
	_assimpModelLoader->setUVDecisionCallback(
		[this](int totalTriangles, UVMethod currentMethod) -> UVMethod {
			if (QThread::currentThread() != this->thread())
			{
				UVMethod result = currentMethod;
				QMetaObject::invokeMethod(this, [this, totalTriangles, currentMethod, &result]() {
					result = promptLargeModelUVDecision(totalTriangles, currentMethod);
					}, Qt::BlockingQueuedConnection);
				return result;
			}
			return promptLargeModelUVDecision(totalTriangles, currentMethod);
		});
	connect(_assimpModelLoader, &AssImpModelLoader::fileReadProcessed, this, &GLWidget::showFileReadingProgress);
	connect(_assimpModelLoader, &AssImpModelLoader::verticesProcessed, this, &GLWidget::showMeshLoadingProgress);
	connect(_assimpModelLoader, &AssImpModelLoader::nodeMeshProgressUpdated, this, &GLWidget::showNodeMeshLoadingProgress);
	connect(this, &GLWidget::loadingAssImpModelCancelled, _assimpModelLoader, &AssImpModelLoader::cancelLoading);

	glLights = std::make_unique<GLLights>();
	// Connect lights loading
	connect(_assimpModelLoader, &AssImpModelLoader::lightsLoaded,
		this, [this](const GltfLightData& lights) {
			setParsedLights(lights);
		});

	const std::string path = PathUtils::getDataDirectory().toStdString() + "/";
	// Text rendering
	_textShader->bind();
	_textRenderer = new TextRenderer(_textShader.get(), width(), height());
	_textRenderer->Load(path + "fonts/arial.ttf", 20);
	_axisTextRenderer = new TextRenderer(_textShader.get(), width(), height());
	_axisTextRenderer->Load(path + "fonts/arialbd.ttf", 16);
	_textShader->release();

	createCappingPlanes();

	createLights();

	// Environment Mapping
	loadEnvMap();
	// IBL Map
	loadIrradianceMap();

	// Load preset environment maps (Studio, Outdoor, Office)
	const QString dataDir = PathUtils::getDataDirectory();

	// Load preset environment maps (Studio, Outdoor, Office)
	// Each preset loads an HDR file, converts to cubemap, and generates IBL maps (irradiance + prefilter)
	{
		QString studioHDRPath = dataDir + "/textures/envmap/skyboxes/HDRI/studio.hdr";
		_studioEnvironmentMap = loadPresetEnvironmentMap(studioHDRPath);
		if (_studioEnvironmentMap)
			generatePresetIBLMaps(_studioEnvironmentMap, _studioIrradianceMap, _studioPrefilterMap, _studioSheenPrefilterMap);
	}

	{
		QString outdoorHDRPath = dataDir + "/textures/envmap/skyboxes/HDRI/outdoor.hdr";
		_outdoorEnvironmentMap = loadPresetEnvironmentMap(outdoorHDRPath);
		if (_outdoorEnvironmentMap)
			generatePresetIBLMaps(_outdoorEnvironmentMap, _outdoorIrradianceMap, _outdoorPrefilterMap, _outdoorSheenPrefilterMap);
	}

	{
		QString officeHDRPath = dataDir + "/textures/envmap/skyboxes/HDRI/office.hdr";
		_officeEnvironmentMap = loadPresetEnvironmentMap(officeHDRPath);
		if (_officeEnvironmentMap)
			generatePresetIBLMaps(_officeEnvironmentMap, _officeIrradianceMap, _officePrefilterMap, _officeSheenPrefilterMap);
	}

	// Shadow mapping
	loadFloor();
	loadGrid();

	createWhiteTexture();
	initTransmissionBuffer();
	initSSSBuffer();

	float size = 15;
	_axisCone = new Cone(_axisShader.get(), _viewRange / size / 15, _viewRange / size / 5, 8.0f, 1.0f);
	_viewCube = new ViewCubeMesh(_viewCubeShader.get(), 1.0f);
	initializeViewCubeLabels();

	// Set lighting information
	_fgShader->bind();
	syncDefaultLightColorUniforms();
	_fgShader->setUniformValue("lightSource.position", _lightPosition + QVector3D(_lightOffsetX, _lightOffsetY, _lightOffsetZ));
	_fgShader->setUniformValue("lightModel.ambient", QVector3D(0.2f, 0.2f, 0.2f));
	_fgShader->setUniformValue("Line.Width", 0.75f);
	_fgShader->setUniformValue("Line.Color", QVector4D(0.05f, 0.0f, 0.05f, 1.0f));
	_fgShader->setUniformValue("envMap", 1);
	_fgShader->setUniformValue("shadowMap", 2);
	_fgShader->setUniformValue("irradianceMap", 3);
	_fgShader->setUniformValue("prefilterMap", 4);
	_fgShader->setUniformValue("brdfLUT", 5);
	_fgShader->setUniformValue("sheenPrefilterMap", 7);
	_fgShader->setUniformValue("charlieLUT", 8);
	_fgShader->setUniformValue("sheenELUT",  9);
	_fgShader->setUniformValue("sheenPrefilterMipLevels", (int)_sheenPrefilterMipLevels);
	_fgShader->setUniformValue("prefilterMipLevels", (int)_prefilterMipLevels);
	_fgShader->setUniformValue("transmissionSceneTexture", 32);
	_fgShader->setUniformValue("transmissionDepthTexture", 33);
	_fgShader->setUniformValue("sssDiffuseTexture", 37);
	_fgShader->setUniformValue("sssDepthTexture", 38);
	_fgShader->setUniformValue("displayMode", static_cast<int>(_displayMode));
	_fgShader->setUniformValue("renderingMode", static_cast<int>(_renderingMode));	
	_fgShader->setUniformValue("selectionHighlighting", _selectionHighlighting);

	updateEnvMapRotationMatrix();

	_debugShader->bind();
	_debugShader->setUniformValue("depthMap", 0);

	_viewMatrix.setToIdentity();
	glEnable(GL_DEPTH_TEST);

	glClearColor(0.0f, 0.0f, 0.0f, 1.f);

	// --- Debug placeholder textures for TextureDebugPanel ---
	// _debugNeutralTex: 1×1 opaque white  — used for all disabled texture slots.
	// _debugNormalTex : 1×1 (128,128,255) — used for normal-map slots (flat tangent-space normal).
	// _debugBlackTex  : 1×1 opaque black  — reserved; contributions are silenced via scalar uniforms,
	//                   not via the texture value, so this is not used in the current override path.
	{
		auto makeDebugTex = [&](GLuint& texId, const GLubyte rgba[4]) {
			glGenTextures(1, &texId);
			glBindTexture(GL_TEXTURE_2D, texId);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0,
			             GL_RGBA, GL_UNSIGNED_BYTE, rgba);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		};

		const GLubyte white[4]         = { 255, 255, 255, 255 };
		const GLubyte neutralNormal[4] = { 128, 128, 255, 255 };
		const GLubyte black[4]         = {   0,   0,   0, 255 };

		makeDebugTex(_debugNeutralTex, white);
		makeDebugTex(_debugNormalTex,  neutralNormal);
		makeDebugTex(_debugBlackTex,   black);

		glBindTexture(GL_TEXTURE_2D, 0);
	}

	_openGLInitialized = true;

	// Keep GLWidget's _originalParsedLights in sync with SceneGraph whenever
	// a file's light data is added or removed (multi-model scene support).
	if (_viewer && _viewer->sceneGraph())
	{
		connect(_viewer->sceneGraph(), &SceneGraph::lightDataChanged,
		        this, &GLWidget::onSceneLightDataChanged,
		        Qt::UniqueConnection);
		connect(_viewer->sceneGraph(), &SceneGraph::structureChanged,
		        this, &GLWidget::invalidateRuntimeVisibilityHierarchy,
		        Qt::UniqueConnection);
	}
}

void GLWidget::resizeGL(int width, int height)
{
	if (!_openGLInitialized)
		return;

	float w = (float)width;
	float h = (float)height;

	// Invalidate selection FBO buffers so they're recreated with new dimensions
	if (_selectionRBO != 0)
	{
		glDeleteRenderbuffers(1, &_selectionRBO);
		_selectionRBO = 0;
	}
	if (_selectionDBO != 0)
	{
		glDeleteRenderbuffers(1, &_selectionDBO);
		_selectionDBO = 0;
	}
	if (_selectionFBO != 0)
	{
		glDeleteFramebuffers(1, &_selectionFBO);
		_selectionFBO = 0;
	}

	glViewport(0, 0, w, h);
	_viewportMatrix = QMatrix4x4(w / 2, 0.0f, 0.0f, 0.0f,
		0.0f, h / 2, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		w / 2 + 0, h / 2 + 0, 0.0f, 1.0f);

	_projectionMatrix.setToIdentity();
	_primaryCamera->setScreenSize(w, h);
	_primaryCamera->setViewRange(_viewRange);
	// Keep the scene radius in the camera up-to-date so that the perspective
	// far plane always covers the full scene regardless of zoom depth.
	_primaryCamera->setSceneRadius(_boundingSphere.getRadius());
	if (_projection == ViewProjection::ORTHOGRAPHIC)
	{
		_primaryCamera->setProjectionType(GLCamera::ProjectionType::ORTHOGRAPHIC);		
	}
	else
	{
		_primaryCamera->setProjectionType(GLCamera::ProjectionType::PERSPECTIVE);		
	}
	_projectionMatrix = _primaryCamera->getProjectionMatrix();
	_viewMatrix = _primaryCamera->getViewMatrix();

	// Resize the text frame
	_textRenderer->setWidth(width);
	_textRenderer->setHeight(height);
	QMatrix4x4 projection;
	projection.ortho(QRect(0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h)));
	_textShader->bind();
	_textShader->setUniformValue("projection", projection);
	_textShader->release();

	resizeTransmissionBuffer(width, height);
	resizeSSSBuffer(width, height);

	update();
}

void GLWidget::paintGL()
{
	if (!_openGLInitialized)
		return;

	QColor topColor = !_visibleSwapped ? _bgTopColor : QColor::fromRgbF(1.0f - _bgTopColor.redF(),
		1.0f - _bgTopColor.greenF(), 1.0f - _bgTopColor.blueF(),
		_bgTopColor.alphaF());
	QColor botColor = !_visibleSwapped ? _bgBotColor : QColor::fromRgbF(1.0f - _bgBotColor.redF(),
		1.0f - _bgBotColor.greenF(), 1.0f - _bgBotColor.blueF(),
		_bgBotColor.alphaF());
	try
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		gradientBackground(topColor.redF(), topColor.greenF(), topColor.blueF(), topColor.alphaF(),
			botColor.redF(), botColor.greenF(), botColor.blueF(), botColor.alphaF(), _gradientStyle);

		_fgShader->bind();
		glLights->bind(_fgShader->programId());
		_fgShader->setUniformValue("lightCount", glLights->getLightCount());

		_modelMatrix.setToIdentity();
		if (_multiViewActive)
		{
			renderMultiView(topColor, botColor);
		}
		else
		{
			renderSingleView(topColor, botColor);
		}

		drawViewCube();

		// Text rendering
		if (_meshStore.size() != 0)
		{
			const std::vector<int>& objectIds = _visibleSwapped ? _hiddenObjectsIds : _displayedObjectsIds;
			if (objectIds.size() > 0)
				_textRenderer->RenderText(_labelNumMeshes.arg(objectIds.size()).toStdString(), 4, 4, 1, QVector3D(1.0f, 1.0f, 0.0f));
		}
	}
	catch (const std::exception& ex)
	{
		std::cout << "Exception raised in GLWidget::paintGL\n" << ex.what() << std::endl;
	}

	// For testing rendered shadow map
	/*_debugShader->bind();
	_debugShader->setUniformValue("near_plane", 1.0f);
	_debugShader->setUniformValue("far_plane", _viewRange);
	_debugShader->setUniformValue("screenSize", QVector2D(width(), height()));
	_debugShader->setUniformValue("transmissionColorTexture", 32);
	_debugShader->setUniformValue("transmissionDepthTexture", 33);	
	renderQuad();*/

	//_brdfShader->bind();
	//renderQuad();
}

void GLWidget::updateView()
{
	update();
}

void GLWidget::setSkyBoxTextureFolder(QString folder)
{
	QApplication::setOverrideCursor(Qt::WaitCursor);

	// Store the folder path for later regeneration in detached contexts
	_currentSkyboxFolder = folder;

	// File pattern map: match flexible identifiers to cube map indices
	QMap<QString, int> faceMap = {
		{"right", 0}, {"posx", 0}, {"px", 0}, {"rt", 0},
		{"left", 1},  {"negx", 1}, {"nx", 1}, {"lt", 1},
		{"top", 2},   {"posy", 2}, {"py", 2}, {"up", 2},
		{"bottom", 3},{"negy", 3}, {"ny", 3}, {"dn", 3}, {"down", 3},
		{"front", 4}, {"posz", 4}, {"pz", 4}, {"ft", 4},
		{"back", 5},  {"negz", 5}, {"nz", 5}, {"bk", 5}
	};

	QStringList faceNames = { "right", "left", "top", "bottom", "front", "back" };
	QStringList supportedFormats = { "jpeg", "jpg", "png", "bmp", "psd", "tga", "gif", "hdr", "pic", "pnm" };

	QStringList files = QDir(folder).entryList(QDir::Files | QDir::Readable, QDir::Name);

	if (files.isEmpty())
	{
		QMessageBox::critical(this, tr("Error"), tr("No files found in selected folder."));
		QApplication::restoreOverrideCursor();
		return;
	}

	makeCurrent();
	glBindTexture(GL_TEXTURE_CUBE_MAP, _environmentMap);

	// Temp holders
	QString skyboxImages[6];
	bool loadedFaces[6] = { false };

	// Try to match each file name with a face using the map
	for (const QString& file : files)
	{
		QString name = QFileInfo(file).baseName().toLower();
		for (auto it = faceMap.constBegin(); it != faceMap.constEnd(); ++it)
		{
			if (name.contains(it.key()) && !loadedFaces[it.value()])
			{
				skyboxImages[it.value()] = folder + "/" + file;
				loadedFaces[it.value()] = true;
				break;
			}
		}
	}

	// Check if all faces were loaded
	bool allFacesLoaded = std::all_of(std::begin(loadedFaces), std::end(loadedFaces),
		[](bool b) { return b; });

	if (!allFacesLoaded)
	{
		// Fallback: try single HDR cubemap image
		QStringList hdrFiles = QDir(folder).entryList(QStringList() << "*.hdr", QDir::Files);
		if (!hdrFiles.isEmpty())
		{
			QString fallbackHDR = folder + "/" + hdrFiles.first();
			if (loadCubemapFromSingleHDR(fallbackHDR))
			{				
				loadIrradianceMap();
				update();
				QApplication::restoreOverrideCursor();
				return;
			}
			else
			{
				QMessageBox::critical(this, tr("Error"),
					tr("Failed to load fallback HDR cubemap from:\n") + fallbackHDR);
			}
		}
		else
		{
			QMessageBox::critical(this, tr("Error"),
				tr("No valid 6-face skybox images or fallback HDR file found in folder."));
		}

		QApplication::restoreOverrideCursor();
		return;
	}

	// Ensure all 6 faces are found
	for (int i = 0; i < 6; ++i)
	{
		if (!loadedFaces[i])
		{
			QString missingFace = faceNames[i];
			QString title = tr("Error");
			QString message = tr("Missing skybox face: %1\nExpected files should include identifiers like posx/negx or right/left, etc.")
				.arg(missingFace);
			QMessageBox::critical(this, title, message);

			QApplication::restoreOverrideCursor();
			return;
		}
	}

	// Load and upload each face
	for (int i = 0; i < 6; ++i)
	{
		int width, height, nrComponents;
		void* data = nullptr;
		std::string fileName = skyboxImages[i].toStdString();

		stbi_set_flip_vertically_on_load(false);
		if (_skyBoxTextureHDRI)
		{
			data = static_cast<float*>(stbi_loadf(fileName.c_str(), &width, &height, &nrComponents, 0));
			if (!data) goto failure;
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB32F,
				width, height, 0, GL_RGB, GL_FLOAT, data);
		}
		else
		{
			data = static_cast<unsigned char*>(stbi_load(fileName.c_str(), &width, &height, &nrComponents, 0));
			if (!data) goto failure;
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
				width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		}
				
		stbi_image_free(data);
		continue;

	failure:
		{
			QMessageBox::critical(this, tr("Error"), tr("Failed to load skybox face:\n") + QString::fromStdString(fileName));
			QApplication::restoreOverrideCursor();
			return;
		}
	}

	// Generate mipmaps ONCE after all 6 faces are loaded
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	// Setup sampler parameters
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	loadIrradianceMap();
	update();
	QApplication::restoreOverrideCursor();
}

bool GLWidget::loadCubemapFromSingleHDR(const QString& filePath)
{
	int imgWidth, imgHeight, channels;
	stbi_set_flip_vertically_on_load(false);
	float* data = stbi_loadf(filePath.toStdString().c_str(), &imgWidth, &imgHeight, &channels, 0);
	if (!data)
	{
		qWarning() << "Failed to load HDR file:" << filePath;
		return false;
	}

	// Check for equirectangular first (2:1 aspect ratio)
	if (imgWidth == 2 * imgHeight)
	{		
		stbi_image_free(data); // Free, we'll reload in conversion function
		return convertEquirectangularToCubemap(filePath);
	}

	glBindTexture(GL_TEXTURE_CUBE_MAP, _environmentMap);

	int faceSize = 0;
	QPoint faceOffsets[6];
	bool validLayout = false;

	// --- Detect 6x1 strip ---
	if (imgWidth % 6 == 0 && imgHeight == imgWidth / 6)
	{
		faceSize = imgHeight;
		for (int i = 0; i < 6; ++i)
			faceOffsets[i] = QPoint(i * faceSize, 0);
		validLayout = true;
	}

	// --- Detect 1x6 vertical strip ---
	else if (imgHeight % 6 == 0 && imgWidth == imgHeight / 6)
	{
		faceSize = imgWidth;
		for (int i = 0; i < 6; ++i)
			faceOffsets[i] = QPoint(0, i * faceSize);
		validLayout = true;
	}

	// --- Detect 3x2 grid ---
	else if (imgWidth % 3 == 0 && imgHeight % 2 == 0 && imgWidth / 3 == imgHeight / 2)
	{
		faceSize = imgWidth / 3;
		QPoint gridOffsets[6] = {
			{0, 0}, // +X
			{1, 0}, // -X
			{2, 0}, // +Y
			{0, 1}, // -Y
			{1, 1}, // +Z
			{2, 1}  // -Z
		};
		for (int i = 0; i < 6; ++i)
			faceOffsets[i] = QPoint(gridOffsets[i].x() * faceSize, gridOffsets[i].y() * faceSize);
		validLayout = true;
	}

	// --- Detect 4x3 or 3x4 cross layout ---
	else if ((imgWidth % 4 == 0 && imgHeight % 3 == 0 && imgWidth / 4 == imgHeight / 3) ||
		(imgWidth % 3 == 0 && imgHeight % 4 == 0 && imgWidth / 3 == imgHeight / 4))
	{
		// Handle 4x3 cross layout
		if (imgWidth / 4 == imgHeight / 3)
		{
			faceSize = imgWidth / 4;
			QPoint crossOffsets[6] = {
				{2, 1}, // +X
				{0, 1}, // -X
				{1, 0}, // +Y
				{1, 2}, // -Y
				{1, 1}, // +Z
				{3, 1}  // -Z
			};
			for (int i = 0; i < 6; ++i)
				faceOffsets[i] = QPoint(crossOffsets[i].x() * faceSize, crossOffsets[i].y() * faceSize);
			validLayout = true;
		}
		// Handle 3x4 cross layout (rotated cross)
		else if (imgWidth / 3 == imgHeight / 4)
		{
			faceSize = imgWidth / 3;
			QPoint crossOffsets[6] = {
				{2, 1}, // +X
				{0, 1}, // -X
				{1, 0}, // +Y
				{1, 2}, // -Y
				{1, 1}, // +Z
				{1, 3}  // -Z
			};
			for (int i = 0; i < 6; ++i)
				faceOffsets[i] = QPoint(crossOffsets[i].x() * faceSize, crossOffsets[i].y() * faceSize);
			validLayout = true;
		}
	}

	// --- Fallback: 1 face only (not a cubemap) ---
	if (!validLayout)
	{
		qWarning() << "Unsupported cubemap layout. Cannot determine layout from image dimensions.";
		stbi_image_free(data);
		return false;
	}

	// --- Upload faces to OpenGL ---
	for (int i = 0; i < 6; ++i)
	{
		const QPoint& offset = faceOffsets[i];
		float* facePixels = new float[faceSize * faceSize * channels];

		for (int y = 0; y < faceSize; ++y)
		{
			const float* src = data + ((offset.y() + y) * imgWidth + offset.x()) * channels;
			float* dst = facePixels + y * faceSize * channels;
			memcpy(dst, src, sizeof(float) * faceSize * channels);
		}

		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB32F,
			faceSize, faceSize, 0, GL_RGB, GL_FLOAT, facePixels);
		delete[] facePixels;
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	stbi_image_free(data);
	return true;
}

bool GLWidget::convertEquirectangularToCubemap(const QString& filePath)
{
	// 1. Load equirectangular HDR as 2D texture
	int imgWidth, imgHeight, channels;
	stbi_set_flip_vertically_on_load(true);
	float* data = stbi_loadf(filePath.toStdString().c_str(), &imgWidth, &imgHeight, &channels, 0);

	if (!data || imgWidth != 2 * imgHeight)
	{
		qWarning() << "Invalid equirectangular HDR file:" << filePath;
		if (data) stbi_image_free(data);
		return false;
	}

	// Validate and sanitize pixel data
	size_t totalPixels = imgWidth * imgHeight * channels;
	int invalidCount = 0;

	for (size_t i = 0; i < totalPixels; i++)
	{
		if (!std::isfinite(data[i]) || data[i] < 0.0f)
		{
			// Replace invalid values with nearby valid pixel or small positive value
			data[i] = (i > 0 && std::isfinite(data[i - 1])) ? data[i - 1] : 0.001f;
			invalidCount++;
		}
		// Clamp extremely bright values that cause issues
		else if (data[i] > 65504.0f)
		{ // Half-float max
			data[i] = 65504.0f;
			invalidCount++;
		}
	}

	if (invalidCount > 0)
	{
		qDebug() << "Fixed" << invalidCount << "invalid pixels in" << filePath;
	}

	// Create temporary 2D texture for equirectangular source
	GLuint equirectTexture;
	glGenTextures(1, &equirectTexture);
	glBindTexture(GL_TEXTURE_2D, equirectTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, imgWidth, imgHeight, 0, GL_RGB, GL_FLOAT, data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	stbi_image_free(data);

	// 2. Create cubemap texture
	// Derive face size from source: equirectangular width covers ~4 faces, so W/4 is a
	// good face size.  Round down to the nearest power-of-two for clean mip chains and
	// clamp to 2048 to keep GPU memory reasonable.
	int cubeSize = 1 << static_cast<int>(std::log2(std::min(imgWidth / 4, 2048)));
	for (int mip = 0; mip < static_cast<int>(std::log2(cubeSize)) + 1; ++mip)
	{
		int mipSize = cubeSize >> mip;
	for (int i = 0; i < 6; ++i)
	{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, mip, GL_RGB32F,
				mipSize, mipSize, 0, GL_RGB, GL_FLOAT, nullptr);
	}
	}

	// 3. Setup framebuffer
	GLuint framebuffer, depthBuffer;
	glGenFramebuffers(1, &framebuffer);
	glGenRenderbuffers(1, &depthBuffer);

	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, cubeSize, cubeSize);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);
	
	// 4. Render each face
	_equirectToCubeShader->bind();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, equirectTexture);
	_equirectToCubeShader->setUniformValue("equirectangularMap", 0);

	glViewport(0, 0, cubeSize, cubeSize);

	// View matrices for each cubemap face
	QMatrix4x4 captureViews[] = {
		// Use QMatrix4x4::lookAt for each face direction
		[]() { QMatrix4x4 m; m.lookAt(QVector3D(0.0f, 0.0f, 0.0f), QVector3D(1,  0,  0), QVector3D(0, -1,  0)); return m; }(), // +X
		[]() { QMatrix4x4 m; m.lookAt(QVector3D(0.0f, 0.0f, 0.0f), QVector3D(-1,  0,  0), QVector3D(0, -1,  0)); return m; }(), // -X
		[]() { QMatrix4x4 m; m.lookAt(QVector3D(0.0f, 0.0f, 0.0f), QVector3D(0,  1,  0), QVector3D(0,  0,  1)); return m; }(), // +Y
		[]() { QMatrix4x4 m; m.lookAt(QVector3D(0.0f, 0.0f, 0.0f), QVector3D(0, -1,  0), QVector3D(0,  0, -1)); return m; }(), // -Y
		[]() { QMatrix4x4 m; m.lookAt(QVector3D(0.0f, 0.0f, 0.0f), QVector3D(0,  0,  1), QVector3D(0, -1,  0)); return m; }(), // +Z
		[]() { QMatrix4x4 m; m.lookAt(QVector3D(0.0f, 0.0f, 0.0f), QVector3D(0,  0, -1), QVector3D(0, -1,  0)); return m; }()  // -Z
	};

	QMatrix4x4 captureProjection;
	captureProjection.perspective(90.0f, 1.0f, 0.1f, 10.0f);
	_equirectToCubeShader->setUniformValue("projection", captureProjection);

	for (int i = 0; i < 6; ++i)
	{
		_equirectToCubeShader->setUniformValue("view", captureViews[i]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, _environmentMap, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Render the conversion cube
		renderConversionCube();		
	}

	// 5. Setup cubemap parameters
	glBindTexture(GL_TEXTURE_CUBE_MAP, _environmentMap);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	// 6. Cleanup
	glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
	glDeleteFramebuffers(1, &framebuffer);
	glDeleteRenderbuffers(1, &depthBuffer);
	glDeleteTextures(1, &equirectTexture);

	return true;
}


bool GLWidget::convertEquirectangularToCubemapQuad(const QString& filePath)
{
	// 1. Load equirectangular HDR as 2D texture
	int imgWidth, imgHeight, channels;
	stbi_set_flip_vertically_on_load(true);
	float* data = stbi_loadf(filePath.toStdString().c_str(), &imgWidth, &imgHeight, &channels, 0);

	if (!data || imgWidth != 2 * imgHeight)
	{
		qWarning() << "Invalid equirectangular HDR file:" << filePath;
		if (data) stbi_image_free(data);
		return false;
	}

	// Create temporary 2D texture for equirectangular source
	GLuint equirectTexture;
	glGenTextures(1, &equirectTexture);
	glBindTexture(GL_TEXTURE_2D, equirectTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, imgWidth, imgHeight, 0, GL_RGB, GL_FLOAT, data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	stbi_image_free(data);

	/// 2. Create cubemap texture - allocate mip 0 for all faces first
	// Derive face size from source: equirectangular width covers ~4 faces, so W/4 is a
	// good face size.  Round down to the nearest power-of-two for clean mip chains and
	// clamp to 2048 to keep GPU memory reasonable.
	int cubeSize = 1 << static_cast<int>(std::log2(std::min(imgWidth / 4, 2048)));
	glBindTexture(GL_TEXTURE_CUBE_MAP, _environmentMap);

	for (int i = 0; i < 6; ++i)
	{
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB32F,
			cubeSize, cubeSize, 0, GL_RGB, GL_FLOAT, nullptr);
	}

	// 3. Create simple full-screen quad
	float quadVertices[] = {
		-1.0f, -1.0f,
		 1.0f, -1.0f,
		 1.0f,  1.0f,
		-1.0f,  1.0f
	};

	unsigned int quadIndices[] = {
		0, 1, 2,
		0, 2, 3
	};

	GLuint quadVAO, quadVBO, quadEBO;
	glGenVertexArrays(1, &quadVAO);
	glGenBuffers(1, &quadVBO);
	glGenBuffers(1, &quadEBO);

	glBindVertexArray(quadVAO);
	glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadIndices), quadIndices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	// 4. Setup framebuffer
	GLuint framebuffer, depthBuffer;
	glGenFramebuffers(1, &framebuffer);
	glGenRenderbuffers(1, &depthBuffer);

	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, cubeSize, cubeSize);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);

	_equirectToCubeQuadShader->bind();
	glViewport(0, 0, cubeSize, cubeSize);

	// Bind equirectangular texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, equirectTexture);
	_equirectToCubeQuadShader->setUniformValue("equirectangularMap", 0);

	// 6. Render each cubemap face
	for (int i = 0; i < 6; ++i)
	{
		_equirectToCubeQuadShader->setUniformValue("faceIndex", i);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, _environmentMap, 0);

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE)
		{
			qWarning() << "Framebuffer incomplete for face" << i;
			continue;
		}

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glBindVertexArray(quadVAO);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	}

	// 7. Setup cubemap parameters and generate mipmaps
	glBindTexture(GL_TEXTURE_CUBE_MAP, _environmentMap);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	// 8. Cleanup
	glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
	glDeleteFramebuffers(1, &framebuffer);
	glDeleteRenderbuffers(1, &depthBuffer);
	glDeleteTextures(1, &equirectTexture);
	glDeleteBuffers(1, &quadVBO);
	glDeleteBuffers(1, &quadEBO);
	glDeleteVertexArrays(1, &quadVAO);

	return true;
}
void GLWidget::renderConversionCube()
{
	if (_conversionCubeVAO == 0)
	{
		float vertices[] = {
			// positions          
			-1.0f,  1.0f, -1.0f,
			-1.0f, -1.0f, -1.0f,
			 1.0f, -1.0f, -1.0f,
			 1.0f, -1.0f, -1.0f,
			 1.0f,  1.0f, -1.0f,
			-1.0f,  1.0f, -1.0f,

			-1.0f, -1.0f,  1.0f,
			-1.0f, -1.0f, -1.0f,
			-1.0f,  1.0f, -1.0f,
			-1.0f,  1.0f, -1.0f,
			-1.0f,  1.0f,  1.0f,
			-1.0f, -1.0f,  1.0f,

			 1.0f, -1.0f, -1.0f,
			 1.0f, -1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f, -1.0f,
			 1.0f, -1.0f, -1.0f,

			-1.0f, -1.0f,  1.0f,
			-1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f, -1.0f,  1.0f,
			-1.0f, -1.0f,  1.0f,

			-1.0f,  1.0f, -1.0f,
			 1.0f,  1.0f, -1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			-1.0f,  1.0f,  1.0f,
			-1.0f,  1.0f, -1.0f,

			-1.0f, -1.0f, -1.0f,
			-1.0f, -1.0f,  1.0f,
			 1.0f, -1.0f, -1.0f,
			 1.0f, -1.0f, -1.0f,
			-1.0f, -1.0f,  1.0f,
			 1.0f, -1.0f,  1.0f
		};

		glGenVertexArrays(1, &_conversionCubeVAO);
		glGenBuffers(1, &_conversionCubeVBO);

		glBindBuffer(GL_ARRAY_BUFFER, _conversionCubeVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

		glBindVertexArray(_conversionCubeVAO);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}

	glBindVertexArray(_conversionCubeVAO);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

QVector3D GLWidget::getLightPosition() const
{
	return _lightPosition;
}

void GLWidget::syncDefaultLightColorUniforms()
{
	// Keep a small internal ambient term for ADS while exposing a single editable light color.
	static constexpr float kDefaultLightAmbientFactor = 0.12f;

	_ambientLight = QVector4D(
		_defaultLightColor.x() * kDefaultLightAmbientFactor,
		_defaultLightColor.y() * kDefaultLightAmbientFactor,
		_defaultLightColor.z() * kDefaultLightAmbientFactor,
		_defaultLightColor.w());
	_diffuseLight = _defaultLightColor;
	_specularLight = _defaultLightColor;

	_fgShader->setUniformValue("lightSource.ambient", _ambientLight.toVector3D());
	_fgShader->setUniformValue("lightSource.diffuse", _diffuseLight.toVector3D());
	_fgShader->setUniformValue("lightSource.specular", _specularLight.toVector3D());
}

void GLWidget::setLightOffset(const QVector3D& offset)
{
	_lightOffsetX = offset.x();
	_lightOffsetY = offset.y();
	_lightOffsetZ = offset.z();
	_shadowMapNeedsInitialization = true;
}

QVector4D GLWidget::getDefaultLightColor() const
{
	return _defaultLightColor;
}

void GLWidget::setDefaultLightColor(const QVector4D& defaultLightColor)
{
	_defaultLightColor = defaultLightColor;
	_fgShader->bind();
	syncDefaultLightColorUniforms();
	_fgShader->release();
}

QQuaternion GLWidget::cameraUpAxisConventionRotation() const
{
	return _cameraUpAxisZUp
		? QQuaternion()
		: QQuaternion::fromAxisAndAngle(QVector3D(1.0f, 0.0f, 0.0f), -90.0f);
}

namespace
{
void orthonormalizeViewBasis(const QVector3D& viewDirection,
	const QVector3D& upHint,
	QVector3D& viewDir,
	QVector3D& upDir,
	QVector3D& rightDir)
{
	viewDir = viewDirection.normalized();
	rightDir = QVector3D::crossProduct(viewDir, upHint).normalized();
	if (rightDir.lengthSquared() <= 1.0e-8f)
		rightDir = QVector3D::crossProduct(viewDir, QVector3D(1.0f, 0.0f, 0.0f)).normalized();
	if (rightDir.lengthSquared() <= 1.0e-8f)
		rightDir = QVector3D::crossProduct(viewDir, QVector3D(0.0f, 1.0f, 0.0f)).normalized();
	upDir = QVector3D::crossProduct(rightDir, viewDir).normalized();
}

void canonicalStandardViewBasis(ViewMode mode, QVector3D& viewDir, QVector3D& upDir, QVector3D& rightDir)
{
	QVector3D viewDirection;
	QVector3D upHint;
	switch (mode)
	{
	case ViewMode::TOP:
		viewDirection = QVector3D(0.0f, 0.0f, -1.0f);
		upHint = QVector3D(0.0f, 1.0f, 0.0f);
		break;
	case ViewMode::BOTTOM:
		viewDirection = QVector3D(0.0f, 0.0f, 1.0f);
		upHint = QVector3D(0.0f, -1.0f, 0.0f);
		break;
	case ViewMode::FRONT:
		viewDirection = QVector3D(0.0f, 1.0f, 0.0f);
		upHint = QVector3D(0.0f, 0.0f, 1.0f);
		break;
	case ViewMode::BACK:
		viewDirection = QVector3D(0.0f, -1.0f, 0.0f);
		upHint = QVector3D(0.0f, 0.0f, 1.0f);
		break;
	case ViewMode::LEFT:
		viewDirection = QVector3D(1.0f, 0.0f, 0.0f);
		upHint = QVector3D(0.0f, 0.0f, 1.0f);
		break;
	case ViewMode::RIGHT:
		viewDirection = QVector3D(-1.0f, 0.0f, 0.0f);
		upHint = QVector3D(0.0f, 0.0f, 1.0f);
		break;
	case ViewMode::ISOMETRIC:
		viewDirection = QVector3D(-1.0f, 1.0f, -1.0f);
		upHint = QVector3D(-1.0f, 1.0f, 0.0f);
		break;
	case ViewMode::DIMETRIC:
		viewDirection = QVector3D(-2.0f, 2.0f, -1.0f);
		upHint = QVector3D(-1.0f, 1.0f, 0.0f);
		break;
	case ViewMode::TRIMETRIC:
		viewDirection = QVector3D(-0.486f, 0.732f, -0.477f);
		upHint = QVector3D(-0.363f, 0.568f, 1.243f);
		break;
	default:
		viewDirection = QVector3D(0.0f, 0.0f, -1.0f);
		upHint = QVector3D(0.0f, 1.0f, 0.0f);
		break;
	}

	orthonormalizeViewBasis(viewDirection, upHint, viewDir, upDir, rightDir);
}
}

QVector3D GLWidget::transformVectorForCameraUpAxis(const QVector3D& vector) const
{
	return cameraUpAxisConventionRotation().rotatedVector(vector);
}

void GLWidget::standardViewBasis(ViewMode mode, QVector3D& viewDir, QVector3D& upDir, QVector3D& rightDir) const
{
	canonicalStandardViewBasis(mode, viewDir, upDir, rightDir);

	const QQuaternion axisRotation = cameraUpAxisConventionRotation();
	viewDir = axisRotation.rotatedVector(viewDir).normalized();
	upDir = axisRotation.rotatedVector(upDir).normalized();
	rightDir = axisRotation.rotatedVector(rightDir).normalized();
}

QQuaternion GLWidget::standardViewRotation(ViewMode mode) const
{
	QVector3D viewDir;
	QVector3D upDir;
	QVector3D rightDir;
	standardViewBasis(mode, viewDir, upDir, rightDir);

	QMatrix4x4 targetMatrix;
	targetMatrix.setToIdentity();
	targetMatrix.setRow(0, QVector4D(rightDir, 0.0f));
	targetMatrix.setRow(1, QVector4D(upDir, 0.0f));
	targetMatrix.setRow(2, QVector4D(-viewDir, 0.0f));
	return QQuaternion::fromRotationMatrix(targetMatrix.toGenericMatrix<3, 3>()).normalized();
}

void GLWidget::syncCameraWorldUp()
{
	const QVector3D worldUp = _cameraUpAxisZUp
		? QVector3D(0.0f, 0.0f, 1.0f)
		: QVector3D(0.0f, 1.0f, 0.0f);

	if (_primaryCamera)
		_primaryCamera->setWorldUpVector(worldUp);
	if (_orthoViewsCamera)
		_orthoViewsCamera->setWorldUpVector(worldUp);
}

void GLWidget::rotateCurrentCameraAroundWorldX(float degrees)
{
	if (!_primaryCamera || std::abs(degrees) <= 0.0001f)
		return;

	const QQuaternion rotation = QQuaternion::fromAxisAndAngle(QVector3D(1.0f, 0.0f, 0.0f), degrees);
	const QVector3D rotatedViewDir = rotation.rotatedVector(_primaryCamera->getViewDir()).normalized();

	// Re-derive right and up from the rotated view direction and the current world-up
	// so the orbit camera carries no spurious roll into the new convention.
	const QVector3D worldUp = currentWorldUpVector();
	QVector3D correctedRight = QVector3D::crossProduct(rotatedViewDir, worldUp).normalized();
	if (correctedRight.lengthSquared() < 1e-6f)
	{
		// viewDir nearly collinear with worldUp — pick any orthogonal fallback
		const QVector3D fallback = (std::abs(worldUp.z()) < 0.9f)
			? QVector3D(0.0f, 0.0f, 1.0f) : QVector3D(1.0f, 0.0f, 0.0f);
		correctedRight = QVector3D::crossProduct(rotatedViewDir, fallback).normalized();
	}
	const QVector3D correctedUp = QVector3D::crossProduct(correctedRight, rotatedViewDir).normalized();

	setView(_primaryCamera->getPosition(), rotatedViewDir, correctedUp, correctedRight);

	if (_primaryCamera->getMode() == GLCamera::CameraMode::Fly ||
		_primaryCamera->getMode() == GLCamera::CameraMode::FirstPerson)
	{
		_primaryCamera->setYawPitchFromViewDir();
	}

	_currentRotation = QQuaternion::fromRotationMatrix(_primaryCamera->getViewMatrix().toGenericMatrix<3, 3>());
	_currentTranslation = _primaryCamera->getPosition();
	_customViewTargetRotation = _currentRotation;
	_customViewAnimationActive = false;
	update();
}

bool GLWidget::sceneUpAxisIsZUp(SceneUpAxis sceneUpAxis) const
{
	return sceneUpAxis == SceneUpAxis::ZUp;
}

QString GLWidget::sceneUpAxisLabel(SceneUpAxis sceneUpAxis) const
{
	return sceneUpAxisIsZUp(sceneUpAxis) ? tr("Z-Up") : tr("Y-Up");
}

void GLWidget::applyAutoOrientCameraConvention(SceneUpAxis sceneUpAxis)
{
	setCameraUpAxisZUp(sceneUpAxisIsZUp(sceneUpAxis));
}

void GLWidget::warnOnConflictingImportedSceneUpAxis(const QString& fileName, SceneUpAxis sceneUpAxis)
{
	const QString importedAxis = sceneUpAxisLabel(sceneUpAxis);
	const QString activeAxis = _cameraUpAxisZUp ? tr("Z-Up") : tr("Y-Up");
	const QString importedFileName = QFileInfo(fileName).fileName();

	QMessageBox::warning(
		this,
		tr("Camera Up-Axis Mismatch"),
		tr("The imported model \"%1\" uses %2, but this view is currently %3.\n\n"
		   "The active camera convention was left unchanged because this view already contains content.")
			.arg(importedFileName, importedAxis, activeAxis));
}

void GLWidget::setCameraUpAxisZUp(bool zUp, bool syncToolbar)
{
	if (_cameraUpAxisZUp == zUp)
	{
		if (syncToolbar && _viewToolbar)
			_viewToolbar->setCameraUpAxisZUp(zUp);
		syncCameraWorldUp();
		return;
	}

	_cameraUpAxisZUp = zUp;
	syncCameraWorldUp();
	rotateCurrentCameraAroundWorldX(zUp ? 90.0f : -90.0f);
	updateEnvMapRotationMatrix();
	updateFloorPlane();
	recalculateVisibleSceneStats(false);
	_shadowMapNeedsInitialization = true;
	initializeViewCubeLabels();

	if (syncToolbar && _viewToolbar)
		_viewToolbar->setCameraUpAxisZUp(zUp);

	emit cameraUpAxisChanged(zUp);
}

void GLWidget::setViewMode(ViewMode mode)
{
	if (!_animateViewTimer->isActive())
	{
		_keyboardNavTimer->stop();

		const QQuaternion q = standardViewRotation(mode);
		const QMatrix4x4  m(q.toRotationMatrix());

		// Compute fit + projected visual centre from the *target* orientation so
		// that the orbit target is correct as soon as the rotation animation begins.
		// Only compute fit view range if there are visible meshes.
		// On an empty scene, keep _viewBoundingSphereDia at the current
		// view range so the zoom animation is a no-op while the
		// rotation animation still proceeds normally.
		const std::vector<int>& visibleIds = _visibleSwapped ? _hiddenObjectsIds : _displayedObjectsIds;
		if (!_meshStore.empty() && !visibleIds.empty())
		{
			QVector3D projCenter;
			_viewBoundingSphereDia = computeFitViewRange(
				m.row(0).toVector3D().normalized(),
				m.row(1).toVector3D().normalized(),
				-m.row(2).toVector3D().normalized(),
				&projCenter);
			_boundingSphere.setCenter(projCenter);
		}
		else
		{
			_viewBoundingSphereDia = _currentViewRange;
		}

		_customViewTargetRotation = q;
		_customViewAnimationActive = true;
		_animateViewTimer->start(5);
		_viewMode = mode;
		_slerpStep = 0.0f;
	}
}

void GLWidget::fitAll()
{

	// Guard: do nothing if the scene has no visible meshes.
	// Without this, computeFitViewRange() operates on degenerate bounds,
	// driving _viewRange to near-zero and hiding the trihedron.
	const std::vector<int>& visibleIds = _visibleSwapped ? _hiddenObjectsIds : _displayedObjectsIds;
	if (_meshStore.empty() || visibleIds.empty())
		return;

	if (_primaryCamera->getMode() == GLCamera::CameraMode::Fly ||
		_primaryCamera->getMode() == GLCamera::CameraMode::FirstPerson)
	{
		checkAndStopTimers();
		_keyboardNavTimer->stop();
		const QVector3D viewDir = _primaryCamera->getViewDir().normalized();
		const QVector3D upDir = _primaryCamera->getUpVector().normalized();
		const QVector3D rightDir = _primaryCamera->getRightVector().normalized();
		const std::vector<QVector3D> corners = collectVisibleCorners();
		if (corners.empty())
			return;

		const float aspect = std::max(static_cast<float>(width()) / std::max(1.0f, static_cast<float>(height())), 0.001f);
		const float halfFovY = qDegreesToRadians(_FOV) * 0.5f;
		const float tanHalfY = std::max(std::tan(halfFovY), 0.001f);
		const float tanHalfX = std::max((aspect >= 1.0f ? tanHalfY * aspect : tanHalfY), 0.001f);
		const float margin = 1.05f;

		float xMin_v = std::numeric_limits<float>::max();
		float xMax_v = -std::numeric_limits<float>::max();
		float yMin_v = std::numeric_limits<float>::max();
		float yMax_v = -std::numeric_limits<float>::max();
		float zMin_v = std::numeric_limits<float>::max();
		float zMax_v = -std::numeric_limits<float>::max();

		for (const QVector3D& c : corners)
		{
			const float xc = QVector3D::dotProduct(c, rightDir);
			const float yc = QVector3D::dotProduct(c, upDir);
			const float zc = QVector3D::dotProduct(c, viewDir);
			xMin_v = std::min(xMin_v, xc);  xMax_v = std::max(xMax_v, xc);
			yMin_v = std::min(yMin_v, yc);  yMax_v = std::max(yMax_v, yc);
			zMin_v = std::min(zMin_v, zc);  zMax_v = std::max(zMax_v, zc);
		}

		const float cx = (xMin_v + xMax_v) * 0.5f;
		const float cy = (yMin_v + yMax_v) * 0.5f;
		const float cz = (zMin_v + zMax_v) * 0.5f;
		const QVector3D projCenter = rightDir * cx + upDir * cy + viewDir * cz;

		float desiredDist = 0.0f;
		for (const QVector3D& c : corners)
		{
			const float xc_rel = QVector3D::dotProduct(c, rightDir) - cx;
			const float yc_rel = QVector3D::dotProduct(c, upDir) - cy;
			const float dc = QVector3D::dotProduct(c, viewDir) - cz;

			float req;
			if (aspect >= 1.0f)
				req = std::max(std::abs(xc_rel) / aspect, std::abs(yc_rel)) / tanHalfY - dc;
			else
				req = std::max(std::abs(xc_rel), std::abs(yc_rel) * aspect) / tanHalfY - dc;

			desiredDist = std::max(desiredDist, req);
		}
		desiredDist = std::max(desiredDist * margin, 0.001f);

		const float shiftFactor = std::min(1.05f / std::sin(halfFovY), 1.25f);
		_viewBoundingSphereDia = std::max(desiredDist / std::max(shiftFactor, 0.001f), 0.0001f);
		_viewRange = _viewBoundingSphereDia;
		_boundingSphere.setCenter(projCenter);
		_primaryCamera->setViewRange(_viewRange);
		_primaryCamera->setView(projCenter - viewDir * desiredDist, viewDir, upDir, rightDir);

		_currentTranslation = _primaryCamera->getPosition();
		_currentViewRange = _viewRange;

		resizeGL(width(), height());
		update();
		emit zoomAndPanSet();
		return;
	}

	// Compute the viewRange and the projected visual centre simultaneously.
	// The projected centre is the midpoint of the geometry's view-space extents
	// for the current orientation — setting it as the orbit target ensures the
	// scene appears centred on screen with equal margins on every side.
	QVector3D projCenter;
	_viewBoundingSphereDia = computeFitViewRange(&projCenter);
	_boundingSphere.setCenter(projCenter);

	if (!_animateFitAllTimer->isActive())
	{
		_keyboardNavTimer->stop();
		_animateFitAllTimer->start(5);
		_slerpStep = 0.0f;
	}
}

void GLWidget::setAutoFitViewOnUpdate(bool update)
{
	_autoFitViewOnUpdate = update;
}

void GLWidget::setSelectionHighlighting(bool highlight)
{
	_selectionHighlighting = highlight;
	_fgShader->setUniformValue("selectionHighlighting", _selectionHighlighting);
	update();
}

void GLWidget::beginWindowZoom()
{
	_windowZoomActive = true;
	setCursor(QCursor(QPixmap(":/icons/res/window-zoom-cursor.png"), 12, 12));
}

void GLWidget::performWindowZoom()
{
	_windowZoomActive = false;

	QRect zoomRect = _rubberBand->geometry();
	if (zoomRect.width() == 0 || zoomRect.height() == 0)
	{
		emit windowZoomEnded();
		return;
	}

	QPoint zoomWinCen = zoomRect.center();
	QRect viewport = getViewportFromPoint(zoomWinCen);
	QMatrix4x4 mvMatrix = _viewMatrix * _modelMatrix;

	// Sample the depth buffer at the rubber-band centre to get the actual scene depth.
	// When the centre pixel is background, scan a 9x9 neighbourhood and take the minimum
	// non-background depth (nearest geometry). This is critical for small rubber-bands on
	// model edges/silhouettes where the centre pixel often lands on background — the error
	// in z_v is then amplified by the zoom ratio, causing visible offset.
	float depthZ;
	{
		makeCurrent();
		float rawDepth = 1.0f;
		int cx = zoomWinCen.x();
		int cy_gl = height() - zoomWinCen.y() - 1;  // flip to OpenGL bottom-up Y
		glReadPixels(cx, cy_gl, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &rawDepth);

		if (rawDepth >= 1.0f)
		{
			// Centre is background — scan a 9x9 neighbourhood for nearest geometry.
			const int halfGrid = 4;
			int x0 = std::max(0,            cx      - halfGrid);
			int y0 = std::max(0,            cy_gl   - halfGrid);
			int x1 = std::min(width()  - 1, cx      + halfGrid);
			int y1 = std::min(height() - 1, cy_gl   + halfGrid);
			int sw = x1 - x0 + 1, sh = y1 - y0 + 1;
			std::vector<float> depthBuf(sw * sh, 1.0f);
			glReadPixels(x0, y0, sw, sh, GL_DEPTH_COMPONENT, GL_FLOAT, depthBuf.data());
			float minDepth = 1.0f;
			for (float d : depthBuf)
				if (d < minDepth) minDepth = d;
			if (minDepth < 1.0f)
				rawDepth = minDepth;
		}

		if (rawDepth >= 1.0f)
		{
			// No geometry found near centre — fall back to bounding sphere centre depth.
			QVector3D Z = (_primaryCamera->getMode() == GLCamera::CameraMode::Orbit)
				? _primaryCamera->getPosition()
				: _boundingSphere.getCenter();
			Z = Z.project(mvMatrix, _projectionMatrix, viewport);
			depthZ = Z.z();
		}
		else
		{
			depthZ = rawDepth;
		}
	}

	// Unproject viewport centre (O) and rubber-band centre (P) at the scene depth.
	// The pan vector P - O brings the rubber-band centre to screen centre for any choice of depth.
	QRect clientRect = getClientRectFromPoint(zoomWinCen);
	QPoint clientWinCen = clientRect.center();
	QVector3D o(clientWinCen.x(), height() - clientWinCen.y(), depthZ);
	QVector3D O = o.unproject(mvMatrix, _projectionMatrix, viewport);

	QVector3D p(zoomWinCen.x(), height() - zoomWinCen.y(), depthZ);
	QVector3D P = p.unproject(mvMatrix, _projectionMatrix, viewport);

	// Pixel-space zoom ratio (fixed: was integer division before).
	double widthRatio  = static_cast<double>(clientRect.width())  / zoomRect.width();
	double heightRatio = static_cast<double>(clientRect.height()) / zoomRect.height();
	_rubberBandZoomRatio = static_cast<GLfloat>((heightRatio < widthRatio) ? heightRatio : widthRatio);

	// Perspective correction: the visible extent at signed view-space depth z_v is
	// proportional to the eye-to-anchor distance. Correct the zoom ratio accordingly.
	if (_projection == ViewProjection::PERSPECTIVE)
	{
		float distanceOld = _primaryCamera->getOrbitDistance();
		if (distanceOld > 0.0f && _currentViewRange > 0.0f)
		{
			const QVector3D target = _primaryCamera->getPosition();
			const QVector3D viewDir = _primaryCamera->getViewDir().normalized();
			const float dc = QVector3D::dotProduct(P - target, viewDir);
			// eye-to-P depth along viewDir = distanceOld + dc.
			// After zoom, we want the new eye-to-P = (distanceOld + dc) / ratio.
			// With D_new + dc = (distanceOld + dc) / ratio  →  D_new = anchor/ratio - dc.
			const float anchorDistanceOld = distanceOld + dc;
			if (anchorDistanceOld > 0.0f)
			{
				const float newDistance = anchorDistanceOld / _rubberBandZoomRatio - dc;
				if (newDistance > 0.0f)
				{
					const float distanceFactor = distanceOld / _currentViewRange;
					const float newViewRange = newDistance / distanceFactor;
					_rubberBandZoomRatio = _currentViewRange / newViewRange;
				}
			}
		}
	}

	// Very small rectangles can feel too aggressive in perspective because even a
	// mathematically correct ratio is visually abrupt near the object. Compress the
	// high end of the zoom ratio to keep the target in frame more reliably.
	if (_projection == ViewProjection::PERSPECTIVE)
	{
		if (_rubberBandZoomRatio > 4.0f)
			_rubberBandZoomRatio = 4.0f + (_rubberBandZoomRatio - 4.0f) * 0.6f;
		if (_rubberBandZoomRatio > 8.0f)
			_rubberBandZoomRatio = 8.0f + (_rubberBandZoomRatio - 8.0f) * 0.4f;
	}

	// Compute the pan that brings the rubber-band centre to screen centre.
	// Both P and O are unprojected at the same depthZ, so (P - O) is already
	// perpendicular to viewDir — no depth stripping is needed.
	_rubberBandPan = P - O;

	if (!_animateWindowZoomTimer->isActive())
	{
		_keyboardNavTimer->stop();
		_animateWindowZoomTimer->start(5);
		_slerpStep = 0.0f;
	}
	emit windowZoomEnded();
}

void GLWidget::setProjection(ViewProjection proj)
{
	_projection = proj;
	if (!_primaryCamera || _primaryCamera->getMode() == GLCamera::CameraMode::Orbit)
	{
		_previousProjection = (proj == ViewProjection::PERSPECTIVE)
			? GLCamera::ProjectionType::PERSPECTIVE
			: GLCamera::ProjectionType::ORTHOGRAPHIC;
	}
	resizeGL(width(), height());
}

GLCamera::CameraMode GLWidget::cameraMode() const
{
	return _primaryCamera ? _primaryCamera->getMode() : GLCamera::CameraMode::Orbit;
}

bool GLWidget::positionGameplayCameraForScene(GLCamera::CameraMode mode)
{
	if (!_primaryCamera ||
		(mode != GLCamera::CameraMode::Fly && mode != GLCamera::CameraMode::FirstPerson))
	{
		return false;
	}

	const std::vector<int>& visibleIds = _visibleSwapped ? _hiddenObjectsIds : _displayedObjectsIds;
	if (_meshStore.empty() || visibleIds.empty())
		return false;

	const QVector3D worldUp = currentWorldUpVector();
	const QVector3D center = _boundingSphere.getCenter();
	const float radius = std::max(_boundingSphere.getRadius(), 0.001f);

	// Derive scene extent along the current world-up axis directly from the bounding box
	// so this is always correct regardless of convention and when recalculate was last called.
	const float lowestUp  = _cameraUpAxisZUp ? static_cast<float>(_boundingBox.zMin())
	                                          : static_cast<float>(_boundingBox.yMin());
	const float highestUp = _cameraUpAxisZUp ? static_cast<float>(_boundingBox.zMax())
	                                          : static_cast<float>(_boundingBox.yMax());
	const float modelHeight = std::max(highestUp - lowestUp, radius * 2.0f);

	// Strip the world-up component to get a purely horizontal direction.
	QVector3D horizontalViewDir = _primaryCamera->getViewDir();
	horizontalViewDir -= QVector3D::dotProduct(horizontalViewDir, worldUp) * worldUp;
	if (horizontalViewDir.lengthSquared() <= 1.0e-8f)
	{
		// Camera looking nearly straight up/down — pick a non-up fallback axis.
		const QVector3D alt = (std::abs(worldUp.x()) < 0.9f) ? QVector3D(1, 0, 0) : QVector3D(0, 1, 0);
		horizontalViewDir = alt - QVector3D::dotProduct(alt, worldUp) * worldUp;
	}
	horizontalViewDir.normalize();

	QVector3D eye;
	QVector3D target = center;
	if (mode == GLCamera::CameraMode::Fly)
	{
		const float flyDistance = std::max(radius * 2.25f, _viewRange * 0.9f);
		const float flyLift = std::clamp(modelHeight * 0.35f, radius * 0.25f, radius * 1.25f);
		const float targetLift = std::clamp(modelHeight * 0.10f, 0.0f, radius * 0.35f);

		eye    = center - horizontalViewDir * flyDistance + worldUp * flyLift;
		target = center + worldUp * targetLift;
	}
	else
	{
		const float eyeHeight    = std::clamp(modelHeight * 0.18f, radius * 0.12f, radius * 0.45f);
		const float walkDistance = std::max(radius * 2.6f, modelHeight * 0.75f);
		const float targetHeight = std::clamp(lowestUp + modelHeight * 0.33f,
		                                      lowestUp + eyeHeight * 0.8f, highestUp);

		eye = center - horizontalViewDir * walkDistance;
		// Replace the up-axis component of eye and target with the desired heights.
		eye    += (lowestUp  + eyeHeight - QVector3D::dotProduct(eye,    worldUp)) * worldUp;
		target += (targetHeight          - QVector3D::dotProduct(target, worldUp)) * worldUp;
	}

	QVector3D viewDir = target - eye;
	if (viewDir.lengthSquared() <= 1.0e-8f)
		viewDir = horizontalViewDir;
	viewDir.normalize();

	QVector3D rightDir = QVector3D::crossProduct(viewDir, worldUp);
	if (rightDir.lengthSquared() <= 1.0e-8f)
		rightDir = QVector3D(1.0f, 0.0f, 0.0f);
	rightDir.normalize();
	QVector3D upDir = QVector3D::crossProduct(rightDir, viewDir).normalized();

	_primaryCamera->setMode(mode);
	_primaryCamera->setZoom(1.0f);
	_primaryCamera->setView(eye, viewDir, upDir, rightDir);
	_primaryCamera->setYawPitchFromViewDir();
	_primaryCamera->updateFlyView();

	_currentTranslation = _primaryCamera->getPosition();
	_currentRotation = QQuaternion::fromRotationMatrix(_primaryCamera->getViewMatrix().toGenericMatrix<3, 3>());
	return true;
}

void GLWidget::setCameraMode(GLCamera::CameraMode mode)
{
	const std::vector<int>& visibleIds = _visibleSwapped ? _hiddenObjectsIds : _displayedObjectsIds;
	const bool hasVisibleScene = !_meshStore.empty() && !visibleIds.empty();

	if (mode == GLCamera::CameraMode::Fly || mode == GLCamera::CameraMode::FirstPerson)
	{
		const bool comingFromOrbit = _primaryCamera->getMode() == GLCamera::CameraMode::Orbit;
		QVector3D orbitEye = _primaryCamera->getPosition();
		const GLCamera::ProjectionType orbitProjection = comingFromOrbit
			? _primaryCamera->getProjectionType()
			: _previousProjection;

		if (comingFromOrbit)
		{
			_previousProjection = orbitProjection;
			const QVector3D viewDir = _primaryCamera->getViewDir().normalized();
			const QVector3D center = _boundingSphere.getCenter();
			const float desiredDist = std::max(_primaryCamera->getOrbitDistance(),
				std::max(_viewRange, _boundingSphere.getRadius() * 1.75f));
			orbitEye = center - viewDir * desiredDist;
		}

		if (_primaryCamera->getProjectionType() != GLCamera::ProjectionType::PERSPECTIVE)
		{
			setProjection(ViewProjection::PERSPECTIVE);
			_previousProjection = orbitProjection;
		}

		// setMode syncs yaw/pitch from the current viewDir, resets up/right vectors
		_primaryCamera->setMode(mode);

		// Drop any zoom scale accumulated in Orbit mode; Fly uses real position instead
		_primaryCamera->setZoom(1.0f);

		// Spawn gameplay cameras around the loaded model instead of inheriting the orbit pivot.
		if (comingFromOrbit && positionGameplayCameraForScene(mode))
		{
			_currentTranslation = _primaryCamera->getPosition();
		}
		else
		{
			// Continue from the actual orbit eye position to avoid a visible jump.
			_primaryCamera->setPosition(orbitEye);
			_currentTranslation = _primaryCamera->getPosition();
		}
	}
	else if (mode == GLCamera::CameraMode::Orbit)
	{
		if (_primaryCamera->getMode() == GLCamera::CameraMode::Fly ||
			_primaryCamera->getMode() == GLCamera::CameraMode::FirstPerson)
		{
			const QVector3D eye = _primaryCamera->getPosition();
			const QVector3D target = eye + _primaryCamera->getViewDir() * _primaryCamera->getOrbitDistance();
			_primaryCamera->setPosition(target);
		}

		_primaryCamera->setMode(mode);
		setProjection(_previousProjection == GLCamera::ProjectionType::PERSPECTIVE ? ViewProjection::PERSPECTIVE : ViewProjection::ORTHOGRAPHIC);
		_currentTranslation = _primaryCamera->getPosition();
		_currentRotation = QQuaternion::fromRotationMatrix(_primaryCamera->getViewMatrix().toGenericMatrix<3, 3>());
		_currentViewRange = _viewRange;
	}

	if (hasVisibleScene)
	{
		fitAll();
	}
	else
	{
		resizeGL(width(), height());
		update();
	}
}

void GLWidget::setRotationActive(bool active)
{
	_viewPanning = false;
	_viewZooming = false;
	_viewRotating = active;
	setCursor(QCursor(QPixmap(":/icons/res/rotatecursor.png")));
	MainWindow::showStatusMessage(tr("Press Esc to deactivate rotation mode"));
}

void GLWidget::setPanningActive(bool active)
{
	_viewRotating = false;
	_viewZooming = false;
	_viewPanning = active;
	setCursor(QCursor(QPixmap(":/icons/res/pancursor.png")));
	MainWindow::showStatusMessage(tr("Press Esc to deactivate panning mode"));
}

void GLWidget::setZoomingActive(bool active)
{
	_viewPanning = false;
	_viewRotating = false;
	_viewZooming = active;
	setCursor(QCursor(QPixmap(":/icons/res/zoomcursor.png")));
	MainWindow::showStatusMessage(tr("Press Esc to deactivate zooming mode"));
}

void GLWidget::setDisplayList(const std::vector<int>& ids)
{
	_displayedObjectsIds = ids;

	std::vector<int> allObjectIDs;
	for (size_t i = 0; i < _meshStore.size(); i++)
	{
		allObjectIDs.push_back(static_cast<int>(i));
	}
	_hiddenObjectsIds.clear();
	std::set_difference(
		allObjectIDs.begin(), allObjectIDs.end(),
		_displayedObjectsIds.begin(), _displayedObjectsIds.end(),
		std::back_inserter(_hiddenObjectsIds)
	);

	if (_hiddenObjectsIds.size() == 0 && _visibleSwapped)
	{
		_visibleSwapped = false;
		emit visibleSwapped(_visibleSwapped);
	}

	_currentTranslation = _primaryCamera->getPosition();
	_boundingSphere.setCenter(0, 0, 0);

	// Recompute all visible-scene aggregates in one pass.
	recalculateVisibleSceneStats(true);
	// Reset smoothed zoom floor to scene radius so a freshly loaded model
	// starts with the standard global clamp, not a stale small value.
	_zoomInLimit = _boundingSphere.getRadius();

	// Reposition lights from per-file user model transforms (derived from the
	// current mesh TRS state — no bounding-sphere anchoring involved).
	updatePunctualLights();

	triggerShadowRecomputation();
	updateFloorPlane();

	if (_primaryCamera->getMode() == GLCamera::CameraMode::Fly ||
		_primaryCamera->getMode() == GLCamera::CameraMode::FirstPerson)
	{
		if (_primaryCamera->getProjectionType() != GLCamera::ProjectionType::PERSPECTIVE)
		{
			setProjection(ViewProjection::PERSPECTIVE);
		}

		positionGameplayCameraForScene(_primaryCamera->getMode());
	}
	else if (_autoFitViewOnUpdate)
	{
		if (!isGltfCameraActive())
		{
			fitAll();
		}
	}

	update();

	emit displayListSet();
}

void GLWidget::recalculateVisibleSceneStats(bool updateMemorySize)
{
	_currentTranslation = _primaryCamera->getPosition();
	_boundingSphere.setCenter(0, 0, 0);
	_boundingSphere.setRadius(0.0f);
	_boundingBox.setLimits(-0.001, -0.001, -0.001, 0.001, 0.001, 0.001);
	_visibleLowestZ = -1.0f;
	_visibleHighestZ = 1.0f;

	const std::vector<int>& visibleIds = _visibleSwapped ? _hiddenObjectsIds : _displayedObjectsIds;
	if (updateMemorySize)
	{
		_displayedObjectsMemSize = 0;
	}

	if (visibleIds.empty())
	{
		_primaryCamera->setPosition(0, 0, 0);
		_currentTranslation = _primaryCamera->getPosition();
		_boundingSphere.setRadius(1.0f);
		return;
	}

	bool firstBox = true;
	float lowestZ = std::numeric_limits<float>::max();
	float highestZ = std::numeric_limits<float>::lowest();
	unsigned long long memSize = 0;

	for (int i : visibleIds)
	{
		try
		{
			TriangleMesh* mesh = _meshStore.at(i);
			if (!isMeshAnimationVisible(mesh))
				continue;
			if (updateMemorySize)
			{
				memSize += mesh->memorySize();
			}

			const BoundingBox meshBox = mesh->getBoundingBox();
			if (firstBox)
			{
				_boundingBox = meshBox;
				firstBox = false;
			}
			else
			{
				_boundingBox.addBox(meshBox);
			}

			const float meshLow  = _cameraUpAxisZUp ? static_cast<float>(meshBox.zMin()) : static_cast<float>(meshBox.yMin());
			const float meshHigh = _cameraUpAxisZUp ? static_cast<float>(meshBox.zMax()) : static_cast<float>(meshBox.yMax());
			lowestZ  = std::min(lowestZ,  meshLow);
			highestZ = std::max(highestZ, meshHigh);
		}
		catch (const std::out_of_range& ex)
		{
			std::cout << ex.what() << std::endl;
		}
	}

	if (updateMemorySize)
	{
		_displayedObjectsMemSize = memSize;
	}

	if (!firstBox)
	{
		_visibleLowestZ = lowestZ;
		_visibleHighestZ = highestZ;

		// Derive the scene bounding sphere center from the axis-aligned bounding box
		// midpoint — order-independent and immune to floating-point perturbations from
		// the world-transform round-trip during export/import.
		//
		// The radius is computed as max over all visible meshes of:
		//   distance(boxCenter, mesh.sphereCenter) + mesh.sphereRadius
		// This is still O(M) and order-independent (it's a simple max), but much tighter
		// than the box half-diagonal for round geometry (e.g. sphere meshes), where the
		// half-diagonal would be sqrt(3)x the actual radius.
		const QVector3D boxCenter(
			static_cast<float>((_boundingBox.xMin() + _boundingBox.xMax()) * 0.5),
			static_cast<float>((_boundingBox.yMin() + _boundingBox.yMax()) * 0.5),
			static_cast<float>((_boundingBox.zMin() + _boundingBox.zMax()) * 0.5)
		);
		float bsRadius = 0.0f;
		for (int i : visibleIds)
		{
			try
			{
				TriangleMesh* mesh = _meshStore.at(i);
				if (!isMeshAnimationVisible(mesh))
					continue;
				BoundingSphere ms = mesh->getBoundingSphere();
				const float d = (ms.getCenter() - boxCenter).length() + ms.getRadius();
				if (d > bsRadius) bsRadius = d;
			}
			catch (const std::out_of_range&) {}
		}
		_boundingSphere.setCenter(boxCenter);
		_boundingSphere.setRadius(bsRadius > 0.0f ? bsRadius : 1.0f);
	}
}

void GLWidget::triggerShadowRecomputation()
{
	if (!_fgShader)
	{
		_shadowMapNeedsInitialization = true;
		return;
	}

	float boundingRadius = _boundingSphere.getRadius();
	_viewBoundingSphereDia = boundingRadius * 2;

	float lightDistance = calculateLightDistance();
	const float coverageHint = (_shadowFrustumExtentW > 0.0f)
		? (std::max)(_shadowFrustumExtentW, _shadowFrustumExtentH)
		: -1.0f;
	float shadowFactor = shadowMapper.calculateShadowFactor(boundingRadius, lightDistance, coverageHint);
	shadowFactor = std::clamp(shadowFactor, 1.0f, 8.0f);

	_shadowWidth = static_cast<int>(1024 * shadowFactor);
	_shadowHeight = static_cast<int>(1024 * shadowFactor);

	// Get SIZE-AWARE shadow parameters
	auto shadowParams = shadowMapper.getShadowQualityParamsSmooth(boundingRadius);
	float shadowSoftness = shadowMapper.calculateShadowSoftness(_viewBoundingSphereDia);
	float sizeScale = shadowMapper.calculateSizeQualityScale(boundingRadius);

	_fgShader->bind();

	_fgShader->setUniformValue("shadowSoftness", shadowSoftness);

	// Size-aware uniforms
	_fgShader->setUniformValue("shadowMaxKernelSize", shadowParams.maxKernelSize);
	_fgShader->setUniformValue("shadowSoftnessScale", shadowParams.softnessScale);
	_fgShader->setUniformValue("shadowMaxSoftnessClamp", shadowParams.maxSoftnessClamp);
	_fgShader->setUniformValue("shadowBiasMin", shadowParams.biasMin);
	_fgShader->setUniformValue("shadowBiasMax", shadowParams.biasMax);
	_fgShader->setUniformValue("shadowTransitionRange", shadowParams.transitionRange);
	_fgShader->setUniformValue("shadowGammaCorrection", shadowParams.gammaCorrection);
	_fgShader->setUniformValue("shadowSizeScale", sizeScale);

	_fgShader->release();

	_shadowMapNeedsInitialization = true;
	makeCurrent();
	loadFloor();	
}

void GLWidget::setShadowQuality(AdaptiveShadowMapper::QualityLevel quality)
{
	shadowMapper.setQuality(quality);
	triggerShadowRecomputation();
	updateFloorPlane();
}

float GLWidget::calculateLightDistance()
{
	QVector3D lightPos = effectiveWorldLightPosition();
	QVector3D center = _boundingSphere.getCenter();
	return (lightPos - center).length();
}

QVector<QUuid> GLWidget::duplicateObjects(const std::vector<int>& ids)
{
	QVector<QUuid> duplicatedUuids;

	makeCurrent();

	for (int id : ids)
	{
		TriangleMesh* originalMesh = _meshStore.at(id);
		if (originalMesh)
		{
			// Clone the mesh
			TriangleMesh* newMesh = originalMesh->clone();
			if (newMesh)
			{
				// Generate unique name with suffix
				QString uniqueName = generateUniqueMeshName(originalMesh->getName());
				newMesh->setName(uniqueName);

				// Add to display
				addToDisplay(newMesh);

				// Store the UUID of the duplicated mesh
				duplicatedUuids.append(newMesh->uuid());

				qDebug() << "Duplicated mesh:" << originalMesh->getName()
					<< "->" << uniqueName
					<< "uuid:" << newMesh->uuid();
			}
		}
	}

	doneCurrent();

	return duplicatedUuids;
}

void GLWidget::updateBoundingSphere()
{
	recalculateVisibleSceneStats(false);
}

void GLWidget::updateBoundingBox()
{	
	recalculateVisibleSceneStats(false);
}

void GLWidget::updateFloorPlane()
{
	if ((!_floorPlane || !_fgShader) && (!_gridPlane || !_gridShader))
		return;

	// Use helper to update floor geometry
	float halfObjectSize = updateFloorGeometry();

	// Use helper to set main light position (now consistent with loadFloor)
	updateMainLightPosition(halfObjectSize);

	_floorPlaneZ = groundPlaneZ();
	const float groundExtent = groundPlaneExtent();
	if (_floorPlane && _fgShader)
	{
		_floorPlane->setPlane(_fgShader.get(), _floorCenter, groundExtent, groundExtent, 1, 1, _floorPlaneZ, _floorTexRepeatS, _floorTexRepeatT, floorPlaneOrientation());
		applyFloorPlaneMaterialSettings();
	}

	// Create fallback light if no punctual lights available
	if (_originalParsedLights.empty())
	{
		if (shouldUseFallbackLightForVisibleScene())
		{
			const QVector3D fallbackLightPos = effectiveWorldLightPosition();
			glLights->createFallbackLight(glm::vec3(
				static_cast<float>(fallbackLightPos.x()),
				static_cast<float>(fallbackLightPos.y()),
				static_cast<float>(fallbackLightPos.z())
			));
			syncPunctualLightUniforms(1, true);
		}
		else
		{
			glLights->setLights({});
			syncPunctualLightUniforms(0, false);
		}
	}

	updateClippingPlane();
}

void GLWidget::syncPunctualLightUniforms(int lightCount, bool hasPunctualLights)
{
	if (!_fgShader)
		return;

	_fgShader->bind();
	_fgShader->setUniformValue("lightCount", lightCount);
	_fgShader->setUniformValue("hasPunctualLights", hasPunctualLights);
}

bool GLWidget::shouldUseFallbackLightForVisibleScene() const
{
	const std::vector<int>& visibleIds = _visibleSwapped ? _hiddenObjectsIds : _displayedObjectsIds;
	bool sawVisibleMesh = false;
	bool sawGltfDerivedMesh = false;

	for (int meshId : visibleIds)
	{
		if (meshId < 0 || meshId >= static_cast<int>(_meshStore.size()))
			continue;

		TriangleMesh* mesh = _meshStore.at(meshId);
		if (!mesh)
			continue;

		sawVisibleMesh = true;
		const QString sourceFile = mesh->getSourceFile().trimmed();
		if (sourceFile.isEmpty())
			continue;

		if (sourceFile.endsWith(".gltf", Qt::CaseInsensitive) ||
			sourceFile.endsWith(".glb", Qt::CaseInsensitive))
		{
			sawGltfDerivedMesh = true;
			continue;
		}

		return true;
	}

	if (!sawVisibleMesh)
		return true;

	return !sawGltfDerivedMesh;
}

void GLWidget::updateClippingPlane()
{
	float xside = _clipXFlipped || _clipXCoeff > 0 ? -1.0f : 1.0f;
	float yside = _clipYFlipped || _clipYCoeff > 0 ? 1.0f : -1.0f;
	float zside = _clipZFlipped || _clipZCoeff > 0 ? -1.0f : 1.0f;
	_clippingPlaneXY->setPlane(_clippingPlaneShader.get(), _floorCenter, _floorSize * 100.0f, _floorSize * 100.0f, 1, 1, -_clipZCoeff * zside, _floorSize, _floorSize);
	_clippingPlaneYZ->setPlane(_clippingPlaneShader.get(), _floorCenter, _floorSize * 100.0f, _floorSize * 100.0f, 1, 1, -_clipXCoeff * xside, _floorSize, _floorSize);
	_clippingPlaneZX->setPlane(_clippingPlaneShader.get(), _floorCenter, _floorSize * 100.0f, _floorSize * 100.0f, 1, 1, -_clipYCoeff * yside, _floorSize, _floorSize);
	_clippingPlanesEditor->setCoefficientLimits(-_boundingBox.getXSize()/2, _boundingBox.getXSize()/2, 
		-_boundingBox.getYSize() / 2, _boundingBox.getYSize() / 2,
		-_boundingBox.getZSize() / 2, _boundingBox.getZSize() / 2);
}

void GLWidget::showClippingPlaneEditor(bool show)
{
	if (show)
	{
		if (_explodedViewPanel && _explodedViewPanel->isVisible())
			showExplodedViewPanel(false);
		if (_viewToolbar)
			_viewToolbar->setExplodedViewChecked(false);
		_clippingPlanesEditor->show();
	}
	else
	{
		_clippingPlanesEditor->hide();
	}
}

void GLWidget::showExplodedViewPanel(bool show)
{
	if (show) {
		if (_clippingPlanesEditor && _clippingPlanesEditor->isVisible())
			showClippingPlaneEditor(false);
		if (_viewToolbar)
			_viewToolbar->setSectionViewChecked(false);
		_explodedViewPanel->captureCurrentSelection();
		_explodedViewPanel->show();
		updateExplosion();

		if (_explodedViewManualPlacementSuppressed && !_explodedViewManualHiddenStates.isEmpty())
		{
			QHash<QUuid, QVector3D> savedExplosionOffsets;
			savedExplosionOffsets.reserve(static_cast<int>(_meshStore.size()));
			for (TriangleMesh* mesh : _meshStore)
			{
				if (!mesh)
					continue;

				savedExplosionOffsets.insert(mesh->uuid(), mesh->explosionOffset());
				mesh->setExplosionOffset(QVector3D());
			}

			for (auto it = _explodedViewManualHiddenStates.cbegin();
			     it != _explodedViewManualHiddenStates.cend(); ++it)
			{
				TriangleMesh* mesh = getMeshByUuid(it.key());
				if (!mesh)
					continue;

				const TransformState& state = it.value();
				applyExplodedViewTransformState(mesh, state, false);
			}

			for (TriangleMesh* mesh : _meshStore)
			{
				if (!mesh)
					continue;

				mesh->setExplosionOffset(savedExplosionOffsets.value(mesh->uuid()));
				mesh->setSceneRenderTransformFast(mesh->getSceneRenderTransform());
			}

			_explodedViewManualPlacementSuppressed = false;
		}
	} else {
		_explodedViewPanel->deactivateInteractiveState();
		if (isExplodedViewManualPlacementActive())
			finishExplodedViewManualPlacement();
		else
			showTransformGizmoForSelection(false);

		if (!_explodedViewManualOriginalStates.isEmpty())
		{
			_explodedViewManualHiddenStates.clear();
			for (auto it = _explodedViewManualOriginalStates.cbegin();
			     it != _explodedViewManualOriginalStates.cend(); ++it)
			{
				TriangleMesh* mesh = getMeshByUuid(it.key());
				if (!mesh)
					continue;

				_explodedViewManualHiddenStates.insert(it.key(), TransformState(
					mesh->getExplodedViewTranslation(),
					mesh->getExplodedViewRotation(),
					mesh->getExplodedViewScaling(),
					mesh->getExplodedViewRotationQuaternion()));
			}
		}

		_explodedViewPanel->hide();
		_explodedViewManager->reset();
		_cachedHintsValid = false;

		// Hide the entire exploded authoring state while the panel is closed.
		// This means clearing auto offsets and temporarily restoring the original
		// mesh TRS for any staged manual placement, without discarding the staged
		// manual pose that will be restored when the panel is shown again.
		for (size_t i = 0; i < _meshStore.size(); ++i)
		{
			if (_meshStore[i])
				_meshStore[i]->setExplosionOffset(QVector3D());
		}

		for (auto it = _explodedViewManualOriginalStates.cbegin();
		     it != _explodedViewManualOriginalStates.cend(); ++it)
		{
			TriangleMesh* mesh = getMeshByUuid(it.key());
			if (!mesh)
				continue;

			const TransformState& state = it.value();
			applyExplodedViewTransformState(mesh, state, false);
		}

		_explodedViewManualPlacementSuppressed = !_explodedViewManualHiddenStates.isEmpty();
		for (size_t i = 0; i < _meshStore.size(); ++i)
		{
			if (_meshStore[i])
				_meshStore[i]->setSceneRenderTransformFast(_meshStore[i]->getSceneRenderTransform());
		}
		update();
	}
}

// ---------------------------------------------------------------------------
// Explosion: recompute offsets from current panel state and trigger repaint.
// ---------------------------------------------------------------------------
void GLWidget::updateExplosion()
{
    if (!_explodedViewPanel || !_explodedViewManager)
        return;

    const QSet<QUuid>& assemblyUuids = _explodedViewPanel->assemblyUuids();
    if (assemblyUuids.isEmpty()) {
        _explodedViewManager->reset();
        for (size_t i = 0; i < _meshStore.size(); ++i)
        {
            if (_meshStore[i])
            {
                _meshStore[i]->setExplosionOffset(QVector3D());
                _meshStore[i]->setSceneRenderTransformFast(_meshStore[i]->getSceneRenderTransform());
            }
        }
        _shadowMapNeedsInitialization = true;
        update();
        return;
    }

    // Clear any previous offsets BEFORE building centroids so that getBoundingSphere()
    // (used in the no-scene-graph fallback) returns the original non-exploded centers.
    for (size_t i = 0; i < _meshStore.size(); ++i)
    {
        if (_meshStore[i])
        {
            _meshStore[i]->setExplosionOffset(QVector3D());
            _meshStore[i]->setSceneRenderTransformFast(_meshStore[i]->getSceneRenderTransform());
        }
    }

    // Build world-space centroids and AABBs for each assembly mesh.
    // Offsets were cleared above, so getBoundingBox() returns the unmodified
    // world-space AABB at the mesh's original (non-exploded) position.
    QHash<QUuid, QVector3D>                          worldCentroids;
    QHash<QUuid, QPair<QVector3D, QVector3D>>        worldBoxes;

    SceneGraph* sg = (_viewer && _viewer->sceneGraph()) ? _viewer->sceneGraph() : nullptr;
    if (sg)
    {
        const auto wt = sg->evaluateWorldTransforms();
        for (const QUuid& uuid : assemblyUuids)
        {
            TriangleMesh* mesh = getMeshByUuid(uuid);
            if (!mesh) continue;

            const BoundingBox bb = mesh->getBoundingBox();
            const QVector3D bbMin(static_cast<float>(bb.xMin()),
                                  static_cast<float>(bb.yMin()),
                                  static_cast<float>(bb.zMin()));
            const QVector3D bbMax(static_cast<float>(bb.xMax()),
                                  static_cast<float>(bb.yMax()),
                                  static_cast<float>(bb.zMax()));

            // Centroid = midpoint of world-space AABB.
            worldCentroids.insert(uuid, (bbMin + bbMax) * 0.5f);
            worldBoxes.insert(uuid, {bbMin, bbMax});
        }
    }
    else
    {
        // No scene graph — fall back to bounding sphere centres.
        for (const QUuid& uuid : assemblyUuids)
        {
            TriangleMesh* mesh = getMeshByUuid(uuid);
            if (!mesh) continue;

            const BoundingBox bb = mesh->getBoundingBox();
            const QVector3D bbMin(static_cast<float>(bb.xMin()),
                                  static_cast<float>(bb.yMin()),
                                  static_cast<float>(bb.zMin()));
            const QVector3D bbMax(static_cast<float>(bb.xMax()),
                                  static_cast<float>(bb.yMax()),
                                  static_cast<float>(bb.zMax()));

            worldCentroids.insert(uuid, (bbMin + bbMax) * 0.5f);
            worldBoxes.insert(uuid, {bbMin, bbMax});
        }
    }

    const bool useAssemblyAwareAutoHints =
        _explodedViewPanel->autoStrategy() == ExplodedViewPanel::AutoStrategy::AssemblyAware;

    // Rebuild O(n²) placement hints only when the assembly or anchor changes,
    // not on every slider tick.
    if (useAssemblyAwareAutoHints)
    {
        const QUuid anchorUuid = _explodedViewPanel->anchorUuid();
        if (!_cachedHintsValid
            || _cachedHintsAssemblyUuids != assemblyUuids
            || _cachedHintsAnchorUuid    != anchorUuid)
        {
            _cachedAutoHints            = AssemblyRelationGraph::buildAutoPlacementHints(assemblyUuids, this, sg);
            _cachedHintsAssemblyUuids   = assemblyUuids;
            _cachedHintsAnchorUuid      = anchorUuid;
            _cachedHintsValid           = true;
        }
    }
    else
    {
        _cachedHintsValid = false;
    }

    _explodedViewManager->recompute(
        assemblyUuids,
        _explodedViewPanel->anchorUuid(),
        _explodedViewPanel->mode(),
        _explodedViewPanel->userVector(),
        _explodedViewPanel->factor(),
        worldCentroids,
        worldBoxes,
        useAssemblyAwareAutoHints ? &_cachedAutoHints : nullptr);

    // Push computed offsets onto the meshes so combinedRenderTransform() returns
    // the exploded position for every render path (main, selection, shadow, etc.).
    for (const QUuid& uuid : assemblyUuids)
    {
        TriangleMesh* mesh = getMeshByUuid(uuid);
        if (mesh)
        {
            mesh->setExplosionOffset(_explodedViewManager->offsetForMesh(uuid));
            mesh->setSceneRenderTransformFast(mesh->getSceneRenderTransform());
        }
    }

    _shadowMapNeedsInitialization = true;
    update();
}

// ---------------------------------------------------------------------------
// Render wrapper: explosion offsets are baked directly into each mesh's
// combinedRenderTransform() via TriangleMesh::_explosionOffset, so both the
// main render and the selection pass see the correct exploded positions
// automatically without any per-call shader state manipulation.
// ---------------------------------------------------------------------------
void GLWidget::renderMeshExploded(TriangleMesh* mesh, DisplayMode mode)
{
    renderMeshWithDisplayMode(mesh, mode);
}

QWidget* GLWidget::attachOverlayPanel(QWidget* contentWidget, const QRect& geometry,
                                      Qt::Alignment, const QString& objectName)
{
	if (!contentWidget)
		return nullptr;

	auto* wrapper = new QWidget(this);
	const QString wrapperName = objectName.isEmpty()
		? QStringLiteral("glOverlayPanel")
		: objectName;
	wrapper->setObjectName(wrapperName);
	wrapper->setAttribute(Qt::WA_StyledBackground, true);
	wrapper->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
	applyOverlayPanelStyle(wrapper, wrapperName);

	QVBoxLayout* layout = new QVBoxLayout(wrapper);
	layout->setContentsMargins(6, 6, 6, 6);
	layout->addWidget(contentWidget);

	wrapper->setGeometry(geometry);
	wrapper->show();
	contentWidget->show();
	wrapper->raise();
	if (wrapperName == QLatin1String("navigationOverlayPanel"))
		_navigationOverlayPanel = wrapper;
	return wrapper;
}

QWidget* GLWidget::takeOverlayPanel(QWidget* contentWidget)
{
	if (!contentWidget)
		return nullptr;

	QWidget* wrapper = contentWidget->parentWidget();
	if (!wrapper || wrapper == this)
		return nullptr;

	if (QLayout* layout = wrapper->layout())
		layout->removeWidget(contentWidget);
	contentWidget->setParent(nullptr);

	if (wrapper == _navigationOverlayPanel)
		_navigationOverlayPanel = nullptr;

	wrapper->deleteLater();
	return wrapper;
}

void GLWidget::refreshDetachedNavigationOverlayTheme()
{
	refreshNavigationOverlayStyle();
}

void GLWidget::applyOverlayPanelStyle(QWidget* wrapper, const QString& objectName)
{
	if (!wrapper)
		return;

	const QColor averageBackgroundColor(
		(_bgTopColor.red() + _bgBotColor.red()) / 2,
		(_bgTopColor.green() + _bgBotColor.green()) / 2,
		(_bgTopColor.blue() + _bgBotColor.blue()) / 2,
		(_bgTopColor.alpha() + _bgBotColor.alpha()) / 2);
	const QColor contrastColor = (averageBackgroundColor.lightnessF() < 0.5)
		? QColor(255, 255, 255)
		: QColor(0, 0, 0);
	const QColor viewerTextColor = contrastColor;
	const bool darkBackground = averageBackgroundColor.lightnessF() < 0.5;
	const QColor panelFieldColor = darkBackground
		? QColor(24, 24, 24, 210)
		: QColor(255, 255, 255, 215);
	const QColor panelTextColor = (panelFieldColor.lightnessF() < 0.5)
		? QColor(255, 255, 255)
		: QColor(0, 0, 0);
	const QColor panelFieldBorderColor = darkBackground
		? QColor(255, 255, 255, 85)
		: QColor(0, 0, 0, 65);
	const QColor treeBaseColor = darkBackground
		? QColor(255, 255, 255, 190)
		: QColor(32, 32, 32, 165);
	const QColor treeAlternateColor = darkBackground
		? QColor(245, 245, 245, 190)
		: QColor(52, 52, 52, 165);
	const QColor treeTextColor = (treeBaseColor.lightnessF() < 0.5)
		? QColor(255, 255, 255)
		: QColor(0, 0, 0);

	wrapper->setStyleSheet(QString(
		"QWidget#%1 {"
		"  background-color: rgba(255, 255, 255, 25%);"
		"  border: 1px solid rgba(255, 255, 255, 40);"
		"  border-radius: 6px;"
		"}"
		"QWidget#%1 QLineEdit {"
		"  background-color: rgba(%5, %6, %7, %8);"
		"  color: rgb(%21, %22, %23);"
		"  border: 1px solid rgba(%9, %10, %11, %12);"
		"  border-radius: 4px;"
		"  padding: 2px 6px;"
		"}"
		"QWidget#%1 QTreeWidget {"
		"  background-color: rgba(%13, %14, %15, %16);"
		"  alternate-background-color: rgba(%17, %18, %19, %20);"
		"  color: rgb(%24, %25, %26);"
		"}"
		/* Tab bar: transparent background, tinted tabs that adapt to dark/light bg */
		"QWidget#%1 QTabBar {"
		"  background-color: transparent;"
		"}"
		"QWidget#%1 QTabWidget,"
		"QWidget#%1 QStackedWidget,"
		"QWidget#%1 QTabWidget::pane {"
		"  background: transparent;"
		"  border: none;"
		"}"
		"QWidget#%1 QWidget[transparentOverlaySurface=\"true\"] {"
		"  background: transparent;"
		"  border: none;"
		"}"
		"QWidget#%1 QTabBar::tab {"
		"  background-color: rgba(%2, %3, %4, 40);"
		"  color: rgb(%21, %22, %23);"
		"  border-radius: 4px;"
		"  padding: 3px 10px;"
		"  margin-right: 2px;"
		"}"
		"QWidget#%1 QTabBar::tab:selected {"
		"  background-color: rgba(%2, %3, %4, 110);"
		"}"
		"QWidget#%1 QTabBar::tab:hover:!selected {"
		"  background-color: rgba(%2, %3, %4, 65);"
		"}"
		"QWidget#%1 QLabel,"
		"QWidget#%1 QCheckBox,"
		"QWidget#%1 QRadioButton,"
		"QWidget#%1 QGroupBox,"
		"QWidget#%1 QPushButton,"
		"QWidget#%1 QToolButton,"
		"QWidget#%1 QLineEdit,"
		"QWidget#%1 QSpinBox,"
		"QWidget#%1 QDoubleSpinBox,"
		"QWidget#%1 QComboBox {"
		"  color: rgb(%21, %22, %23);"
		"}")
		.arg(objectName)
		.arg(contrastColor.red())
		.arg(contrastColor.green())
		.arg(contrastColor.blue())
		.arg(panelFieldColor.red())
		.arg(panelFieldColor.green())
		.arg(panelFieldColor.blue())
		.arg(panelFieldColor.alpha())
		.arg(panelFieldBorderColor.red())
		.arg(panelFieldBorderColor.green())
		.arg(panelFieldBorderColor.blue())
		.arg(panelFieldBorderColor.alpha())
		.arg(treeBaseColor.red())
		.arg(treeBaseColor.green())
		.arg(treeBaseColor.blue())
		.arg(treeBaseColor.alpha())
		.arg(treeAlternateColor.red())
		.arg(treeAlternateColor.green())
		.arg(treeAlternateColor.blue())
		.arg(treeAlternateColor.alpha())
		.arg(panelTextColor.red())
		.arg(panelTextColor.green())
		.arg(panelTextColor.blue())
		.arg(treeTextColor.red())
		.arg(treeTextColor.green())
		.arg(treeTextColor.blue()));

	QPalette wrapperPalette = wrapper->palette();
	wrapperPalette.setColor(QPalette::WindowText, contrastColor);
	wrapperPalette.setColor(QPalette::Text, contrastColor);
	wrapperPalette.setColor(QPalette::ButtonText, contrastColor);
	wrapperPalette.setColor(QPalette::HighlightedText, darkBackground ? QColor(255, 255, 255) : QColor(0, 0, 0));
	wrapper->setPalette(wrapperPalette);
	wrapper->setProperty("overlayPanelLightText", panelTextColor.lightnessF() > 0.5);
	wrapper->setProperty("overlayViewerLightText", viewerTextColor.lightnessF() > 0.5);
	wrapper->setProperty("overlayPanelTreeLightText", treeTextColor.lightnessF() > 0.5);

	const auto navigationDescendants = wrapper->findChildren<QWidget*>();
	for (QWidget* child : navigationDescendants)
	{
		if (!child)
			continue;

		const bool treeLike = qobject_cast<QTreeView*>(child) != nullptr;
		const bool transparentOverlayText = child->property("transparentOverlayText").toBool();
		const QColor childTextColor = (treeLike || transparentOverlayText) ? viewerTextColor : panelTextColor;
		child->setProperty("overlayPanelLightText", childTextColor.lightnessF() > 0.5);
		child->setProperty("overlayViewerLightText", viewerTextColor.lightnessF() > 0.5);
		QPalette palette = child->palette();
		palette.setColor(QPalette::WindowText, childTextColor);
		palette.setColor(QPalette::Text, childTextColor);
		palette.setColor(QPalette::ButtonText, childTextColor);
		palette.setColor(QPalette::HighlightedText, treeLike ? viewerTextColor : panelTextColor);
		child->setPalette(palette);

		if (auto* treeView = qobject_cast<QTreeView*>(child))
		{
			QPalette viewportPalette = treeView->viewport()->palette();
			viewportPalette.setColor(QPalette::WindowText, viewerTextColor);
			viewportPalette.setColor(QPalette::Text, viewerTextColor);
			viewportPalette.setColor(QPalette::ButtonText, viewerTextColor);
			viewportPalette.setColor(QPalette::HighlightedText, viewerTextColor);
			treeView->viewport()->setPalette(viewportPalette);
			treeView->viewport()->update();
			treeView->update();
		}
		else if (auto* variantsPanel = qobject_cast<MaterialVariantsPanel*>(child))
		{
			variantsPanel->refreshDetachedOverlayTheme();
		}
		else if (auto* animationsPanel = qobject_cast<AnimationsPanel*>(child))
		{
			animationsPanel->refreshDetachedOverlayTheme();
		}
	}
}

float GLWidget::groundPlaneZ()
{
	const float workspaceExtent = static_cast<float>(std::max({
		_boundingBox.getXSize(),
		_boundingBox.getYSize(),
		_boundingBox.getZSize()
	}));
	const float lowestUp = _cameraUpAxisZUp
		? static_cast<float>(_boundingBox.zMin())
		: static_cast<float>(_boundingBox.yMin());
	return lowestUp - (_floorSize * _floorOffsetPercent) - computeFloorDepthBias(workspaceExtent, _floorSize);
}

void GLWidget::refreshNavigationOverlayStyle()
{
	if (_navigationOverlayPanel)
		applyOverlayPanelStyle(_navigationOverlayPanel, QStringLiteral("navigationOverlayPanel"));
}

void GLWidget::setClippingPlaneHatchMode(ClippingPlaneHatchMode mode)
{
	_hatchMode = mode;
	update();
}

void GLWidget::setClippingPlaneHatchPattern(HatchPattern pattern)
{
	_hatchPattern = pattern;
	update();
}

void GLWidget::setHatchTiling(int tiling)
{
	_hatchTiling = tiling;
	update();
}

void GLWidget::setHatchLineThickness(float width)
{
	_hatchThickness = width;
	update();
}

void GLWidget::setHatchIntensity(float spacing)
{
	_hatchIntensity = spacing;
	update();
}

void GLWidget::setHatchLayers(int layers)
{
	_hatchLayers = layers;
	update();
}

void GLWidget::setHatchLineColor(const QColor& color)
{
	_hatchLineColor = QVector3D(color.redF(), color.greenF(), color.blueF());
}

void GLWidget::setHatchTexture(const QString& path)
{
	_hatchTexturePath = path;
	_cappingTexture = loadTextureFromFile(_hatchTexturePath.toStdString().c_str());
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, _cappingTexture);
	update();
}

void GLWidget::showAxis(bool show)
{
	_showAxis = show;
	_fgShader->bind();
	_fgShader->setUniformValue("showAxis", _showAxis);
	update();
}

void GLWidget::showTransformGizmoForSelection(bool show)
{
	if (!show && _transformGizmoTranslating)
		finishTransformGizmoTranslationDrag(false);
	if (!show && _transformGizmoScaling)
		finishTransformGizmoScaleDrag(false);
	if (!show && _transformGizmoRotating)
		finishTransformGizmoRotationDrag(false);
	_transformGizmoRequested = show;
	if (show)
		MainWindow::showStatusMessage(
			_explodedViewManualPlacementActive
				? tr("Manual exploded placement active: translate or rotate the selected meshes to refine the staged pose")
				: tr("Transform gizmo active: drag the corner box handle to scale uniformly"),
			5000);
	else
		MainWindow::showStatusMessage(QString(), 0);
	syncTransformGizmoToSelection();
	update();
}

bool GLWidget::beginExplodedViewManualPlacement(const QVector<QUuid>& selectionUuids)
{
	if (!_viewer || !_selectionManager)
		return false;

	_explodedViewManualPlacementSessionUuids.clear();
	_explodedViewManualSessionStartStates.clear();
	_explodedViewManualSessionStartMatrices.clear();
	_explodedViewManualSessionStartPivot = QVector3D(0.0f, 0.0f, 0.0f);
	_explodedViewManualSessionTranslationDelta = QVector3D(0.0f, 0.0f, 0.0f);
	_explodedViewManualSessionRotationQuat = QQuaternion(1.0f, 0.0f, 0.0f, 0.0f);
	_explodedViewManualSessionRotationEuler = QVector3D(0.0f, 0.0f, 0.0f);
	_explodedViewManualDragStartTranslationDelta = QVector3D(0.0f, 0.0f, 0.0f);
	_explodedViewManualDragStartRotationQuat = QQuaternion(1.0f, 0.0f, 0.0f, 0.0f);
	_explodedViewManualDragStartRotationEuler = QVector3D(0.0f, 0.0f, 0.0f);
	if (!selectionUuids.isEmpty())
	{
		for (const QUuid& uuid : selectionUuids)
		{
			TriangleMesh* mesh = getMeshByUuid(uuid);
			if (uuid.isNull() || !mesh)
				continue;

			_explodedViewManualPlacementSessionUuids.insert(uuid);
			_explodedViewManualSessionStartStates.insert(uuid, TransformState(
				mesh->getExplodedViewTranslation(),
				mesh->getExplodedViewRotation(),
				mesh->getExplodedViewScaling(),
				mesh->getExplodedViewRotationQuaternion()));
			_explodedViewManualSessionStartMatrices.insert(uuid, mesh->getExplodedViewTransformation());
			if (_explodedViewManualOriginalStates.contains(uuid))
				continue;

			_explodedViewManualOriginalStates.insert(uuid, TransformState(
				mesh->getExplodedViewTranslation(),
				mesh->getExplodedViewRotation(),
				mesh->getExplodedViewScaling(),
				mesh->getExplodedViewRotationQuaternion()));
		}
	}
	else
	{
		const QList<int> selectedIds = _selectionManager->getSelectedIds();
		if (selectedIds.isEmpty())
			return false;

		for (int id : selectedIds)
		{
			const QUuid uuid = getUuidByIndex(id);
			TriangleMesh* mesh = getMeshByIndex(id);
			if (uuid.isNull() || !mesh)
				continue;

			_explodedViewManualPlacementSessionUuids.insert(uuid);
			_explodedViewManualSessionStartStates.insert(uuid, TransformState(
				mesh->getExplodedViewTranslation(),
				mesh->getExplodedViewRotation(),
				mesh->getExplodedViewScaling(),
				mesh->getExplodedViewRotationQuaternion()));
			_explodedViewManualSessionStartMatrices.insert(uuid, mesh->getExplodedViewTransformation());
			if (_explodedViewManualOriginalStates.contains(uuid))
				continue;

			_explodedViewManualOriginalStates.insert(uuid, TransformState(
				mesh->getExplodedViewTranslation(),
				mesh->getExplodedViewRotation(),
				mesh->getExplodedViewScaling(),
				mesh->getExplodedViewRotationQuaternion()));
		}
	}

	if (_explodedViewManualPlacementSessionUuids.isEmpty())
		return false;

	_explodedViewManualPlacementActive = true;
	_explodedViewManualSessionStartPivot = computeTransformGizmoPivot();
	showTransformGizmoForSelection(true);
	emit explodedViewManualPlacementChanged();
	return true;
}

bool GLWidget::hasExplodedViewManualTransformChanges() const
{
	for (auto it = _explodedViewManualOriginalStates.cbegin(); it != _explodedViewManualOriginalStates.cend(); ++it)
	{
		TransformState currentState;
		if (_explodedViewManualPlacementSuppressed)
		{
			auto hiddenIt = _explodedViewManualHiddenStates.find(it.key());
			if (hiddenIt == _explodedViewManualHiddenStates.end())
				continue;
			currentState = hiddenIt.value();
		}
		else
		{
			const TriangleMesh* mesh = getMeshByUuid(it.key());
			if (!mesh)
				continue;

			currentState = TransformState(
				mesh->getExplodedViewTranslation(),
				mesh->getExplodedViewRotation(),
				mesh->getExplodedViewScaling(),
				mesh->getExplodedViewRotationQuaternion());
		}
		if (!transformStatesNearlyEqual(it.value(), currentState))
			return true;
	}

	return false;
}

QSet<QUuid> GLWidget::explodedViewManualPlacementUuids() const
{
	QSet<QUuid> uuids;
	for (auto it = _explodedViewManualOriginalStates.cbegin(); it != _explodedViewManualOriginalStates.cend(); ++it)
	{
		TransformState currentState;
		if (_explodedViewManualPlacementSuppressed)
		{
			auto hiddenIt = _explodedViewManualHiddenStates.find(it.key());
			if (hiddenIt == _explodedViewManualHiddenStates.end())
				continue;
			currentState = hiddenIt.value();
		}
		else
		{
			const TriangleMesh* mesh = getMeshByUuid(it.key());
			if (!mesh)
				continue;

			currentState = TransformState(
				mesh->getExplodedViewTranslation(),
				mesh->getExplodedViewRotation(),
				mesh->getExplodedViewScaling(),
				mesh->getExplodedViewRotationQuaternion());
		}
		if (!transformStatesNearlyEqual(it.value(), currentState))
			uuids.insert(it.key());
	}

	return uuids;
}

QVector3D GLWidget::explodedViewManualPlacementTranslationDelta() const
{
	return _explodedViewManualSessionTranslationDelta;
}

QVector3D GLWidget::explodedViewManualPlacementRotationDelta() const
{
	return _explodedViewManualSessionRotationEuler;
}

QMap<QUuid, TransformState> GLWidget::explodedViewManualStates() const
{
	QMap<QUuid, TransformState> states;
	for (auto it = _explodedViewManualOriginalStates.cbegin(); it != _explodedViewManualOriginalStates.cend(); ++it)
	{
		TransformState currentState;
		if (_explodedViewManualPlacementSuppressed)
		{
			auto hiddenIt = _explodedViewManualHiddenStates.find(it.key());
			if (hiddenIt == _explodedViewManualHiddenStates.end())
				continue;
			currentState = hiddenIt.value();
		}
		else
		{
			const TriangleMesh* mesh = getMeshByUuid(it.key());
			if (!mesh)
				continue;

			currentState = TransformState(
				mesh->getExplodedViewTranslation(),
				mesh->getExplodedViewRotation(),
				mesh->getExplodedViewScaling(),
				mesh->getExplodedViewRotationQuaternion());
		}
		if (!transformStatesNearlyEqual(it.value(), currentState))
			states.insert(it.key(), currentState);
	}

	return states;
}

void GLWidget::restoreExplodedViewManualStates(const QMap<QUuid, TransformState>& states)
{
	clearExplodedViewManualPlacement();
	if (states.isEmpty())
		return;

	_explodedViewManualOriginalStates.clear();
	for (auto it = states.cbegin(); it != states.cend(); ++it)
	{
		TriangleMesh* mesh = getMeshByUuid(it.key());
		if (!mesh)
			continue;

		_explodedViewManualOriginalStates.insert(it.key(), TransformState());
		applyExplodedViewTransformState(mesh, it.value(), false);

		const QVector3D savedExplosionOffset = mesh->explosionOffset();
		mesh->setExplosionOffset(QVector3D());
		mesh->fullUpdateRuntimeBounds();
		mesh->setExplosionOffset(savedExplosionOffset);
		mesh->setSceneRenderTransformFast(mesh->getSceneRenderTransform());
	}

	_shadowMapNeedsInitialization = true;
	update();
	emit explodedViewManualPlacementChanged();
}

void GLWidget::setExplodedViewManualPlacementTranslationDelta(const QVector3D& delta)
{
	if (!_explodedViewManualPlacementActive || _explodedViewManualSessionStartStates.isEmpty())
		return;

	_explodedViewManualSessionTranslationDelta = delta;
	applyExplodedViewManualPlacementSessionTransform();
	emit explodedViewManualPlacementChanged();
}

void GLWidget::setExplodedViewManualPlacementRotationDelta(const QVector3D& delta)
{
	if (!_explodedViewManualPlacementActive || _explodedViewManualSessionStartStates.isEmpty())
		return;

	_explodedViewManualSessionRotationEuler = delta;
	_explodedViewManualSessionRotationQuat = quaternionFromMeshEuler(delta);
	applyExplodedViewManualPlacementSessionTransform();
	emit explodedViewManualPlacementChanged();
}

void GLWidget::finishExplodedViewManualPlacement()
{
	QVector<QUuid> changedUuids;
	changedUuids.reserve(_explodedViewManualOriginalStates.size());
	for (auto it = _explodedViewManualOriginalStates.cbegin(); it != _explodedViewManualOriginalStates.cend(); ++it)
	{
		TriangleMesh* mesh = getMeshByUuid(it.key());
		if (!mesh)
			continue;

		const TransformState currentState(
			mesh->getExplodedViewTranslation(),
			mesh->getExplodedViewRotation(),
			mesh->getExplodedViewScaling(),
			mesh->getExplodedViewRotationQuaternion());
		if (!transformStatesNearlyEqual(it.value(), currentState))
			changedUuids.append(it.key());
	}

	showTransformGizmoForSelection(false);
	_explodedViewManualPlacementActive = false;
	_explodedViewManualPlacementSessionUuids.clear();
	_explodedViewManualSessionStartStates.clear();
	_explodedViewManualSessionStartMatrices.clear();
	_explodedViewManualSessionStartPivot = QVector3D(0.0f, 0.0f, 0.0f);
	_explodedViewManualSessionTranslationDelta = QVector3D(0.0f, 0.0f, 0.0f);
	_explodedViewManualSessionRotationQuat = QQuaternion(1.0f, 0.0f, 0.0f, 0.0f);
	_explodedViewManualSessionRotationEuler = QVector3D(0.0f, 0.0f, 0.0f);
	_explodedViewManualDragStartTranslationDelta = QVector3D(0.0f, 0.0f, 0.0f);
	_explodedViewManualDragStartRotationQuat = QQuaternion(1.0f, 0.0f, 0.0f, 0.0f);
	_explodedViewManualDragStartRotationEuler = QVector3D(0.0f, 0.0f, 0.0f);

	for (const QUuid& uuid : changedUuids)
	{
		if (TriangleMesh* mesh = getMeshByUuid(uuid))
		{
			const QVector3D savedExplosionOffset = mesh->explosionOffset();
			mesh->setExplosionOffset(QVector3D());
			mesh->fullUpdateRuntimeBounds();
			mesh->setExplosionOffset(savedExplosionOffset);
			mesh->setSceneRenderTransformFast(mesh->getSceneRenderTransform());
		}
	}

	const QList<int> selectedIds = _selectionManager ? _selectionManager->getSelectedIds() : QList<int>{};
	for (TriangleMesh* mesh : _meshStore)
	{
		if (mesh)
			mesh->deselect();
	}
	for (int id : selectedIds)
	{
		if (id >= 0 && id < static_cast<int>(_meshStore.size()) && _meshStore[id])
			_meshStore[id]->select();
	}

	_shadowMapNeedsInitialization = true;
	update();
	emit explodedViewManualPlacementChanged();
}

void GLWidget::clearExplodedViewManualPlacement()
{
	QHash<QUuid, QVector3D> savedExplosionOffsets;
	savedExplosionOffsets.reserve(static_cast<int>(_meshStore.size()));
	for (TriangleMesh* mesh : _meshStore)
	{
		if (!mesh)
			continue;

		savedExplosionOffsets.insert(mesh->uuid(), mesh->explosionOffset());
		mesh->setExplosionOffset(QVector3D());
	}

	_explodedViewManualPlacementActive = false;
	_explodedViewManualPlacementSessionUuids.clear();
	_explodedViewManualSessionStartStates.clear();
	_explodedViewManualSessionStartMatrices.clear();
	_explodedViewManualSessionStartPivot = QVector3D(0.0f, 0.0f, 0.0f);
	_explodedViewManualSessionTranslationDelta = QVector3D(0.0f, 0.0f, 0.0f);
	_explodedViewManualSessionRotationQuat = QQuaternion(1.0f, 0.0f, 0.0f, 0.0f);
	_explodedViewManualSessionRotationEuler = QVector3D(0.0f, 0.0f, 0.0f);
	_explodedViewManualDragStartTranslationDelta = QVector3D(0.0f, 0.0f, 0.0f);
	_explodedViewManualDragStartRotationQuat = QQuaternion(1.0f, 0.0f, 0.0f, 0.0f);
	_explodedViewManualDragStartRotationEuler = QVector3D(0.0f, 0.0f, 0.0f);
	_explodedViewManualHiddenStates.clear();
	_explodedViewManualPlacementSuppressed = false;
	showTransformGizmoForSelection(false);

	for (auto it = _explodedViewManualOriginalStates.cbegin(); it != _explodedViewManualOriginalStates.cend(); ++it)
	{
		TriangleMesh* mesh = getMeshByUuid(it.key());
		if (!mesh)
			continue;

		const TransformState& state = it.value();
		applyExplodedViewTransformState(mesh, state, false);
	}

	for (TriangleMesh* mesh : _meshStore)
	{
		if (!mesh)
			continue;

		mesh->setExplosionOffset(savedExplosionOffsets.value(mesh->uuid()));
		mesh->setSceneRenderTransformFast(mesh->getSceneRenderTransform());
	}

	_explodedViewManualOriginalStates.clear();
	_shadowMapNeedsInitialization = true;
	update();
	emit explodedViewManualPlacementChanged();
}

void GLWidget::showShadows(bool show)
{
	_shadowsEnabled = show;
	_fgShader->bind();
	_fgShader->setUniformValue("shadowsEnabled", _shadowsEnabled);
	update();
}

void GLWidget::showSelfShadows(bool show)
{
	_selfShadowsEnabled = show;		
	_fgShader->bind();
	_fgShader->setUniformValue("selfShadowsEnabled", _selfShadowsEnabled);
	update();
}

void GLWidget::showEnvironment(bool show)
{
	_envMapEnabled = show;
	_fgShader->bind();
	_fgShader->setUniformValue("envMapEnabled", _envMapEnabled);
	update();
}

void GLWidget::showSkyBox(bool show)
{
	_skyBoxEnabled = show;
	_fgShader->bind();
	_fgShader->setUniformValue("skyBoxEnabled", _skyBoxEnabled);
	update();
}

void GLWidget::blurSkyBox(bool blur)
{
	setSkyBoxBlurPercent(blur ? 100 : 0);
}

void GLWidget::setSkyBoxBlurPercent(int percent)
{
	_skyBoxBlurPercent = std::clamp(percent, 0, 100);
	update();
}

void GLWidget::showReflections(bool show)
{
	_reflectionsEnabled = show;
	_fgShader->bind();
	_fgShader->setUniformValue("reflectionsEnabled", _reflectionsEnabled);
	update();
}

void GLWidget::showFloor(bool show)
{
	setGroundMode(show ? GroundMode::Floor : GroundMode::None);
}

void GLWidget::setGroundMode(GroundMode mode)
{
	if (_groundMode == mode)
		return;

	_groundMode = mode;
	updateFloorPlane();
	update();
	emit floorShown(_groundMode == GroundMode::Floor);
}

void GLWidget::setFloorTexture(QImage img)
{
	_floorTexImage = convertToGLFormat(img);
	syncFloorPlaneAlbedoTexture();
}

void GLWidget::showFloorTexture(bool show)
{
	_floorTextureDisplayed = show;
	syncFloorPlaneAlbedoTexture();
}

void GLWidget::addToDisplay(TriangleMesh* mesh)
{	
	if(mesh == nullptr)
	{
		qDebug() << "Error: Attempted to add a null mesh to display.";
		return;
	}
	_meshStore.push_back(mesh);
	_displayedObjectsIds.push_back(static_cast<int>(_meshStore.size() - 1));

	//if(_progressiveLoadingEnabled)
		//_viewer->updateDisplayList();	
}

void GLWidget::removeFromDisplay(int index)
{
	TriangleMesh* mesh = _meshStore[index];
	_meshStore.erase(_meshStore.begin() + index);
	delete mesh;
	if (_meshStore.size() == 0)
	{
		_displayedObjectsIds.clear();
		_hiddenObjectsIds.clear();
		if (_visibleSwapped)
		{
			_visibleSwapped = false;
			emit visibleSwapped(_visibleSwapped);
		}
	}
	// If display list is empty, clear punctual lights
	if (_displayedObjectsIds.empty())
	{
		_originalParsedLights.clear();
		_currentRepositionedLights.clear();
		_animatedLightTransformSourceFile.clear();
		_animatedParsedLights.clear();
		_animatedLightVisibilitySourceFile.clear();
		_animatedLightVisibilityMask.clear();
		_animatedMeshVisibilitySourceFile.clear();
		_animatedHiddenMeshUuids.clear();
		glLights->setLights({});
		syncPunctualLightUniforms(0, false);
	}
}

void GLWidget::centerScreen(std::vector<int> selectedIDs)
{
	_centerScreenObjectIDs.clear();
	_centerScreenObjectIDs = selectedIDs;
	_selectionBoundingSphere.setCenter(0, 0, 0);
	_selectionBoundingSphere.setRadius(0.0);
	if (_displayedObjectsMemSize > MAX_MODEL_SIZE_BYTES)
		_lowResEnabled = true;
	int count = 0;
	for (int id : _centerScreenObjectIDs)
	{
		TriangleMesh* mesh = _meshStore.at(id);
		if (mesh)
		{
			if (count == 0)
				_selectionBoundingSphere = mesh->getBoundingSphere();
			else
				_selectionBoundingSphere.addSphere(mesh->getBoundingSphere());
		}
		count++;
	}
	if (!_animateCenterScreenTimer->isActive())
	{
		_keyboardNavTimer->stop();
		_animateCenterScreenTimer->start(5);
		_slerpStep = 0.0f;
	}
}

void GLWidget::select(int id)
{
	try {
		_meshStore.at(id)->select();
	}
	catch (const std::exception& ex) {
		std::cout << "Exception raised in GLWidget::select\n" << ex.what() << std::endl;
	}
}

void GLWidget::deselect(int id)
{
	try {
		_meshStore.at(id)->deselect();
	}
	catch (const std::exception& ex) {
		std::cout << "Exception raised in GLWidget::select\n" << ex.what() << std::endl;
	}
}

bool GLWidget::loadAssImpModel(const QString& fileName, const UVMethod& uvMethod, QString& error, bool progressiveLoading)
{
	_progressiveLoadingEnabled = progressiveLoading;
	_cancelRequested = false;
	_loadCancelled = false;
	_pendingSceneUuids.clear();
	const bool hadExistingMeshes = !_meshStore.empty();
	MainWindow::clearFileLoadCancel();
	bool success = false;

	makeCurrent();
	QString displayFileName = fileName;
	if (fileName.length() > 100)
	{
		// Extract just the filename from the full path
		QString fileOnly = fileName.section('/', -1);
		// Calculate how much of the path to truncate
		int remainingLength = 100 - fileOnly.length() - 3; // 3 for "..."
		QString truncatedPath = fileName.left(remainingLength).section('/', 0, -2);
		displayFileName = truncatedPath + "/.../" + fileOnly;
	}
	MainWindow::showStatusMessage(tr("Reading file: ") + displayFileName);
	MainWindow::showProgressBar();
	if (_assimpModelLoader)
	{
		AssImpModelLoader* loadingWorker = new AssImpModelLoader();
		loadingWorker->setUVDecisionCallback(
			[this](int totalTriangles, UVMethod currentMethod) -> UVMethod {
				if (QThread::currentThread() != this->thread())
				{
					UVMethod result = currentMethod;
					QMetaObject::invokeMethod(this, [this, totalTriangles, currentMethod, &result]() {
						result = promptLargeModelUVDecision(totalTriangles, currentMethod);
						}, Qt::BlockingQueuedConnection);
					return result;
				}
				return promptLargeModelUVDecision(totalTriangles, currentMethod);
			});

		QEventLoop waitLoop;
		QThread loadingThread;
		bool loadingCompleted = false;

		QMetaObject::Connection finishedConnection = connect(
			loadingWorker,
			&AssImpModelLoader::loadingFinished,
			this,
			[this, loadingWorker, &success, &error, &loadingCompleted, &waitLoop](bool successFlag, const aiScene* /*scene*/) {
				loadingCompleted = true;
				success = successFlag;
				error = loadingWorker->getErrorMessage();
				if (error == "Model loading cancelled by user.")
				{
					_loadCancelled = true;
				}
				waitLoop.quit();
			},
			Qt::QueuedConnection);

		QMetaObject::Connection cancelledConnection = connect(
			loadingWorker,
			&AssImpModelLoader::loadingCancelled,
			this,
			[this, &error]() {
				_loadCancelled = true;
				error = "Model loading cancelled by user.";
			},
			Qt::QueuedConnection);

		QMetaObject::Connection fileProgressConnection = connect(
			loadingWorker,
			&AssImpModelLoader::fileReadProcessed,
			this,
			&GLWidget::showFileReadingProgress,
			Qt::QueuedConnection);

		QMetaObject::Connection meshProgressConnection = connect(
			loadingWorker,
			&AssImpModelLoader::verticesProcessed,
			this,
			&GLWidget::showMeshLoadingProgress,
			Qt::QueuedConnection);

		QMetaObject::Connection nodeProgressConnection = connect(
			loadingWorker,
			&AssImpModelLoader::nodeMeshProgressUpdated,
			this,
			&GLWidget::showNodeMeshLoadingProgress,
			Qt::QueuedConnection);

		QMetaObject::Connection batchConnection = connect(
			loadingWorker,
			&AssImpModelLoader::meshBatchReady,
			this,
			&GLWidget::onMeshBatchReady,
			Qt::BlockingQueuedConnection);

		QMetaObject::Connection cancelRequestConnection = connect(
			this,
			&GLWidget::loadingAssImpModelCancelled,
			loadingWorker,
			&AssImpModelLoader::cancelLoading,
			Qt::QueuedConnection);

		QMetaObject::Connection lightsConnection = connect(
			loadingWorker,
			&AssImpModelLoader::lightsLoaded,
			this,
			[this](const GltfLightData& lights) {
				setParsedLights(lights);
			},
			Qt::QueuedConnection);

		loadingWorker->moveToThread(&loadingThread);

		loadingThread.start();
		QMetaObject::invokeMethod(
			loadingWorker,
			[loadingWorker, fileName, uvMethod, progressiveLoading]() {
				loadingWorker->setUVGenerationMethod(uvMethod);
				loadingWorker->loadModel(fileName.toStdString(), progressiveLoading);
			},
			Qt::QueuedConnection);

		waitLoop.exec();

		loadingThread.quit();
		loadingThread.wait();

		if (!loadingCompleted)
		{
			success = false;
			if (error.isEmpty())
			{
				error = tr("Model loading did not finish correctly.");
			}
		}

		makeCurrent();
		std::vector<AssImpMeshData> meshes = loadingWorker->getMeshes();
		if (meshes.empty())
		{
			if (error.isEmpty())
			{
				error = loadingWorker->getErrorMessage();
			}
			success = false;
			if (error == "Model loading cancelled by user.")
			{
				_loadCancelled = true;
			}
		}
		else
		{
			success = true;
			if (!_progressiveLoadingEnabled)
			{
				for (const AssImpMeshData& meshData : meshes)
				{
					AssImpMesh* mesh = createMeshFromData(meshData);
					addToDisplay(mesh);
					_pendingSceneUuids.append(mesh->uuid());
				}
			}
		}

		_assimpScene = loadingWorker->getScene();
		_globalSceneTransform = loadingWorker->getGlobalSceneTransform();
		const bool autoOrientCameraEnabled = loadingWorker->isAutoOrientActive();
		const SceneUpAxis sceneUpAxis = loadingWorker->sceneUpAxis();
		if (_assimpScene)
		{
			// Populate the SceneGraph before the scene is deep-copied and merged,
			// while the original aiNode* tree is still intact and unmodified.
			if (success && !_pendingSceneUuids.isEmpty())
			{
				_viewer->sceneGraph()->appendFromScene(
					_assimpScene, fileName, _pendingSceneUuids,
					SceneUtils::glmToAiMatrix(_globalSceneTransform),
					loadingWorker->wasAutoOrientApplied(),
					loadingWorker->wasAutoScaleApplied());

				// Register per-file punctual lights (if any) for this file.
				// setLightData() emits lightDataChanged() which triggers the
				// PunctualLightsPanel to refresh and GLWidget to rebuild the GPU list.
				if (!_pendingLightData.isEmpty())
				{
					_pendingLightData.sourceFile = fileName; // authoritative path
					_viewer->sceneGraph()->setLightData(fileName, _pendingLightData);
				}

				// Register KHR_materials_variants data (if any) for this file.
				const GltfVariantData& vd = loadingWorker->getVariantData();
				if (!vd.isEmpty())
					_viewer->sceneGraph()->setVariantData(fileName, vd);

				const GltfAnimationData& ad = loadingWorker->getAnimationData();
				const bool preservesNodeTransforms =
					std::any_of(meshes.begin(), meshes.end(),
						[](const AssImpMeshData& meshData)
						{
							return meshData.preserveNodeTransform;
						});
				if (!ad.isEmpty() || ad.hasSkinning)
				{
					_viewer->sceneGraph()->setAnimationData(fileName, ad);
					_runtimeAnimationsByFile.remove(fileName);
					if (_activeAnimationFile == fileName)
					{
						_animationTimer->stop();
						_animationPlaying = false;
						_activeAnimationFile.clear();
						_activeAnimationClip = -1;
						_animationCurrentTimeSeconds = 0.0;
					}
					syncFileNodeTransforms(fileName);
					if (!ad.clips.isEmpty())
						setActiveAnimation(fileName, 0);
				}
				else if (preservesNodeTransforms)
				{
					_runtimeAnimationsByFile.remove(fileName);
					syncFileNodeTransforms(fileName);
				}

				// Register glTF camera data (if any) for this file.
				const GltfCameraData& cd = loadingWorker->getCameraData();
				if (!cd.isEmpty())
					_viewer->sceneGraph()->setGltfCameraData(fileName, cd);
			}
			_pendingSceneUuids.clear();

			// Record how many meshes were in _globalScene BEFORE merging.
			// Each newly loaded TriangleMesh has a sceneIndex relative to its
			// own per-model aiScene (0-based).  After mergeScene() appends the
			// new model's meshes starting at oldMeshCount, those per-model
			// indices become stale.  We fix them up here so that every
			// TriangleMesh in _meshStore always holds its correct position in
			// _globalScene->mMeshes[], which syncSceneToMeshStore() relies on.
			const unsigned int oldMeshCount =
				_globalScene ? _globalScene->mNumMeshes : 0u;

			aiScene* copiedScene = SceneUtils::deepCopyScene(_assimpScene);
			SceneUtils::mergeScene(&_globalScene, copiedScene);

			// Offset the sceneIndices of the meshes that were just added.
			// They are the last meshes.size() entries in _meshStore because
			// addToDisplay() always appends.
			if (oldMeshCount > 0 && !meshes.empty())
			{
				const int newCount = static_cast<int>(meshes.size());
				const int storeSize = static_cast<int>(_meshStore.size());
				const int firstNew  = storeSize - newCount;
				for (int i = firstNew; i < storeSize; ++i)
				{
					TriangleMesh* tm = _meshStore[i];
					if (tm && tm->getSceneIndex() >= 0)
						tm->setSceneIndex(
							static_cast<int>(oldMeshCount) + tm->getSceneIndex());
				}
			}
		}
		_assimpScene = nullptr;

		if (success && autoOrientCameraEnabled)
		{
			const bool sceneIsZUp = sceneUpAxisIsZUp(sceneUpAxis);
			if (!hadExistingMeshes)
			{
				applyAutoOrientCameraConvention(sceneUpAxis);
			}
			else if (sceneIsZUp != _cameraUpAxisZUp)
			{
				warnOnConflictingImportedSceneUpAxis(fileName, sceneUpAxis);
			}
		}

		disconnect(finishedConnection);
		disconnect(cancelledConnection);
		disconnect(fileProgressConnection);
		disconnect(meshProgressConnection);
		disconnect(nodeProgressConnection);
		disconnect(batchConnection);
		disconnect(cancelRequestConnection);
		disconnect(lightsConnection);

		loadingWorker->moveToThread(this->thread());
		delete loadingWorker;
	}

	if (_loadCancelled)
	{
		if (!_meshStore.empty())
		{
			success = true;
		}
		if (_meshStore.empty())
		{
			MainWindow::showStatusMessage(tr("Model loading cancelled"), 3000);
		}
		else
		{
			MainWindow::showStatusMessage(
				tr("Model loading cancelled after importing %1 meshes").arg(_meshStore.size()),
				4000);
		}
	}
	else
	{
		MainWindow::showStatusMessage("");
	}

	MainWindow::setProgressValue(0);
	MainWindow::hideProgressBar();
	_cancelRequested = false;

	return success;
}

bool GLWidget::generateUVsForMeshes(const std::vector<int>& ids, const UVMethod& uvMethod, const UVConfig& uvConfig, QString& error)
{
	int meshCnt = ids.size();
	if (meshCnt == 0)
		return false;
	bool success = true;
	makeCurrent();
	MainWindow::showProgressBar(false);
	MainWindow::setProgressValue(0);
	MainWindow::showStatusMessage(tr("Generating UVs for %1 meshes").arg(meshCnt));
	float count = 0;
	for (int id : ids)
	{
		try
		{
			AssImpMesh* mesh = dynamic_cast<AssImpMesh*>(_meshStore.at(id));
			if (mesh)
			{								
				success = _assimpModelLoader->regenerateUVs(mesh, uvMethod, uvConfig);
				if (success)
				{
					MainWindow::showStatusMessage(tr("Updating mesh: ") + mesh->getName());
					int progress = static_cast<int>((++count / meshCnt) * 100.0f);					
					MainWindow::setProgressValue(progress);					
				}
				else
				{
					error = _assimpModelLoader->getErrorMessage();
					success = false;
					break;
				}
			}
		}
		catch (const std::exception& ex)
		{
			std::cout << "Exception in GLWidget::generateUVs\n" << ex.what() << std::endl;
		}
	}
	MainWindow::showStatusMessage("");
	MainWindow::setProgressValue(0);
	MainWindow::hideProgressBar();
	return success;
}


void GLWidget::showFileReadingProgress(float percent)
{
	MainWindow::setProgressValue((int)((float)percent * 100.0f));
	makeCurrent();
}

void GLWidget::showMeshLoadingProgress(float /*percent*/)
{
	makeCurrent();
}

void GLWidget::showNodeMeshLoadingProgress(int processedNodes, int totalNodes, int processedMeshes, int totalMeshes, bool uvProcessed)
{
	QString statusMessage = (uvProcessed) ? tr("Generating UVs... ") : "";
	statusMessage += QString(tr("Processing node: %1/%2  Mesh: %3/%4"))
		.arg(processedNodes)
		.arg(totalNodes)
		.arg(processedMeshes)
		.arg(totalMeshes);
	MainWindow::showStatusMessage(statusMessage);

	if (totalNodes > 0)
	{
		MainWindow::setProgressValue(static_cast<int>((static_cast<float>(processedNodes) / static_cast<float>(totalNodes)) * 100.0f));
	}

	makeCurrent();
}

void GLWidget::swapVisible(bool checked)
{
	_visibleSwapped = checked;
	recalculateVisibleSceneStats(false);
	triggerShadowRecomputation();
	updateFloorPlane();
	fitAll();

	emit visibleSwapped(checked);
}

void GLWidget::cancelAssImpModelLoading()
{
	if (_cancelRequested)
		return;

	_cancelRequested = true;
	MainWindow::requestFileLoadCancel();
	MainWindow::setCancelButtonEnabled(false);
	MainWindow::setCancelButtonText(tr("Cancelling..."));
	MainWindow::showStatusMessage(tr("Cancelling model load..."));
	emit loadingAssImpModelCancelled();	
}

void GLWidget::invertADSOpacityTexMap(const std::vector<int>& ids, const bool& inverted)
{
	for (int id : ids)
	{
		try
		{
			TriangleMesh* mesh = _meshStore[id];
			mesh->invertOpacityADSMap(inverted);
		}
		catch (const std::exception& ex)
		{
			std::cout << "Exception in GLWidget::invertADSOpacityTexMap\n" << ex.what() << std::endl;
		}
	}
}

void GLWidget::setMaterialToObjects(const std::vector<int>& ids, const GLMaterial& mat)
{
	for (int id : ids)
	{
		try
		{
			TriangleMesh* mesh = _meshStore[id];
			mesh->setMaterial(mat);
			if (mat.hasTransmission() || mat.diffuseTransmissionFactor() > 0.0f)
				setTransmissionEnabled(true);
		}
		catch (const std::exception& ex)
		{
			std::cout << "Exception in GLWidget::setMaterialToObjects\n" << ex.what() << std::endl;
		}
	}
}

void GLWidget::setTexturesToObjects(const std::vector<int>& ids, const GLMaterial& mat)
{
	for (int id : ids)
	{
		try
		{
			TriangleMesh* mesh = _meshStore[id];
			GLMaterial resolved = resolveMaterialTextures(this, mat);
			mesh->setTextureMaps(resolved);
			mesh->invertOpacityADSMap(resolved.isOpacityMapInverted());
			mesh->invertOpacityPBRMap(resolved.isOpacityMapInverted());
		}
		catch (const std::exception& ex)
		{
			std::cout << "Exception in GLWidget::setTexturesToObjects\n" << ex.what() << std::endl;
		}
	}
}

void GLWidget::synchronizeTextureCache(const GLMaterial* material, GLMaterial::TextureType type)
{
	if (!material) return;

	const GLMaterial::Texture& matTex = material->texture(type);
	if (matTex.path.empty()) return;

	TextureSamplerSettings samplers{
		matTex.wrapS,
		matTex.wrapT,
		matTex.minFilter,
		matTex.magFilter
	};

	makeCurrent();
	getOrLoadTextureCached(QString::fromStdString(matTex.path), samplers);
}

void GLWidget::clearTextureCache()
{
	for (auto& entry : _texCache)
	{
		if (entry.second.lastGPUTexture != 0)
		{
			glDeleteTextures(1, &entry.second.lastGPUTexture);
		}
	}
	_texCache.clear();
	_texRefCount.clear();
}

void GLWidget::setPBRAlbedoColor(const std::vector<int>& ids, const QColor& col)
{
	for (int id : ids)
	{
		try
		{
			TriangleMesh* mesh = _meshStore[id];
			mesh->setPBRAlbedoColor(col.red() / 256.0f, col.green() / 256.0f, col.blue() / 256.0f);
		}
		catch (const std::exception& ex)
		{
			std::cout << "Exception in GLWidget::setPBRAlbedoColor\n" << ex.what() << std::endl;
		}
	}
}

void GLWidget::setPBRMetallic(const std::vector<int>& ids, const float& val)
{
	for (int id : ids)
	{
		try
		{
			TriangleMesh* mesh = _meshStore[id];
			mesh->setPBRMetallic(val);
		}
		catch (const std::exception& ex)
		{
			std::cout << "Exception in GLWidget::setPBRMetallic\n" << ex.what() << std::endl;
		}
	}
}

void GLWidget::setPBRRoughness(const std::vector<int>& ids, const float& val)
{
	for (int id : ids)
	{
		try
		{
			TriangleMesh* mesh = _meshStore[id];
			mesh->setPBRRoughness(val);
		}
		catch (const std::exception& ex)
		{
			std::cout << "Exception in GLWidget::setPBRRoughness\n" << ex.what() << std::endl;
		}
	}
}

void GLWidget::clearPBRTexMaps(const std::vector<int>& ids)
{
	for (int id : ids)
	{
		try
		{
			TriangleMesh* mesh = _meshStore[id];
			mesh->clearAllPBRMaps();
		}
		catch (const std::exception& ex)
		{
			std::cout << "Exception in GLWidget::clearPBRTextures\n" << ex.what() << std::endl;
		}
	}
}

void GLWidget::clearPBRAlbedoTexMap(const std::vector<int>& ids)
{
	for (int id : ids)
	{
		try
		{
			TriangleMesh* mesh = _meshStore[id];
			unsigned int texId = mesh->getAlbedoPBRMap();
			mesh->clearAlbedoPBRMap();
			releaseTexture(texId);
		}
		catch (const std::exception& ex)
		{
			std::cout << "Exception in GLWidget::clearAlbedoTexture\n" << ex.what() << std::endl;
		}
	}
}

void GLWidget::clearPBRMetallicTexMap(const std::vector<int>& ids)
{
	for (int id : ids)
	{
		try
		{
			TriangleMesh* mesh = _meshStore[id];
			unsigned int texId = mesh->getMetallicPBRMap();
			mesh->clearMetallicPBRMap();
			releaseTexture(texId);
		}
		catch (const std::exception& ex)
		{
			std::cout << "Exception in GLWidget::clearMetallicTexture\n" << ex.what() << std::endl;
		}
	}
}

void GLWidget::clearPBRRoughnessTexMap(const std::vector<int>& ids)
{
	for (int id : ids)
	{
		try
		{
			TriangleMesh* mesh = _meshStore[id];
			unsigned int texId = mesh->getRoughnessPBRMap();
			mesh->clearRoughnessPBRMap();
			releaseTexture(texId);
		}
		catch (const std::exception& ex)
		{
			std::cout << "Exception in GLWidget::clearRoughnessTexture\n" << ex.what() << std::endl;
		}
	}
}

void GLWidget::clearPBRNormalTexMap(const std::vector<int>& ids)
{
	for (int id : ids)
	{
		try
		{
			TriangleMesh* mesh = _meshStore[id];
			unsigned int texId = mesh->getNormalPBRMap();
			mesh->clearNormalPBRMap();
			releaseTexture(texId);
		}
		catch (const std::exception& ex)
		{
			std::cout << "Exception in GLWidget::clearNormalTexture\n" << ex.what() << std::endl;
		}
	}
}

void GLWidget::clearPBRAOTexMap(const std::vector<int>& ids)
{
	for (int id : ids)
	{
		try
		{
			TriangleMesh* mesh = _meshStore[id];
			unsigned int texId = mesh->getAOPBRMap();
			mesh->clearAOPBRMap();
			releaseTexture(texId);
		}
		catch (const std::exception& ex)
		{
			std::cout << "Exception in GLWidget::clearAOTexture\n" << ex.what() << std::endl;
		}
	}
}

void GLWidget::invertPBROpacityTexMap(const std::vector<int>& ids, const bool& inverted)
{
	for (int id : ids)
	{
		try
		{
			TriangleMesh* mesh = _meshStore[id];
			mesh->invertOpacityPBRMap(inverted);
		}
		catch (const std::exception& ex)
		{
			std::cout << "Exception in GLWidget::invertPBROpacityTexMap\n" << ex.what() << std::endl;
		}
	}
}

void GLWidget::clearPBROpacityTexMap(const std::vector<int>& ids)
{
	for (int id : ids)
	{
		try
		{
			TriangleMesh* mesh = _meshStore[id];
			unsigned int texId = mesh->getOpacityPBRMap();
			mesh->clearOpacityPBRMap();
			releaseTexture(texId);
		}
		catch (const std::exception& ex)
		{
			std::cout << "Exception in GLWidget::clearPBROpacityTexMap\n" << ex.what() << std::endl;
		}
	}
}

void GLWidget::clearPBRHeightTexMap(const std::vector<int>& ids)
{
	for (int id : ids)
	{
		try
		{
			TriangleMesh* mesh = _meshStore[id];
			unsigned int texId = mesh->getHeightPBRMap();
			mesh->clearHeightPBRMap();
			releaseTexture(texId);
		}
		catch (const std::exception& ex)
		{
			std::cout << "Exception in GLWidget::clearHeightTexture\n" << ex.what() << std::endl;
		}
	}
}

void GLWidget::clearPBRTransmissionTexMap(const std::vector<int>& ids)
{
	for (int id : ids)
	{
		try
		{
			TriangleMesh* mesh = _meshStore[id];
			unsigned int texId = mesh->getTransmissionPBRMap();
			mesh->clearTransmissionPBRMap();
			releaseTexture(texId);
		}
		catch (const std::exception& ex)
		{
			std::cout << "Exception in GLWidget::clearTransmissionTexture\n" << ex.what() << std::endl;
		}
	}
}

void GLWidget::clearPBRIORTexMap(const std::vector<int>& ids)
{
	for (int id : ids)
	{
		try
		{
			TriangleMesh* mesh = _meshStore[id];
			unsigned int texId = mesh->getIORPBRMap();
			mesh->clearIORPBRMap();
			releaseTexture(texId);
		}
		catch (const std::exception& ex)
		{
			std::cout << "Exception in GLWidget::clearIORTexture\n" << ex.what() << std::endl;
		}
	}
}


void GLWidget::clearPBRSheenColorTexMap(const std::vector<int>& ids)
{
	for (int id : ids)
	{
		try
		{
			TriangleMesh* mesh = _meshStore[id];
			unsigned int texId = mesh->getSheenColorPBRMap();
			mesh->clearSheenColorPBRMap();
			releaseTexture(texId);
		}
		catch (const std::exception& ex)
		{
			std::cout << "Exception in GLWidget::clearSheenColorTexture\n" << ex.what() << std::endl;
		}
	}
}

void GLWidget::clearPBRSheenRoughnessTexMap(const std::vector<int>& ids)
{
	for (int id : ids)
	{
		try
		{
			TriangleMesh* mesh = _meshStore[id];
			unsigned int texId = mesh->getSheenRoughnessPBRMap();
			mesh->clearSheenRoughnessPBRMap();
			releaseTexture(texId);
		}
		catch (const std::exception& ex)
		{
			std::cout << "Exception in GLWidget::clearSheenRoughnessTexture\n" << ex.what() << std::endl;
		}
	}
}

void GLWidget::clearPBRClearcoatTexMap(const std::vector<int>& ids)
{
	for (int id : ids)
	{
		try
		{
			TriangleMesh* mesh = _meshStore[id];
			unsigned int texId = mesh->getClearcoatPBRMap();
			mesh->clearClearcoatPBRMap();
			releaseTexture(texId);
		}
		catch (const std::exception& ex)
		{
			std::cout << "Exception in GLWidget::clearClearcoatTexture\n" << ex.what() << std::endl;
		}
	}
}

void GLWidget::clearPBRClearcoatRoughnessTexMap(const std::vector<int>& ids)
{
	for (int id : ids)
	{
		try
		{
			TriangleMesh* mesh = _meshStore[id];
			unsigned int texId = mesh->getClearcoatRoughnessPBRMap();
			mesh->clearClearcoatRoughnessPBRMap();
			releaseTexture(texId);
		}
		catch (const std::exception& ex)
		{
			std::cout << "Exception in GLWidget::clearClearcoatRoughnessTexture\n" << ex.what() << std::endl;
		}
	}
}

void GLWidget::clearPBRClearcoatNormalTexMap(const std::vector<int>& ids)
{
	for (int id : ids)
	{
		try
		{
			TriangleMesh* mesh = _meshStore[id];
			unsigned int texId = mesh->getClearcoatNormalPBRMap();
			mesh->clearClearcoatNormalPBRMap();
			releaseTexture(texId);
		}
		catch (const std::exception& ex)
		{
			std::cout << "Exception in GLWidget::clearClearcoatNormalTexture\n" << ex.what() << std::endl;
		}
	}
}

void GLWidget::setTransformation(const std::vector<int>& ids, const QVector3D& trans, const QVector3D& rot, const QVector3D& scale)
{
	for (int id : ids)
	{
		try
		{
			TriangleMesh* mesh = _meshStore[id];
			mesh->setTranslation(trans);
			mesh->setRotation(rot);
			mesh->setScaling(scale);
		}
		catch (const std::exception& ex)
		{
			std::cout << "Exception in GLWidget::setTransformation\n" << ex.what() << std::endl;
		}
	}

	// Lights/cameras follow automatically: updatePunctualLights() derives the
	// per-file user transform straight from the meshes' TRS state.
	recalculateVisibleSceneStats(false);
	if (_explodedViewPanel && _explodedViewPanel->isVisible())
		updateExplosion();
	updatePunctualLights();
	triggerShadowRecomputation();
	updateFloorPlane();
	fitAll();
}

void GLWidget::resetTransformation(const std::vector<int>& ids)
{
	for (int id : ids)
	{
		try
		{
			TriangleMesh* mesh = _meshStore[id];
			mesh->resetTransformations();			
		}
		catch (const std::exception& ex)
		{
			std::cout << "Exception in GLWidget::resetTransformation\n" << ex.what() << std::endl;
		}
	}

	// Reset light offsets when model transformations are reset
	_lightOffsetX = 0.0f;
	_lightOffsetY = 0.0f;
	_lightOffsetZ = 0.0f;

	recalculateVisibleSceneStats(false);
	if (_explodedViewPanel && _explodedViewPanel->isVisible())
		updateExplosion();
	updatePunctualLights();
	fitAll();
	triggerShadowRecomputation();
}

void GLWidget::applyTransforms(const QMap<int, TransformState>& transforms, bool fitView)
{
	if (transforms.isEmpty())
		return;

	makeCurrent();

	// Apply all transformations to individual meshes
	for (auto it = transforms.begin(); it != transforms.end(); ++it)
	{
		int index = it.key();
		const TransformState& state = it.value();

		if (index >= 0 && index < static_cast<int>(_meshStore.size()))
		{
			TriangleMesh* mesh = _meshStore[index];
			if (mesh)
			{
				mesh->setTranslation(state.translation);
				if (state.hasExactRotation)
					mesh->setRotationQuaternion(state.rotationQuat, state.rotation);
				else
					mesh->setRotation(state.rotation);
				mesh->setScaling(state.scale);
			}
		}
	}

	// Check if this is a model-level transformation
	// (all meshes are being transformed)
	bool isModelLevelTransform = (transforms.size() == static_cast<int>(_meshStore.size()));

	// Lights/cameras follow automatically: updatePunctualLights() derives the
	// per-file user transform straight from the meshes' TRS state.

	// Update all dependent systems once
	recalculateVisibleSceneStats(false);
	if (_explodedViewPanel && _explodedViewPanel->isVisible())
		updateExplosion();
	updatePunctualLights();
	if (isModelLevelTransform && isGltfCameraActive() && _viewer)
	{
		const GltfCameraData camData =
			_viewer->sceneGraph()->gltfCameraDataForFile(_activeGltfCameraFile);
		if (_activeGltfCameraIndex >= 0 && _activeGltfCameraIndex < camData.cameras.size())
		{
			const GltfCameraEntry& cam = camData.cameras[_activeGltfCameraIndex];
			// Always apply the static transform first; then let applyAnimationPose
			// override it for animated cameras (same logic as activateGltfCamera).
			applyGltfCameraEntryTransform(cam);
			const bool animationOwnsThisFile =
				_activeAnimationFile == _activeGltfCameraFile && _activeAnimationClip >= 0;
			if (animationOwnsThisFile)
			{
				applyAnimationPose(_activeAnimationFile, _activeAnimationClip, _animationCurrentTimeSeconds);
			}
		}
	}
	triggerShadowRecomputation();
	updateFloorPlane();
	if (fitView && !isGltfCameraActive())
	{
		fitAll();
	}

	doneCurrent();
}

void GLWidget::createShaderPrograms()
{
    const QString path = PathUtils::getDataDirectory() + "/";
	// Foreground objects shader program
	// Per fragment lighting
	_fgShader = std::make_unique<ShaderProgram>(); _fgShader->setObjectName("_fgShader");
    _fgShader->loadCompileAndLinkShaderFromFile(path + "shaders/main_scene.vert",
        path + "shaders/main_scene.frag");
	// Flat-shading variant: same vert + frag shaders, plus a geometry shader that
	// computes the true face normal via cross(edge0,edge1) and writes it into
	// v_flatNormal / v_reflectionFlatNormal for all three output vertices.
	// Used for DisplayMode::FLATSHADED on GL_TRIANGLES meshes only.
	_fgFlatShader = std::make_unique<ShaderProgram>(); _fgFlatShader->setObjectName("_fgFlatShader");
	_fgFlatShader->loadCompileAndLinkShaderFromFile(path + "shaders/main_scene_flat.vert",
	    path + "shaders/main_scene.frag",
	    path + "shaders/main_scene_flat.geom");
	// Axis
	_axisShader = std::make_unique<ShaderProgram>(); _axisShader->setObjectName("_axisShader");
	_axisShader->loadCompileAndLinkShaderFromFile(path + "shaders/axis.vert", path + "shaders/axis.frag");
	// Vertex Normal
	_vertexNormalShader = std::make_unique<ShaderProgram>(); _vertexNormalShader->setObjectName("_vertexNormalShader");
	_vertexNormalShader->loadCompileAndLinkShaderFromFile(path + "shaders/vertex_normal.vert",
        path + "shaders/vertex_normal.frag", path + "shaders/vertex_normal.geom");
	// Face Normal
	_faceNormalShader = std::make_unique<ShaderProgram>(); _faceNormalShader->setObjectName("_faceNormalShader");
	_faceNormalShader->loadCompileAndLinkShaderFromFile(path + "shaders/face_normal.vert",
        path + "shaders/face_normal.frag", path + "shaders/face_normal.geom");
	// Shadow mapping
	_shadowMappingShader = std::make_unique<ShaderProgram>(); _shadowMappingShader->setObjectName("_shadowMappingShader");
	_shadowMappingShader->loadCompileAndLinkShaderFromFile(path + "shaders/shadow_mapping_depth.vert",
        path + "shaders/shadow_mapping_depth.frag");
	// Sky Box
	_skyBoxShader = std::make_unique<ShaderProgram>(); _skyBoxShader->setObjectName("_skyBoxShader");
	_skyBoxShader->loadCompileAndLinkShaderFromFile(path + "shaders/skybox.vert", path + "shaders/skybox.frag");
	// Grid
	_gridShader = std::make_unique<ShaderProgram>(); _gridShader->setObjectName("_gridShader");
	_gridShader->loadCompileAndLinkShaderFromFile(path + "shaders/grid.vert", path + "shaders/grid.frag");
	// Irradiance Map (now uses fullscreen triangle)
	_irradianceShader = std::make_unique<ShaderProgram>(); _irradianceShader->setObjectName("_irradianceShader");
	_irradianceShader->loadCompileAndLinkShaderFromFile(path + "shaders/fullscreen_triangle.vert", path + "shaders/irradiance_convolution.frag");
	// Prefilter Map (now uses fullscreen triangle)
	_prefilterShader = std::make_unique<ShaderProgram>(); _prefilterShader->setObjectName("_prefilterShader");
	_prefilterShader->loadCompileAndLinkShaderFromFile(path + "shaders/fullscreen_triangle.vert", path + "shaders/prefilter.frag");
	// Sheen/Charlie Prefilter Map
	_sheenPrefilterShader = std::make_unique<ShaderProgram>(); _sheenPrefilterShader->setObjectName("_sheenPrefilterShader");
	_sheenPrefilterShader->loadCompileAndLinkShaderFromFile(path + "shaders/fullscreen_triangle.vert", path + "shaders/prefilter_charlie.frag");
	// BRDF LUT Map
	_brdfShader = std::make_unique<ShaderProgram>(); _brdfShader->setObjectName("_brdfShader");
	_brdfShader->loadCompileAndLinkShaderFromFile(path + "shaders/brdf.vert", path + "shaders/brdf.frag");
	// Text shader program
	_textShader = std::make_unique<ShaderProgram>(); _textShader->setObjectName("_textShader");
	_textShader->loadCompileAndLinkShaderFromFile(path + "shaders/text.vert", path + "shaders/text.frag");
	// Background gradient shader program
	_bgShader = std::make_unique<ShaderProgram>(); _bgShader->setObjectName("_bgShader");
	_bgShader->loadCompileAndLinkShaderFromFile(path + "shaders/background.vert", path + "shaders/background.frag");
	// Background split shader program
	_bgSplitShader = std::make_unique<ShaderProgram>(); _bgSplitShader->setObjectName("_bgSplitShader");
	_bgSplitShader->loadCompileAndLinkShaderFromFile(path + "shaders/splitScreen.vert", path + "shaders/splitScreen.frag");
	// Light Cube shader program
	_lightCubeShader = std::make_unique<ShaderProgram>(); _lightCubeShader->setObjectName("_lightCubeShader");
	_lightCubeShader->loadCompileAndLinkShaderFromFile(path + "shaders/light_cube.vert", path + "shaders/light_cube.frag");
	// View Cube shader program
	_viewCubeShader = std::make_unique<ShaderProgram>(); _viewCubeShader->setObjectName("_viewCubeShader");
	_viewCubeShader->loadCompileAndLinkShaderFromFile(path + "shaders/viewcube.vert", path + "shaders/viewcube.frag");
	// View Cube label shader program
	_viewCubeLabelShader = std::make_unique<ShaderProgram>(); _viewCubeLabelShader->setObjectName("_viewCubeLabelShader");
	_viewCubeLabelShader->loadCompileAndLinkShaderFromFile(path + "shaders/viewcube_label.vert", path + "shaders/viewcube_label.frag");
	// Clipping Plane shader program
	_clippingPlaneShader = std::make_unique<ShaderProgram>(); _clippingPlaneShader->setObjectName("_clippingPlaneShader");
	_clippingPlaneShader->loadCompileAndLinkShaderFromFile(path + "shaders/clipping_plane.vert", path + "shaders/clipping_plane.frag");
	// Clipped Mesh shader program
	_clippedMeshShader = std::make_unique<ShaderProgram>(); _clippedMeshShader->setObjectName("_clippedMeshShader");
	_clippedMeshShader->loadCompileAndLinkShaderFromFile(path + "shaders/clipped_mesh.vert", path + "shaders/clipped_mesh.frag");
	// Selection shader program
	_selectionShader = std::make_unique<ShaderProgram>(); _selectionShader->setObjectName("_selectionShader");
	_selectionShader->loadCompileAndLinkShaderFromFile(path + "shaders/selection.vert", path + "shaders/selection.frag");
	// Equirectangular to Cube conversion shader
	_equirectToCubeShader = std::make_unique<ShaderProgram>();
	_equirectToCubeShader->setObjectName("_equirectToCubeShader");
	_equirectToCubeShader->loadCompileAndLinkShaderFromFile( path + "shaders/equirect_to_cube.vert", path + "shaders/equirect_to_cube.frag");
	// Equirectangular to Cube Quad conversion shader
	_equirectToCubeQuadShader = std::make_unique<ShaderProgram>();
	_equirectToCubeQuadShader->setObjectName("_equirectToCubeQuadShader");
	_equirectToCubeQuadShader->loadCompileAndLinkShaderFromFile(path + "shaders/equirect_to_cube_quad.vert", path + "shaders/equirect_to_cube_quad.frag");
	// Downsample shader program
	_downsampleShader = std::make_unique<ShaderProgram>();
	_downsampleShader->setObjectName("_downsampleShader");
	_downsampleShader->loadCompileAndLinkShaderFromFile(path + "shaders/downsample_cubemap.vert", path + "shaders/downsample_cubemap.frag");


	// Shadow Depth quad shader program - for debugging
	_debugShader = std::make_unique<ShaderProgram>(); _debugShader->setObjectName("_debugShader");
	_debugShader->loadCompileAndLinkShaderFromFile(path + "shaders/debug_quad.vert", path + "shaders/debug_quad_depth.frag");
}

void GLWidget::createCappingPlanes()
{
    const QString path = PathUtils::getDataDirectory() + "/";
	_clippingPlaneXY = new Plane(_clippingPlaneShader.get(), QVector3D(0, 0, 0), 1000, 1000, 1, 1);
	_clippingPlaneYZ = new Plane(_clippingPlaneShader.get(), QVector3D(0, 0, 0), 1000, 1000, 1, 1);
	_clippingPlaneZX = new Plane(_clippingPlaneShader.get(), QVector3D(0, 0, 0), 1000, 1000, 1, 1);
    _cappingTexture = loadTextureFromFile(QString(path + "textures/patterns/hatch_03.png").toStdString().c_str());
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, _cappingTexture);

	// Stable sampling for any scale
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glGenerateMipmap(GL_TEXTURE_2D);
	// (Optional) if supported:
	GLfloat aniso = 8.0f;
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso);
}

void GLWidget::createLights()
{
	_lightCube = new Cube(_lightCubeShader.get(), 10);
	_lightSphere = new Sphere(_lightCubeShader.get(), 1, 16, 16);
}

void GLWidget::createFullscreenTriangle()
{
	// Fullscreen triangle vertices in clip space
	// Forms a triangle that covers entire viewport
	const float verts[6] = {
		-1.0f, -1.0f,  // Bottom-left
		 3.0f, -1.0f,  // Bottom-right (extends past viewport)
		-1.0f,  3.0f   // Top-left (extends past viewport)
	};

	glGenVertexArrays(1, &_fsTriVAO);
	glGenBuffers(1, &_fsTriVBO);

	glBindVertexArray(_fsTriVAO);
	glBindBuffer(GL_ARRAY_BUFFER, _fsTriVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

	// Set up vertex attribute: 2D position at location 0
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	_fsTriInitialized = true;
}

void GLWidget::drawFullscreenTriangle()
{
	if (!_fsTriInitialized)
	{
		qWarning() << "Fullscreen triangle not initialized!";
		return;
	}

	glBindVertexArray(_fsTriVAO);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);
}

void GLWidget::setIBLFaceBasis(QOpenGLShaderProgram* prog, int faceIndex)
{
	auto setM = [prog](const QVector3D& U, const QVector3D& V, const QVector3D& W) {
		QMatrix3x3 m;
		m(0, 0) = U.x(); m(1, 0) = U.y(); m(2, 0) = U.z();
		m(0, 1) = V.x(); m(1, 1) = V.y(); m(2, 1) = V.z();
		m(0, 2) = W.x(); m(1, 2) = W.y(); m(2, 2) = W.z();
		prog->setUniformValue("faceBasis", m);
		};

	// Basis vectors with 90° X-axis rotation applied
	// (Same rotation as: model.rotate(90.0f, QVector3D(1.0f, 0.0f, 0.0f)))
	switch (faceIndex)
	{
	case 0: // Right (+X)
		setM(QVector3D(0.0f, 1.0f, 0.0f),
			QVector3D(0.0f, 0.0f, 1.0f),
			QVector3D(1.0f, 0.0f, 0.0f));
		break;

	case 1: // Left (-X)
		setM(QVector3D(0.0f, -1.0f, 0.0f),
			QVector3D(0.0f, 0.0f, 1.0f),
			QVector3D(-1.0f, 0.0f, 0.0f));
		break;

	case 2: // Top (+Y)
		setM(QVector3D(1.0f, 0.0f, 0.0f),
			QVector3D(0.0f, -1.0f, 0.0f),
			QVector3D(0.0f, 0.0f, -1.0f));
		break;

	case 3: // Bottom (-Y)
		setM(QVector3D(1.0f, 0.0f, 0.0f),
			QVector3D(0.0f, 1.0f, 0.0f),
			QVector3D(0.0f, 0.0f, 1.0f));
		break;

	case 4: // Front (+Z)
		setM(QVector3D(1.0f, 0.0f, 0.0f),
			QVector3D(0.0f, 0.0f, 1.0f),
			QVector3D(0.0f, -1.0f, 0.0f));
		break;

	case 5: // Back (-Z)
		setM(QVector3D(-1.0f, 0.0f, 0.0f),
			QVector3D(0.0f, 0.0f, 1.0f),
			QVector3D(0.0f, 1.0f, 0.0f));
		break;
	}
}

void GLWidget::ensureShadowMapResources()
{
	if (_shadowMap == 0 || _shadowMapNeedsInitialization)
	{
		if (_shadowMap != 0)
		{
			glDeleteTextures(1, &_shadowMap);
			_shadowMap = 0;
		}
		glGenTextures(1, &_shadowMap);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, _shadowMap);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, _shadowWidth, _shadowHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);		
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
		// attach depth texture as FBO's depth buffer
		if (_shadowMapFBO != 0)
		{
			glDeleteFramebuffers(1, &_shadowMapFBO);
			_shadowMapFBO = 0;
		}
		glGenFramebuffers(1, &_shadowMapFBO);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _shadowMapFBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _shadowMap, 0);
		unsigned long status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE)
			std::cout << "Frame buffer creation failed!" << std::endl;
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, defaultFramebufferObject());
		// Restore active unit: createGPUTextureFromImage() binds without setting
		// glActiveTexture first, so if unit 2 is still active it steals the shadow
		// map slot and shadows disappear for the rest of the frame.
		glActiveTexture(GL_TEXTURE0);
	}
}

void GLWidget::loadFloor()
{
	ensureShadowMapResources();

	// Use helper to update floor geometry
	float halfObjectSize = updateFloorGeometry();

	// Use helper to set main light position
	updateMainLightPosition(halfObjectSize);

	float floorPlaneCoeff = _meshStore.empty()
		? -_floorSize - (_floorSize * 0.05f)
		: groundPlaneZ();
	_floorPlaneZ = floorPlaneCoeff;

	const float groundExtent = groundPlaneExtent();
	if (_floorPlane == nullptr)
	{
		_floorPlane = new FloorPlane(_fgShader.get(), _floorCenter, groundExtent, groundExtent, 1, 1, floorPlaneCoeff, _floorTexRepeatS, _floorTexRepeatT, floorPlaneOrientation());
	}
	else
	{
		_floorPlane->setPlane(_fgShader.get(), _floorCenter, groundExtent, groundExtent, 1, 1, floorPlaneCoeff, _floorTexRepeatS, _floorTexRepeatT, floorPlaneOrientation());
	}

	// Use helper to apply common material/texture settings
	applyFloorPlaneMaterialSettings();
}

void GLWidget::loadGrid()
{
	// Endless grid is rendered as a fullscreen procedural pass, so no mesh is needed.
	if (_gridPlane != nullptr)
	{
		delete _gridPlane;
		_gridPlane = nullptr;
	}
}

void GLWidget::applyFloorPlaneMaterialSettings()
{
	if (_floorPlane == nullptr)
		return;

	_floorPlane->setAmbientMaterial(QVector3D(0.0f, 0.0f, 0.0f));
	_floorPlane->setDiffuseMaterial(QVector3D(1.0f, 1.0f, 1.0f));
	_floorPlane->setSpecularMaterial(QVector3D(0.5f, 0.5f, 0.5f));
	_floorPlane->setShininess(16.0f);
	syncFloorPlaneAlbedoTexture();
}

void GLWidget::syncFloorPlaneAlbedoTexture()
{
	if (_floorPlane == nullptr)
		return;

	GLMaterial material = _floorPlane->getMaterial();
	const GLuint oldAlbedoTex = static_cast<GLuint>(material.albedoTextureId());
	const GLuint oldDiffuseTex = static_cast<GLuint>(material.diffuseTextureId());

	if (!_floorTextureDisplayed || _floorTexImage.isNull())
	{
		if (oldAlbedoTex != 0)
		{
			glDeleteTextures(1, &oldAlbedoTex);
		}
		if (oldDiffuseTex != 0 && oldDiffuseTex != oldAlbedoTex)
		{
			glDeleteTextures(1, &oldDiffuseTex);
		}

		GLMaterial::Texture resetAlbedo;
		resetAlbedo.type = "albedo";
		GLMaterial::Texture resetDiffuse;
		resetDiffuse.type = "diffuse";

		material.setTexture(GLMaterial::TextureType::Albedo, resetAlbedo);
		material.setTexture(GLMaterial::TextureType::Diffuse, resetDiffuse);
		material.setAlbedoMap(QString());
		material.setAlbedoTextureId(0);
		material.setDiffuseMap(QString());
		material.setDiffuseTextureId(0);
		_floorPlane->setMaterial(material);
		return;
	}

	const TextureSamplerSettings samplers{
		GL_REPEAT,
		GL_REPEAT,
		GL_LINEAR_MIPMAP_LINEAR,
		GL_LINEAR
	};

	const GLuint newFloorTex = createGPUTextureFromImage(_floorTexImage, samplers);
	if (newFloorTex == 0)
		return;

	if (oldAlbedoTex != 0)
	{
		glDeleteTextures(1, &oldAlbedoTex);
	}
	if (oldDiffuseTex != 0 && oldDiffuseTex != oldAlbedoTex)
	{
		glDeleteTextures(1, &oldDiffuseTex);
	}

	GLMaterial::Texture albedoTexture = material.texture(GLMaterial::TextureType::Albedo);
	albedoTexture.id = newFloorTex;
	albedoTexture.type = "albedo";
	albedoTexture.path = "generated://floor-albedo";
	albedoTexture.hasAlpha = _floorTexImage.hasAlphaChannel();
	albedoTexture.wrapS = samplers.wrapS;
	albedoTexture.wrapT = samplers.wrapT;
	albedoTexture.minFilter = samplers.minFilter;
	albedoTexture.magFilter = samplers.magFilter;
	albedoTexture.imageData = _floorTexImage;
	material.setTexture(GLMaterial::TextureType::Albedo, albedoTexture);
	material.setAlbedoMap("generated://floor-albedo");
	material.setAlbedoTextureId(static_cast<int>(newFloorTex));

	GLMaterial::Texture resetDiffuse;
	resetDiffuse.type = "diffuse";
	material.setTexture(GLMaterial::TextureType::Diffuse, resetDiffuse);
	material.setDiffuseMap(QString());
	material.setDiffuseTextureId(0);
	_floorPlane->setMaterial(material);
}

Plane::Orientation GLWidget::floorPlaneOrientation() const
{
	return _cameraUpAxisZUp ? Plane::Orientation::XY_ZNormal : Plane::Orientation::XZ_YNormal;
}

QVector3D GLWidget::currentWorldUpVector() const
{
	return _cameraUpAxisZUp
		? QVector3D(0.0f, 0.0f, 1.0f)
		: QVector3D(0.0f, 1.0f, 0.0f);
}

float GLWidget::coordinateAlongCurrentWorldUp(const QVector3D& point) const
{
	return _cameraUpAxisZUp ? point.z() : point.y();
}

void GLWidget::setCoordinateAlongCurrentWorldUp(QVector3D& point, float value) const
{
	if (_cameraUpAxisZUp)
		point.setZ(value);
	else
		point.setY(value);
}

QVector3D GLWidget::effectiveWorldLightOffset() const
{
	return transformVectorForCameraUpAxis(QVector3D(_lightOffsetX, _lightOffsetY, _lightOffsetZ));
}

QVector3D GLWidget::effectiveWorldLightPosition() const
{
	return _lightPosition + effectiveWorldLightOffset();
}

void GLWidget::updateMainLightPosition(float halfObjectSize)
{
	const QVector3D lateralAxis1 = transformVectorForCameraUpAxis(QVector3D(1.0f, 0.0f, 0.0f)).normalized();
	// Z-up: (1,0,0)+(0,1,0) → light at X+Y+ corner, height in Z  (illuminates X+/Y+/Z+ faces)
	// Y-up: (1,0,0)+(0,0,-1) → light at X+Z- corner, height in Y.
	// This preserves the original isometric "shadow falls in front" feel after
	// rotating the viewer convention from Z-up to Y-up.
	const QVector3D lateralAxis2 = _cameraUpAxisZUp
		? transformVectorForCameraUpAxis(QVector3D(0.0f, 1.0f, 0.0f)).normalized()
		: QVector3D(0.0f, 0.0f, -1.0f);
	_lightPosition = _floorCenter + (lateralAxis1 + lateralAxis2) * (_floorSize * 1.25f);

	if (_meshStore.empty())
	{
		setCoordinateAlongCurrentWorldUp(_lightPosition, _floorSize);
	}
	else
	{
		float highestUp = coordinateAlongCurrentWorldUp(_boundingSphere.getCenter()) + _boundingSphere.getRadius();
		setCoordinateAlongCurrentWorldUp(_lightPosition,
			highestUp + halfObjectSize * 5.0f + (_floorSize * _floorOffsetPercent));
	}
}

float GLWidget::updateFloorGeometry()
{
	float halfObjectSize = _boundingSphere.getRadius();
	_floorCenter = _boundingSphere.getCenter();

	if (_cameraUpAxisZUp)
	{
		// Z is up: floor footprint is XY; if the height (Z) dominates, size to it
		if (_boundingBox.getZSize() >= _boundingBox.getXSize() && _boundingBox.getZSize() >= _boundingBox.getYSize())
			_floorSize = _boundingBox.getZSize();
		else
			_floorSize = std::max(_boundingBox.getYSize(), _boundingBox.getXSize()) / 1.25f;
	}
	else
	{
		// Y is up: floor footprint is XZ; if the height (Y) dominates, size to it
		if (_boundingBox.getYSize() >= _boundingBox.getXSize() && _boundingBox.getYSize() >= _boundingBox.getZSize())
			_floorSize = _boundingBox.getYSize();
		else
			_floorSize = std::max(_boundingBox.getXSize(), _boundingBox.getZSize()) / 1.25f;
	}

	_lightCube->setSize(halfObjectSize * 0.1f);

	return halfObjectSize;
}

bool GLWidget::userModelTransformForFile(const QString& sourceFile,
                                         QMatrix4x4& outTransform) const
{
	bool found = false;
	QMatrix4x4 trsf;
	for (TriangleMesh* mesh : _meshStore)
	{
		if (!mesh || mesh->getSourceFile() != sourceFile)
			continue;
		if (!found)
		{
			trsf = mesh->getTransformation();
			found = true;
		}
		else if (mesh->getTransformation() != trsf)
		{
			// Meshes of this file were transformed individually — there is no
			// single model-level transform the lights/cameras could follow.
			return false;
		}
	}

	if (!found || trsf.isIdentity())
		return false;

	outTransform = trsf;
	return true;
}

namespace
{
// Average axis scale of the upper-3x3 — matches the avgScale convention used
// for light range/intensity compensation elsewhere (export, import correction).
float uniformScaleOf(const QMatrix4x4& m)
{
	const float sx = QVector3D(m(0, 0), m(1, 0), m(2, 0)).length();
	const float sy = QVector3D(m(0, 1), m(1, 1), m(2, 1)).length();
	const float sz = QVector3D(m(0, 2), m(1, 2), m(2, 2)).length();
	return (sx + sy + sz) / 3.0f;
}
} // namespace

void GLWidget::updatePunctualLights()
{
	if (_originalParsedLights.empty())
	{
		return;
	}

	// Start with original positions, unless an animation runtime is actively
	// overriding the bound light nodes for the current animated file.
	const std::vector<GPULight>& sourceLights =
		(!_animatedLightTransformSourceFile.isEmpty() &&
		 _animatedParsedLights.size() == _originalParsedLights.size())
		? _animatedParsedLights
		: _originalParsedLights;
	_currentRepositionedLights = sourceLights;

	// Follow user model-level transforms: each light is moved by the exact
	// TRS matrix its file's meshes carry (derived fresh from the mesh store,
	// so undo/redo and every transform entry point are covered).  Lights of
	// untransformed files keep their parsed absolute positions — the visible
	// scene bounds are irrelevant by construction.
	if (_lightFileIndexMap.size() == static_cast<int>(_currentRepositionedLights.size()))
	{
		QHash<QString, QMatrix4x4> transformCache;
		for (int i = 0; i < static_cast<int>(_currentRepositionedLights.size()); ++i)
		{
			const QString& file = _lightFileIndexMap[i].file;
			if (!transformCache.contains(file))
			{
				QMatrix4x4 m; // identity when no model-level transform exists
				userModelTransformForFile(file, m);
				transformCache.insert(file, m);
			}
			const QMatrix4x4& m = transformCache.value(file);
			if (m.isIdentity())
				continue;

			auto& light = _currentRepositionedLights[i];

			const QVector3D pos = m.map(
				QVector3D(light.position.x, light.position.y, light.position.z));
			light.position = glm::vec3(pos.x(), pos.y(), pos.z());

			const QVector3D dir = m.mapVector(
				QVector3D(light.direction.x, light.direction.y, light.direction.z)).normalized();
			light.direction = glm::vec3(dir.x(), dir.y(), dir.z());

			const float s = uniformScaleOf(m);

			// Range scales with the model so attenuation reaches the same
			// relative extent.
			if (light.range > 0.0f)
				light.range *= s;

			// Appearance-preserving intensity compensation (inverse-square
			// law) for point and spot lights: a light moved s× further from
			// the scaled surfaces needs s² the candela to light them equally.
			if (light.type != static_cast<int>(LightType::Directional))
				light.intensity *= (s * s);
		}
	}

	if (!_animatedLightVisibilitySourceFile.isEmpty() &&
		_animatedLightVisibilityMask.size() == static_cast<qsizetype>(_currentRepositionedLights.size()))
	{
		std::vector<GPULight> visibleLights;
		visibleLights.reserve(_currentRepositionedLights.size());
		for (int lightIndex = 0; lightIndex < static_cast<int>(_currentRepositionedLights.size()); ++lightIndex)
		{
			if (_animatedLightVisibilityMask[lightIndex])
				visibleLights.push_back(_currentRepositionedLights[lightIndex]);
		}
		_currentRepositionedLights = std::move(visibleLights);
	}

	// Apply per-light enabled filter before GPU upload.
	// _currentRepositionedLights is kept as the FULL set so gizmos remain
	// visible for disabled lights.  Only the GPU-uploaded subset changes.
	//
	// If the map is absent (MVF flat-light path or legacy code path) fall back
	// to uploading everything, preserving pre-existing behaviour.
	if (_viewer && _viewer->sceneGraph() &&
	    !_lightFileIndexMap.isEmpty() &&
	    static_cast<int>(_currentRepositionedLights.size()) == _lightFileIndexMap.size())
	{
		std::vector<GPULight> toUpload;
		toUpload.reserve(_currentRepositionedLights.size());
		auto* sg = _viewer->sceneGraph();
		for (int i = 0; i < static_cast<int>(_currentRepositionedLights.size()); ++i)
		{
			const LightOrigin& origin = _lightFileIndexMap[i];
			const GltfLightData& ld   = sg->lightDataForFile(origin.file);
			if (origin.index < static_cast<int>(ld.lights.size()) &&
			    ld.lights[origin.index].enabled)
			{
				toUpload.push_back(_currentRepositionedLights[i]);
			}
		}
		glLights->setLights(toUpload);
		syncPunctualLightUniforms(static_cast<int>(toUpload.size()),
		                          !toUpload.empty());
	}
	else
	{
		glLights->setLights(_currentRepositionedLights);
	}
}

void GLWidget::setAnimatedLightVisibilityState(const QString& sourceFile, const QVector<bool>& visibleByParsedLight)
{
	_animatedLightVisibilitySourceFile = sourceFile;
	_animatedLightVisibilityMask = visibleByParsedLight;
	updatePunctualLights();
}

void GLWidget::setAnimatedLightTransformState(const QString& sourceFile, const std::vector<GPULight>& animatedLights)
{
	_animatedLightTransformSourceFile = sourceFile;
	_animatedParsedLights = animatedLights;
	updatePunctualLights();
}

void GLWidget::clearAnimatedLightTransformState(const QString& sourceFile)
{
	if (_animatedLightTransformSourceFile != sourceFile)
		return;

	_animatedLightTransformSourceFile.clear();
	_animatedParsedLights.clear();
	updatePunctualLights();
}

void GLWidget::clearAnimatedLightVisibilityState(const QString& sourceFile)
{
	if (_animatedLightVisibilitySourceFile != sourceFile)
		return;

	_animatedLightVisibilitySourceFile.clear();
	_animatedLightVisibilityMask.clear();
	updatePunctualLights();
}

void GLWidget::setAnimatedMeshVisibilityState(const QString& sourceFile, const QSet<QUuid>& hiddenMeshUuids)
{
	const bool activatingForFile = (_animatedMeshVisibilitySourceFile != sourceFile);
	_animatedMeshVisibilitySourceFile = sourceFile;
	_animatedHiddenMeshUuids = hiddenMeshUuids;
	recalculateVisibleSceneStats();
	updatePunctualLights();
	if (activatingForFile)
		fitAll();
}

void GLWidget::clearAnimatedMeshVisibilityState(const QString& sourceFile)
{
	if (_animatedMeshVisibilitySourceFile != sourceFile)
		return;

	_animatedMeshVisibilitySourceFile.clear();
	_animatedHiddenMeshUuids.clear();
	recalculateVisibleSceneStats();
	updatePunctualLights();
}

void GLWidget::loadEnvMap()
{	
    const QString path = PathUtils::getDataDirectory() + "/";

	_skyBox = new Cube(_skyBoxShader.get(), 1);
	_skyBoxShader->bind();
	_skyBoxShader->setUniformValue("skybox", 1);
	
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	glGenTextures(1, &_environmentMap);
	
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_CUBE_MAP, _environmentMap);

	setSkyBoxTextureFolder(path + "textures/envmap/skyboxes/LDRI/@Default");	
}

void GLWidget::loadIrradianceMap()
{
	// Setup framebuffer for offscreen rendering.
	// Use zero-init + scope-exit lambda so captureFBO/RBO are always freed even
	// if an early return is added in future.
	unsigned int captureFBO = 0;
	unsigned int captureRBO = 0;
	glGenFramebuffers(1, &captureFBO);
	glGenRenderbuffers(1, &captureRBO);
	auto cleanupFBO = [&]() {
		glDeleteFramebuffers(1, &captureFBO);
		glDeleteRenderbuffers(1, &captureRBO);
	};

	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

	// Save GL state
	glDisable(GL_BLEND);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_STENCIL_TEST);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_TRUE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glDisable(GL_CULL_FACE);

	// Create irradiance cubemap
	if (_irradianceMap)
		glDeleteTextures(1, &_irradianceMap);
	glGenTextures(1, &_irradianceMap);
	//std::cout << "GLWidget::loadIrradianceMap : _irradianceMap = " << _irradianceMap << std::endl;
	glBindTexture(GL_TEXTURE_CUBE_MAP, _irradianceMap);

	constexpr int irradianceSize = 64;
	for (unsigned int i = 0; i < 6; ++i)
	{
		if (_skyBoxTextureHDRI)
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB32F,
				irradianceSize, irradianceSize, 0, GL_RGB, GL_FLOAT, nullptr);
		else
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
				irradianceSize, irradianceSize, 0, GL_RGB, GL_HALF_FLOAT, nullptr);
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Setup framebuffer for irradiance rendering
	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, irradianceSize, irradianceSize);

	// ==== IRRADIANCE PASS: Use fullscreen triangle ====
	_irradianceShader->bind();
	_irradianceShader->setUniformValue("environmentMap", 1);
	_irradianceShader->setUniformValue("resolution", QVector2D(irradianceSize, irradianceSize));

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_CUBE_MAP, _environmentMap);

	glViewport(0, 0, irradianceSize, irradianceSize);
	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);

	for (unsigned int i = 0; i < 6; ++i)
	{
		// Bind this face to the framebuffer
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, _irradianceMap, 0);

		GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
		{
			qWarning() << "Irradiance FBO incomplete at face" << i << "Status:" << fboStatus;
			continue;
		}

		// Set per-face basis matrix
		_irradianceShader->bind();
		setIBLFaceBasis(_irradianceShader.get(), i);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		// Draw fullscreen triangle (not the cube!)
		drawFullscreenTriangle();
	}

	glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());

	// ==== PREFILTER PASS: Create prefilter cubemap ====
	if (_prefilterMap)
		glDeleteTextures(1, &_prefilterMap);
	glGenTextures(1, &_prefilterMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, _prefilterMap);

	constexpr int prefilterSize = 512;
	unsigned int maxMipLevels = static_cast<unsigned int>(std::log2(prefilterSize)) + 1;
	// Khronos uses only 5 effective LOD levels (lowestMipLevel=4 in ibl_sampler.js).
	// LOD formula: lod = roughness * (effectiveMipLevels - 1).
	// Roughness 0..1 maps to mips 0..4 only; tail mips (5-9) baked at roughness=1
	// for texture completeness but are never sampled by the LOD formula.
	constexpr unsigned int effectiveMipLevels = 5;
	_prefilterMipLevels = effectiveMipLevels;

	// Allocate all mip levels upfront (full chain for GL_LINEAR_MIPMAP_LINEAR completeness)
	for (unsigned int mip = 0; mip < maxMipLevels; ++mip)
	{
		unsigned int mipSize = static_cast<unsigned int>(prefilterSize * std::pow(0.5, mip));
		for (unsigned int i = 0; i < 6; ++i)
		{
			if (_skyBoxTextureHDRI)
				glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, mip, GL_RGB32F,
					mipSize, mipSize, 0, GL_RGB, GL_FLOAT, nullptr);
			else
				glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, mip, GL_RGB16F,
					mipSize, mipSize, 0, GL_RGB, GL_HALF_FLOAT, nullptr);
		}
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_ANISOTROPY_EXT, _anisotropicFilteringLevel);

	// Get environment map resolution for importance sampling
	GLint envMapWidth = 512;
	glBindTexture(GL_TEXTURE_CUBE_MAP, _environmentMap);
	glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_TEXTURE_WIDTH, &envMapWidth);

	// Render each mip level with fullscreen triangle
	_prefilterShader->bind();
	_prefilterShader->setUniformValue("environmentMap", 1);
	_prefilterShader->setUniformValue("environmentMapResolution", static_cast<float>(envMapWidth));

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_CUBE_MAP, _environmentMap);

	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);

	for (unsigned int mip = 0; mip < maxMipLevels; ++mip)
	{
		unsigned int mipWidth = prefilterSize * std::pow(0.5, mip);
		unsigned int mipHeight = prefilterSize * std::pow(0.5, mip);

		// Resize renderbuffer for this mip level
		glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
		glViewport(0, 0, mipWidth, mipHeight);

		// Spread roughness 0..1 over the first effectiveMipLevels mips (matches Khronos).
		// Tail mips (beyond effective range) get roughness=1.0 for texture completeness.
		float roughness = (mip < effectiveMipLevels)
			? (float)mip / (float)(effectiveMipLevels - 1)
			: 1.0f;
		_prefilterShader->bind();
		_prefilterShader->setUniformValue("roughness", roughness);
		_prefilterShader->setUniformValue("resolution", QVector2D(mipWidth, mipHeight));

		for (unsigned int i = 0; i < 6; ++i)
		{
			// Bind this mip level of this face to framebuffer
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, _prefilterMap, mip);

			GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
			{
				qWarning() << "Prefilter FBO incomplete at mip" << mip << "face" << i
					<< "Status:" << fboStatus;
				continue;
			}

			// Set per-face basis matrix
			_prefilterShader->bind();
			setIBLFaceBasis(_prefilterShader.get(), i);

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

			// Draw fullscreen triangle (not the cube!)
			drawFullscreenTriangle();
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());

	// ==== SHEEN PREFILTER PASS: Create Charlie prefilter cubemap ====
	if (_sheenPrefilterMap)
		glDeleteTextures(1, &_sheenPrefilterMap);
	glGenTextures(1, &_sheenPrefilterMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, _sheenPrefilterMap);

	constexpr int sheenPrefilterSize = 256;
	// Khronos-compatible sheen mip scheme: 5 effective levels (roughness 0..1 over mips 0..4).
	// LOD formula: lod = roughness * (sheenEffectiveMipLevels - 1) = roughness * 4.
	// At sheenRoughness=0.1 this gives lod=0.4 vs the old lod=0.8 (9 mip/textureQueryLevels).
	// Mip 0 (roughness=0) collapses to the mirror-reflection sample, so low-roughness sheen
	// IBL reflects the environment instead of being nearly black.
	constexpr int sheenEffectiveMipLevels = 5;
	const unsigned int sheenMaxMipLevels = static_cast<unsigned int>(std::log2(sheenPrefilterSize)) + 1;

	for (unsigned int mip = 0; mip < sheenMaxMipLevels; ++mip)
	{
		unsigned int mipSize = static_cast<unsigned int>(sheenPrefilterSize * std::pow(0.5, mip));
		for (unsigned int i = 0; i < 6; ++i)
		{
			if (_skyBoxTextureHDRI)
				glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, mip, GL_RGB32F,
					mipSize, mipSize, 0, GL_RGB, GL_FLOAT, nullptr);
			else
				glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, mip, GL_RGB16F,
					mipSize, mipSize, 0, GL_RGB, GL_HALF_FLOAT, nullptr);
		}
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_ANISOTROPY_EXT, _anisotropicFilteringLevel);

	_sheenPrefilterShader->bind();
	_sheenPrefilterShader->setUniformValue("environmentMap", 1);
	_sheenPrefilterShader->setUniformValue("environmentMapResolution", static_cast<float>(envMapWidth));

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_CUBE_MAP, _environmentMap);
	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);

	for (unsigned int mip = 0; mip < sheenMaxMipLevels; ++mip)
	{
		unsigned int mipWidth = sheenPrefilterSize * std::pow(0.5, mip);
		unsigned int mipHeight = sheenPrefilterSize * std::pow(0.5, mip);

		glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
		glViewport(0, 0, mipWidth, mipHeight);

		// Roughness for this mip: spread evenly over [0..1] for mips 0..(sheenEffectiveMipLevels-1),
		// clamp to 1.0 for any extra mips (allocated for texture completeness, never sampled by LOD).
		float roughness = (mip < static_cast<unsigned int>(sheenEffectiveMipLevels))
			? static_cast<float>(mip) / static_cast<float>(sheenEffectiveMipLevels - 1)
			: 1.0f;
		_sheenPrefilterShader->bind();
		_sheenPrefilterShader->setUniformValue("roughness", roughness);
		_sheenPrefilterShader->setUniformValue("resolution", QVector2D(mipWidth, mipHeight));

		for (unsigned int i = 0; i < 6; ++i)
		{
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, _sheenPrefilterMap, mip);

			GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
			{
				qWarning() << "Sheen prefilter FBO incomplete at mip" << mip << "face" << i
					<< "Status:" << fboStatus;
				continue;
			}

			_sheenPrefilterShader->bind();
			setIBLFaceBasis(_sheenPrefilterShader.get(), i);

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			drawFullscreenTriangle();
		}
	}
	_sheenPrefilterMipLevels = sheenEffectiveMipLevels;

	glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());

	// PBR: generate a 2D LUT from the BRDF equations used.
	// ----------------------------------------------------
	if (_brdfLUTTexture)
		glDeleteTextures(1, &_brdfLUTTexture);
	glGenTextures(1, &_brdfLUTTexture);
	//std::cout << "GLWidget::loadIrradianceMap : _brdfLUTTexture = " << _brdfLUTTexture << std::endl;

	constexpr int lutTextureSize = 512;
	// pre-allocate enough memory for the LUT texture.
	glBindTexture(GL_TEXTURE_2D, _brdfLUTTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, lutTextureSize, lutTextureSize, 0, GL_RGB, GL_FLOAT, 0);
	// be sure to set wrapping mode to GL_CLAMP_TO_EDGE
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// then re-configure capture framebuffer object and render screen-space quad with BRDF shader.
	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, lutTextureSize, lutTextureSize);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _brdfLUTTexture, 0);

	glViewport(0, 0, lutTextureSize, lutTextureSize);
	_brdfShader->bind();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	renderQuad();

	glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());

	// Bind pre-computed IBL data to texture units
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_CUBE_MAP, _irradianceMap);
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_CUBE_MAP, _prefilterMap);
	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D, _brdfLUTTexture);
	glActiveTexture(GL_TEXTURE7);
	glBindTexture(GL_TEXTURE_CUBE_MAP, _sheenPrefilterMap);

	auto resolveKhronosLUTPath = [](const QString& fileName) -> QString
	{
		const QString dataCandidate = QDir(PathUtils::getDataDirectory()).absoluteFilePath("textures/khronos/" + fileName);
		if (QFileInfo::exists(dataCandidate))
		{
			return dataCandidate;
		}

		// Development fallback when running from the source tree instead of an installed app layout.
		const QString sourceCandidate = QDir(QDir::currentPath()).absoluteFilePath("textures/khronos/" + fileName);
		if (QFileInfo::exists(sourceCandidate))
		{
			return sourceCandidate;
		}

		return dataCandidate;
	};

	auto loadKhronosLUTTexture = [this, &resolveKhronosLUTPath](const QString& fileName, GLuint& textureId)
	{
		const QString filePath = resolveKhronosLUTPath(fileName);
		if (filePath.isEmpty())
		{
			qWarning() << "Failed to resolve Khronos LUT texture:" << fileName;
			if (textureId != 0)
			{
				glDeleteTextures(1, &textureId);
				textureId = 0;
			}
			return;
		}

		QImage image(filePath);
		if (image.isNull())
		{
			qWarning() << "Failed to load Khronos LUT texture:" << filePath;
			if (textureId != 0)
			{
				glDeleteTextures(1, &textureId);
				textureId = 0;
			}
			return;
		}

		QImage glImage = image.convertToFormat(QImage::Format_RGBA8888);
		if (textureId != 0)
			glDeleteTextures(1, &textureId);
		glGenTextures(1, &textureId);
		glBindTexture(GL_TEXTURE_2D, textureId);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, glImage.width(), glImage.height(), 0,
			GL_RGBA, GL_UNSIGNED_BYTE, glImage.constBits());
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	};

	if (_charlieLUTTexture == 0)
	{
		loadKhronosLUTTexture("lut_charlie.png", _charlieLUTTexture);
	}
	if (_sheenELUTTexture == 0)
	{
		loadKhronosLUTTexture("lut_sheen_E.png", _sheenELUTTexture);
	}

	glActiveTexture(GL_TEXTURE8);
	glBindTexture(GL_TEXTURE_2D, _charlieLUTTexture);
	glActiveTexture(GL_TEXTURE9);
	glBindTexture(GL_TEXTURE_2D, _sheenELUTTexture);

	// Cleanup temporary FBO
	cleanupFBO();
}

// Helper: Load HDR file and convert to cubemap
// Returns the created cubemap texture ID (or 0 on failure)
GLuint GLWidget::loadPresetEnvironmentMap(const QString& hdrFilePath)
{
	// Load equirectangular HDR
	int imgWidth, imgHeight, channels;
	stbi_set_flip_vertically_on_load(true);
	float* data = stbi_loadf(hdrFilePath.toStdString().c_str(), &imgWidth, &imgHeight, &channels, 0);

	if (!data || imgWidth != 2 * imgHeight)
	{
		qWarning() << "Failed to load HDR file:" << hdrFilePath;
		if (data) stbi_image_free(data);
		return 0;
	}

	// Create temporary 2D texture for equirectangular source
	GLuint equirectTexture;
	glGenTextures(1, &equirectTexture);
	glBindTexture(GL_TEXTURE_2D, equirectTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, imgWidth, imgHeight, 0, GL_RGB, GL_FLOAT, data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	stbi_image_free(data);

	// Create cubemap
	GLuint cubemap;
	glGenTextures(1, &cubemap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);

	int cubeSize = 512;
	for (int mip = 0; mip < static_cast<int>(std::log2(cubeSize)) + 1; ++mip)
	{
		int mipSize = cubeSize >> mip;
		for (int i = 0; i < 6; ++i)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, mip, GL_RGB32F,
				mipSize, mipSize, 0, GL_RGB, GL_FLOAT, nullptr);
		}
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Setup FBO for conversion
	GLuint framebuffer, depthBuffer;
	glGenFramebuffers(1, &framebuffer);
	glGenRenderbuffers(1, &depthBuffer);

	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, cubeSize, cubeSize);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);

	// Convert equirectangular to cubemap
	_equirectToCubeShader->bind();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, equirectTexture);
	_equirectToCubeShader->setUniformValue("equirectangularMap", 0);

	glViewport(0, 0, cubeSize, cubeSize);

	QMatrix4x4 captureViews[] = {
		[]() { QMatrix4x4 m; m.lookAt(QVector3D(0.0f, 0.0f, 0.0f), QVector3D(1,  0,  0), QVector3D(0, -1,  0)); return m; }(),
		[]() { QMatrix4x4 m; m.lookAt(QVector3D(0.0f, 0.0f, 0.0f), QVector3D(-1,  0,  0), QVector3D(0, -1,  0)); return m; }(),
		[]() { QMatrix4x4 m; m.lookAt(QVector3D(0.0f, 0.0f, 0.0f), QVector3D(0,  1,  0), QVector3D(0,  0,  1)); return m; }(),
		[]() { QMatrix4x4 m; m.lookAt(QVector3D(0.0f, 0.0f, 0.0f), QVector3D(0, -1,  0), QVector3D(0,  0, -1)); return m; }(),
		[]() { QMatrix4x4 m; m.lookAt(QVector3D(0.0f, 0.0f, 0.0f), QVector3D(0,  0,  1), QVector3D(0, -1,  0)); return m; }(),
		[]() { QMatrix4x4 m; m.lookAt(QVector3D(0.0f, 0.0f, 0.0f), QVector3D(0,  0, -1), QVector3D(0, -1,  0)); return m; }()
	};

	QMatrix4x4 captureProjection;
	captureProjection.perspective(90.0f, 1.0f, 0.1f, 10.0f);
	_equirectToCubeShader->setUniformValue("projection", captureProjection);

	for (int i = 0; i < 6; ++i)
	{
		_equirectToCubeShader->setUniformValue("view", captureViews[i]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubemap, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		renderConversionCube();
	}

	glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
	glDeleteFramebuffers(1, &framebuffer);
	glDeleteRenderbuffers(1, &depthBuffer);
	glDeleteTextures(1, &equirectTexture);

	return cubemap;
}

// Helper: Generate irradiance and prefilter maps for a preset cubemap
// Returns true on success
bool GLWidget::generatePresetIBLMaps(GLuint sourceCubemap, GLuint& outIrradianceMap, GLuint& outPrefilterMap, GLuint& outSheenPrefilterMap)
{
	if (!sourceCubemap) return false;

	// Setup FBO for offscreen rendering
	unsigned int captureFBO, captureRBO;
	glGenFramebuffers(1, &captureFBO);
	glGenRenderbuffers(1, &captureRBO);

	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

	// Create irradiance map
	if (outIrradianceMap)
		glDeleteTextures(1, &outIrradianceMap);
	glGenTextures(1, &outIrradianceMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, outIrradianceMap);

	constexpr int irradianceSize = 64;
	for (unsigned int i = 0; i < 6; ++i)
	{
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB32F,
			irradianceSize, irradianceSize, 0, GL_RGB, GL_FLOAT, nullptr);
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Generate irradiance
	_irradianceShader->bind();
	_irradianceShader->setUniformValue("environmentMap", 1);
	_irradianceShader->setUniformValue("resolution", QVector2D(irradianceSize, irradianceSize));

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_CUBE_MAP, sourceCubemap);

	glViewport(0, 0, irradianceSize, irradianceSize);
	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, irradianceSize, irradianceSize);

	for (unsigned int i = 0; i < 6; ++i)
	{
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, outIrradianceMap, 0);

		GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
		{
			qWarning() << "Irradiance FBO incomplete at face" << i;
			continue;
		}

		_irradianceShader->bind();
		setIBLFaceBasis(_irradianceShader.get(), i);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		drawFullscreenTriangle();
	}

	glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());

	// Create prefilter map
	if (outPrefilterMap)
		glDeleteTextures(1, &outPrefilterMap);
	glGenTextures(1, &outPrefilterMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, outPrefilterMap);

	constexpr int prefilterSize = 512;
	unsigned int maxMipLevels = static_cast<unsigned int>(std::log2(prefilterSize)) + 1;
	constexpr unsigned int effectiveMipLevels = 5;

	// Allocate all mip levels (full chain for completeness)
	for (unsigned int mip = 0; mip < maxMipLevels; ++mip)
	{
		unsigned int mipSize = static_cast<unsigned int>(prefilterSize * std::pow(0.5, mip));
		for (unsigned int i = 0; i < 6; ++i)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, mip, GL_RGB32F,
				mipSize, mipSize, 0, GL_RGB, GL_FLOAT, nullptr);
		}
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Get source resolution for importance sampling
	GLint envMapWidth = 512;
	glBindTexture(GL_TEXTURE_CUBE_MAP, sourceCubemap);
	glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_TEXTURE_WIDTH, &envMapWidth);

	// Render prefilter mip levels
	_prefilterShader->bind();
	_prefilterShader->setUniformValue("environmentMap", 1);
	_prefilterShader->setUniformValue("environmentMapResolution", static_cast<float>(envMapWidth));

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_CUBE_MAP, sourceCubemap);

	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);

	for (unsigned int mip = 0; mip < maxMipLevels; ++mip)
	{
		unsigned int mipWidth = prefilterSize * std::pow(0.5, mip);
		unsigned int mipHeight = prefilterSize * std::pow(0.5, mip);

		glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
		glViewport(0, 0, mipWidth, mipHeight);

		float roughness = (mip < effectiveMipLevels)
			? (float)mip / (float)(effectiveMipLevels - 1)
			: 1.0f;
		_prefilterShader->bind();
		_prefilterShader->setUniformValue("roughness", roughness);
		_prefilterShader->setUniformValue("resolution", QVector2D(mipWidth, mipHeight));

		for (unsigned int i = 0; i < 6; ++i)
		{
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, outPrefilterMap, mip);

			GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
			{
				qWarning() << "Prefilter FBO incomplete at mip" << mip << "face" << i;
				continue;
			}

			_prefilterShader->bind();
			setIBLFaceBasis(_prefilterShader.get(), i);

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			drawFullscreenTriangle();
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());

	// Create Charlie/sheen prefilter map
	if (outSheenPrefilterMap)
		glDeleteTextures(1, &outSheenPrefilterMap);
	glGenTextures(1, &outSheenPrefilterMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, outSheenPrefilterMap);

	constexpr int sheenPrefilterSize = 256;
	// Same Khronos-compatible scheme as the primary environment sheen prefilter.
	constexpr int sheenEffectiveMipLevels = 5;
	const unsigned int sheenMaxMipLevels = static_cast<unsigned int>(std::log2(sheenPrefilterSize)) + 1;

	for (unsigned int mip = 0; mip < sheenMaxMipLevels; ++mip)
	{
		unsigned int mipSize = static_cast<unsigned int>(sheenPrefilterSize * std::pow(0.5, mip));
		for (unsigned int i = 0; i < 6; ++i)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, mip, GL_RGB32F,
				mipSize, mipSize, 0, GL_RGB, GL_FLOAT, nullptr);
		}
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	_sheenPrefilterShader->bind();
	_sheenPrefilterShader->setUniformValue("environmentMap", 1);
	_sheenPrefilterShader->setUniformValue("environmentMapResolution", static_cast<float>(envMapWidth));

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_CUBE_MAP, sourceCubemap);
	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);

	for (unsigned int mip = 0; mip < sheenMaxMipLevels; ++mip)
	{
		unsigned int mipWidth = sheenPrefilterSize * std::pow(0.5, mip);
		unsigned int mipHeight = sheenPrefilterSize * std::pow(0.5, mip);

		glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
		glViewport(0, 0, mipWidth, mipHeight);

		float roughness = (mip < static_cast<unsigned int>(sheenEffectiveMipLevels))
			? static_cast<float>(mip) / static_cast<float>(sheenEffectiveMipLevels - 1)
			: 1.0f;
		_sheenPrefilterShader->bind();
		_sheenPrefilterShader->setUniformValue("roughness", roughness);
		_sheenPrefilterShader->setUniformValue("resolution", QVector2D(mipWidth, mipHeight));

		for (unsigned int i = 0; i < 6; ++i)
		{
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, outSheenPrefilterMap, mip);

			GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
			{
				qWarning() << "Sheen prefilter FBO incomplete at mip" << mip << "face" << i;
				continue;
			}

			_sheenPrefilterShader->bind();
			setIBLFaceBasis(_sheenPrefilterShader.get(), i);

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			drawFullscreenTriangle();
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());

	glDeleteFramebuffers(1, &captureFBO);
	glDeleteRenderbuffers(1, &captureRBO);

	return true;
}

GLuint GLWidget::getEnvironmentMap(int index, bool regenerate)
{
	switch(index)
	{
		case 0:  // ViewerIBL
			if (regenerate && !_currentSkyboxFolder.isEmpty())
			{
				loadEnvMap();
			}
			return _environmentMap;
		case 1:  // Studio
			return _studioEnvironmentMap;
		case 2:  // Outdoor
			return _outdoorEnvironmentMap;
		case 3:  // Office
			return _officeEnvironmentMap;
		default:
			return 0;
	}
}

GLuint GLWidget::getIrradianceMap(int index, bool regenerate)
{
	switch(index)
	{
		case 0:  // ViewerIBL
			if (regenerate && !_currentSkyboxFolder.isEmpty())
			{
				loadIrradianceMap();
			}
			return _irradianceMap;
		case 1:  // Studio
			return _studioIrradianceMap;
		case 2:  // Outdoor
			return _outdoorIrradianceMap;
		case 3:  // Office
			return _officeIrradianceMap;
		default:
			return 0;
	}
}

GLuint GLWidget::getPrefilterMap(int index, bool regenerate)
{
	switch(index)
	{
		case 0:  // ViewerIBL
			if (regenerate && !_currentSkyboxFolder.isEmpty())
			{
				loadIrradianceMap();  // This creates both irradiance AND prefilter
			}
			return _prefilterMap;
		case 1:  // Studio
			return _studioPrefilterMap;
		case 2:  // Outdoor
			return _outdoorPrefilterMap;
		case 3:  // Office
			return _officePrefilterMap;
		default:
			return 0;
	}
}

GLuint GLWidget::getSheenPrefilterMap(int index, bool regenerate)
{
	switch(index)
	{
		case 0:  // ViewerIBL
			if (regenerate && !_currentSkyboxFolder.isEmpty())
			{
				loadIrradianceMap();
			}
			return _sheenPrefilterMap;
		case 1:  // Studio
			return _studioSheenPrefilterMap;
		case 2:  // Outdoor
			return _outdoorSheenPrefilterMap;
		case 3:  // Office
			return _officeSheenPrefilterMap;
		default:
			return 0;
	}
}

void GLWidget::renderSingleView(QColor& topColor, QColor& botColor)
{
	QMatrix4x4 projection;
	projection.ortho(QRect(0.0f, 0.0f, static_cast<float>(width()), static_cast<float>(height())));
	_textShader->bind();
	_textShader->setUniformValue("projection", projection);
	_textShader->release();
	glViewport(0, 0, width(), height());
	if (_shadowsEnabled)
		renderToShadowBuffer();

	if (sceneHasVisibleSSSMaterials())
		renderToSSSBuffer(_primaryCamera);

	if (_transmissionEnabled && sceneHasVisibleTransmissionMaterials())
		renderToTransmissionBuffer(_primaryCamera, topColor, botColor);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	gradientBackground(topColor.redF(), topColor.greenF(), topColor.blueF(), topColor.alphaF(),
		botColor.redF(), botColor.greenF(), botColor.blueF(), botColor.alphaF(), _gradientStyle);
	render(_primaryCamera);
	drawTransformGizmo(_primaryCamera);
	if(_userShowCornerAxisOverride)
		drawCornerAxis(_cornerAxisPosition);
}

void GLWidget::setCornerAxisPosition(CornerAxisPosition position)
{
	_cornerAxisPosition = normalizeCornerAxisPosition(position);
	update();
}

void GLWidget::applyExplodedViewTransforms(const QMap<int, TransformState>& transforms, bool fitView)
{
	Q_UNUSED(fitView);

	QHash<QUuid, QVector3D> savedExplosionOffsets;
	savedExplosionOffsets.reserve(transforms.size());

	for (auto it = transforms.cbegin(); it != transforms.cend(); ++it)
	{
		const int index = it.key();
		if (index < 0 || index >= static_cast<int>(_meshStore.size()))
			continue;

		TriangleMesh* mesh = _meshStore[index];
		if (!mesh)
			continue;

		savedExplosionOffsets.insert(mesh->uuid(), mesh->explosionOffset());
		mesh->setExplosionOffset(QVector3D());
		applyExplodedViewTransformState(mesh, it.value(), false);
		mesh->fullUpdateRuntimeBounds();
	}

	for (auto it = transforms.cbegin(); it != transforms.cend(); ++it)
	{
		const int index = it.key();
		if (index < 0 || index >= static_cast<int>(_meshStore.size()))
			continue;

		TriangleMesh* mesh = _meshStore[index];
		if (!mesh)
			continue;

		mesh->setExplosionOffset(savedExplosionOffsets.value(mesh->uuid()));
		mesh->setSceneRenderTransformFast(mesh->getSceneRenderTransform());
	}

	emit explodedViewManualPlacementChanged();
	update();
}

void GLWidget::renderMultiView(QColor& topColor, QColor& botColor)
{
	glViewport(0, 0, width(), height());
	if (_shadowsEnabled)
		renderToShadowBuffer();

	if (sceneHasVisibleSSSMaterials())
		renderToSSSBuffer(_primaryCamera);

	if (_transmissionEnabled && sceneHasVisibleTransmissionMaterials())
		renderToTransmissionBuffer(_primaryCamera, topColor, botColor);

	gradientBackground(topColor.redF(), topColor.greenF(), topColor.blueF(), topColor.alphaF(),
		botColor.redF(), botColor.greenF(), botColor.blueF(), botColor.alphaF(), _gradientStyle);
	const std::vector<QVector3D> multiViewCorners = collectVisibleCorners();
	const QVector3D sharedMultiViewCenter = _primaryCamera->getPosition();
	// Computed range (eye-relative from orbit center) gives the correct full-scene fit.
	// Scale it by the perspective zoom ratio so ISO zoom/pan drives ortho zoom/pan.
	const float zoomScale = (_viewBoundingSphereDia > 0.0f)
		? (_viewRange / _viewBoundingSphereDia) : 1.0f;
	const float sharedMultiViewRange = computeSharedOrthographicMultiViewRange(
		multiViewCorners, width() / 2, height() / 2, sharedMultiViewCenter) * zoomScale;
	// Render orthographic views with ortho view camera
	// Top View
	glViewport(0, 0, width() / 2, height() / 2);
	configureOrthoSubviewCamera(
		ViewMode::TOP, multiViewCorners, width() / 2, height() / 2, sharedMultiViewCenter, sharedMultiViewRange);
	render(_orthoViewsCamera);
	_textRenderer->RenderText(_labelTop.toStdString(), -50, 5, 1.6f, QVector3D(1.0f, 1.0f, 0.0f), TextRenderer::VAlignment::VTOP, TextRenderer::HAlignment::HRIGHT);

	// Front View
	glViewport(0, height() / 2, width() / 2, height() / 2);
	configureOrthoSubviewCamera(
		ViewMode::FRONT, multiViewCorners, width() / 2, height() / 2, sharedMultiViewCenter, sharedMultiViewRange);
	render(_orthoViewsCamera);
	_textRenderer->RenderText(_labelFront.toStdString(), -50, 5, 1.6f, QVector3D(1.0f, 1.0f, 0.0f), TextRenderer::VAlignment::VTOP, TextRenderer::HAlignment::HRIGHT);

	// Left View
	glViewport(width() / 2, height() / 2, width() / 2, height() / 2);
	configureOrthoSubviewCamera(
		ViewMode::LEFT, multiViewCorners, width() / 2, height() / 2, sharedMultiViewCenter, sharedMultiViewRange);
	render(_orthoViewsCamera);
	_textRenderer->RenderText(_labelLeft.toStdString(), -50, 5, 1.6f, QVector3D(1.0f, 1.0f, 0.0f), TextRenderer::VAlignment::VTOP, TextRenderer::HAlignment::HRIGHT);

	// Render isometric view with primary camera
	// Isometric View
	glViewport(width() / 2, 0, width() / 2, height() / 2);
	render(_primaryCamera);
	//std::string viewLabel = _viewMode == ViewMode::DIMETRIC ? "Dimetric" : _viewMode
		//== ViewMode::TRIMETRIC ? "Trimetric" : "Isometric";
	QString viewLabel;
	switch (_viewMode)
	{
	case ViewMode::DIMETRIC: viewLabel = _labelDimetric; break;
	case ViewMode::TRIMETRIC: viewLabel = _labelTrimetric; break;
	default: viewLabel = _labelIsometric; break;
	}
	_textRenderer->RenderText(viewLabel.toStdString(), -50, 5, 1.6f, QVector3D(1.0f, 1.0f, 0.0f), TextRenderer::VAlignment::VTOP, TextRenderer::HAlignment::HRIGHT);

	// draw screen partitioning lines
	splitScreen();
}

void GLWidget::drawFloor(const bool& drawReflection)
{
	TriangleMesh::resetTextureBindingCacheForCurrentContext();

	// Per-pass uniforms: 3 that differ between passes + 2 that drawMesh() may overwrite.
	// bind() is required here: drawMesh() ends with _prog->release() per mesh, leaving
	// no program active; the bind before each pass restores _fgShader so the uniform
	// uploads and the matrix uploads in FloorPlane::render() reach the right program.
	auto configureGroundPass = [this](bool reflectedPass, bool textureEnabled)
	{
		_fgShader->bind();
		_fgShader->setUniformValue("sssCapture", false);
		_fgShader->setUniformValue("envMapEnabled", false);
		_fgShader->setUniformValue("isReflectedPass", reflectedPass);
		_fgShader->setUniformValue("renderingMode", static_cast<int>(RenderingMode::ADS_BLINN_PHONG));
		_fgShader->setUniformValue("floorTextureEnabled", textureEnabled);
	};

	// Units 32/33: prevent the floor from sampling the transmission FBO while it is
	// the active render target (or stale from a previous frame). Scene meshes in the
	// reflection pass do not rebind these units, so one call covers all three passes.
	// Units 37/38 (sssDiffuseTexture/sssDepthTexture) need no protection here:
	// floor rendering never has hasVolumeScattering=true so sampleCapturedSSSDiffuse
	// is never called.
	glActiveTexture(GL_TEXTURE0 + 32);
	glBindTexture(GL_TEXTURE_2D, _whiteTexture);
	glActiveTexture(GL_TEXTURE0 + 33);
	glBindTexture(GL_TEXTURE_2D, _whiteTexture);
	glActiveTexture(GL_TEXTURE0);

	// Upload uniforms shared by both floor passes once; only per-pass values are
	// re-uploaded via configureGroundPass() before each individual pass.
	_fgShader->bind();
	_fgShader->setUniformValue("floorRendering", true);
	_fgShader->setUniformValue("groundMode", static_cast<int>(_groundMode));
	_fgShader->setUniformValue("topColor", QVector4D(_bgTopColor.red(), _bgTopColor.green(), _bgTopColor.blue(), _bgTopColor.alpha()));
	_fgShader->setUniformValue("botColor", QVector4D(_bgBotColor.red(), _bgBotColor.green(), _bgBotColor.blue(), _bgBotColor.alpha()));
	_fgShader->setUniformValue("screenSize", QVector2D(width(), height()));
	_fgShader->setUniformValue("screenCenter", _floorCenter);
	_fgShader->setUniformValue("gradientStyle", _gradientStyle);
	_fgShader->setUniformValue("floorSize", groundPlaneExtent());
	_fgShader->setUniformValue("groundReferenceSize", _floorSize);
	_fgShader->setUniformValue("worldUpAxis", _cameraUpAxisZUp ? 2 : 1);

	//https://open.gl/depthstencils
	glEnable(GL_STENCIL_TEST);
	glClear(GL_STENCIL_BUFFER_BIT);
	glStencilMask(0x0);
	glStencilFunc(GL_ALWAYS, 1, 0xFF);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	glStencilMask(0xFF);
	glDepthMask(GL_FALSE);
	glClear(GL_STENCIL_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);

	// Draw floor
	configureGroundPass(true, false);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	_floorPlane->setOpacity(0.1f);
	_floorPlane->render();
	// FloorPlane writes mesh-like material uniforms through _fgShader without
	// participating in AssImpMesh's shared material-uniform cache. Invalidate
	// that cache so subsequent scene meshes always republish their own state.
	AssImpMesh::resetSharedUniformStateCache();
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);

	
	// Draw model reflection
	glStencilFunc(GL_EQUAL, 1, 0xFF);
	glStencilMask(0x00);
	glDepthMask(GL_TRUE);

	QMatrix4x4 model;

	// Mirror the scene across the actual floor plane. Using live animated bounds
	// to derive this offset makes reflected animated parts appear to bob the
	// entire reflection up and down as their runtime AABBs change frame to frame.
	const float floorPos = _floorPlaneZ;
	if (_cameraUpAxisZUp)
	{
		model.translate(0.0f, 0.0f, 2.0f * floorPos);
		model.scale(1.0f, 1.0f, -1.0f);
	}
	else
	{
		model.translate(0.0f, 2.0f * floorPos, 0.0f);
		model.scale(1.0f, -1.0f, 1.0f);
	}

	_fgShader->bind();
	_fgShader->setUniformValue("sssCapture", false);
	_fgShader->setUniformValue("modelMatrix", model);
	TriangleMesh::setCurrentRenderContext(model, _viewMatrix);
	if (_reflectionsEnabled && drawReflection)
	{
		_fgShader->setUniformValue("renderingMode", static_cast<int>(_renderingMode));
		drawMesh(_fgShader.get());
	}

	glStencilMask(0x00);
	glDisable(GL_STENCIL_TEST);
		
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	configureGroundPass(false, _floorTextureDisplayed);
	_fgShader->setProperty("globalModelMatrix", QVariant::fromValue(_modelMatrix));
	TriangleMesh::setCurrentRenderContext(_modelMatrix, _viewMatrix);
	_floorPlane->setOpacity(0.95f);
	_floorPlane->render();
	// The final visible floor pass also mutates _fgShader material uniforms,
	// so clear the shared cache before later transparent scene draws.
	AssImpMesh::resetSharedUniformStateCache();
	glDisable(GL_CULL_FACE);
	_fgShader->bind();
	_fgShader->setUniformValue("floorRendering", false);
	_fgShader->setUniformValue("groundMode", static_cast<int>(GroundMode::None));
	_fgShader->setUniformValue("renderingMode", static_cast<int>(_renderingMode));
	glDisable(GL_BLEND);

	_fgShader->setUniformValue("envMapEnabled", _envMapEnabled);
	glActiveTexture(GL_TEXTURE0);
}

void GLWidget::drawGrid()
{
	if (!_gridShader)
		return;

	QMatrix4x4 viewProjection = _projectionMatrix * _viewMatrix;
	QMatrix4x4 inverseViewProjection = viewProjection.inverted();
	_gridShader->bind();
	_gridShader->setUniformValue("inverseViewProjectionMatrix", inverseViewProjection);
	_gridShader->setUniformValue("viewProjectionMatrix", viewProjection);
	_gridShader->setUniformValue("cameraPos", _primaryCamera->getRenderPosition());
	_gridShader->setUniformValue("screenCenter", _floorCenter);
	_gridShader->setUniformValue("groundReferenceSize", _floorSize);
	_gridShader->setUniformValue("floorSize", groundPlaneExtent());
	_gridShader->setUniformValue("gridPlaneZ", _floorPlaneZ);
	_gridShader->setUniformValue("worldUpAxis", _cameraUpAxisZUp ? 2 : 1);
	_gridShader->setUniformValue("opacity", 0.95f);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	drawFullscreenTriangle();
	glDisable(GL_BLEND);
}

float GLWidget::groundPlaneScaleFactor() const
{
	return (_groundMode == GroundMode::Grid) ? 200.0f : _floorSizeFactor;
}

float GLWidget::groundPlaneExtent() const
{
	return _floorSize * groundPlaneScaleFactor();
}

void GLWidget::drawSkyBox()
{
	_skyBox->setProg(_skyBoxShader.get());
	_skyBoxShader->bind();
	const bool usePrefilterBlur = _skyBoxBlurPercent > 0 && _prefilterMap != 0 && _prefilterMipLevels > 0;
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_CUBE_MAP, usePrefilterBlur ? _prefilterMap : _environmentMap);
	_skyBoxShader->setUniformValue("skybox", 1);
	QMatrix4x4 projection;
	projection.perspective(_skyBoxFOV, (float)width() / (float)height(), 0.1f, 100.0f);
	QMatrix4x4 view = _viewMatrix;
	// Remove translation
	view.setColumn(3, QVector4D(0, 0, 0, 1));
	QMatrix4x4 model;
	model.rotate(cameraUpAxisConventionRotation());
	if (!usePrefilterBlur)
		model.rotate(90.0f, QVector3D(1.0f, 0.0f, 0.0f)); // Z-up correction for raw env map
	model.rotate(_skyBoxZRotation, QVector3D(0.0f, 1.0f, 0.0f)); // User Z rotation (always applied)
	float skyboxLod = 0.0f;
	if (usePrefilterBlur)
	{
		// Reserve the top 10% of the old LOD range to avoid visible
		// banding/pixelation in the blurriest prefilter mips.
		const float t = (static_cast<float>(_skyBoxBlurPercent) / 100.0f) * 0.9f;
		skyboxLod = std::pow(t, 1.5f) * static_cast<float>(_prefilterMipLevels - 1);
	}
	_skyBox->setSceneRenderTransformFast(model);
	_skyBoxShader->setProperty("globalModelMatrix", QVariant::fromValue(QMatrix4x4()));
	_skyBoxShader->setProperty("viewMatrix", QVariant::fromValue(view));
	_skyBoxShader->setUniformValue("modelMatrix", model);
	_skyBoxShader->setUniformValue("viewMatrix", view);
	_skyBoxShader->setUniformValue("projectionMatrix", projection);
	_skyBoxShader->setUniformValue("hdrToneMapping", _hdrToneMapping);
	_skyBoxShader->setUniformValue("gammaCorrection", _gammaCorrection);
	_skyBoxShader->setUniformValue("screenGamma", _screenGamma);
	_skyBoxShader->setUniformValue("envMapExposure", _envMapExposure);
	_skyBoxShader->setUniformValue("iblExposure", _iblExposure);
	_skyBoxShader->setUniformValue("toneMapMode", static_cast<int>(_toneMappingMode));
	_skyBoxShader->setUniformValue("useSkyboxLod", usePrefilterBlur);
	_skyBoxShader->setUniformValue("skyboxLod", skyboxLod);
	
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL); // change depth function so depth test passes when values are equal to depth buffer's content
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	_skyBox->render();
	glDepthFunc(GL_LESS); // set depth function back to default
	glDisable((GL_DEPTH_TEST));
}

void GLWidget::drawMesh(QOpenGLShaderProgram* prog, int activeCapPlaneIndex)
{
	QVector3D camPos = _primaryCamera->getRenderPosition();
	setupClippingUniforms(prog, camPos);

	if (_meshStore.empty()) return;

	const std::vector<int>& objectIds = _visibleSwapped ? _hiddenObjectsIds : _displayedObjectsIds;

	// Split — applying cap-plane straddle culling during collection
	std::vector<int> opaqueIds;
	std::vector<std::pair<float, int>> transparent; // (distance, id)

	opaqueIds.reserve(objectIds.size());
	transparent.reserve(objectIds.size());

	for (int id : objectIds)
	{
		if (auto* mesh = _meshStore.at(id))
		{
			// Capping stencil pass: skip meshes outside frustum or that don't
			// intersect the active cap plane — they contribute nothing to stencil.
			// Skinned meshes are exempt from frustum culling (same rationale as
			// isMeshVisible): their bind-pose AABB is stale after the initial
			// animation pose is applied, so the test would incorrectly drop them.
			if (activeCapPlaneIndex >= 0)
			{
				if (!mesh->hasSkinning() && isMeshOutsideFrustum(mesh)) continue;
				if (!isMeshStraddlesCapPlane(mesh, activeCapPlaneIndex)) continue;
			}

			if (mesh->isTransparent())
			{
				// Use a stable distance metric (camera -> mesh bounds center in world space)
				const QVector3D c = mesh->getBoundingSphere().getCenter();   // return center in world space
				const float R = mesh->getBoundingSphere().getRadius();
				const float d = (c - camPos).length();     // squared is fine for sorting
				// farthest point distance
				float farthest = d + R;
				transparent.emplace_back(farthest, id);
			}
			else
			{
				opaqueIds.push_back(id);
			}
		}
	}

	// 1) OPAQUE PASS: depth test ON, depth writes ON, blending OFF
	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);

	for (int id : opaqueIds)
	{
		if (auto* mesh = _meshStore.at(id))
		{
			mesh->setProg(prog);
			//mesh->render();             // render must NOT disable depth writes here
			renderMeshExploded(mesh, _displayMode);
		}
	}

	// 2) TRANSPARENT PASS: depth test ON, depth writes OFF, blending ON
	//    sort BACK-TO-FRONT (farthest first)
	std::sort(transparent.begin(), transparent.end(),
		[](const auto& a, const auto& b) { return a.first > b.first; });

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_TRUE);

	for (auto& it : transparent)
	{
		if (auto* mesh = _meshStore.at(it.second))
		{
			mesh->setProg(prog);
			//mesh->render();             // render must preserve writes-off for this pass
			renderMeshExploded(mesh, _displayMode);
		}
	}

	// restore baseline
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
}

void GLWidget::drawOpaqueMeshes(QOpenGLShaderProgram* prog, int activeClipPlaneIndex)
{
	TriangleMesh::resetTextureBindingCacheForCurrentContext();
	TriangleMesh::resetBoundProgramCacheForCurrentContext();

	QVector3D camPos = _primaryCamera->getRenderPosition();
	setupClippingUniforms(prog, camPos);

	if (_meshStore.empty()) return;

	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);

	// Bind shader and set uniforms that are identical for every opaque mesh once,
	// outside the loop, to avoid redundant driver calls per draw.
	prog->bind();
	TriangleMesh::recordProgramBindCall(true);
	TriangleMesh::notifyProgramBound(prog);
	// Suppress hover highlighting while Ctrl is held — avoids flashes during
	// Ctrl+drag view manipulation as the pointer crosses mesh boundaries.
	const bool ctrlHeld = QGuiApplication::queryKeyboardModifiers() & Qt::ControlModifier;
	const bool hoverHighlightingEnabled = !ctrlHeld &&
		(_selectionManager->getHoverMode() != HoverHighlightMode::Disabled);
	prog->setUniformValue("hoverHighlighting", hoverHighlightingEnabled);
	prog->setUniformValue("hoverColor", QVector3D(1.0f, 0.84f, 0.0f));
	const int sssObjectIdLocation = prog->uniformLocation("sssObjectId");
	// Collect visible opaque meshes, then sort by texture signature to
	// minimise GPU texture state changes across consecutive draw calls.
	std::vector<std::pair<uint64_t, int>> opaque;
	if (_runtimeVisibilityPrepared && _runtimeVisibilityRootIndex >= 0)
	{
		std::vector<int> candidateIds;
		candidateIds.reserve((_visibleSwapped ? _hiddenObjectsIds : _displayedObjectsIds).size());
		collectVisibleMeshIdsForPass(_runtimeVisibilityRootIndex, activeClipPlaneIndex, false, candidateIds);
		opaque.reserve(candidateIds.size());
		for (int id : candidateIds)
		{
			if (auto* mesh = _meshStore.at(id))
				opaque.emplace_back(mesh->getRenderMaterialSortKey(), id);
		}
	}
	else
	{
		const std::vector<int>& objectIds = _visibleSwapped ? _hiddenObjectsIds : _displayedObjectsIds;
		opaque.reserve(objectIds.size());
		for (int id : objectIds)
		{
			if (auto* mesh = _meshStore.at(id))
				if (!mesh->isTransparent() && isMeshVisible(mesh, activeClipPlaneIndex))
					opaque.emplace_back(mesh->getRenderMaterialSortKey(), id);
		}
	}
	std::sort(opaque.begin(), opaque.end(),
		[](const auto& a, const auto& b) { return a.first < b.first; });
	for (auto& [key, id] : opaque)
	{
		if (auto* mesh = _meshStore.at(id))
		{
			mesh->setProg(prog);
			// renderMeshWithDisplayMode() releases the program after each mesh draw,
			// so the per-mesh hover uniform must always rebind before updating.
			prog->bind();
			TriangleMesh::recordProgramBindCall(true);
			prog->setUniformValue("hovered",
				hoverHighlightingEnabled && id == _selectionManager->getHoveredId());
			if (sssObjectIdLocation >= 0)
				prog->setUniformValue(sssObjectIdLocation, float(id + 1));
			renderMeshExploded(mesh, _displayMode);
		}
	}
}


void GLWidget::drawTransparentMeshes(QOpenGLShaderProgram* prog, int activeClipPlaneIndex)
{
	TriangleMesh::resetTextureBindingCacheForCurrentContext();
	TriangleMesh::resetBoundProgramCacheForCurrentContext();

	QVector3D camPos = _primaryCamera->getRenderPosition();
	setupClippingUniforms(prog, camPos);

	if (_meshStore.empty()) return;

	std::vector<std::pair<float, int>> transparent;
	if (_runtimeVisibilityPrepared && _runtimeVisibilityRootIndex >= 0)
	{
		std::vector<int> candidateIds;
		candidateIds.reserve((_visibleSwapped ? _hiddenObjectsIds : _displayedObjectsIds).size());
		collectVisibleMeshIdsForPass(_runtimeVisibilityRootIndex, activeClipPlaneIndex, true, candidateIds);
		transparent.reserve(candidateIds.size());
		for (int id : candidateIds)
		{
			if (auto* mesh = _meshStore.at(id))
			{
				const QVector3D c = mesh->getBoundingSphere().getCenter();
				const float R = mesh->getBoundingSphere().getRadius();
				const float d = (c - camPos).length();
				float farthest = d + R;
				transparent.emplace_back(farthest, id);
			}
		}
	}
	else
	{
		const std::vector<int>& objectIds = _visibleSwapped ? _hiddenObjectsIds : _displayedObjectsIds;
		transparent.reserve(objectIds.size());

		for (int id : objectIds)
		{
			if (auto* mesh = _meshStore.at(id))
			{
				if (mesh->isTransparent())
				{
					if (!isMeshVisible(mesh, activeClipPlaneIndex)) continue;
					const QVector3D c = mesh->getBoundingSphere().getCenter();
					const float R = mesh->getBoundingSphere().getRadius();
					const float d = (c - camPos).length();
					float farthest = d + R;
					transparent.emplace_back(farthest, id);
				}
			}
		}
	}

	// Sort far-to-near
	std::sort(transparent.begin(), transparent.end(),
		[](const auto& a, const auto& b) { return a.first > b.first; });

	glEnable(GL_BLEND);
	glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA,
		GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	// Bind once and set uniforms constant across all transparent meshes
	prog->bind();
	TriangleMesh::recordProgramBindCall(true);
	TriangleMesh::notifyProgramBound(prog);
	const bool ctrlHeldT = QGuiApplication::queryKeyboardModifiers() & Qt::ControlModifier;
	const bool hoverHighlightingEnabledT = !ctrlHeldT &&
		(_selectionManager->getHoverMode() != HoverHighlightMode::Disabled);
	prog->setUniformValue("hoverHighlighting", hoverHighlightingEnabledT);
	prog->setUniformValue("hoverColor", QVector3D(1.0f, 0.84f, 0.0f));
	const int sssObjectIdLocation = prog->uniformLocation("sssObjectId");
	for (auto& it : transparent)
	{
		if (auto* mesh = _meshStore.at(it.second))
		{
			const int id = it.second;
			mesh->setProg(prog);
			// renderMeshWithDisplayMode() releases the program after each mesh draw,
			// so the per-mesh hover uniform must always rebind before updating.
			prog->bind();
			TriangleMesh::recordProgramBindCall(true);
			prog->setUniformValue("hovered",
				hoverHighlightingEnabledT && id == _selectionManager->getHoveredId());
			if (sssObjectIdLocation >= 0)
				prog->setUniformValue(sssObjectIdLocation, float(id + 1));
			renderMeshExploded(mesh, _displayMode);
		}
	}

	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
}

// ---------------------------------------------------------------------------
// Visibility culling helpers
// ---------------------------------------------------------------------------

void GLWidget::invalidateRuntimeVisibilityHierarchy()
{
	_runtimeVisibilityHierarchyDirty = true;
	_runtimeVisibilityPrepared = false;
	_runtimeVisibilityRootIndex = -1;
	_runtimeVisibilityMeshStoreCount = static_cast<int>(_meshStore.size());
	_runtimeVisibilityBoundsRevision = 0;
	_runtimeVisibilityNodes.clear();
	_runtimeBaseVisibleMask.clear();
}

int GLWidget::buildRuntimeVisibilityNodeRecursive(const SceneNode* node,
                                                  const QHash<QUuid, int>& meshIndexByUuid)
{
	if (!node)
		return -1;

	const int nodeIndex = _runtimeVisibilityNodes.size();
	_runtimeVisibilityNodes.push_back(RuntimeVisibilityNode{});
	_runtimeVisibilityNodes[nodeIndex].sceneNode = node;
	_runtimeVisibilityNodes[nodeIndex].meshIndices.reserve(node->meshUuids.size());

	for (const QUuid& uuid : node->meshUuids)
	{
		const auto it = meshIndexByUuid.find(uuid);
		if (it != meshIndexByUuid.end())
			_runtimeVisibilityNodes[nodeIndex].meshIndices.push_back(it.value());
	}

	_runtimeVisibilityNodes[nodeIndex].children.reserve(node->children.size());
	for (const SceneNode* child : node->children)
	{
		const int childIndex = buildRuntimeVisibilityNodeRecursive(child, meshIndexByUuid);
		if (childIndex >= 0)
			_runtimeVisibilityNodes[nodeIndex].children.push_back(childIndex);
	}

	return nodeIndex;
}

void GLWidget::rebuildRuntimeVisibilityHierarchy()
{
	_runtimeVisibilityNodes.clear();
	_runtimeVisibilityRootIndex = -1;
	_runtimeVisibilityPrepared = false;

	if (!_viewer || !_viewer->sceneGraph())
	{
		_runtimeVisibilityHierarchyDirty = false;
		_runtimeVisibilityMeshStoreCount = static_cast<int>(_meshStore.size());
		_runtimeVisibilityBoundsRevision = 0;
		return;
	}

	QHash<QUuid, int> meshIndexByUuid;
	meshIndexByUuid.reserve(static_cast<int>(_meshStore.size()));
	for (int meshIndex = 0; meshIndex < static_cast<int>(_meshStore.size()); ++meshIndex)
	{
		if (TriangleMesh* mesh = _meshStore[meshIndex])
			meshIndexByUuid.insert(mesh->uuid(), meshIndex);
	}

	_runtimeVisibilityRootIndex = buildRuntimeVisibilityNodeRecursive(
		_viewer->sceneGraph()->root(),
		meshIndexByUuid);
	_runtimeVisibilityHierarchyDirty = false;
	_runtimeVisibilityMeshStoreCount = static_cast<int>(_meshStore.size());
	_runtimeVisibilityBoundsRevision = 0;
}

bool GLWidget::ensureRuntimeVisibilityHierarchy()
{
	if (_runtimeVisibilityHierarchyDirty ||
	    _runtimeVisibilityMeshStoreCount != static_cast<int>(_meshStore.size()))
	{
		rebuildRuntimeVisibilityHierarchy();
	}

	return _runtimeVisibilityRootIndex >= 0 &&
	       _runtimeVisibilityRootIndex < _runtimeVisibilityNodes.size();
}

bool GLWidget::refreshRuntimeVisibilityNodeBounds(
	int nodeIndex,
	const std::vector<unsigned char>& baseVisibleMask,
	bool refreshBounds)
{
	if (nodeIndex < 0 || nodeIndex >= _runtimeVisibilityNodes.size())
		return false;

	RuntimeVisibilityNode& runtimeNode = _runtimeVisibilityNodes[nodeIndex];
	bool boundsInitialized = false;
	bool hasVisibleMesh = false;
	BoundingBox bounds;

	for (int meshIndex : std::as_const(runtimeNode.meshIndices))
	{
		if (meshIndex < 0 || meshIndex >= static_cast<int>(_meshStore.size()))
			continue;
		if (meshIndex >= static_cast<int>(baseVisibleMask.size()) || !baseVisibleMask[meshIndex])
			continue;

		TriangleMesh* mesh = _meshStore[meshIndex];
		if (!mesh || !isMeshAnimationVisible(mesh))
			continue;

		if (!boundsInitialized)
		{
			bounds = mesh->getBoundingBox();
			boundsInitialized = true;
		}
		else
		{
			bounds.addBox(mesh->getBoundingBox());
		}
		hasVisibleMesh = true;
	}

	for (int childIndex : std::as_const(runtimeNode.children))
	{
		if (!refreshRuntimeVisibilityNodeBounds(childIndex, baseVisibleMask, refreshBounds))
			continue;

		const RuntimeVisibilityNode& childNode = _runtimeVisibilityNodes[childIndex];
		if (refreshBounds && !boundsInitialized)
		{
			bounds = childNode.subtreeBounds;
			boundsInitialized = true;
		}
		else if (refreshBounds)
		{
			bounds.addBox(childNode.subtreeBounds);
		}
		hasVisibleMesh = true;
	}

	runtimeNode.subtreeHasVisibleMeshes = hasVisibleMesh;
	if (refreshBounds && boundsInitialized)
		runtimeNode.subtreeBounds = bounds;

	return hasVisibleMesh;
}

void GLWidget::refreshRuntimeVisibilityCacheForCurrentView()
{
	_runtimeVisibilityPrepared = false;
	if (!ensureRuntimeVisibilityHierarchy())
		return;

	const std::vector<int>& visibleIds = _visibleSwapped ? _hiddenObjectsIds : _displayedObjectsIds;
	_runtimeBaseVisibleMask.assign(_meshStore.size(), 0u);
	for (int meshIndex : visibleIds)
	{
		if (meshIndex >= 0 && meshIndex < static_cast<int>(_runtimeBaseVisibleMask.size()))
			_runtimeBaseVisibleMask[meshIndex] = 1u;
	}

	const quint64 currentBoundsRevision = TriangleMesh::currentRuntimeBoundsRevision();
	const bool refreshBounds = (_runtimeVisibilityBoundsRevision != currentBoundsRevision);
	refreshRuntimeVisibilityNodeBounds(
		_runtimeVisibilityRootIndex,
		_runtimeBaseVisibleMask,
		refreshBounds);
	if (refreshBounds)
		_runtimeVisibilityBoundsRevision = currentBoundsRevision;
	_runtimeVisibilityPrepared = true;
}

void GLWidget::collectVisibleMeshIdsForPass(int nodeIndex,
                                            int activeClipPlaneIndex,
                                            bool wantTransparent,
                                            std::vector<int>& out) const
{
	if (nodeIndex < 0 || nodeIndex >= _runtimeVisibilityNodes.size())
		return;

	const RuntimeVisibilityNode& runtimeNode = _runtimeVisibilityNodes[nodeIndex];
	if (!runtimeNode.subtreeHasVisibleMeshes)
		return;
	if (isBoundingBoxOutsideFrustum(runtimeNode.subtreeBounds))
		return;

	if (activeClipPlaneIndex >= 0)
	{
		if (isBoundingBoxInvisibleInAllClipPasses(runtimeNode.subtreeBounds))
			return;
		if (activeClipPlaneIndex == 0 && isBoundingBoxFullyClipped_X(runtimeNode.subtreeBounds))
			return;
		if (activeClipPlaneIndex == 1 && isBoundingBoxFullyClipped_Y(runtimeNode.subtreeBounds))
			return;
		if (activeClipPlaneIndex == 2 && isBoundingBoxFullyClipped_Z(runtimeNode.subtreeBounds))
			return;
	}

	for (int meshIndex : std::as_const(runtimeNode.meshIndices))
	{
		if (meshIndex < 0 || meshIndex >= static_cast<int>(_meshStore.size()))
			continue;
		TriangleMesh* mesh = _meshStore[meshIndex];
		if (!mesh)
			continue;
		if (meshIndex >= static_cast<int>(_runtimeBaseVisibleMask.size()) ||
		    !_runtimeBaseVisibleMask[meshIndex])
			continue;
		if (!isMeshVisible(mesh, activeClipPlaneIndex))
			continue;
		if (mesh->isTransparent() == wantTransparent)
			out.push_back(meshIndex);
	}

	for (int childIndex : std::as_const(runtimeNode.children))
		collectVisibleMeshIdsForPass(childIndex, activeClipPlaneIndex, wantTransparent, out);
}

void GLWidget::extractFrustumPlanes()
{
	// Gribb-Hartmann method: rows of the combined VP matrix define the 6 frustum
	// planes in world space. Vertices are already in world space (no model matrix),
	// so VP = projectionMatrix * viewMatrix suffices.
	const QMatrix4x4 vp = _projectionMatrix * _viewMatrix;
	const QVector4D r0 = vp.row(0);
	const QVector4D r1 = vp.row(1);
	const QVector4D r2 = vp.row(2);
	const QVector4D r3 = vp.row(3);

	_frustumPlanes[0] = r3 + r0;  // left
	_frustumPlanes[1] = r3 - r0;  // right
	_frustumPlanes[2] = r3 + r1;  // bottom
	_frustumPlanes[3] = r3 - r1;  // top
	_frustumPlanes[4] = r3 + r2;  // near
	_frustumPlanes[5] = r3 - r2;  // far

	// Normalize so that w is a true signed world-space distance
	for (int i = 0; i < 6; ++i)
	{
		const float len = QVector3D(_frustumPlanes[i].x(), _frustumPlanes[i].y(), _frustumPlanes[i].z()).length();
		if (len > 1e-6f)
			_frustumPlanes[i] /= len;
	}
}

bool GLWidget::isBoundingBoxOutsideFrustum(const BoundingBox& bb) const
{
	for (int i = 0; i < 6; ++i)
	{
		const QVector4D& p = _frustumPlanes[i];
		const float sx = p.x() >= 0.0f ? static_cast<float>(bb.xMax()) : static_cast<float>(bb.xMin());
		const float sy = p.y() >= 0.0f ? static_cast<float>(bb.yMax()) : static_cast<float>(bb.yMin());
		const float sz = p.z() >= 0.0f ? static_cast<float>(bb.zMax()) : static_cast<float>(bb.zMin());
		if (p.x() * sx + p.y() * sy + p.z() * sz + p.w() < 0.0f)
			return true;
	}
	return false;
}

bool GLWidget::isMeshOutsideFrustum(const TriangleMesh* mesh) const
{
	return mesh ? isBoundingBoxOutsideFrustum(mesh->getBoundingBox()) : true;
}

bool GLWidget::isMeshFullyInsideFrustum(const TriangleMesh* mesh) const
{
	const BoundingBox& bb = mesh->getBoundingBox();
	for (int i = 0; i < 6; ++i)
	{
		const QVector4D& p = _frustumPlanes[i];
		// Negative support point: the AABB corner LEAST inside this plane.
		// If even this worst-case corner is on the positive (inside) side,
		// the whole AABB is guaranteed to lie within this half-space.
		const float sx = p.x() >= 0.0f ? static_cast<float>(bb.xMin())
		                                : static_cast<float>(bb.xMax());
		const float sy = p.y() >= 0.0f ? static_cast<float>(bb.yMin())
		                                : static_cast<float>(bb.yMax());
		const float sz = p.z() >= 0.0f ? static_cast<float>(bb.zMin())
		                                : static_cast<float>(bb.zMax());
		if (p.x() * sx + p.y() * sy + p.z() * sz + p.w() < 0.0f)
			return false;   // at least one plane has a corner outside
	}
	return true;
}

// Returns the minimum bounding-sphere radius among meshes that are completely
// enclosed within the current view frustum.  Used to derive a dynamic zoom
// floor: as the camera zooms into a sub-mesh, large meshes leave the frustum
// and the floor shrinks to match the focused geometry, preventing the eye from
// ever entering the mesh the user is actually looking at.
float GLWidget::computeFullyVisibleMinMeshRadius() const
{
	const std::vector<int>& ids =
		_visibleSwapped ? _hiddenObjectsIds : _displayedObjectsIds;

	float minRadius = std::numeric_limits<float>::max();
	for (int id : ids)
	{
		if (id < 0 || id >= static_cast<int>(_meshStore.size())) continue;
		const TriangleMesh* mesh = _meshStore[id];
		if (!mesh) continue;
		if (!isMeshAnimationVisible(mesh)) continue;   // hidden by animation

		if (mesh->hasSkinning())
		{
			// The stored AABB is the bind-pose box and is unreliable for
			// frustum testing after skinning is applied.  Skip the frustum
			// test entirely and always count the mesh — this ensures animated
			// models participate in sub-mesh zoom granularity regardless of pose.
			const float r = mesh->getBoundingSphere().getRadius();
			if (r > 0.0f)
				minRadius = std::min(minRadius, r);
			continue;
		}

		if (isMeshOutsideFrustum(mesh))      continue;   // not visible at all
		if (!isMeshFullyInsideFrustum(mesh)) continue;   // only partially in view

		const float r = mesh->getBoundingSphere().getRadius();
		if (r > 0.0f)
			minRadius = std::min(minRadius, r);
	}

	// Fall back to global scene radius when nothing qualifies
	// (e.g. zoomed so far out the whole scene spans the viewport).
	return (minRadius < std::numeric_limits<float>::max())
		? minRadius
		: _boundingSphere.getRadius();
}

// Update _zoomInLimit with asymmetric smoothing:
//   • Decreases are applied immediately so zoom-in is never blocked.
//   • Increases are blended gradually (factor 0.12 ≈ 10 events to reach ~72%)
//     so the clamp creeps back rather than snapping when the focused mesh
//     transitions out of the "fully inside" frustum set.
void GLWidget::updateZoomInLimit()
{
	const float rawFloor = computeFullyVisibleMinMeshRadius();
	if (rawFloor <= _zoomInLimit)
		_zoomInLimit = rawFloor;                                          // drop: immediate
	else
		_zoomInLimit += (rawFloor - _zoomInLimit) * 0.12f;          // rise: gradual
}

bool GLWidget::isBoundingBoxFullyClipped_X(const BoundingBox& bb) const
{
	const float tx = _clipXCoeff + static_cast<float>(_boundingBox.center().getX());
	return _clipXFlipped
		? static_cast<float>(bb.xMax()) < tx
		: static_cast<float>(bb.xMin()) > tx;
}

bool GLWidget::isBoundingBoxFullyClipped_Y(const BoundingBox& bb) const
{
	const float ty = _clipYCoeff + static_cast<float>(_boundingBox.center().getY());
	return _clipYFlipped
		? static_cast<float>(bb.yMax()) < ty
		: static_cast<float>(bb.yMin()) > ty;
}

bool GLWidget::isBoundingBoxFullyClipped_Z(const BoundingBox& bb) const
{
	const float tz = _clipZCoeff + static_cast<float>(_boundingBox.center().getZ());
	return _clipZFlipped
		? static_cast<float>(bb.zMax()) < tz
		: static_cast<float>(bb.zMin()) > tz;
}

bool GLWidget::isMeshFullyClipped_X(const TriangleMesh* mesh) const
{
	return mesh ? isBoundingBoxFullyClipped_X(mesh->getBoundingBox()) : true;
}

bool GLWidget::isMeshFullyClipped_Y(const TriangleMesh* mesh) const
{
	return mesh ? isBoundingBoxFullyClipped_Y(mesh->getBoundingBox()) : true;
}

bool GLWidget::isMeshFullyClipped_Z(const TriangleMesh* mesh) const
{
	return mesh ? isBoundingBoxFullyClipped_Z(mesh->getBoundingBox()) : true;
}

bool GLWidget::isBoundingBoxFullyKept_X(const BoundingBox& bb) const
{
	const float tx = _clipXCoeff + static_cast<float>(_boundingBox.center().getX());
	return _clipXFlipped
		? static_cast<float>(bb.xMin()) >= tx
		: static_cast<float>(bb.xMax()) <= tx;
}

bool GLWidget::isBoundingBoxFullyKept_Y(const BoundingBox& bb) const
{
	const float ty = _clipYCoeff + static_cast<float>(_boundingBox.center().getY());
	return _clipYFlipped
		? static_cast<float>(bb.yMin()) >= ty
		: static_cast<float>(bb.yMax()) <= ty;
}

bool GLWidget::isBoundingBoxFullyKept_Z(const BoundingBox& bb) const
{
	const float tz = _clipZCoeff + static_cast<float>(_boundingBox.center().getZ());
	return _clipZFlipped
		? static_cast<float>(bb.zMin()) >= tz
		: static_cast<float>(bb.zMax()) <= tz;
}

bool GLWidget::isMeshFullyKept_X(const TriangleMesh* mesh) const
{
	return mesh ? isBoundingBoxFullyKept_X(mesh->getBoundingBox()) : false;
}

bool GLWidget::isMeshFullyKept_Y(const TriangleMesh* mesh) const
{
	return mesh ? isBoundingBoxFullyKept_Y(mesh->getBoundingBox()) : false;
}

bool GLWidget::isMeshFullyKept_Z(const TriangleMesh* mesh) const
{
	return mesh ? isBoundingBoxFullyKept_Z(mesh->getBoundingBox()) : false;
}

bool GLWidget::isBoundingBoxStraddlesCapPlane(const BoundingBox& bb, int planeIndex) const
{
	switch (planeIndex)
	{
	case 0: return !isBoundingBoxFullyClipped_X(bb) && !isBoundingBoxFullyKept_X(bb);
	case 1: return !isBoundingBoxFullyClipped_Y(bb) && !isBoundingBoxFullyKept_Y(bb);
	case 2: return !isBoundingBoxFullyClipped_Z(bb) && !isBoundingBoxFullyKept_Z(bb);
	default: return true;
	}
}

bool GLWidget::isMeshStraddlesCapPlane(const TriangleMesh* mesh, int planeIndex) const
{
	return mesh ? isBoundingBoxStraddlesCapPlane(mesh->getBoundingBox(), planeIndex) : false;
}

bool GLWidget::isBoundingBoxInvisibleInAllClipPasses(const BoundingBox& bb) const
{
	if (_clipYZEnabled && !isBoundingBoxFullyClipped_X(bb)) return false;
	if (_clipZXEnabled && !isBoundingBoxFullyClipped_Y(bb)) return false;
	if (_clipXYEnabled && !isBoundingBoxFullyClipped_Z(bb)) return false;
	return true;
}

bool GLWidget::isMeshInvisibleInAllClipPasses(const TriangleMesh* mesh) const
{
	return mesh ? isBoundingBoxInvisibleInAllClipPasses(mesh->getBoundingBox()) : true;
}

bool GLWidget::isMeshAnimationVisible(const TriangleMesh* mesh) const
{
	if (!mesh)
		return false;
	if (_animatedMeshVisibilitySourceFile.isEmpty())
		return true;
	if (mesh->getSourceFile() != _animatedMeshVisibilitySourceFile)
		return true;
	return !_animatedHiddenMeshUuids.contains(mesh->uuid());
}

bool GLWidget::isMeshVisible(const TriangleMesh* mesh, int activeClipPlaneIndex) const
{
	if (!isMeshAnimationVisible(mesh)) return false;

	// 1. Frustum cull — applied in every pass, clipping or not.
	// Skip frustum culling for any skinned mesh.
	// For GPU-skinned meshes the world-space bounding box stored in the mesh
	// is the BIND-POSE (T-pose) box — it reflects the raw vertex positions
	// before joint matrices are applied.  Our viewer always applies the
	// animation pose at load time (time = 0), so the actual rendered positions
	// can differ significantly from the bind-pose box even when no animation is
	// actively playing.  A frustum test against the stale bind-pose AABB would
	// incorrectly cull visible limbs, causing them to vanish on zoom-in.
	// Skipping the test for all skinned meshes is safe: the GPU rasterizer
	// clips any geometry that is genuinely outside the viewport at no extra cost,
	// and the performance cost of drawing a few extra off-screen skinned meshes
	// is negligible compared to the cost of the skinning shader itself.
	if (!mesh->hasSkinning() && isMeshOutsideFrustum(mesh)) return false;

	// 2. No clip planes in this pass → frustum result is final
	if (activeClipPlaneIndex < 0) return true;

	// 3. Pre-pass elimination: if ALL active planes fully clip this mesh it is
	//    invisible across every union pass — skip it entirely
	if (isMeshInvisibleInAllClipPasses(mesh)) return false;

	// 4. Per-pass cull: skip if fully clipped by the one plane active in this pass
	if (activeClipPlaneIndex == 0 && isMeshFullyClipped_X(mesh)) return false;
	if (activeClipPlaneIndex == 1 && isMeshFullyClipped_Y(mesh)) return false;
	if (activeClipPlaneIndex == 2 && isMeshFullyClipped_Z(mesh)) return false;

	return true;
}

bool GLWidget::sceneHasVisibleTransmissionMaterials() const
{
	const std::vector<int>& ids = _visibleSwapped ? _hiddenObjectsIds : _displayedObjectsIds;
	for (int id : ids)
	{
		if (id < 0 || id >= static_cast<int>(_meshStore.size()))
			continue;
		const TriangleMesh* mesh = _meshStore[id];
		if (!mesh || !isMeshAnimationVisible(mesh))
			continue;

		const GLMaterial& mat = mesh->getMaterial();
		if (mat.hasTransmission() || mat.diffuseTransmissionFactor() > 0.0f)
			return true;
	}
	return false;
}

bool GLWidget::sceneHasVisibleSSSMaterials() const
{
	const std::vector<int>& ids = _visibleSwapped ? _hiddenObjectsIds : _displayedObjectsIds;
	for (int id : ids)
	{
		if (id < 0 || id >= static_cast<int>(_meshStore.size()))
			continue;
		const TriangleMesh* mesh = _meshStore[id];
		if (!mesh || !isMeshAnimationVisible(mesh))
			continue;
		if (mesh->getMaterial().hasVolumeScattering())
			return true;
	}
	return false;
}

// ---------------------------------------------------------------------------

void GLWidget::drawMeshesWithClipping(QOpenGLShaderProgram* prog,
	bool transparentPass)
{
	TriangleMesh::resetTextureBindingCacheForCurrentContext();

	//glPolygonMode(GL_FRONT_AND_BACK, _displayMode == DisplayMode::WIREFRAME ? GL_LINE : GL_FILL);
	//glLineWidth(_displayMode == DisplayMode::WIREFRAME ? 1.25 : 1.0);

	// https://stackoverflow.com/questions/16901829/how-to-clip-only-intersection-not-union-of-clipping-planes
	// If any clipping is active
	if (_clipYZEnabled || _clipZXEnabled || _clipXYEnabled)
	{
		// Then draw meshes with clip planes enabled.
		// Each pass activates one plane to produce the union of all half-spaces.
		// activeClipPlaneIndex (0/1/2) tells the draw functions which single plane
		// is active so per-pass AABB culling tests only that plane.
		if (_clipYZEnabled)
		{
			glEnable(GL_CLIP_DISTANCE0);
			if (transparentPass) drawTransparentMeshes(prog, 0);
			else                 drawOpaqueMeshes(prog, 0);
			glDisable(GL_CLIP_DISTANCE0);
		}
		if (_clipZXEnabled)
		{
			glEnable(GL_CLIP_DISTANCE1);
			if (transparentPass) drawTransparentMeshes(prog, 1);
			else                 drawOpaqueMeshes(prog, 1);
			glDisable(GL_CLIP_DISTANCE1);
		}
		if (_clipXYEnabled)
		{
			glEnable(GL_CLIP_DISTANCE2);
			if (transparentPass) drawTransparentMeshes(prog, 2);
			else                 drawOpaqueMeshes(prog, 2);
			glDisable(GL_CLIP_DISTANCE2);
		}
	}
	else
	{
		// No clipping at all — frustum culling only (activeClipPlaneIndex = -1)
		if (transparentPass) drawTransparentMeshes(prog);
		else                 drawOpaqueMeshes(prog);
	}
}


void GLWidget::setCommonUniforms(QOpenGLShaderProgram* prog, GLCamera* camera)
{
	QVector3D camPos = camera->getRenderPosition();
	QVector3D camDir = camera->getViewDir();
	const QVector3D shaderLightPos = effectiveWorldLightPosition();

	prog->setUniformValue("lightSource.position",
		shaderLightPos);
	prog->setUniformValue("modelViewMatrix", _modelViewMatrix);
	prog->setUniformValue("normalMatrix", _modelViewMatrix.normalMatrix());
	const QMatrix4x4 projMatrix = camera->getProjectionMatrix();
	prog->setUniformValue("projectionMatrix", projMatrix);
	prog->setUniformValue("inverseProjectionMatrix", projMatrix.inverted());
	prog->setUniformValue("viewportMatrix", _viewportMatrix);

	const QVector3D worldUp = currentWorldUpVector();
	QVector3D viewDir = _primaryCamera->getViewDir();
	bool floorVisible = QVector3D::dotProduct(viewDir, worldUp) < 0.0f;
	bool showShadows = (_shadowsEnabled && floorVisible && !_lowResEnabled && camera == _primaryCamera);
	const bool interactionFastPath = _lowResEnabled && (_displayedObjectsMemSize > MAX_MODEL_SIZE_BYTES);

	prog->setUniformValue("shadowsEnabled", showShadows);
	prog->setUniformValue("selfShadowsEnabled", _selfShadowsEnabled);
	prog->setUniformValue("interactionFastPath", interactionFastPath);
	prog->setUniformValue("cameraPos", camPos);
	prog->setUniformValue("cameraDir", camDir);
	prog->setUniformValue("lightPos",
		shaderLightPos);
	TriangleMesh::setCurrentRenderContext(_modelMatrix, camera->getViewMatrix());
	prog->setUniformValue("modelMatrix", _modelMatrix);
	prog->setUniformValue("viewMatrix", camera->getViewMatrix());
	prog->setUniformValue("lightSpaceMatrix", _lightSpaceMatrix);
	prog->setUniformValue("lightFarPlane", _shadowFarDist);
	prog->setUniformValue("hdrToneMapping", _hdrToneMapping);
	prog->setUniformValue("gammaCorrection", _gammaCorrection);
	prog->setUniformValue("screenGamma", _screenGamma);
	prog->setUniformValue("envMapExposure", _envMapExposure);
	prog->setUniformValue("iblExposure", _iblExposure);
	prog->setUniformValue("toneMapMode", static_cast<int>(_toneMappingMode));	
	prog->setUniformValue("selectionHighlighting", _selectionHighlighting);

	prog->setUniformValue("transmissionFramebufferSize",
		QVector2D(_transmissionTextureWidth, _transmissionTextureHeight));
	prog->setUniformValue("sssFramebufferSize",
		QVector2D(_sssTextureWidth, _sssTextureHeight));

	prog->setUniformValue("useDefaultLights", _useDefaultLights);
	prog->setUniformValue("usePunctualLights", _usePunctualLights);
	prog->setUniformValue("useIBL", _useIBL);

	prog->setUniformValue("worldUpAxis", _cameraUpAxisZUp ? 2 : 1);

	bindIBLTextures();
}


// ---------------------------------------------------------------------------
// syncUniformsToFlatShader
// Copies every active uniform from _fgShader to _fgFlatShader using
// glGetActiveUniform / glGetUniform* / glProgramUniform*.  This is called
// once per flat-shaded triangle-mesh draw so that the flat shader always has
// the same per-frame and per-mesh state as the main shader without requiring
// every setUniformValue call site to be duplicated.
//
// glProgramUniform* (OpenGL 4.1 core) writes directly to the named program
// object without needing to bind it — no pipeline state disturbance.
// ---------------------------------------------------------------------------
void GLWidget::syncUniformsToFlatShader()
{
    if (!_fgFlatShader || !_fgFlatShader->isLinked()) return;

    const GLuint srcProg = _fgShader->programId();
    const GLuint dstProg = _fgFlatShader->programId();

    GLint numUniforms = 0;
    glGetProgramiv(srcProg, GL_ACTIVE_UNIFORMS, &numUniforms);

    for (GLint idx = 0; idx < numUniforms; ++idx)
    {
        char rawName[512];
        GLsizei nameLen = 0;
        GLint   size    = 0;    // array size (1 for non-arrays)
        GLenum  type    = 0;
        glGetActiveUniform(srcProg, static_cast<GLuint>(idx),
                           static_cast<GLsizei>(sizeof(rawName)),
                           &nameLen, &size, &type, rawName);

        // glGetActiveUniform appends "[0]" for the base name of arrays.
        // Strip it so we can re-attach "[N]" ourselves per element.
        QString baseName = QString::fromLatin1(rawName, nameLen);
        if (baseName.endsWith(QLatin1String("[0]")))
            baseName.chop(3);

        for (GLint elem = 0; elem < size; ++elem)
        {
            const QString elemName = (size > 1)
                ? QStringLiteral("%1[%2]").arg(baseName).arg(elem)
                : baseName;

            const QByteArray en   = elemName.toLatin1();
            const char*      encs = en.constData();

            const GLint srcLoc = glGetUniformLocation(srcProg, encs);
            const GLint dstLoc = glGetUniformLocation(dstProg, encs);
            if (srcLoc < 0 || dstLoc < 0) continue;

            switch (type)
            {
            case GL_FLOAT: {
                GLfloat v; glGetUniformfv(srcProg, srcLoc, &v);
                glProgramUniform1f(dstProg, dstLoc, v);
                break;
            }
            case GL_FLOAT_VEC2: {
                GLfloat v[2]; glGetUniformfv(srcProg, srcLoc, v);
                glProgramUniform2fv(dstProg, dstLoc, 1, v);
                break;
            }
            case GL_FLOAT_VEC3: {
                GLfloat v[3]; glGetUniformfv(srcProg, srcLoc, v);
                glProgramUniform3fv(dstProg, dstLoc, 1, v);
                break;
            }
            case GL_FLOAT_VEC4: {
                GLfloat v[4]; glGetUniformfv(srcProg, srcLoc, v);
                glProgramUniform4fv(dstProg, dstLoc, 1, v);
                break;
            }
            case GL_INT:
            case GL_BOOL: {
                GLint v; glGetUniformiv(srcProg, srcLoc, &v);
                glProgramUniform1i(dstProg, dstLoc, v);
                break;
            }
            case GL_UNSIGNED_INT: {
                GLuint v; glGetUniformuiv(srcProg, srcLoc, &v);
                glProgramUniform1ui(dstProg, dstLoc, v);
                break;
            }
            case GL_FLOAT_MAT2: {
                GLfloat v[4]; glGetUniformfv(srcProg, srcLoc, v);
                glProgramUniformMatrix2fv(dstProg, dstLoc, 1, GL_FALSE, v);
                break;
            }
            case GL_FLOAT_MAT3: {
                GLfloat v[9]; glGetUniformfv(srcProg, srcLoc, v);
                glProgramUniformMatrix3fv(dstProg, dstLoc, 1, GL_FALSE, v);
                break;
            }
            case GL_FLOAT_MAT4: {
                GLfloat v[16]; glGetUniformfv(srcProg, srcLoc, v);
                glProgramUniformMatrix4fv(dstProg, dstLoc, 1, GL_FALSE, v);
                break;
            }
            // Sampler types: the uniform value is the texture-unit integer.
            // Texture bindings themselves are global GL state — just copy the unit.
            case GL_SAMPLER_1D:
            case GL_SAMPLER_2D:
            case GL_SAMPLER_3D:
            case GL_SAMPLER_CUBE:
            case GL_SAMPLER_2D_ARRAY:
            case GL_SAMPLER_2D_SHADOW:
            case GL_SAMPLER_CUBE_SHADOW:
            case GL_SAMPLER_BUFFER:
            case GL_INT_SAMPLER_1D:
            case GL_INT_SAMPLER_2D:
            case GL_INT_SAMPLER_3D:
            case GL_INT_SAMPLER_BUFFER:
            case GL_UNSIGNED_INT_SAMPLER_1D:
            case GL_UNSIGNED_INT_SAMPLER_2D:
            case GL_UNSIGNED_INT_SAMPLER_3D:
            case GL_UNSIGNED_INT_SAMPLER_BUFFER: {
                GLint v; glGetUniformiv(srcProg, srcLoc, &v);
                glProgramUniform1i(dstProg, dstLoc, v);
                break;
            }
            default:
                break;
            }
        }
    }
}


void GLWidget::drawSectionCapping()
{
	// We use a lightweight shader without lighting and stuff for drawing the clipped mesh
	_clippedMeshShader->bind();
	_clippedMeshShader->setUniformValue("modelMatrix", _modelMatrix);
	_clippedMeshShader->setUniformValue("viewMatrix", _viewMatrix);
	_clippedMeshShader->setUniformValue("projectionMatrix", _projectionMatrix);
	QVector3D pos = _primaryCamera->getRenderPosition();

	_clippedMeshShader->setUniformValue("clipPlaneX", QVector4D(_modelViewMatrix.map(QVector3D(_clipXFlipped ? 1 : -1, 0, 0) + pos),
		(_clipXFlipped ? 1 : -1) * (pos.x() - (_clipXCoeff + _boundingBox.center().getX()))));
	_clippedMeshShader->setUniformValue("clipPlaneY", QVector4D(_modelViewMatrix.map(QVector3D(0, _clipYFlipped ? 1 : -1, 0) + pos),
		(_clipYFlipped ? 1 : -1) * (pos.y() - (_clipYCoeff + _boundingBox.center().getY()))));
	_clippedMeshShader->setUniformValue("clipPlaneZ", QVector4D(_modelViewMatrix.map(QVector3D(0, 0, _clipZFlipped ? 1 : -1) + pos),
		(_clipZFlipped ? 1 : -1) * (pos.z() - (_clipZCoeff + _boundingBox.center().getZ()))));
	_clippedMeshShader->setUniformValue("clipPlane", QVector4D(_modelViewMatrix.map(QVector3D(_clipDX, _clipDY, _clipDZ) + pos),
		pos.x() * _clipDX + pos.y() * _clipDY + pos.z() * _clipDZ));

	for (int i = 0; i < 3; ++i)
	{
		// Clipping Planes
		if (_clipYZEnabled && i == 0)
			glEnable(GL_CLIP_DISTANCE0);
		if (_clipZXEnabled && i == 1)
			glEnable(GL_CLIP_DISTANCE1);
		if (_clipXYEnabled && i == 2)
			glEnable(GL_CLIP_DISTANCE2);

		// https://www.opengl.org/archives/resources/code/samples/advanced/advanced97/notes/node10.html
		// https://glbook.gamedev.net/GLBOOK/glbook.gamedev.net/moglgp/advclip.html
		// https://stackoverflow.com/questions/16901829/how-to-clip-only-intersection-not-union-of-clipping-planes
		// 1) The stencil buffer, color buffer, and depth buffer are cleared,
		glClear(GL_STENCIL_BUFFER_BIT);
		glStencilMask(0x0);
		glDisable(GL_DEPTH_TEST);
		// and color buffer writes are disabled.
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

		glEnable(GL_STENCIL_TEST);
		glStencilMask(0xFF);
		glStencilFunc(GL_ALWAYS, 0, 0);

		// 2) The capping polygon is rendered into the depth buffer,
		// drawCappingPlane

		// then depth buffer writes are disabled.
		glDepthMask(GL_FALSE);

		// 3) The stencil operation is set to increment the stencil value where the depth test passes,
		glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);

		// and the model is drawn with glCullFace(GL FRONT).
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
		drawMesh(_clippedMeshShader.get(), i);

		// 4) The stencil operation is then set to decrement the stencil value where the depth test passes,
		glStencilOp(GL_KEEP, GL_KEEP, GL_DECR);

		// and the model is drawn with glCullFace(GL BACK)
		glCullFace(GL_BACK);
		drawMesh(_clippedMeshShader.get(), i);
		glDisable(GL_CULL_FACE);

		//At this point, the stencil buffer is 1 wherever the clipping plane is enclosed by
		// the frontfacing and backfacing surfaces of the object.
		// 5) The depth buffer is cleared, color buffer writes are enabled,
		//glClear(GL_DEPTH_BUFFER_BIT);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glEnable(GL_DEPTH_TEST);

		// and the polygon representing the clipping plane is now drawn using whatever material properties are desired,
		// with the stencil function set to GL EQUAL and the reference value set to 1.
		// This draws the color and depth values of the cap into the framebuffer only where the stencil values equal 1.
		glStencilFunc(GL_EQUAL, 1, 0xFF);		
		glDepthMask(GL_TRUE);
		glEnable(GL_DEPTH_TEST);
		glDisable(GL_CLIP_DISTANCE0);
		glDisable(GL_CLIP_DISTANCE1);
		glDisable(GL_CLIP_DISTANCE2);
		// drawCappingPlane
		{
			QMatrix4x4 model;
			Point P = _boundingBox.center();

			_clippingPlaneShader->bind();
			_clippingPlaneShader->setProperty("globalModelMatrix", QVariant::fromValue(QMatrix4x4()));
			_clippingPlaneShader->setProperty("viewMatrix", QVariant::fromValue(_viewMatrix));
			_clippingPlaneShader->setUniformValue("viewMatrix", _viewMatrix);
			_clippingPlaneShader->setUniformValue("projectionMatrix", _projectionMatrix);
			glActiveTexture(GL_TEXTURE6);
			glBindTexture(GL_TEXTURE_2D, _cappingTexture);
			_clippingPlaneShader->setUniformValue("hatchMap", 6);
			float yAng = _clipXFlipped || _clipXCoeff > 0 ? 90.0f : -90.0f;
			float xAng = _clipYFlipped || _clipYCoeff > 0 ? 90.0f : -90.0f;
			float zAng = _clipZFlipped || _clipZCoeff > 0 ? 0.0f : 180.0f;

			bool wantTexture = _hatchMode == ClippingPlaneHatchMode::TEXTURE/* read from UI or stored flag */;
			bool wantFlipU = false/* read from UI or stored flag */;
			bool wantFlipV = false/* read from UI or stored flag */;

			// Pick a consistent density: e.g., ~3 tiles across the model diagonal
			const float sceneDiag = _boundingBox.boundingRadius() * 2.0f;
			const float tilesAcross = wantTexture ? 3.0f : _hatchTiling;
			const float worldUnitsPerTile = sceneDiag / tilesAcross;

			_clippingPlaneShader->setUniformValue("worldUnitsPerTile", worldUnitsPerTile);
			// procedural hatch params (tweak to taste)
			_clippingPlaneShader->setUniformValue("hatchThickness", _hatchThickness);
			_clippingPlaneShader->setUniformValue("hatchIntensity", _hatchIntensity);
			_clippingPlaneShader->setUniformValue("hatchLayers", _hatchLayers);
			_clippingPlaneShader->setUniformValue("hatchLineColor", _hatchLineColor);			
			_clippingPlaneShader->setUniformValue("hatchPattern", static_cast<int>(_hatchPattern));
			
			_clippingPlaneShader->setUniformValue("useTexture", wantTexture);

			// texture flip control: (1,1) normal; (-1,1) flip U; (1,-1) flip V			
			QVector2D texFlip = QVector2D(wantFlipU ? -1.0f : 1.0f, wantFlipV ? -1.0f : 1.0f);
			_clippingPlaneShader->setUniformValue("textureFlip", texFlip);

			// YZ Plane			
			model.translate(QVector3D(P.getX(), P.getY(), P.getZ()));
			model.rotate(yAng, QVector3D(0.0f, 1.0f, 0.0f));
			_clippingPlaneShader->bind();
			_clippingPlaneYZ->setSceneRenderTransformFast(model);
			_clippingPlaneShader->setUniformValue("planeColor", QVector3D(0.20f, 0.5f, 0.5f));			
			if (_clipYZEnabled && i == 0)
			{
				const float xPlane = P.getX() + _clipXCoeff;
				// Origin at plane through bbox center
				_clippingPlaneShader->setUniformValue("hatchOrigin", QVector3D(xPlane, P.getY(), P.getZ()));
				// World-space basis on the plane: U=+Y, V=+Z
				_clippingPlaneShader->setUniformValue("uDir", QVector3D(0.f, 1.f, 0.f));
				_clippingPlaneShader->setUniformValue("vDir", QVector3D(0.f, 0.f, 1.f));
				_clippingPlaneYZ->render();
			}

			// ZX Plane
			model.setToIdentity();
			model.translate(QVector3D(P.getX(), P.getY(), P.getZ()));
			model.rotate(xAng, QVector3D(1.0f, 0.0f, 0.0f));
			_clippingPlaneShader->bind();
			_clippingPlaneZX->setSceneRenderTransformFast(model);
			_clippingPlaneShader->setUniformValue("planeColor", QVector3D(0.5f, 0.20f, 0.5f));
			if (_clipZXEnabled && i == 1)
			{
				const float yPlane = P.getY() + _clipYCoeff;
				_clippingPlaneShader->setUniformValue("hatchOrigin", QVector3D(P.getX(), yPlane, P.getZ()));
				// U=+Z, V=+X
				_clippingPlaneShader->setUniformValue("uDir", QVector3D(0.f, 0.f, 1.f));
				_clippingPlaneShader->setUniformValue("vDir", QVector3D(1.f, 0.f, 0.f));
				_clippingPlaneZX->render();
			}

			// XY Plane
			model.setToIdentity();
			model.translate(QVector3D(P.getX(), P.getY(), P.getZ()));
			model.rotate(zAng, QVector3D(1.0f, 0.0f, 0.0f));
			_clippingPlaneShader->bind();
			_clippingPlaneXY->setSceneRenderTransformFast(model);
			_clippingPlaneShader->setUniformValue("planeColor", QVector3D(0.5f, 0.5f, 0.20f));
			if (_clipXYEnabled && i == 2)
			{
				const float zPlane = P.getZ() + _clipZCoeff;
				_clippingPlaneShader->setUniformValue("hatchOrigin", QVector3D(P.getX(), P.getY(), zPlane));
				// U=+X, V=+Y
				_clippingPlaneShader->setUniformValue("uDir", QVector3D(1.f, 0.f, 0.f));
				_clippingPlaneShader->setUniformValue("vDir", QVector3D(0.f, 1.f, 0.f));
				_clippingPlaneXY->render();
			}
		}

		// Clipping Planes
		if (_clipYZEnabled && i == 0)
			glDisable(GL_CLIP_DISTANCE0);
		if (_clipZXEnabled && i == 1)
			glDisable(GL_CLIP_DISTANCE1);
		if (_clipXYEnabled && i == 2)
			glDisable(GL_CLIP_DISTANCE2);
	}

	// 6) Finally, stenciling is disabled, the OpenGL clipping plane is applied, and the
	// clipped object is drawn with color and depth enabled.
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_CULL_FACE);	
}

void GLWidget::drawVertexNormals()
{
	QVector3D pos = _primaryCamera->getRenderPosition();
	setupClippingUniforms(_vertexNormalShader.get(), pos);

	if (_meshStore.size() != 0)
	{
		for (int i : (_visibleSwapped ? _hiddenObjectsIds : _displayedObjectsIds))
		{
			if (_showVertexNormals)
			{
				TriangleMesh* mesh = _meshStore.at(i);
				mesh->setProg(_vertexNormalShader.get());
				mesh->getVAO().bind();
				glDrawElements(GL_TRIANGLES, static_cast<int>(mesh->getPoints().size()), GL_UNSIGNED_INT, 0);
				mesh->getVAO().release();
			}
		}
	}
}

void GLWidget::drawFaceNormals()
{
	QVector3D pos = _primaryCamera->getRenderPosition();
	setupClippingUniforms(_faceNormalShader.get(), pos);

	if (_meshStore.size() != 0)
	{
		for (int i : (_visibleSwapped ? _hiddenObjectsIds : _displayedObjectsIds))
		{
			if (_showFaceNormals)
			{
				TriangleMesh* mesh = _meshStore.at(i);
				mesh->setProg(_faceNormalShader.get());
				mesh->getVAO().bind();
				glDrawElements(GL_TRIANGLES, static_cast<int>(mesh->getPoints().size()), GL_UNSIGNED_INT, 0);
				mesh->getVAO().release();
			}
		}
	}
}

void GLWidget::drawAxis(GLCamera* camera)
{
	if (!camera)
		return;

	const float axisViewRange = std::max(camera->getViewRange(), 0.0001f);
	float size = 15;
	// Labels
	QVector3D xAxis(axisViewRange / size, 0, 0);
	xAxis = xAxis.project(_modelViewMatrix, _projectionMatrix, QRect(0, 0, width(), height()));
	_axisTextRenderer->RenderText(_labelAxisX.toStdString(), xAxis.x(), height() - xAxis.y(), 1, QVector3D(1.0f, 1.0f, 0.0f), TextRenderer::VAlignment::VBOTTOM);

	QVector3D yAxis(0, axisViewRange / size, 0);
	yAxis = yAxis.project(_modelViewMatrix, _projectionMatrix, QRect(0, 0, width(), height()));
	_axisTextRenderer->RenderText(_labelAxisY.toStdString(), yAxis.x(), height() - yAxis.y(), 1, QVector3D(1.0f, 1.0f, 0.0f), TextRenderer::VAlignment::VBOTTOM);

	QVector3D zAxis(0, 0, axisViewRange / size);
	zAxis = zAxis.project(_modelViewMatrix, _projectionMatrix, QRect(0, 0, width(), height()));
	_axisTextRenderer->RenderText(_labelAxisZ.toStdString(), zAxis.x(), height() - zAxis.y(), 1, QVector3D(1.0f, 1.0f, 0.0f), TextRenderer::VAlignment::VBOTTOM);

	// Axes Lines
	if (!_axisVAO.isCreated())
	{
		_axisVAO.create();
		_axisVAO.bind();
	}

	// Vertex Buffer
	if (!_axisVBO.isCreated())
	{
		_axisVBO = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
		_axisVBO.create();
	}
	_axisVBO.bind();
	_axisVBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
	std::vector<float> vertices = {
		0, 0, 0,
		axisViewRange / size, 0, 0,
		0, 0, 0,
		0, axisViewRange / size, 0,
		0, 0, 0,
		0, 0, axisViewRange / size };
	_axisVBO.allocate(vertices.data(), static_cast<int>(vertices.size() * sizeof(float)));

	// Color Buffer
	if (!_axisCBO.isCreated())
	{
		_axisCBO = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
		_axisCBO.create();
	}
	_axisCBO.bind();
	_axisCBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
	std::vector<float> colors = {
		1, 0, 0,
		1, 0, 0,
		0, 1, 0,
		0, 1, 0,
		0, 0, 1,
		0, 0, 1 };
	_axisCBO.allocate(colors.data(), static_cast<int>(colors.size() * sizeof(float)));

	_axisShader->bind();

	_axisVBO.bind();
	_axisShader->enableAttributeArray("vertexPosition");
	_axisShader->setAttributeBuffer("vertexPosition", GL_FLOAT, 0, 3);

	_axisCBO.bind();
	_axisShader->enableAttributeArray("vertexColor");
	_axisShader->setAttributeBuffer("vertexColor", GL_FLOAT, 0, 3);

	_axisShader->setUniformValue("modelViewMatrix", _modelViewMatrix);
	_axisShader->setUniformValue("projectionMatrix", _projectionMatrix);

	_axisShader->setUniformValue("renderCone", false);

	_axisVAO.bind();
	glLineWidth(2.5);
	glDrawArrays(GL_LINES, 0, 6);
	glLineWidth(1);

	// Axes Cones
	// X Axis
	_axisCone->setParameters(axisViewRange / size / 15, axisViewRange / size / 5, 8.0f, 1.0f);
	_axisShader->setUniformValue("renderCone", true);
	QMatrix4x4 model;
	model.translate(axisViewRange / size, 0, 0);
	model.rotate(90, QVector3D(0, 1.0f, 0));
	_axisShader->setUniformValue("coneColor", QVector3D(1.0f, 0.0, 0.0));
	_axisShader->setUniformValue("modelViewMatrix", _viewMatrix * model);
	_axisCone->getVAO().bind();
	glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(_axisCone->getPoints().size()), GL_UNSIGNED_INT, 0);
	_axisCone->getVAO().release();

	// Y Axis
	model.setToIdentity();
	model.translate(0, axisViewRange / size, 0);
	model.rotate(90, QVector3D(-1.0f, 0, 0));
	_axisShader->bind();
	_axisShader->setUniformValue("coneColor", QVector3D(0.0, 1.0f, 0.0));
	_axisShader->setUniformValue("modelViewMatrix", _viewMatrix * model);
	_axisCone->getVAO().bind();
	glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(_axisCone->getPoints().size()), GL_UNSIGNED_INT, 0);
	_axisCone->getVAO().release();

	// Z Axis
	model.setToIdentity();
	model.translate(0, 0, axisViewRange / size);
	_axisShader->bind();
	_axisShader->setUniformValue("coneColor", QVector3D(0.0, 0.0, 1.0f));
	_axisShader->setUniformValue("modelViewMatrix", _viewMatrix * model);
	_axisCone->getVAO().bind();
	glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(_axisCone->getPoints().size()), GL_UNSIGNED_INT, 0);
	_axisCone->getVAO().release();

	_axisVAO.release();
	_axisShader->release();
}

void GLWidget::drawTransformGizmo(GLCamera* camera)
{
	if (!_transformGizmo || !_transformGizmoRequested)
		return;

	syncTransformGizmoToSelection();
	if (!_transformGizmo->isVisible())
		return;

	const BoundingSphere selectionSphere = computeTransformGizmoSelectionSphere();
	const float selectionRadius = selectionSphere.getRadius() > 0.0f
		? selectionSphere.getRadius()
		: _boundingSphere.getRadius();
	const float fallbackScale = (std::max)(selectionRadius * 0.9f, 0.01f);
	_transformGizmo->render(_axisShader.get(), _axisCone, camera, _viewMatrix, _projectionMatrix, fallbackScale);
}

BoundingSphere GLWidget::computeTransformGizmoSelectionSphere() const
{
	if (!_viewer)
		return BoundingSphere();

	const std::vector<int> selectedIds = activeTransformGizmoSelectionIds();
	BoundingSphere combinedSphere;
	QVector<QVector3D> centers;
	QVector<float> radii;

	for (int id : selectedIds)
	{
		if (id < 0 || id >= static_cast<int>(_meshStore.size()))
			continue;

		TriangleMesh* mesh = _meshStore[id];
		if (!mesh)
			continue;

		centers.push_back(mesh->getStableTransformCenter());
		radii.push_back(mesh->getStableTransformRadius());
	}

	if (centers.isEmpty())
		return BoundingSphere();

	QVector3D cog(0.0f, 0.0f, 0.0f);
	for (const QVector3D& center : centers)
		cog += center;
	cog /= static_cast<float>(centers.size());

	float combinedRadius = 0.0f;
	for (int i = 0; i < centers.size(); ++i)
	{
		combinedRadius = (std::max)(combinedRadius, (centers[i] - cog).length() + radii[i]);
	}

	combinedSphere.setCenter(cog);
	combinedSphere.setRadius(combinedRadius);
	return combinedSphere;
}

QVector3D GLWidget::computeTransformGizmoPivot() const
{
	return computeTransformGizmoSelectionSphere().getCenter();
}

void GLWidget::applyExplodedViewManualPlacementSessionTransform()
{
	if (_explodedViewManualSessionStartStates.isEmpty())
		return;

	QMatrix4x4 rotationAroundPivot;
	rotationAroundPivot.setToIdentity();
	rotationAroundPivot.translate(_explodedViewManualSessionStartPivot);
	rotationAroundPivot.rotate(_explodedViewManualSessionRotationQuat);
	rotationAroundPivot.translate(-_explodedViewManualSessionStartPivot);

	QMatrix4x4 translationMatrix;
	translationMatrix.setToIdentity();
	translationMatrix.translate(_explodedViewManualSessionTranslationDelta);

	for (auto it = _explodedViewManualSessionStartStates.cbegin();
	     it != _explodedViewManualSessionStartStates.cend(); ++it)
	{
		TriangleMesh* mesh = getMeshByUuid(it.key());
		if (!mesh)
			continue;

		const TransformState& startState = it.value();
		const QMatrix4x4 startMatrix = _explodedViewManualSessionStartMatrices.value(
			it.key(), mesh->getExplodedViewTransformation());
		const QMatrix4x4 combinedMatrix = translationMatrix * rotationAroundPivot * startMatrix;
		const QVector3D exactTranslation(
			combinedMatrix(0, 3),
			combinedMatrix(1, 3),
			combinedMatrix(2, 3));

		const QQuaternion baseQuat = startState.hasExactRotation
			? startState.rotationQuat.normalized()
			: quaternionFromMeshEuler(startState.rotation);
		const QQuaternion exactRotationQuat =
			(_explodedViewManualSessionRotationQuat * baseQuat).normalized();

		QMatrix4x4 displayRotationMatrix;
		displayRotationMatrix.setToIdentity();
		displayRotationMatrix.rotate(exactRotationQuat);
		const QVector3D displayRotation = extractMeshRotationFromMatrix(displayRotationMatrix);

		mesh->setExplodedViewTranslationFast(exactTranslation);
		mesh->setExplodedViewRotationQuaternionFast(exactRotationQuat, displayRotation);
		mesh->setExplodedViewScalingFast(startState.scale);
	}

	_shadowMapNeedsInitialization = true;
	update();
}

std::vector<int> GLWidget::activeTransformGizmoSelectionIds() const
{
	if (_explodedViewManualPlacementActive)
	{
		std::vector<int> lockedIds;
		lockedIds.reserve(_explodedViewManualPlacementSessionUuids.size());
		for (const QUuid& uuid : _explodedViewManualPlacementSessionUuids)
		{
			const int id = getIndexByUuid(uuid);
			if (id >= 0)
				lockedIds.push_back(id);
		}
		return lockedIds;
	}

	return _viewer ? _viewer->getSelectedIDs() : std::vector<int>();
}

void GLWidget::syncTransformGizmoToSelection()
{
	if (!_transformGizmo)
		return;

	if (!_transformGizmoRequested || !_viewer)
	{
		if (_transformGizmoTranslating)
			finishTransformGizmoTranslationDrag(false);
		if (_transformGizmoScaling)
			finishTransformGizmoScaleDrag(false);
		if (_transformGizmoRotating)
			finishTransformGizmoRotationDrag(false);
		_transformGizmo->setVisible(false);
		return;
	}

	const std::vector<int> selectedIds = activeTransformGizmoSelectionIds();
	if (selectedIds.empty())
	{
		if (_transformGizmoTranslating)
			finishTransformGizmoTranslationDrag(false);
		if (_transformGizmoScaling)
			finishTransformGizmoScaleDrag(false);
		if (_transformGizmoRotating)
			finishTransformGizmoRotationDrag(false);
		_transformGizmo->setVisible(false);
		return;
	}

	if (_transformGizmoRotating || _transformGizmoScaling)
		_transformGizmo->setPivot(_transformGizmoStartPivot);
	else
		_transformGizmo->setPivot(computeTransformGizmoPivot());
	_transformGizmo->setVisible(true);
}

bool GLWidget::beginTransformGizmoDrag(TransformGizmo::Handle handle, const QPoint& pixel)
{
	switch (handle)
	{
	case TransformGizmo::Handle::TranslateX:
	case TransformGizmo::Handle::TranslateY:
	case TransformGizmo::Handle::TranslateZ:
		return beginTransformGizmoTranslationDrag(handle, pixel);
	case TransformGizmo::Handle::UniformScale:
		return beginTransformGizmoScaleDrag(handle, pixel, true);
	case TransformGizmo::Handle::RotateXY:
	case TransformGizmo::Handle::RotateYZ:
	case TransformGizmo::Handle::RotateZX:
		return beginTransformGizmoRotationDrag(handle, pixel);
	default:
		return false;
	}
}

bool GLWidget::beginTransformGizmoTranslationDrag(TransformGizmo::Handle handle, const QPoint& pixel)
{
	if (!_transformGizmoRequested || !_transformGizmo || !_viewer)
		return false;

	QVector3D axis;
	switch (handle)
	{
	case TransformGizmo::Handle::TranslateX:
		axis = QVector3D(1.0f, 0.0f, 0.0f);
		break;
	case TransformGizmo::Handle::TranslateY:
		axis = QVector3D(0.0f, 1.0f, 0.0f);
		break;
	case TransformGizmo::Handle::TranslateZ:
		axis = QVector3D(0.0f, 0.0f, 1.0f);
		break;
	default:
		return false;
	}

	_transformGizmoTranslating = true;
	_transformGizmoScaling = false;
	_transformGizmoUniformScaling = false;
	_transformGizmoRotating = false;
	_transformGizmoDragStartPixel = pixel;
	_transformGizmoDragAxis = axis;
	const BoundingSphere selectionSphere = computeTransformGizmoSelectionSphere();
	_transformGizmoStartPivot = selectionSphere.getCenter();
	const float selectionRadius = selectionSphere.getRadius() > 0.0f
		? selectionSphere.getRadius()
		: _boundingSphere.getRadius();
	_transformGizmoDragScale = (std::max)(selectionRadius * 0.9f, 0.01f);
	_transformGizmoStartStates.clear();
	_transformGizmoStartCenters.clear();
	_transformGizmoStartMatrices.clear();
	_transformGizmoCurrentTranslationDelta = QVector3D(0.0f, 0.0f, 0.0f);
	_transformGizmoCurrentScaleDelta = QVector3D(1.0f, 1.0f, 1.0f);
	_transformGizmoCurrentRotationDelta = QVector3D(0.0f, 0.0f, 0.0f);
	_transformGizmoLoggedTranslationUpdate = false;
	if (_explodedViewManualPlacementActive)
		_explodedViewManualDragStartTranslationDelta = _explodedViewManualSessionTranslationDelta;

	for (int id : activeTransformGizmoSelectionIds())
	{
		if (id < 0 || id >= static_cast<int>(_meshStore.size()))
			continue;

		if (TriangleMesh* mesh = _meshStore[id])
		{
			_transformGizmoStartStates[id] = _explodedViewManualPlacementActive
				? explodedViewTransformState(mesh)
				: TransformState(
					mesh->getTranslation(),
					mesh->getRotation(),
					mesh->getScaling(),
					mesh->getRotationQuaternion());
			_transformGizmoStartCenters[id] = mesh->getBoundingSphere().getCenter();
			_transformGizmoStartMatrices[id] = _explodedViewManualPlacementActive
				? explodedViewTransformMatrix(mesh)
				: mesh->getTransformation();
		}
	}

	if (_viewer->tabWidgetVizAttribs->currentWidget() == _viewer->transformationsPage)
	{
		_viewer->objectTransformPanel->setTranslationValues(QVector3D(0.0f, 0.0f, 0.0f));
	}

	return !_transformGizmoStartStates.isEmpty();
}

void GLWidget::updateTransformGizmoTranslationDrag(const QPoint& pixel)
{
	if (!_transformGizmoTranslating || !_viewer || _transformGizmoStartStates.isEmpty())
		return;

	const QRect viewport = getViewportFromPoint(pixel);
	const GLCamera* camera = getCameraForPoint(pixel);
	if (!camera)
		return;

	const QMatrix4x4 viewMatrix = camera->getViewMatrix();
	const QMatrix4x4 projectionMatrix = camera->getProjectionMatrix();
	const QVector3D pivotScreen3 = _transformGizmoStartPivot.project(viewMatrix, projectionMatrix, viewport);
	const QVector3D axisEndWorld = _transformGizmoStartPivot + (_transformGizmoDragAxis * _transformGizmoDragScale);
	const QVector3D axisEndScreen3 = axisEndWorld.project(viewMatrix, projectionMatrix, viewport);

	const QVector2D pivotScreen(pivotScreen3.x(), pivotScreen3.y());
	const QVector2D axisScreen = QVector2D(axisEndScreen3.x(), axisEndScreen3.y()) - pivotScreen;
	const float axisScreenLength = axisScreen.length();
	if (axisScreenLength <= 1.0e-4f)
		return;

	const QVector2D axisScreenDir = axisScreen / axisScreenLength;
	const QVector2D mouseDelta = QVector2D(pixel.x() - _transformGizmoDragStartPixel.x(),
		_transformGizmoDragStartPixel.y() - pixel.y());
	const float projectedPixels = QVector2D::dotProduct(mouseDelta, axisScreenDir);
	const float worldDistance = (projectedPixels / axisScreenLength) * _transformGizmoDragScale;
	_transformGizmoCurrentTranslationDelta = _transformGizmoDragAxis * worldDistance;

	for (auto it = _transformGizmoStartStates.begin(); it != _transformGizmoStartStates.end(); ++it)
	{
		const int id = it.key();
		if (id < 0 || id >= static_cast<int>(_meshStore.size()))
			continue;

		if (TriangleMesh* mesh = _meshStore[id])
		{
			const TransformState& startState = it.value();
			// Use fast setters during drag — only updates AABB from 8 corners (O(1))
			// instead of re-transforming all vertices (O(N)) for each mouse event.
			if (_explodedViewManualPlacementActive)
			{
				mesh->setExplodedViewTranslationFast(startState.translation + _transformGizmoCurrentTranslationDelta);
				if (startState.hasExactRotation)
					mesh->setExplodedViewRotationQuaternionFast(startState.rotationQuat, startState.rotation);
				else
					mesh->setExplodedViewRotationFast(startState.rotation);
				mesh->setExplodedViewScalingFast(startState.scale);
			}
			else
			{
				mesh->setTranslationFast(startState.translation + _transformGizmoCurrentTranslationDelta);
				mesh->setRotationFast(startState.rotation);
				mesh->setScalingFast(startState.scale);
			}
		}
	}

	if (_viewer->tabWidgetVizAttribs->currentWidget() == _viewer->transformationsPage)
	{
		_viewer->objectTransformPanel->setTranslationValues(_transformGizmoCurrentTranslationDelta);
	}
	if (_explodedViewManualPlacementActive)
	{
		_explodedViewManualSessionTranslationDelta =
			_explodedViewManualDragStartTranslationDelta + _transformGizmoCurrentTranslationDelta;
		emit explodedViewManualPlacementChanged();
	}

	_shadowMapNeedsInitialization = true;
	update();
}

void GLWidget::finishTransformGizmoTranslationDrag(bool commit)
{
	if (!_transformGizmoTranslating)
		return;

	_transformGizmoTranslating = false;

	if (!_viewer || _transformGizmoStartStates.isEmpty())
		return;

	QMap<QUuid, TransformState> oldStatesByUuid;
	QMap<QUuid, TransformState> newStatesByUuid;

	for (auto it = _transformGizmoStartStates.begin(); it != _transformGizmoStartStates.end(); ++it)
	{
		const int id = it.key();
		if (id < 0 || id >= static_cast<int>(_meshStore.size()))
			continue;

		TriangleMesh* mesh = _meshStore[id];
		if (!mesh)
			continue;

		const QUuid uuid = getUuidByIndex(id);
		if (uuid.isNull())
			continue;

		oldStatesByUuid.insert(uuid, it.value());
		newStatesByUuid.insert(uuid, _explodedViewManualPlacementActive
			? explodedViewTransformState(mesh)
			: TransformState(
				mesh->getTranslation(),
				mesh->getRotation(),
				mesh->getScaling(),
				mesh->getRotationQuaternion()));
	}

	const bool moved = _transformGizmoCurrentTranslationDelta.lengthSquared() > 1.0e-8f;

	if (_explodedViewManualPlacementActive)
	{
		if (commit && moved && !oldStatesByUuid.isEmpty())
		{
			_viewer->getUndoStack()->push(new TransformCommand(
				_viewer,
				this,
				oldStatesByUuid,
				newStatesByUuid,
				tr("Translate Exploded Placement"),
				false,
				TransformCommand::Target::ExplodedViewTransform));
		}
		else if (!commit)
			_explodedViewManualSessionTranslationDelta = _explodedViewManualDragStartTranslationDelta;
		emit explodedViewManualPlacementChanged();
		update();
	}
	else if (commit && moved && !oldStatesByUuid.isEmpty())
	{
		_viewer->getUndoStack()->push(new TransformCommand(
			_viewer, this, oldStatesByUuid, newStatesByUuid, tr("Translate Selection"), false));
	}
	else
	{
		for (auto it = _transformGizmoStartStates.begin(); it != _transformGizmoStartStates.end(); ++it)
		{
			const int id = it.key();
			if (id < 0 || id >= static_cast<int>(_meshStore.size()))
				continue;

			if (TriangleMesh* mesh = _meshStore[id])
			{
				const TransformState& startState = it.value();
				if (_explodedViewManualPlacementActive)
					applyExplodedViewTransformState(mesh, startState, false);
				else
				{
					mesh->setTranslation(startState.translation);
					if (startState.hasExactRotation)
						mesh->setRotationQuaternion(startState.rotationQuat, startState.rotation);
					else
						mesh->setRotation(startState.rotation);
					mesh->setScaling(startState.scale);
				}
			}
		}
		update();
	}

	_transformGizmoStartStates.clear();
	_transformGizmoStartCenters.clear();
	_transformGizmoStartMatrices.clear();
	_transformGizmoCurrentTranslationDelta = QVector3D(0.0f, 0.0f, 0.0f);
	_transformGizmoLoggedTranslationUpdate = false;
	_viewer->updateTransformationValues();
}

bool GLWidget::beginTransformGizmoScaleDrag(TransformGizmo::Handle handle, const QPoint& pixel, bool uniformScale)
{
	if (!_transformGizmoRequested || !_transformGizmo || !_viewer)
		return false;
	if (_explodedViewManualPlacementActive)
		return false;

	QVector3D axis;
	switch (handle)
	{
	case TransformGizmo::Handle::TranslateX:
		axis = QVector3D(1.0f, 0.0f, 0.0f);
		break;
	case TransformGizmo::Handle::TranslateY:
		axis = QVector3D(0.0f, 1.0f, 0.0f);
		break;
	case TransformGizmo::Handle::TranslateZ:
		axis = QVector3D(0.0f, 0.0f, 1.0f);
		break;
	case TransformGizmo::Handle::UniformScale:
		axis = QVector3D(1.0f, 1.0f, 1.0f).normalized();
		break;
	default:
		return false;
	}

	_transformGizmoScaling = true;
	_transformGizmoUniformScaling = uniformScale;
	_transformGizmoTranslating = false;
	_transformGizmoRotating = false;
	_transformGizmoDragStartPixel = pixel;
	_transformGizmoDragAxis = axis;
	const BoundingSphere selectionSphere = computeTransformGizmoSelectionSphere();
	_transformGizmoStartPivot = selectionSphere.getCenter();
	const float selectionRadius = selectionSphere.getRadius() > 0.0f
		? selectionSphere.getRadius()
		: _boundingSphere.getRadius();
	_transformGizmoDragScale = (std::max)(selectionRadius * 0.9f, 0.01f);
	_transformGizmoStartStates.clear();
	_transformGizmoStartCenters.clear();
	_transformGizmoStartMatrices.clear();
	_transformGizmoCurrentTranslationDelta = QVector3D(0.0f, 0.0f, 0.0f);
	_transformGizmoCurrentScaleDelta = QVector3D(1.0f, 1.0f, 1.0f);
	_transformGizmoCurrentRotationDelta = QVector3D(0.0f, 0.0f, 0.0f);

	for (int id : activeTransformGizmoSelectionIds())
	{
		if (id < 0 || id >= static_cast<int>(_meshStore.size()))
			continue;

		if (TriangleMesh* mesh = _meshStore[id])
		{
			_transformGizmoStartStates[id] = _explodedViewManualPlacementActive
				? explodedViewTransformState(mesh)
				: TransformState(
					mesh->getTranslation(),
					mesh->getRotation(),
					mesh->getScaling(),
					mesh->getRotationQuaternion());
			_transformGizmoStartCenters[id] = mesh->getBoundingSphere().getCenter();
			_transformGizmoStartMatrices[id] = _explodedViewManualPlacementActive
				? explodedViewTransformMatrix(mesh)
				: mesh->getTransformation();
		}
	}

	if (_viewer->tabWidgetVizAttribs->currentWidget() == _viewer->transformationsPage)
	{
		_viewer->objectTransformPanel->setScaleValues(QVector3D(1.0f, 1.0f, 1.0f));
	}

	return !_transformGizmoStartStates.isEmpty();
}

void GLWidget::updateTransformGizmoScaleDrag(const QPoint& pixel)
{
	if (!_transformGizmoScaling || !_viewer || _transformGizmoStartStates.isEmpty())
		return;

	const QRect viewport = getViewportFromPoint(pixel);
	const GLCamera* camera = getCameraForPoint(pixel);
	if (!camera)
		return;

	const QMatrix4x4 viewMatrix = camera->getViewMatrix();
	const QMatrix4x4 projectionMatrix = camera->getProjectionMatrix();
	const QVector3D pivotScreen3 = _transformGizmoStartPivot.project(viewMatrix, projectionMatrix, viewport);
	const QVector3D axisEndWorld = _transformGizmoStartPivot + (_transformGizmoDragAxis * _transformGizmoDragScale);
	const QVector3D axisEndScreen3 = axisEndWorld.project(viewMatrix, projectionMatrix, viewport);

	const QVector2D pivotScreen(pivotScreen3.x(), pivotScreen3.y());
	const QVector2D axisScreen = QVector2D(axisEndScreen3.x(), axisEndScreen3.y()) - pivotScreen;
	const float axisScreenLength = axisScreen.length();
	if (axisScreenLength <= 1.0e-4f)
		return;

	const QVector2D axisScreenDir = axisScreen / axisScreenLength;
	const QVector2D mouseDelta = QVector2D(pixel.x() - _transformGizmoDragStartPixel.x(),
		_transformGizmoDragStartPixel.y() - pixel.y());
	const float projectedPixels = QVector2D::dotProduct(mouseDelta, axisScreenDir);
	const float uniformFactor = (std::max)(0.01f, 1.0f + (projectedPixels / axisScreenLength));

	if (_transformGizmoUniformScaling)
	{
		_transformGizmoCurrentScaleDelta = QVector3D(uniformFactor, uniformFactor, uniformFactor);
	}
	else if (_transformGizmoDragAxis.x() > 0.5f)
	{
		_transformGizmoCurrentScaleDelta = QVector3D(uniformFactor, 1.0f, 1.0f);
	}
	else if (_transformGizmoDragAxis.y() > 0.5f)
	{
		_transformGizmoCurrentScaleDelta = QVector3D(1.0f, uniformFactor, 1.0f);
	}
	else
	{
		_transformGizmoCurrentScaleDelta = QVector3D(1.0f, 1.0f, uniformFactor);
	}

	for (auto it = _transformGizmoStartStates.begin(); it != _transformGizmoStartStates.end(); ++it)
	{
		const int id = it.key();
		if (id < 0 || id >= static_cast<int>(_meshStore.size()))
			continue;

		if (TriangleMesh* mesh = _meshStore[id])
		{
			const TransformState& startState = it.value();
			QVector3D scaledTranslation = startState.translation;
			QVector3D scaledScale = startState.scale;

			if (_transformGizmoUniformScaling)
			{
				scaledTranslation = _transformGizmoStartPivot +
					(startState.translation - _transformGizmoStartPivot) * uniformFactor;
				scaledScale = startState.scale * uniformFactor;
			}
			else if (_transformGizmoDragAxis.x() > 0.5f)
			{
				scaledTranslation.setX(_transformGizmoStartPivot.x() +
					(startState.translation.x() - _transformGizmoStartPivot.x()) * uniformFactor);
				scaledScale.setX(startState.scale.x() * uniformFactor);
			}
			else if (_transformGizmoDragAxis.y() > 0.5f)
			{
				scaledTranslation.setY(_transformGizmoStartPivot.y() +
					(startState.translation.y() - _transformGizmoStartPivot.y()) * uniformFactor);
				scaledScale.setY(startState.scale.y() * uniformFactor);
			}
			else
			{
				scaledTranslation.setZ(_transformGizmoStartPivot.z() +
					(startState.translation.z() - _transformGizmoStartPivot.z()) * uniformFactor);
				scaledScale.setZ(startState.scale.z() * uniformFactor);
			}

			mesh->setTranslationFast(scaledTranslation);
			if (startState.hasExactRotation)
				mesh->setRotationQuaternionFast(startState.rotationQuat, startState.rotation);
			else
				mesh->setRotationFast(startState.rotation);
			mesh->setScalingFast(scaledScale);
		}
	}

	if (_viewer->tabWidgetVizAttribs->currentWidget() == _viewer->transformationsPage)
	{
		_viewer->objectTransformPanel->setScaleValues(_transformGizmoCurrentScaleDelta);
	}

	_shadowMapNeedsInitialization = true;
	update();
}

void GLWidget::finishTransformGizmoScaleDrag(bool commit)
{
	if (!_transformGizmoScaling)
		return;

	_transformGizmoScaling = false;
	_transformGizmoUniformScaling = false;

	if (!_viewer || _transformGizmoStartStates.isEmpty())
		return;

	QMap<QUuid, TransformState> oldStatesByUuid;
	QMap<QUuid, TransformState> newStatesByUuid;

	for (auto it = _transformGizmoStartStates.begin(); it != _transformGizmoStartStates.end(); ++it)
	{
		const int id = it.key();
		if (id < 0 || id >= static_cast<int>(_meshStore.size()))
			continue;

		TriangleMesh* mesh = _meshStore[id];
		if (!mesh)
			continue;

		const QUuid uuid = getUuidByIndex(id);
		if (uuid.isNull())
			continue;

		oldStatesByUuid.insert(uuid, it.value());
		newStatesByUuid.insert(uuid, TransformState(
			mesh->getTranslation(),
			mesh->getRotation(),
			mesh->getScaling(),
			mesh->getRotationQuaternion()));
	}

	const QVector3D scaleDelta = _transformGizmoCurrentScaleDelta - QVector3D(1.0f, 1.0f, 1.0f);
	const bool scaled = scaleDelta.lengthSquared() > 1.0e-8f;

	if (_explodedViewManualPlacementActive)
	{
		update();
	}
	else if (commit && scaled && !oldStatesByUuid.isEmpty())
	{
		_viewer->getUndoStack()->push(new TransformCommand(
			_viewer, this, oldStatesByUuid, newStatesByUuid, tr("Scale Selection"), false));
	}
	else
	{
		for (auto it = _transformGizmoStartStates.begin(); it != _transformGizmoStartStates.end(); ++it)
		{
			const int id = it.key();
			if (id < 0 || id >= static_cast<int>(_meshStore.size()))
				continue;

			if (TriangleMesh* mesh = _meshStore[id])
			{
				const TransformState& startState = it.value();
				mesh->setTranslation(startState.translation);
				if (startState.hasExactRotation)
					mesh->setRotationQuaternion(startState.rotationQuat, startState.rotation);
				else
					mesh->setRotation(startState.rotation);
				mesh->setScaling(startState.scale);
			}
		}
		update();
	}

	_transformGizmoStartStates.clear();
	_transformGizmoStartCenters.clear();
	_transformGizmoStartMatrices.clear();
	_transformGizmoCurrentScaleDelta = QVector3D(1.0f, 1.0f, 1.0f);
	_viewer->updateTransformationValues();
}

bool GLWidget::beginTransformGizmoRotationDrag(TransformGizmo::Handle handle, const QPoint& pixel)
{
	if (!_transformGizmoRequested || !_transformGizmo || !_viewer)
		return false;

	QVector3D axis;
	switch (handle)
	{
	case TransformGizmo::Handle::RotateXY:
		axis = QVector3D(0.0f, 0.0f, 1.0f);
		break;
	case TransformGizmo::Handle::RotateYZ:
		axis = QVector3D(1.0f, 0.0f, 0.0f);
		break;
	case TransformGizmo::Handle::RotateZX:
		axis = QVector3D(0.0f, 1.0f, 0.0f);
		break;
	default:
		return false;
	}

	const QRect viewport = getViewportFromPoint(pixel);
	const GLCamera* camera = getCameraForPoint(pixel);
	if (!camera)
		return false;

	QVector3D rayOrigin;
	QVector3D rayDir;
	if (!convertPixelToRay(pixel, viewport, height(), camera->getViewMatrix(), camera->getProjectionMatrix(), rayOrigin, rayDir))
		return false;

	const BoundingSphere selectionSphere = computeTransformGizmoSelectionSphere();
	const QVector3D pivot = selectionSphere.getCenter();
	QVector3D hitPoint;
	if (!intersectRayPlane(rayOrigin, rayDir, pivot, axis, hitPoint))
		return false;

	QVector3D startVector = hitPoint - pivot;
	if (startVector.lengthSquared() <= 1.0e-8f)
		return false;
	startVector.normalize();

	_transformGizmoRotating = true;
	_transformGizmoScaling = false;
	_transformGizmoUniformScaling = false;
	_transformGizmoTranslating = false;
	_transformGizmoDragStartPixel = pixel;
	_transformGizmoRotationPlaneNormal = axis;
	_transformGizmoRotationStartVector = startVector;
	_transformGizmoStartPivot = pivot;
	_transformGizmoStartStates.clear();
	_transformGizmoStartCenters.clear();
	_transformGizmoStartMatrices.clear();
	_transformGizmoCurrentRotationDelta = QVector3D(0.0f, 0.0f, 0.0f);
	_transformGizmoCurrentTranslationDelta = QVector3D(0.0f, 0.0f, 0.0f);
	_transformGizmoCurrentScaleDelta = QVector3D(1.0f, 1.0f, 1.0f);
	if (_explodedViewManualPlacementActive)
	{
		_explodedViewManualDragStartRotationQuat = _explodedViewManualSessionRotationQuat;
		_explodedViewManualDragStartRotationEuler = _explodedViewManualSessionRotationEuler;
	}

	for (int id : activeTransformGizmoSelectionIds())
	{
		if (id < 0 || id >= static_cast<int>(_meshStore.size()))
			continue;

		if (TriangleMesh* mesh = _meshStore[id])
		{
			_transformGizmoStartStates[id] = TransformState(
				mesh->getTranslation(),
				mesh->getRotation(),
				mesh->getScaling(),
				mesh->getRotationQuaternion());
			_transformGizmoStartCenters[id] = mesh->getBoundingSphere().getCenter();
			_transformGizmoStartMatrices[id] = mesh->getTransformation();
		}
	}

	if (_viewer->tabWidgetVizAttribs->currentWidget() == _viewer->transformationsPage)
	{
		_viewer->objectTransformPanel->setRotationValues(QVector3D(0.0f, 0.0f, 0.0f));
	}

	return !_transformGizmoStartStates.isEmpty();
}

void GLWidget::updateTransformGizmoRotationDrag(const QPoint& pixel)
{
	if (!_transformGizmoRotating || !_viewer || _transformGizmoStartStates.isEmpty())
		return;

	const QRect viewport = getViewportFromPoint(pixel);
	const GLCamera* camera = getCameraForPoint(pixel);
	if (!camera)
		return;

	QVector3D rayOrigin;
	QVector3D rayDir;
	if (!convertPixelToRay(pixel, viewport, height(), camera->getViewMatrix(), camera->getProjectionMatrix(), rayOrigin, rayDir))
		return;

	QVector3D hitPoint;
	if (!intersectRayPlane(rayOrigin, rayDir, _transformGizmoStartPivot,
		_transformGizmoRotationPlaneNormal, hitPoint))
		return;

	QVector3D currentVector = hitPoint - _transformGizmoStartPivot;
	if (currentVector.lengthSquared() <= 1.0e-8f)
		return;
	currentVector.normalize();

	const QVector3D crossVec = QVector3D::crossProduct(_transformGizmoRotationStartVector, currentVector);
	const float sinAngle = QVector3D::dotProduct(_transformGizmoRotationPlaneNormal, crossVec);
	const float cosAngle = QVector3D::dotProduct(_transformGizmoRotationStartVector, currentVector);
	const float angleDegrees = qRadiansToDegrees(std::atan2(sinAngle, cosAngle));

	if (_transformGizmoRotationPlaneNormal.x() > 0.5f)
		_transformGizmoCurrentRotationDelta = QVector3D(angleDegrees, 0.0f, 0.0f);
	else if (_transformGizmoRotationPlaneNormal.y() > 0.5f)
		_transformGizmoCurrentRotationDelta = QVector3D(0.0f, angleDegrees, 0.0f);
	else
		_transformGizmoCurrentRotationDelta = QVector3D(0.0f, 0.0f, angleDegrees);

	for (auto it = _transformGizmoStartStates.begin(); it != _transformGizmoStartStates.end(); ++it)
	{
		const int id = it.key();
		if (id < 0 || id >= static_cast<int>(_meshStore.size()))
			continue;

		if (TriangleMesh* mesh = _meshStore[id])
		{
			const TransformState& startState = it.value();
			QMatrix4x4 deltaMatrix;
			deltaMatrix.setToIdentity();
			deltaMatrix.translate(_transformGizmoStartPivot);
			deltaMatrix.rotate(angleDegrees, _transformGizmoRotationPlaneNormal);
			deltaMatrix.translate(-_transformGizmoStartPivot);

			QMatrix4x4 rotationOnlyMatrix;
			rotationOnlyMatrix.setToIdentity();
			rotationOnlyMatrix.rotate(angleDegrees, _transformGizmoRotationPlaneNormal);
			const QMatrix4x4 startMatrix = _transformGizmoStartMatrices.value(
				id, _explodedViewManualPlacementActive ? mesh->getExplodedViewTransformation() : mesh->getTransformation());
			const QMatrix4x4 combinedMatrix = deltaMatrix * startMatrix;
			const QVector3D exactTranslation(
				combinedMatrix(0, 3),
				combinedMatrix(1, 3),
				combinedMatrix(2, 3));

			const QQuaternion deltaQuat =
				QQuaternion::fromRotationMatrix(rotationOnlyMatrix.toGenericMatrix<3, 3>()).normalized();
			const QQuaternion exactRotationQuat = startState.hasExactRotation
				? (deltaQuat * startState.rotationQuat).normalized()
				: deltaQuat;
			QMatrix4x4 displayRotationMatrix;
			displayRotationMatrix.setToIdentity();
			displayRotationMatrix.rotate(exactRotationQuat);
			const QVector3D displayRotation = extractMeshRotationFromMatrix(displayRotationMatrix);
			if (_explodedViewManualPlacementActive)
			{
				mesh->setExplodedViewTranslationFast(exactTranslation);
				mesh->setExplodedViewRotationQuaternionFast(exactRotationQuat, displayRotation);
				mesh->setExplodedViewScalingFast(startState.scale);
			}
			else
			{
				mesh->setTranslationFast(exactTranslation);
				mesh->setRotationQuaternionFast(exactRotationQuat, displayRotation);
				mesh->setScalingFast(startState.scale);
			}
		}
	}

	if (_viewer->tabWidgetVizAttribs->currentWidget() == _viewer->transformationsPage)
	{
		_viewer->objectTransformPanel->setRotationValues(_transformGizmoCurrentRotationDelta);
	}
	if (_explodedViewManualPlacementActive)
	{
		QMatrix4x4 rotationOnlyMatrix;
		rotationOnlyMatrix.setToIdentity();
		rotationOnlyMatrix.rotate(angleDegrees, _transformGizmoRotationPlaneNormal);
		const QQuaternion deltaQuat =
			QQuaternion::fromRotationMatrix(rotationOnlyMatrix.toGenericMatrix<3, 3>()).normalized();
		_explodedViewManualSessionRotationQuat =
			(deltaQuat * _explodedViewManualDragStartRotationQuat).normalized();

		QMatrix4x4 sessionRotationMatrix;
		sessionRotationMatrix.setToIdentity();
		sessionRotationMatrix.rotate(_explodedViewManualSessionRotationQuat);
		_explodedViewManualSessionRotationEuler =
			extractMeshRotationFromMatrix(sessionRotationMatrix);
		emit explodedViewManualPlacementChanged();
	}

	_shadowMapNeedsInitialization = true;
	update();
}

void GLWidget::finishTransformGizmoRotationDrag(bool commit)
{
	if (!_transformGizmoRotating)
		return;

	_transformGizmoRotating = false;

	if (!_viewer || _transformGizmoStartStates.isEmpty())
		return;

	QMap<QUuid, TransformState> oldStatesByUuid;
	QMap<QUuid, TransformState> newStatesByUuid;

	for (auto it = _transformGizmoStartStates.begin(); it != _transformGizmoStartStates.end(); ++it)
	{
		const int id = it.key();
		if (id < 0 || id >= static_cast<int>(_meshStore.size()))
			continue;

		TriangleMesh* mesh = _meshStore[id];
		if (!mesh)
			continue;

		const QUuid uuid = getUuidByIndex(id);
		if (uuid.isNull())
			continue;

		oldStatesByUuid.insert(uuid, it.value());
		newStatesByUuid.insert(uuid, _explodedViewManualPlacementActive
			? explodedViewTransformState(mesh)
			: TransformState(
				mesh->getTranslation(),
				mesh->getRotation(),
				mesh->getScaling(),
				mesh->getRotationQuaternion()));
	}

	const bool moved = _transformGizmoCurrentRotationDelta.lengthSquared() > 1.0e-8f;

	if (_explodedViewManualPlacementActive)
	{
		if (commit && moved && !oldStatesByUuid.isEmpty())
		{
			_viewer->getUndoStack()->push(new TransformCommand(
				_viewer,
				this,
				oldStatesByUuid,
				newStatesByUuid,
				tr("Rotate Exploded Placement"),
				false,
				TransformCommand::Target::ExplodedViewTransform));
		}
		else if (!commit)
		{
			_explodedViewManualSessionRotationQuat = _explodedViewManualDragStartRotationQuat;
			_explodedViewManualSessionRotationEuler = _explodedViewManualDragStartRotationEuler;
		}
		emit explodedViewManualPlacementChanged();
		update();
	}
	else if (commit && moved && !oldStatesByUuid.isEmpty())
	{
		_viewer->getUndoStack()->push(new TransformCommand(
			_viewer, this, oldStatesByUuid, newStatesByUuid, tr("Rotate Selection"), false));
	}
	else
	{
		for (auto it = _transformGizmoStartStates.begin(); it != _transformGizmoStartStates.end(); ++it)
		{
			const int id = it.key();
			if (id < 0 || id >= static_cast<int>(_meshStore.size()))
				continue;

			if (TriangleMesh* mesh = _meshStore[id])
			{
				const TransformState& startState = it.value();
				if (_explodedViewManualPlacementActive)
					applyExplodedViewTransformState(mesh, startState, false);
				else
				{
					mesh->setTranslation(startState.translation);
					if (startState.hasExactRotation)
						mesh->setRotationQuaternion(startState.rotationQuat, startState.rotation);
					else
						mesh->setRotation(startState.rotation);
					mesh->setScaling(startState.scale);
				}
			}
		}
		update();
	}

	_transformGizmoStartStates.clear();
	_transformGizmoStartCenters.clear();
	_transformGizmoStartMatrices.clear();
	_transformGizmoCurrentRotationDelta = QVector3D(0.0f, 0.0f, 0.0f);
	_viewer->updateTransformationValues();
}

void GLWidget::drawCornerAxis(CornerAxisPosition position)
{
	const int axisSize = std::max(1, std::min(width(), height()) / 10);
	int viewportX = 0;
	int viewportY = 0;

	// Determine the viewport position based on the CornerAxisPosition
	switch (position)
	{
	case CornerAxisPosition::TOP_LEFT:
		viewportX = 0;
		viewportY = height() - axisSize;
		break;
	case CornerAxisPosition::TOP_RIGHT:
		viewportX = width() - axisSize;
		viewportY = height() - axisSize;
		break;
	case CornerAxisPosition::BOTTOM_LEFT:
		viewportX = 0;
		viewportY = 0;
		break;
	case CornerAxisPosition::BOTTOM_RIGHT:
		viewportX = width() - axisSize;
		viewportY = 0;
		break;
	}

	// Set the viewport for the corner axis
	glViewport(viewportX, viewportY, axisSize, axisSize);

	QMatrix4x4 mat = _viewMatrix;
	mat.setColumn(3, QVector4D(0, 0, 0, 1));
	mat.setRow(3, QVector4D(0, 0, 0, 1));
	QMatrix4x4 axisProjection;
	axisProjection.ortho(-1.6f, 1.6f, -1.6f, 1.6f, -4.0f, 4.0f);

	const float axisLength = 1.0f;
	const float labelScale = std::max(0.55f, axisSize / 110.0f);

	const unsigned int prevTextWidth = _axisTextRenderer->width();
	const unsigned int prevTextHeight = _axisTextRenderer->height();
	_axisTextRenderer->setWidth(axisSize);
	_axisTextRenderer->setHeight(axisSize);

	QMatrix4x4 textProjection;
	textProjection.ortho(QRect(0.0f, 0.0f, static_cast<float>(axisSize), static_cast<float>(axisSize)));
	_textShader->bind();
	_textShader->setUniformValue("projection", textProjection);
	_textShader->release();

	// Labels
	QVector3D xAxis(axisLength, 0, 0);
	xAxis = xAxis.project(mat, axisProjection, QRect(0, 0, axisSize, axisSize));
	_axisTextRenderer->RenderText(_labelAxisX.toStdString(), xAxis.x(), axisSize - xAxis.y(), labelScale, QVector3D(1.0f, 1.0f, 0.0f), TextRenderer::VAlignment::VBOTTOM);

	QVector3D yAxis(0, axisLength, 0);
	yAxis = yAxis.project(mat, axisProjection, QRect(0, 0, axisSize, axisSize));
	_axisTextRenderer->RenderText(_labelAxisY.toStdString(), yAxis.x(), axisSize - yAxis.y(), labelScale, QVector3D(1.0f, 1.0f, 0.0f), TextRenderer::VAlignment::VBOTTOM);

	QVector3D zAxis(0, 0, axisLength);
	zAxis = zAxis.project(mat, axisProjection, QRect(0, 0, axisSize, axisSize));
	_axisTextRenderer->RenderText(_labelAxisZ.toStdString(), zAxis.x(), axisSize - zAxis.y(), labelScale, QVector3D(1.0f, 1.0f, 0.0f), TextRenderer::VAlignment::VBOTTOM);

	// Axes
	if (!_axisVAO.isCreated())
	{
		_axisVAO.create();
		_axisVAO.bind();
	}

	// Vertex Buffer
	if (!_axisVBO.isCreated())
	{
		_axisVBO = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
		_axisVBO.create();
	}
	_axisVBO.bind();
	_axisVBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
	std::vector<float> vertices = {
		0, 0, 0,
		axisLength, 0, 0,
		0, 0, 0,
		0, axisLength, 0,
		0, 0, 0,
		0, 0, axisLength };
	_axisVBO.allocate(vertices.data(), static_cast<int>(vertices.size() * sizeof(float)));

	// Color Buffer
	if (!_axisCBO.isCreated())
	{
		_axisCBO = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
		_axisCBO.create();
	}
	_axisCBO.bind();
	_axisCBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
	std::vector<float> colors = {
		1, 1, 1,
		1, 1, 1,
		1, 1, 1,
		1, 1, 1,
		1, 1, 1,
		1, 1, 1 };
	_axisCBO.allocate(colors.data(), static_cast<int>(colors.size() * sizeof(float)));

	_axisShader->bind();

	_axisVBO.bind();
	_axisShader->enableAttributeArray("vertexPosition");
	_axisShader->setAttributeBuffer("vertexPosition", GL_FLOAT, 0, 3);

	_axisCBO.bind();
	_axisShader->enableAttributeArray("vertexColor");
	_axisShader->setAttributeBuffer("vertexColor", GL_FLOAT, 0, 3);

	_axisShader->setUniformValue("modelViewMatrix", mat);
	_axisShader->setUniformValue("projectionMatrix", axisProjection);

	_axisShader->setUniformValue("renderCone", false);

	_axisVAO.bind();
	glLineWidth(2.0);
	glDrawArrays(GL_LINES, 0, 6);
	glLineWidth(1);

	// Axes Cones
	// X Axis
	_axisCone->setParameters(axisLength / 15.0f, axisLength / 5.0f, 8.0f, 1.0f);
	_axisShader->setUniformValue("renderCone", true);
	mat.translate(axisLength, 0, 0);
	mat.rotate(90, QVector3D(0, 1.0f, 0));
	_axisShader->bind();
	_axisShader->setUniformValue("coneColor", QVector3D(1.0f, 1.0f, 1.0f));
	_axisShader->setUniformValue("modelViewMatrix", mat);
	_axisCone->getVAO().bind();
	glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(_axisCone->getPoints().size()), GL_UNSIGNED_INT, 0);
	_axisCone->getVAO().release();

	// Y Axis
	mat = _viewMatrix;
	mat.setColumn(3, QVector4D(0, 0, 0, 1));
	mat.setRow(3, QVector4D(0, 0, 0, 1));
	mat.translate(0, axisLength, 0);
	mat.rotate(90, QVector3D(-1.0f, 0, 0));
	_axisShader->bind();
	_axisShader->setUniformValue("modelViewMatrix", mat);
	_axisCone->getVAO().bind();
	glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(_axisCone->getPoints().size()), GL_UNSIGNED_INT, 0);
	_axisCone->getVAO().release();

	// Z Axis
	mat = _viewMatrix;
	mat.setColumn(3, QVector4D(0, 0, 0, 1));
	mat.setRow(3, QVector4D(0, 0, 0, 1));
	mat.translate(0, 0, axisLength);
	_axisShader->bind();
	_axisShader->setUniformValue("modelViewMatrix", mat);
	_axisCone->getVAO().bind();
	glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(_axisCone->getPoints().size()), GL_UNSIGNED_INT, 0);
	_axisCone->getVAO().release();

	_axisVAO.release();
	_axisShader->release();

	_axisTextRenderer->setWidth(prevTextWidth);
	_axisTextRenderer->setHeight(prevTextHeight);
	QMatrix4x4 projection;
	projection.ortho(QRect(0.0f, 0.0f, static_cast<float>(width()), static_cast<float>(height())));
	_textShader->bind();
	_textShader->setUniformValue("projection", projection);
	_textShader->release();

	glViewport(0, 0, width(), height());
}

QRect GLWidget::viewCubeRect() const
{
	const int side = std::max(kViewCubeStyle.minViewportSize,
		std::min(std::min(width(), height()) / 5, kViewCubeStyle.maxViewportSize));
	const int padding = kViewCubeStyle.viewportPadding;
	return QRect(width() - side - padding, padding, side, side);
}

QRect GLWidget::viewCubeScreenRect() const
{
	const QRect viewportRect = viewCubeRect();
	return QRect(viewportRect.x(),
	             height() - viewportRect.y() - viewportRect.height(),
	             viewportRect.width(),
	             viewportRect.height());
}

bool GLWidget::computeViewCubeRenderState(QRect& viewportRect,
                                          QMatrix4x4& viewMatrix,
                                          QMatrix4x4& projectionMatrix,
                                          QMatrix4x4& modelMatrix,
                                          float& cubeScale) const
{
	if (!_viewCube)
		return false;

	viewportRect = viewCubeRect();
	if (viewportRect.width() <= 0 || viewportRect.height() <= 0)
		return false;

	QMatrix4x4 viewRotation = _viewMatrix;
	viewRotation.setColumn(3, QVector4D(0.0f, 0.0f, 0.0f, 1.0f));
	viewRotation.setRow(3, QVector4D(0.0f, 0.0f, 0.0f, 1.0f));

	viewMatrix.setToIdentity();
	viewMatrix.translate(0.0f, 0.0f, -kViewCubeStyle.eyeDistance);
	viewMatrix *= viewRotation;

	projectionMatrix.setToIdentity();
	const float aspect = static_cast<float>(viewportRect.width()) / static_cast<float>(std::max(1, viewportRect.height()));
	cubeScale = 1.0f;
	if (_projection == ViewProjection::ORTHOGRAPHIC)
	{
		const float orthoHalfHeight = kViewCubeStyle.orthographicHalfHeight;
		const float orthoHalfWidth = orthoHalfHeight * aspect;
		projectionMatrix.ortho(-orthoHalfWidth, orthoHalfWidth, -orthoHalfHeight, orthoHalfHeight, 0.1f, 10.0f);
		cubeScale = kViewCubeStyle.orthographicScale;
	}
	else
	{
		projectionMatrix.perspective(26.0f, aspect, 0.1f, 10.0f);
		cubeScale = kViewCubeStyle.perspectiveScale;
	}

	modelMatrix.setToIdentity();
	modelMatrix.scale(cubeScale);
	return true;
}

bool GLWidget::orientCameraToViewCubeNormal(const QVector3D& outwardNormal)
{
	if (!_primaryCamera || outwardNormal.lengthSquared() <= 1.0e-8f || isGltfCameraActive())
		return false;

	checkAndStopTimers();
	_keyboardNavTimer->stop();

	const QVector3D normalizedNormal = outwardNormal.normalized();
	const auto isAxisNormal = [&normalizedNormal](const QVector3D& axis) {
		return (normalizedNormal - axis).lengthSquared() <= 1.0e-6f;
	};

	const QVector3D topNormal = transformVectorForCameraUpAxis(QVector3D(0.0f, 0.0f, 1.0f));
	const QVector3D bottomNormal = transformVectorForCameraUpAxis(QVector3D(0.0f, 0.0f, -1.0f));
	const QVector3D frontNormal = transformVectorForCameraUpAxis(QVector3D(0.0f, -1.0f, 0.0f));
	const QVector3D rearNormal = transformVectorForCameraUpAxis(QVector3D(0.0f, 1.0f, 0.0f));
	const QVector3D leftNormal = transformVectorForCameraUpAxis(QVector3D(-1.0f, 0.0f, 0.0f));
	const QVector3D rightNormal = transformVectorForCameraUpAxis(QVector3D(1.0f, 0.0f, 0.0f));

	if (isAxisNormal(topNormal))
	{
		_customViewTargetRotation = standardViewRotation(ViewMode::TOP);
		_viewMode = ViewMode::TOP;
		if (_viewToolbar)
			_viewToolbar->setDefaultStandardViewAction(StandardViewActions::TOP);
	}
	else if (isAxisNormal(bottomNormal))
	{
		_customViewTargetRotation = standardViewRotation(ViewMode::BOTTOM);
		_viewMode = ViewMode::BOTTOM;
		if (_viewToolbar)
			_viewToolbar->setDefaultStandardViewAction(StandardViewActions::BOTTOM);
	}
	else if (isAxisNormal(frontNormal))
	{
		_customViewTargetRotation = standardViewRotation(ViewMode::FRONT);
		_viewMode = ViewMode::FRONT;
		if (_viewToolbar)
			_viewToolbar->setDefaultStandardViewAction(StandardViewActions::FRONT);
	}
	else if (isAxisNormal(rearNormal))
	{
		_customViewTargetRotation = standardViewRotation(ViewMode::BACK);
		_viewMode = ViewMode::BACK;
		if (_viewToolbar)
			_viewToolbar->setDefaultStandardViewAction(StandardViewActions::REAR);
	}
	else if (isAxisNormal(leftNormal))
	{
		_customViewTargetRotation = standardViewRotation(ViewMode::LEFT);
		_viewMode = ViewMode::LEFT;
		if (_viewToolbar)
			_viewToolbar->setDefaultStandardViewAction(StandardViewActions::LEFT);
	}
	else if (isAxisNormal(rightNormal))
	{
		_customViewTargetRotation = standardViewRotation(ViewMode::RIGHT);
		_viewMode = ViewMode::RIGHT;
		if (_viewToolbar)
			_viewToolbar->setDefaultStandardViewAction(StandardViewActions::RIGHT);
	}
	else
	{
		const QVector3D viewDir = -normalizedNormal;
		// Keep the View Cube aligned to the viewer's Z-up convention while
		// picking a stable screen-up vector for face, edge, and corner targets.
		QVector3D upSeed = _cameraUpAxisZUp
			? QVector3D(0.0f, 0.0f, 1.0f)
			: QVector3D(0.0f, 1.0f, 0.0f);
		if (std::abs(QVector3D::dotProduct(viewDir, upSeed)) > 0.95f)
			upSeed = QVector3D(1.0f, 0.0f, 0.0f);

		QVector3D right = QVector3D::crossProduct(viewDir, upSeed);
		if (right.lengthSquared() <= 1.0e-8f)
			right = QVector3D::crossProduct(viewDir, QVector3D(1.0f, 0.0f, 0.0f));
		if (right.lengthSquared() <= 1.0e-8f)
			return false;
		right.normalize();
		QVector3D up = QVector3D::crossProduct(right, viewDir).normalized();

		QMatrix4x4 targetMatrix;
		targetMatrix.setRow(0, QVector4D(right, 0.0f));
		targetMatrix.setRow(1, QVector4D(up, 0.0f));
		targetMatrix.setRow(2, QVector4D(-viewDir, 0.0f));
		targetMatrix.setRow(3, QVector4D(0.0f, 0.0f, 0.0f, 1.0f));
		_customViewTargetRotation = QQuaternion::fromRotationMatrix(targetMatrix.toGenericMatrix<3, 3>()).normalized();
		_viewMode = ViewMode::NONE;
	}

	const std::vector<int>& visibleIds = _visibleSwapped ? _hiddenObjectsIds : _displayedObjectsIds;
	if (!_meshStore.empty() && !visibleIds.empty())
	{
		const QMatrix4x4 targetRotationMatrix(_customViewTargetRotation.toRotationMatrix());
		const QVector3D right = targetRotationMatrix.row(0).toVector3D().normalized();
		const QVector3D up = targetRotationMatrix.row(1).toVector3D().normalized();
		const QVector3D viewDir = -targetRotationMatrix.row(2).toVector3D().normalized();
		QVector3D projCenter;
		_viewBoundingSphereDia = computeFitViewRange(right, up, viewDir, &projCenter);
		_boundingSphere.setCenter(projCenter);
	}
	else
	{
		_viewBoundingSphereDia = _currentViewRange;
	}

	_customViewAnimationActive = true;
	_slerpStep = 0.0f;
	if (!_animateViewTimer->isActive())
		_animateViewTimer->start(5);
	return true;
}

bool GLWidget::handleViewCubeClick(const QPoint& pixel)
{
	if (!_showViewCubeOverride || !_viewCube || !_primaryCamera || !viewCubeScreenRect().contains(pixel) || isGltfCameraActive())
		return false;

	QVector3D outwardNormal;
	int regionId = -1;
	if (!pickViewCubeRegionAtPixel(pixel, outwardNormal, &regionId))
		return true;

	orientCameraToViewCubeNormal(outwardNormal);
	return true;
}

bool GLWidget::pickViewCubeRegionAtPixel(const QPoint& pixel, QVector3D& outwardNormal, int* regionId) const
{
	outwardNormal = QVector3D();
	if (regionId)
		*regionId = -1;
	if (!_showViewCubeOverride || !_viewCube || !_primaryCamera || !viewCubeScreenRect().contains(pixel) || isGltfCameraActive())
		return false;

	QRect viewportRect;
	QMatrix4x4 viewMatrix;
	QMatrix4x4 projectionMatrix;
	QMatrix4x4 modelMatrix;
	float cubeScale = 1.0f;
	if (!computeViewCubeRenderState(viewportRect, viewMatrix, projectionMatrix, modelMatrix, cubeScale))
		return false;

	const int glX = pixel.x();
	const int glY = height() - pixel.y() - 1;
	const QVector3D nearPoint(glX, glY, 0.0f);
	const QVector3D farPoint(glX, glY, 1.0f);
	const QVector3D rayOrigin = nearPoint.unproject(viewMatrix, projectionMatrix, viewportRect);
	const QVector3D rayFar = farPoint.unproject(viewMatrix, projectionMatrix, viewportRect);
	QVector3D rayDir = rayFar - rayOrigin;
	if (rayDir.lengthSquared() <= 1.0e-8f)
		return false;
	rayDir.normalize();

	ViewCubeMesh::RegionHit hit;
	if (!_viewCube->pickRegion(rayOrigin, rayDir, modelMatrix, hit))
		return false;

	outwardNormal = hit.outwardNormal;
	if (regionId)
		*regionId = hit.regionId;
	return true;
}

void GLWidget::updateViewCubeHover(const QPoint& pixel, Qt::MouseButtons buttons)
{
	const int previousRegionId = _viewCubeHoveredRegionId;
	if (!_showViewCubeOverride || buttons != Qt::NoButton || !_viewCube || isGltfCameraActive() || !viewCubeScreenRect().contains(pixel))
	{
		_viewCubeHoveredRegionId = -1;
	}
	else
	{
		QVector3D outwardNormal;
		int regionId = -1;
		_viewCubeHoveredRegionId = pickViewCubeRegionAtPixel(pixel, outwardNormal, &regionId) ? regionId : -1;
	}

	if (_viewCubeHoveredRegionId != previousRegionId)
		update();
}

void GLWidget::initializeViewCubeLabels()
{
	if (!_viewCubeLabelShader)
		return;

	const auto labelFaces = buildViewCubeLabelFaces(_labelTop, _labelFront, _labelLeft,
		tr("Bottom"), tr("Rear"), tr("Right"), cameraUpAxisConventionRotation());

	const TextureSamplerSettings samplers = {
		GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR
	};

	for (GLuint& texture : _viewCubeLabelTextures)
	{
		if (texture != 0)
		{
			glDeleteTextures(1, &texture);
			texture = 0;
		}
	}

	for (int i = 0; i < static_cast<int>(labelFaces.size()); ++i)
	{
		QImage image(kViewCubeStyle.labelTextureSize, kViewCubeStyle.labelTextureSize, QImage::Format_RGBA8888);
		image.fill(Qt::transparent);

		QPainter painter(&image);
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.setRenderHint(QPainter::TextAntialiasing, true);
		QFont font(QStringLiteral("Arial"));
		font.setBold(true);
		font.setPixelSize(kViewCubeStyle.labelFontPixelSize);
		font.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
		painter.setFont(font);
		painter.setPen(kViewCubeStyle.labelTextColor);
		painter.drawText(image.rect(), Qt::AlignCenter, labelFaces[i].text);
		painter.end();

		_viewCubeLabelTextures[i] = uploadDecodedTextureImage(image, samplers);
	}

	if (_viewCubeLabelVAO == 0)
		glGenVertexArrays(1, &_viewCubeLabelVAO);
	if (_viewCubeLabelVBO == 0)
		glGenBuffers(1, &_viewCubeLabelVBO);

	const float quadVertices[] = {
		-0.5f, -0.5f, 0.0f, 0.0f, 1.0f,
		 0.5f, -0.5f, 0.0f, 1.0f, 1.0f,
		 0.5f,  0.5f, 0.0f, 1.0f, 0.0f,
		-0.5f, -0.5f, 0.0f, 0.0f, 1.0f,
		 0.5f,  0.5f, 0.0f, 1.0f, 0.0f,
		-0.5f,  0.5f, 0.0f, 0.0f, 0.0f
	};

	glBindVertexArray(_viewCubeLabelVAO);
	glBindBuffer(GL_ARRAY_BUFFER, _viewCubeLabelVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(0));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
	glBindVertexArray(0);
}

void GLWidget::drawViewCube()
{
	if (!_showViewCubeOverride || !_viewCube || !_viewCubeShader)
		return;

	QRect viewportRect;
	QMatrix4x4 viewMatrix;
	QMatrix4x4 projectionMatrix;
	QMatrix4x4 modelMatrix;
	float cubeScale = 1.0f;
	if (!computeViewCubeRenderState(viewportRect, viewMatrix, projectionMatrix, modelMatrix, cubeScale))
		return;

	int prevViewport[4];
	glGetIntegerv(GL_VIEWPORT, prevViewport);

	GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
	GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
	GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
	GLboolean scissorWasEnabled = glIsEnabled(GL_SCISSOR_TEST);
	GLboolean stencilWasEnabled = glIsEnabled(GL_STENCIL_TEST);
	GLboolean polygonOffsetFillWasEnabled = glIsEnabled(GL_POLYGON_OFFSET_FILL);
	GLboolean depthMask = GL_TRUE;
	GLboolean colorMask[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
	GLint frontFaceMode = GL_CCW;
	GLint cullFaceMode = GL_BACK;
	GLint depthFuncMode = GL_LESS;
	glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
	glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
	glGetIntegerv(GL_FRONT_FACE, &frontFaceMode);
	glGetIntegerv(GL_CULL_FACE_MODE, &cullFaceMode);
	glGetIntegerv(GL_DEPTH_FUNC, &depthFuncMode);

	// The main scene may leave behind render-state choices that are correct for
	// PBR/ADS mesh passes but wrong for the View Cube overlay. Establish a
	// known-good state here so the cube remains visually stable across display
	// modes, rendering modes, and intermediate overlay passes.
	glEnable(GL_SCISSOR_TEST);
	glScissor(viewportRect.x(), viewportRect.y(), viewportRect.width(), viewportRect.height());
	glClear(GL_DEPTH_BUFFER_BIT);
	glViewport(viewportRect.x(), viewportRect.y(), viewportRect.width(), viewportRect.height());
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_POLYGON_OFFSET_FILL);
	glDepthMask(GL_TRUE);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);
	glDepthFunc(GL_LEQUAL);
	_viewCube->setSceneRenderTransformFast(modelMatrix);
	_viewCube->setProg(_viewCubeShader.get());

	_viewCubeShader->bind();
	_viewCubeShader->setProperty("globalModelMatrix", QVariant::fromValue(QMatrix4x4()));
	_viewCubeShader->setProperty("viewMatrix", QVariant::fromValue(viewMatrix));
	_viewCubeShader->setUniformValue("viewMatrix", viewMatrix);
	_viewCubeShader->setUniformValue("projectionMatrix", projectionMatrix);
	_viewCubeShader->setUniformValue("lightDirView", QVector3D(0.0f, 0.0f, 1.0f));
	_viewCubeShader->setUniformValue("baseColor", kViewCubeStyle.baseFaceColor);
	_viewCubeShader->setUniformValue("ambientStrength", kViewCubeStyle.baseAmbient);
	_viewCubeShader->setUniformValue("diffuseStrength", kViewCubeStyle.baseDiffuse);
	_viewCube->render();
	_viewCubeShader->setUniformValue("baseColor", kViewCubeStyle.primaryFaceColor);
	_viewCubeShader->setUniformValue("ambientStrength", kViewCubeStyle.primaryAmbient);
	_viewCubeShader->setUniformValue("diffuseStrength", kViewCubeStyle.primaryDiffuse);
	for (int regionId = 0; regionId < _viewCube->regionCount(); ++regionId)
	{
		if (_viewCube->isPrimaryFaceRegion(regionId))
			_viewCube->renderRegion(regionId);
	}
	if (_viewCubeHoveredRegionId >= 0)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_CULL_FACE);
		_viewCubeShader->setUniformValue("baseColor", kViewCubeStyle.hoverFaceColor);
		_viewCubeShader->setUniformValue("ambientStrength", kViewCubeStyle.hoverAmbient);
		_viewCubeShader->setUniformValue("diffuseStrength", kViewCubeStyle.hoverDiffuse);
		_viewCube->renderRegion(_viewCubeHoveredRegionId);
		glDisable(GL_BLEND);
		if (cullWasEnabled)
			glEnable(GL_CULL_FACE);
	}
	drawViewCubeLabels(viewMatrix, projectionMatrix, cubeScale);

	if (!depthWasEnabled)
		glDisable(GL_DEPTH_TEST);
	glDepthMask(depthMask);
	glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
	if (cullWasEnabled)
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);
	glCullFace(cullFaceMode);
	glFrontFace(frontFaceMode);
	glDepthFunc(depthFuncMode);
	if (blendWasEnabled)
		glEnable(GL_BLEND);
	if (stencilWasEnabled)
		glEnable(GL_STENCIL_TEST);
	if (polygonOffsetFillWasEnabled)
		glEnable(GL_POLYGON_OFFSET_FILL);
	if (!scissorWasEnabled)
		glDisable(GL_SCISSOR_TEST);

	glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
}

void GLWidget::drawViewCubeLabels(const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix, float cubeScale)
{
	if (!_viewCubeLabelShader || _viewCubeLabelVAO == 0)
		return;

	const std::array<QMatrix4x4, 6> labelTransforms = [this, cubeScale]() {
		std::array<QMatrix4x4, 6> transforms;
		const float offset = kViewCubeStyle.labelFaceOffset * cubeScale;
		const float scale = kViewCubeStyle.labelFaceScale * cubeScale;

		auto faceTransform = [offset, scale](const QVector3D& center,
			const QVector3D& right,
			const QVector3D& up,
			const QVector3D& normal) {
			QMatrix4x4 matrix;
			matrix.setColumn(0, QVector4D(right.normalized() * scale, 0.0f));
			matrix.setColumn(1, QVector4D(up.normalized() * scale, 0.0f));
			matrix.setColumn(2, QVector4D(normal.normalized(), 0.0f));
			matrix.setColumn(3, QVector4D(center + normal.normalized() * offset, 1.0f));
			return matrix;
		};

		const auto faces = buildViewCubeLabelFaces(_labelTop, _labelFront, _labelLeft,
			tr("Bottom"), tr("Rear"), tr("Right"), cameraUpAxisConventionRotation());
		for (int i = 0; i < static_cast<int>(faces.size()); ++i)
			transforms[i] = faceTransform(QVector3D(0.0f, 0.0f, 0.0f), faces[i].right, faces[i].up, faces[i].normal);

		return transforms;
	}();

	GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
	GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);

	_viewCubeLabelShader->bind();
	_viewCubeLabelShader->setUniformValue("viewMatrix", viewMatrix);
	_viewCubeLabelShader->setUniformValue("projectionMatrix", projectionMatrix);
	_viewCubeLabelShader->setUniformValue("labelTexture", 0);

	glActiveTexture(GL_TEXTURE0);
	glBindVertexArray(_viewCubeLabelVAO);

	for (int i = 0; i < static_cast<int>(_viewCubeLabelTextures.size()); ++i)
	{
		if (_viewCubeLabelTextures[i] == 0)
			continue;
		glBindTexture(GL_TEXTURE_2D, _viewCubeLabelTextures[i]);
		_viewCubeLabelShader->setUniformValue("modelMatrix", labelTransforms[i]);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, 0);
	if (!blendWasEnabled)
		glDisable(GL_BLEND);
	if (cullWasEnabled)
		glEnable(GL_CULL_FACE);
}

void GLWidget::drawLights()
{
	QMatrix4x4 model;
	model.translate(effectiveWorldLightPosition());
	_lightCubeShader->bind();
	QMatrix4x4 viewMat = _viewMatrix;	
	_lightCubeShader->setProperty("globalModelMatrix", QVariant::fromValue(QMatrix4x4()));
	_lightCubeShader->setProperty("viewMatrix", QVariant::fromValue(viewMat));
	_lightCubeShader->setUniformValue("viewMatrix", viewMat);
	_lightCubeShader->setUniformValue("projectionMatrix", _projectionMatrix);
	_lightCubeShader->setUniformValue("lightColor", _diffuseLight.toVector3D());	
	_lightCubeShader->setUniformValue("intensity", 1.0f);
	_lightCubeShader->setUniformValue("intensityScale", 1.0f);  // Tune brightness
	_lightCube->setSceneRenderTransformFast(model);
	_lightCube->render();

	// Draw punctual lights — only those whose enabled flag is set (tree checkbox AND Show Lights).
	if (!_currentRepositionedLights.empty())
	{
		const float sceneRadius = std::max(_boundingSphere.getRadius(), 0.001f);
		// Keep punctual-light spheres visually aligned with the default light cube size.
		// The cube side is baked to sceneRadius * 0.1f, while Sphere is created with radius 1.
		// Matching the sphere diameter to the cube side means a scale of sceneRadius * 0.05f.
		const float pointSpotScale = sceneRadius * 0.05f;
		const float directionalScale = pointSpotScale * 0.75f;
		const QVector3D sceneCenter = _boundingSphere.getCenter();

		// Build a fast enabled-lookup from the scene graph (same path used by the GPU upload).
		// Falls back to "all enabled" when the map is absent (legacy/MVF path).
		SceneGraph* sg = (_viewer ? _viewer->sceneGraph() : nullptr);
		const bool hasEnabledMap = sg && !_lightFileIndexMap.isEmpty()
		    && static_cast<int>(_currentRepositionedLights.size()) == _lightFileIndexMap.size();

		for (int lightIdx = 0; lightIdx < static_cast<int>(_currentRepositionedLights.size()); ++lightIdx)
		{
			// Respect per-light enabled flag — disabled lights have no gizmo.
			if (hasEnabledMap)
			{
				const LightOrigin& origin = _lightFileIndexMap[lightIdx];
				const GltfLightData& ld   = sg->lightDataForFile(origin.file);
				if (origin.index >= static_cast<int>(ld.lights.size()) || !ld.lights[origin.index].enabled)
					continue;
			}

			const GPULight& light = _currentRepositionedLights[lightIdx];

			// Map intensity logarithmically to [0.3, 1.0] so gizmos are always visible
			// (even at intensity=0) while brighter lights appear more luminous.
			// log10(intensity + 1) / 3  maps  0 → 0,  100 → 0.67,  999 → 1.0
			const float logVal            = std::log10(light.intensity + 1.0f) / 3.0f;
			const float displayBrightness = 0.3f + 0.7f * std::min(logVal, 1.0f);

			// Emission colour: the light's own colour modulated by display brightness.
			const QVector3D emissiveColor(light.color.x * displayBrightness,
			                              light.color.y * displayBrightness,
			                              light.color.z * displayBrightness);

			QMatrix4x4 lightModel;
			const LightType lightType = static_cast<LightType>(light.type);

			if (lightType == LightType::Directional)
			{
				// Directional lights are defined by direction rather than emitter position.
				// Show them as a small cube offset from the scene center along the incoming direction
				// so multiple directional lights don't collapse visually at the origin.
				QVector3D dir(light.direction.x, light.direction.y, light.direction.z);
				if (dir.lengthSquared() < 1e-6f)
					dir = QVector3D(0.0f, 0.0f, -1.0f);
				dir.normalize();
				const QVector3D markerPos = sceneCenter - dir * (sceneRadius + directionalScale * 6.0f);
				lightModel.translate(markerPos);
				lightModel.scale(directionalScale);
			}
			else
			{
				lightModel.translate(light.position.x, light.position.y, light.position.z);
				lightModel.scale(pointSpotScale);
			}

			_lightCubeShader->bind();
			_lightCubeShader->setProperty("globalModelMatrix", QVariant::fromValue(QMatrix4x4()));
			_lightCubeShader->setProperty("viewMatrix", QVariant::fromValue(viewMat));
			_lightCubeShader->setUniformValue("viewMatrix", viewMat);
			_lightCubeShader->setUniformValue("projectionMatrix", _projectionMatrix);
			_lightCubeShader->setUniformValue("lightColor", emissiveColor);
			_lightCubeShader->setUniformValue("intensity", 1.0f);
			_lightCubeShader->setUniformValue("intensityScale", 1.0f);

			if (lightType == LightType::Directional)
			{
				_lightCube->setSceneRenderTransformFast(lightModel);
				_lightCube->render();
			}
			else
			{
				_lightSphere->setSceneRenderTransformFast(lightModel);
				_lightSphere->render();
			}
		}
	}
}

void GLWidget::bindIBLTextures()
{
	_fgShader->setUniformValue("irradianceMap", 3);
	glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_CUBE_MAP, _irradianceMap);
	_fgShader->setUniformValue("prefilterMap", 4);
	glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_CUBE_MAP, _prefilterMap);
	_fgShader->setUniformValue("brdfLUT", 5);
	glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, _brdfLUTTexture);
	_fgShader->setUniformValue("sheenPrefilterMap", 7);
	glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_CUBE_MAP, _sheenPrefilterMap);
	_fgShader->setUniformValue("charlieLUT", 8);
	glActiveTexture(GL_TEXTURE8); glBindTexture(GL_TEXTURE_2D, _charlieLUTTexture);
	_fgShader->setUniformValue("sheenELUT",  9);
	glActiveTexture(GL_TEXTURE9); glBindTexture(GL_TEXTURE_2D, _sheenELUTTexture);
	// Effective mip count for sheen LOD: lod = roughness * (sheenPrefilterMipLevels - 1)
	int sheenMips = (_sheenPrefilterMipLevels > 0) ? (int)_sheenPrefilterMipLevels : 5;
	_fgShader->setUniformValue("sheenPrefilterMipLevels", sheenMips);
	// Effective mip count for GGX specular LOD: lod = roughness * (prefilterMipLevels - 1)
	int prefilterMips = (_prefilterMipLevels > 0) ? (int)_prefilterMipLevels : 5;
	_fgShader->setUniformValue("prefilterMipLevels", prefilterMips);
}


void GLWidget::render(GLCamera* camera)
{
	QElapsedTimer frameTimer;
	const bool profileRendering =
		QSettings().value(QStringLiteral("profileRenderingCheckBox"), false).toBool();
	TriangleMesh::beginRenderDiagnosticsFrame(profileRendering);
	if (profileRendering)
		frameTimer.start();

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glFrontFace(GL_CCW);
	glDisable(GL_POLYGON_OFFSET_FILL);
	glDepthMask(GL_TRUE);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glStencilMask(0xFF);
	glDisable(GL_STENCIL_TEST);

	_viewMatrix.setToIdentity();
	_viewMatrix = camera->getViewMatrix();
	// Keep the scene radius in the camera current every frame so the ortho
	// near/far planes always cover the full scene, including animated models
	// whose GPU-skinned geometry extends beyond the bind-pose bounding sphere.
	camera->setSceneRadius(_boundingSphere.getRadius());
	_projectionMatrix = camera->getProjectionMatrix();
	_modelViewMatrix = _viewMatrix * _modelMatrix;

	// Extract frustum planes once per frame for AABB culling
	extractFrustumPlanes();
	refreshRuntimeVisibilityCacheForCurrentView();

	// --- 1) Skybox ---
	if (_skyBoxEnabled)
	{
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		drawSkyBox();
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
	}

	// --- 2) Opaque meshes (with clipping) ---
	glActiveTexture(GL_TEXTURE0 + 37);
	glBindTexture(GL_TEXTURE_2D, _sssCaptureTexture != 0 ? _sssCaptureTexture : _whiteTexture);
	glActiveTexture(GL_TEXTURE0 + 38);
	glBindTexture(GL_TEXTURE_2D, _sssDepthTexture != 0 ? _sssDepthTexture : _whiteTexture);
	glActiveTexture(GL_TEXTURE0);

	_fgShader->bind();
	TriangleMesh::recordProgramBindCall(true);
	setCommonUniforms(_fgShader.get(), camera);	
	{
		QElapsedTimer opaqueTimer;
		if (profileRendering)
			opaqueTimer.start();
		drawMeshesWithClipping(_fgShader.get(), false); // opaque pass
		if (profileRendering)
			TriangleMesh::recordOpaquePassCpuMs(static_cast<double>(opaqueTimer.nsecsElapsed()) / 1000000.0);
	}
	_fgShader->release();

	// --- 2.5) Section caps (after opaque, before floor & transparents) ---
	if (_cappingEnabled &&
		!_sectionCapsSuppressedDuringInteraction &&
		(_clipYZEnabled || _clipZXEnabled || _clipXYEnabled))
	{
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(1.0f, 1.0f); // pull forward
		drawSectionCapping();
		glDisable(GL_POLYGON_OFFSET_FILL);
	}

	// --- 3) Ground ---
	if (_displayMode == DisplayMode::REALSHADED &&
		_groundMode != GroundMode::None && !_cappingEnabled &&
		!_meshStore.empty() &&
		camera != _orthoViewsCamera)
	{
		if (_groundMode == GroundMode::Floor)
		{
			glActiveTexture(GL_TEXTURE0 + 32);
			glBindTexture(GL_TEXTURE_2D,
				(camera == _primaryCamera && _transmissionColorTexture != 0) ? _transmissionColorTexture : _whiteTexture);
			glActiveTexture(GL_TEXTURE0 + 33);
			glBindTexture(GL_TEXTURE_2D,
				(camera == _primaryCamera && _transmissionDepthTexture != 0) ? _transmissionDepthTexture : _whiteTexture);
			glActiveTexture(GL_TEXTURE0);
			QElapsedTimer floorTimer;
			if (profileRendering)
				floorTimer.start();
			drawFloor();
			if (profileRendering)
				TriangleMesh::recordFloorPassCpuMs(static_cast<double>(floorTimer.nsecsElapsed()) / 1000000.0);
		}
		else if (_groundMode == GroundMode::Grid)
		{
			drawGrid();
		}
	}

	if (camera == _primaryCamera)
	{
		// Bind transmission texture for shader sampling
		glActiveTexture(GL_TEXTURE0 + 32);
		glBindTexture(GL_TEXTURE_2D, _transmissionColorTexture);

		glActiveTexture(GL_TEXTURE0 + 33);
		glBindTexture(GL_TEXTURE_2D, _transmissionDepthTexture);
	}

	// --- 4) Transparent meshes (with clipping) ---
	_fgShader->bind();
	TriangleMesh::recordProgramBindCall(true);
	setCommonUniforms(_fgShader.get(), camera);
	{
		QElapsedTimer transparentTimer;
		if (profileRendering)
			transparentTimer.start();
		drawMeshesWithClipping(_fgShader.get(), true); // transparent pass
		if (profileRendering)
			TriangleMesh::recordTransparentPassCpuMs(static_cast<double>(transparentTimer.nsecsElapsed()) / 1000000.0);
	}
	_fgShader->release();

	// --- 5) Overlays ---
	if (_showAxis && _userShowAxisOverride) drawAxis(camera);
	if (_showLights) drawLights();
	if (profileRendering)
		TriangleMesh::recordFrameCpuMs(static_cast<double>(frameTimer.nsecsElapsed()) / 1000000.0);
	TriangleMesh::flushRenderDiagnostics();
}



void GLWidget::renderToShadowBuffer()
{
	if (!_shadowMapNeedsInitialization)
		return;
	_shadowMapNeedsInitialization = false;

	// save current viewport
	int viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);

	/// Shadow Mapping
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glViewport(0, 0, _shadowWidth, _shadowHeight);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _shadowMapFBO);
	glClear(GL_DEPTH_BUFFER_BIT);
	glDisable(GL_CULL_FACE);

	// 1. render depth of scene to texture (from light's perspective)
	// --------------------------------------------------------------
	QMatrix4x4 lightProjection, lightView;	
	QVector3D center = _boundingSphere.getCenter();
	float radius = _boundingSphere.getRadius();
	QVector3D lightPos = effectiveWorldLightPosition();

	// Light looks at scene center
	QVector3D lightDir = center;
	QVector3D lightUp = currentWorldUpVector();
	if (std::abs(QVector3D::dotProduct((lightDir - lightPos).normalized(), lightUp)) > 0.98f)
	{
		// Light direction nearly collinear with world-up: pick the least-aligned world axis
		const float ax = std::abs(lightUp.x());
		const float ay = std::abs(lightUp.y());
		const float az = std::abs(lightUp.z());
		lightUp = (ax <= ay && ax <= az) ? QVector3D(1.0f, 0.0f, 0.0f)
		        : (ay <= az)             ? QVector3D(0.0f, 1.0f, 0.0f)
		                                 : QVector3D(0.0f, 0.0f, 1.0f);
	}

	lightView.lookAt(lightPos, lightDir, lightUp);

	BoundingBox shadowCasterBounds;
	BoundingBox skinnedBounds;
	bool hasShadowCasterBounds = false;
	bool hasSkinnedBounds = false;
	const std::vector<int>& visibleIds = _visibleSwapped ? _hiddenObjectsIds : _displayedObjectsIds;
	for (int i : visibleIds)
	{
		try
		{
			TriangleMesh* mesh = _meshStore.at(i);
			if (!mesh || !isMeshAnimationVisible(mesh))
				continue;

			const BoundingBox bb = mesh->getBoundingBox();
			if (!hasShadowCasterBounds)
			{
				shadowCasterBounds = bb;
				hasShadowCasterBounds = true;
			}
			else
			{
				shadowCasterBounds.addBox(bb);
			}

			if (mesh->hasSkinning())
			{
				if (!hasSkinnedBounds) { skinnedBounds = bb; hasSkinnedBounds = true; }
				else skinnedBounds.addBox(bb);
			}
		}
		catch (const std::exception&)
		{
		}
	}

	if (!hasShadowCasterBounds)
	{
		const float fallbackExtent = (std::max)(radius, 0.001f);
		shadowCasterBounds.setLimits(
			center.x() - fallbackExtent, center.x() + fallbackExtent,
			center.y() - fallbackExtent, center.y() + fallbackExtent,
			center.z() - fallbackExtent, center.z() + fallbackExtent);
	}

	if (hasSkinnedBounds)
	{
		const float skinPad = (std::max)(static_cast<float>(skinnedBounds.getMaxDimension()) * 0.1f, 0.01f);
		skinnedBounds.setLimits(
			skinnedBounds.xMin() - skinPad, skinnedBounds.xMax() + skinPad,
			skinnedBounds.yMin() - skinPad, skinnedBounds.yMax() + skinPad,
			skinnedBounds.zMin() - skinPad, skinnedBounds.zMax() + skinPad);
		shadowCasterBounds.addBox(skinnedBounds);
	}

	float lsMinX =  std::numeric_limits<float>::max();
	float lsMaxX = -std::numeric_limits<float>::max();
	float lsMinY =  std::numeric_limits<float>::max();
	float lsMaxY = -std::numeric_limits<float>::max();
	float lsMinZ =  std::numeric_limits<float>::max();
	float lsMaxZ = -std::numeric_limits<float>::max();
	const QMatrix4x4 lightModel = lightView * _modelMatrix;
	for (const QVector3D& corner : shadowCasterBounds.getCorners())
	{
		const QVector3D ls = lightModel.map(corner);
		lsMinX = (std::min)(lsMinX, ls.x());
		lsMaxX = (std::max)(lsMaxX, ls.x());
		lsMinY = (std::min)(lsMinY, ls.y());
		lsMaxY = (std::max)(lsMaxY, ls.y());
		lsMinZ = (std::min)(lsMinZ, ls.z());
		lsMaxZ = (std::max)(lsMaxZ, ls.z());
	}

	const float rawW = lsMaxX - lsMinX;
	const float rawH = lsMaxY - lsMinY;
	_shadowFrustumExtentW = rawW;
	_shadowFrustumExtentH = rawH;
	const float texelPadX = rawW / (std::max)(static_cast<float>(_shadowWidth), 1.0f) * 4.0f;
	const float texelPadY = rawH / (std::max)(static_cast<float>(_shadowHeight), 1.0f) * 4.0f;
	const float xyPad = (std::max)((std::max)(texelPadX, texelPadY), (std::max)(radius * 0.005f, 0.001f));
	const float nearDist = (std::max)(0.01f, -lsMaxZ);
	const float farDist = (std::max)(nearDist + 0.01f, -lsMinZ + radius * 1.5f);
	_shadowFarDist = farDist;

	lightProjection.ortho(
		lsMinX - xyPad, lsMaxX + xyPad,
		lsMinY - xyPad, lsMaxY + xyPad,
		nearDist, farDist
	);

	_lightSpaceMatrix = lightProjection * lightView;
	TriangleMesh::setCurrentRenderContext(_modelMatrix, lightView);

	// render scene from light's point of view
	_shadowMappingShader->bind();
	_shadowMappingShader->setUniformValue("lightSpaceMatrix", _lightSpaceMatrix);
	_shadowMappingShader->setUniformValue("model", _modelMatrix);
	_shadowMappingShader->setProperty("globalModelMatrix", QVariant::fromValue(_modelMatrix));

	const float fsMinX = lsMinX - xyPad;
	const float fsMaxX = lsMaxX + xyPad;
	const float fsMinY = lsMinY - xyPad;
	const float fsMaxY = lsMaxY + xyPad;
	const float fsMinZ = -farDist;
	const float fsMaxZ = -nearDist;

	auto isMeshOutsideShadowVolume = [&](const TriangleMesh* mesh) -> bool
	{
		if (!mesh)
			return true;

		const BoundingSphere sphere = mesh->getBoundingSphere();
		const QMatrix4x4 worldTransform = _modelMatrix * mesh->combinedRenderTransform();
		const QVector3D col0(worldTransform(0, 0), worldTransform(1, 0), worldTransform(2, 0));
		const QVector3D col1(worldTransform(0, 1), worldTransform(1, 1), worldTransform(2, 1));
		const QVector3D col2(worldTransform(0, 2), worldTransform(1, 2), worldTransform(2, 2));
		const float maxScale = (std::max)(1.0f, (std::max)(col0.length(), (std::max)(col1.length(), col2.length())));
		const float radiusWithSlack = (std::max)(sphere.getRadius() * maxScale, 0.001f) * 1.05f;
		const QVector3D centerLS = (lightView * worldTransform * QVector4D(sphere.getCenter(), 1.0f)).toVector3D();

		return centerLS.x() + radiusWithSlack < fsMinX ||
		       centerLS.x() - radiusWithSlack > fsMaxX ||
		       centerLS.y() + radiusWithSlack < fsMinY ||
		       centerLS.y() - radiusWithSlack > fsMaxY ||
		       centerLS.z() + radiusWithSlack < fsMinZ ||
		       centerLS.z() - radiusWithSlack > fsMaxZ;
	};

	if (_meshStore.size() != 0)
	{
		for (int i : visibleIds)
		{
			try
			{
				TriangleMesh* mesh = _meshStore.at(i);
				if (mesh && isMeshAnimationVisible(mesh) && !isMeshOutsideShadowVolume(mesh))
				{
					mesh->setProg(_shadowMappingShader.get());
					mesh->getVAO().bind();					
					mesh->renderShadow();
					mesh->getVAO().release();
				}
			}
			catch (const std::exception& ex)
			{
				std::cout << "Exception raised in GLWidget::renderToShadowBuffer\n" << ex.what() << std::endl;
			}
		}
	}

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, defaultFramebufferObject());
	// End Shadow Mapping
	// restore viewport
	glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
}

int GLWidget::processSelection(const QPoint& pixel)
{
	int id = -1;

	// Get the list of objects to render (all visible objects)
	const auto& visibleIds = _visibleSwapped ? _hiddenObjectsIds : _displayedObjectsIds;

	if (visibleIds.empty())
		return -1;

	// Note: Even with one visible object, we must perform FBO rendering and color picking
	// to determine if the click actually hit that object (vs empty space).
	// FBO rendering with depth testing will correctly return -1 if click missed.

	// Render all visible objects to FBO and perform color picking to determine topmost
	makeCurrent();

	// Validate GL context and dimensions
	if (width() <= 0 || height() <= 0)
		return -1;

	if(_selectionFBO == 0)
		glGenFramebuffers(1, &_selectionFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, _selectionFBO);
#ifdef GL_FRAMEBUFFER_DEFAULT_SAMPLES
		glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_SAMPLES, 0);
#else // MacOS
		glFramebufferParameteri(GL_FRAMEBUFFER, 0, 0);
#endif

	if(_selectionRBO == 0)
		glGenRenderbuffers(1, &_selectionRBO);
	glBindRenderbuffer(GL_RENDERBUFFER, _selectionRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA, width(), height());
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _selectionRBO);
	GLenum DrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, DrawBuffers);
	if(_selectionDBO == 0)
		glGenRenderbuffers(1, &_selectionDBO);
	glBindRenderbuffer(GL_RENDERBUFFER, (GLuint)_selectionDBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width(), height());
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _selectionDBO);
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cout << "Failed to create selection framebuffer: " << status << std::endl;
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, defaultFramebufferObject());
		return -1;
	}

	// save current viewport
	int viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);

	// Resolve the camera and sub-viewport for the clicked pixel.
	// In multi-view mode each quadrant uses a different camera; the selection FBO
	// must replicate the exact same view/projection and GL viewport so that the
	// rendered pixel positions match the on-screen positions.
	GLCamera* selCamera = getCameraForPoint(pixel);

	int selVpX = 0, selVpY = 0, selVpW = width(), selVpH = height();
	if (_multiViewActive)
	{
		const int hw = width() / 2, hh = height() / 2;
		// Qt pixel coords have y=0 at top; GL viewport y=0 at bottom.
		if (pixel.x() < width() / 2 && pixel.y() > height() / 2)        // Qt bottom-left → Top view
			{ selVpX = 0;  selVpY = 0;  selVpW = hw; selVpH = hh; }
		else if (pixel.x() < width() / 2 && pixel.y() <= height() / 2)  // Qt top-left   → Front view
			{ selVpX = 0;  selVpY = hh; selVpW = hw; selVpH = hh; }
		else if (pixel.x() >= width() / 2 && pixel.y() < height() / 2)  // Qt top-right  → Left view
			{ selVpX = hw; selVpY = hh; selVpW = hw; selVpH = hh; }
		else                                                               // Qt bottom-right → Isometric
			{ selVpX = hw; selVpY = 0;  selVpW = hw; selVpH = hh; }
	}

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glViewport(0, 0, width(), height());
	glBindFramebuffer(GL_FRAMEBUFFER, _selectionFBO);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glViewport(selVpX, selVpY, selVpW, selVpH);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	_selectionShader->bind();
	_selectionShader->setUniformValue("projectionMatrix", selCamera->getProjectionMatrix());
	_selectionShader->setProperty("globalModelMatrix", QVariant::fromValue(_modelMatrix));
	_selectionShader->setUniformValue("viewMatrix", selCamera->getViewMatrix());

	// Render ALL visible objects to FBO (not just ray-hit ones)
	// This ensures color picking is a true fallback method, independent of ray test results
	for (int i : std::as_const(visibleIds))
	{
		try
		{
			TriangleMesh* mesh = _meshStore.at(i);
			if (mesh && isMeshAnimationVisible(mesh))
			{
				QColor pickColor = indexToColor(i + 1);
				_selectionShader->bind();

				const float r = pickColor.redF();
				const float g = pickColor.greenF();
				const float b = pickColor.blueF();
				const float a = pickColor.alphaF();

				_selectionShader->setUniformValue("pickingColor", QVector4D(r, g, b, a));
				// Explosion offset is baked into combinedRenderTransform() via
				// TriangleMesh::_explosionOffset — no extra handling needed here.
				_selectionShader->setUniformValue("modelMatrix", mesh->combinedRenderTransform());
				_selectionShader->setUniformValue("hasSkinning", mesh->hasSkinning());
				_selectionShader->setUniformValue("jointCount", static_cast<int>(mesh->jointPalette().size()));
				if (mesh->hasSkinning() && !mesh->jointPalette().isEmpty())
				{
					const int maxJoints = std::min(static_cast<int>(mesh->jointPalette().size()), 128);
					for (int jointIndex = 0; jointIndex < maxJoints; ++jointIndex)
					{
						const QString uniformName = QStringLiteral("jointMatrices[%1]").arg(jointIndex);
						_selectionShader->setUniformValue(uniformName.toUtf8().constData(), mesh->jointPalette()[jointIndex]);
					}
				}
				mesh->setProg(_selectionShader.get());
				mesh->getVAO().bind();
				if (mesh->getIndices().empty())
					glDrawArrays(mesh->getPrimitiveMode(), 0, static_cast<int>(mesh->getPoints().size() / 3));
				else
					glDrawElements(mesh->getPrimitiveMode(), static_cast<int>(mesh->getIndices().size()), GL_UNSIGNED_INT, 0);
				mesh->getVAO().release();
				glFlush();
				glFinish();
			}
		}
		catch (const std::exception& ex)
		{
			std::cout << "Exception raised in GLWidget::renderToSelectionBuffer\n" << ex.what() << std::endl;
		}
	}

	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	int pixelWinSize = 2;

	// Calculate safe pixel read coordinates with bounds checking.
	// The selection FBO is always width() x height() in size.
	// pixel is in Qt widget coords (y=0 at top); GL FBO coords have y=0 at bottom.
	int readX = pixel.x() - pixelWinSize / 2;
	int readY = height() - pixel.y() - 1 + pixelWinSize / 2;

	// Clamp to FBO bounds
	if (readX < 0) readX = 0;
	if (readY < 0) readY = 0;
	if (readX + pixelWinSize > width())  readX = width()  - pixelWinSize;
	if (readY + pixelWinSize > height()) readY = height() - pixelWinSize;

	// Ensure dimensions don't exceed bounds
	int readWidth = pixelWinSize;
	int readHeight = pixelWinSize;
	if (readX + readWidth > width())   readWidth  = width()  - readX;
	if (readY + readHeight > height()) readHeight = height() - readY;

	if (readWidth <= 0 || readHeight <= 0)
	{
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, defaultFramebufferObject());
		glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
		return -1; // Click outside viewport
	}

	std::vector<float> res(static_cast<size_t>(readWidth) * readHeight * 4);
	glReadPixels(readX, readY, readWidth, readHeight, GL_RGBA, GL_FLOAT, res.data());
	std::map<int, int> voteCount;
	for (size_t i = 0; i < res.size(); i += 4)
	{
		QColor col = QColor::fromRgbF(res[i + 0], res[i + 1], res[i + 2], res[i + 3]);
		unsigned int colId = colorToIndex(col);
		if (colId != 0)
			voteCount[colId - 1]++;
	}
	if (!voteCount.empty())
		id = std::max_element(voteCount.begin(), voteCount.end(), voteCount.value_comp())->first;

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, defaultFramebufferObject());
	// restore viewport
	glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

	return id;
}

void GLWidget::renderQuad()
{
	if (_quadVAO == 0)
	{
		float quadVertices[] = {
			// positions        // texture Coords
			-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
			-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
			1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
			1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
		};
		// setup plane VAO
		glGenVertexArrays(1, &_quadVAO);
		glGenBuffers(1, &_quadVBO);
		glBindVertexArray(_quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, _quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	}
	glBindVertexArray(_quadVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
}

void GLWidget::renderMeshWithDisplayMode(TriangleMesh* mesh, DisplayMode mode)
{
	QElapsedTimer meshModeTimer;
	const bool profiling = TriangleMesh::renderDiagnosticsEnabled();
	if (profiling)
		meshModeTimer.start();

	_fgShader->bind();
	TriangleMesh::recordProgramBindCall(true);
	TriangleMesh::notifyProgramBound(_fgShader.get()); // keep program cache coherent with raw bind
	GLint modelViewLoc;
	GLint prog = 0;
	switch (mode)
	{
		// ============================================
	case DisplayMode::SHADED:
	case DisplayMode::REALSHADED:
		// ============================================
		// SHADED / REALSHADED: Solid rendering only
		// ============================================
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glLineWidth(1.0f);
		glDisable(GL_POLYGON_OFFSET_FILL);
		_fgShader->bind();
		TriangleMesh::recordProgramBindCall(true);
		mesh->render();
		break;

	case DisplayMode::FLATSHADED:
		// ============================================
		// FLATSHADED: Use a geometry shader to compute
		// the true face normal for triangle meshes.
		// Non-triangle primitives fall back to _fgShader
		// (flat qualifier picks provoking-vertex normal).
		// ============================================
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glLineWidth(1.0f);
		glDisable(GL_POLYGON_OFFSET_FILL);
		if (_fgFlatShader && _fgFlatShader->isLinked()
		    && mesh->getPrimitiveMode() == GL_TRIANGLES)
		{
			// Sync all current _fgShader uniform state (per-frame + per-mesh
			// from previous draw) to _fgFlatShader before updating per-mesh
			// uniforms via mesh->render().
			syncUniformsToFlatShader();
			_fgFlatShader->bind();
			TriangleMesh::recordProgramBindCall(true);
			mesh->setProg(_fgFlatShader.get());
			mesh->render();
			// Restore main shader so subsequent draws (wireframe overlay, etc.)
			// use the correct program.
			_fgShader->bind();
			TriangleMesh::recordProgramBindCall(true);
		}
		else
		{
			_fgShader->bind();
			TriangleMesh::recordProgramBindCall(true);
			mesh->render();
		}
		break;

		// ============================================
	case DisplayMode::WIREFRAME:
		// ============================================
		// WIREFRAME: Lines only
		// ============================================
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glLineWidth(1.25f);
		glDisable(GL_POLYGON_OFFSET_FILL);
		_fgShader->bind();
		TriangleMesh::recordProgramBindCall(true);
		mesh->render();
		break;

		// ============================================
	case DisplayMode::WIRESHADED:
		// Pass 1: Solid
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glLineWidth(1.0f);
		_fgShader->bind();
		TriangleMesh::recordProgramBindCall(true);
		_fgShader->setUniformValue("isWireframePass", false);
		mesh->render();
				
		// Pass 2: Wireframe overlay
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glLineWidth(1.5f);
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(-1.0f, -1.0f);

		_fgShader->bind();		
		TriangleMesh::recordProgramBindCall(true);
		_fgShader->setUniformValue("isWireframePass", true);
		mesh->render();

		glDisable(GL_POLYGON_OFFSET_FILL);
		_fgShader->setUniformValue("isWireframePass", false);
		break;

	default:
		// Safety fallback: solid rendering
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glLineWidth(1.0f);
		glDisable(GL_POLYGON_OFFSET_FILL);
		_fgShader->bind();
		TriangleMesh::recordProgramBindCall(true);
		mesh->render();
		break;
	}

	// Reset to default state (important!)
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glLineWidth(1.0f);
	glDisable(GL_POLYGON_OFFSET_FILL);

	_fgShader->release();
	TriangleMesh::resetBoundProgramCacheForCurrentContext();
	if (profiling)
		TriangleMesh::recordRenderMeshWithDisplayModeCpuMs(static_cast<double>(meshModeTimer.nsecsElapsed()) / 1000000.0);
}

void GLWidget::gradientBackground(float top_r, float top_g, float top_b, float top_a,
	float bot_r, float bot_g, float bot_b, float bot_a, int gradientStyle)
{
	glViewport(0, 0, width(), height());
	if (!_bgVAO.isCreated())
	{
		_bgVAO.create();
	}

	glDisable(GL_DEPTH_TEST);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	_bgShader->bind();		
	_bgShader->setUniformValue("top_color", QVector4D(top_r, top_g, top_b, top_a));
	_bgShader->setUniformValue("bot_color", QVector4D(bot_r, bot_g, bot_b, bot_a));
	_bgShader->setUniformValue("gradient_style", gradientStyle);  // Pass the gradient style

	_bgVAO.bind();
	glDrawArrays(GL_TRIANGLES, 0, 3);

	glEnable(GL_DEPTH_TEST);

	_bgVAO.release();
	_bgShader->release();
}

void GLWidget::loadBgColorSettings()
{
	QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());

	// Retrieve and validate top color
	QVariant topColorValue = settings.value("Background/TopColor");
	if (topColorValue.isValid() && topColorValue.canConvert<QColor>())
	{
		_bgTopColor = topColorValue.value<QColor>();
	}
	else
	{
		_bgTopColor = QColor::fromRgbF(0.45f, 0.45f, 0.45f, 1.0f);
	}

	// Retrieve and validate bottom color
	QVariant bottomColorValue = settings.value("Background/BottomColor");
	if (bottomColorValue.isValid() && bottomColorValue.canConvert<QColor>())
	{
		_bgBotColor = bottomColorValue.value<QColor>();
	}
	else
	{		
		_bgBotColor = QColor::fromRgbF(0.9f, 0.9f, 0.9f, 1.0f);
		
	}

	// Retrieve and validate gradient style
	QVariant gradientStyleValue = settings.value("Background/GradientStyle");
	if (gradientStyleValue.isValid() && gradientStyleValue.canConvert<int>())
	{
		int style = gradientStyleValue.toInt();
		if (style >= 0 && style <= 3)
		{
			_gradientStyle = style;
		}
		else
		{
			_gradientStyle = 0; // Default to vertical gradient
		}
	}
	else
	{
		_gradientStyle = 0; // Default to vertical gradient
	}
}


void GLWidget::splitScreen()
{
	if (!_bgSplitVAO.isCreated())
	{
		_bgSplitVAO.create();
		_bgSplitVAO.bind();
	}

	if (!_bgSplitVBO.isCreated())
	{
		_bgSplitVBO = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
		_bgSplitVBO.create();
		_bgSplitVBO.bind();
		_bgSplitVBO.setUsagePattern(QOpenGLBuffer::StaticDraw);

		static const std::vector<float> vertices = {
			-static_cast<float>(width()) / 2,
			0,
			static_cast<float>(width()) / 2,
			0,
			0,
			-static_cast<float>(height()) / 2,
			0,
			static_cast<float>(height()) / 2,
		};

		_bgSplitVBO.allocate(vertices.data(), static_cast<int>(vertices.size() * sizeof(float)));

		_bgSplitShader->bind();
		_bgSplitShader->enableAttributeArray("vertexPosition");
		_bgSplitShader->setAttributeBuffer("vertexPosition", GL_FLOAT, 0, 2);

		_bgSplitVBO.release();
	}

	glViewport(0, 0, width(), height());

	glDisable(GL_DEPTH_TEST);

	_bgSplitVAO.bind();
	glLineWidth(0.5);
	glDrawArrays(GL_LINES, 0, 4);
	glLineWidth(1);

	glEnable(GL_DEPTH_TEST);

	_bgSplitVAO.release();
	_bgSplitShader->release();
}

void GLWidget::setupClippingUniforms(QOpenGLShaderProgram* prog, QVector3D pos)
{
	prog->bind();
	TriangleMesh::recordProgramBindCall(true);
	if (_clipYZEnabled || _clipZXEnabled || _clipXYEnabled || !(_clipDX == 0 && _clipDY == 0 && _clipDZ == 0))
	{
		prog->setUniformValue("sectionActive", true);
	}
	else
	{
		prog->setUniformValue("sectionActive", false);
	}
	prog->setUniformValue("modelViewMatrix", _modelViewMatrix);
	prog->setUniformValue("projectionMatrix", _projectionMatrix);
	prog->setUniformValue("clipPlaneX", QVector4D(_modelViewMatrix.map(QVector3D(_clipXFlipped ? 1 : -1, 0, 0) + pos),
		(_clipXFlipped ? 1 : -1) * (pos.x() - (_clipXCoeff + _boundingBox.center().getX()))));
	prog->setUniformValue("clipPlaneY", QVector4D(_modelViewMatrix.map(QVector3D(0, _clipYFlipped ? 1 : -1, 0) + pos),
		(_clipYFlipped ? 1 : -1) * (pos.y() - (_clipYCoeff + _boundingBox.center().getY()))));
	prog->setUniformValue("clipPlaneZ", QVector4D(_modelViewMatrix.map(QVector3D(0, 0, _clipZFlipped ? 1 : -1) + pos),
		(_clipZFlipped ? 1 : -1) * (pos.z() - (_clipZCoeff + _boundingBox.center().getZ()))));
	prog->setUniformValue("clipPlane", QVector4D(_modelViewMatrix.map(QVector3D(_clipDX, _clipDY, _clipDZ) + pos),
		pos.x() * _clipDX + pos.y() * _clipDY + pos.z() * _clipDZ));
}


AssImpMesh* GLWidget::createMeshFromData(const AssImpMeshData& meshData)
{
	std::vector<GLMaterial::Texture> textures = meshData.textures;
	for (GLMaterial::Texture& texture : textures)
	{
		const QString originalPath = QString::fromStdString(texture.path);

		TextureSamplerSettings samplers{ texture.wrapS, texture.wrapT, texture.minFilter, texture.magFilter };
		if (!texture.imageData.isNull())
		{
			const QString cacheKey = originalPath.isEmpty()
				? QStringLiteral("embedded://%1/%2/%3")
					.arg(meshData.name)
					.arg(QString::fromStdString(texture.type))
					.arg(textures.size())
				: originalPath;
			texture.id = getOrCreateTextureCached(cacheKey, texture.imageData, samplers);
		}
		else if (!texture.path.empty())
		{
			const QString texturePath = QString::fromStdString(texture.path);
			if (texturePath.endsWith(".ktx2", Qt::CaseInsensitive))
			{
				texture.id = getOrLoadKtx2TextureCached(texturePath, texture.type, samplers);
			}
			else
			{
				texture.id = getOrLoadTextureCached(texturePath, samplers);
			}
		}
		else if (texture.id != 0)
		{
			// Keep externally prepared IDs only when there is no path/image payload to re-resolve.
		}
	}

	GLMaterial resolvedMaterial = meshData.material;
	for (const GLMaterial::Texture& texture : textures)
	{
		const QString texturePath = QString::fromStdString(texture.path);
		if (texture.type == "albedoMap" || texture.type == "texture_diffuse")
		{
			resolvedMaterial.setAlbedoTextureId(texture.id);
			if (!texturePath.isEmpty()) resolvedMaterial.setAlbedoMap(texturePath);
		}
		else if (texture.type == "metallicMap" || texture.type == "texture_specular")
		{
			resolvedMaterial.setMetallicTextureId(texture.id);
			if (!texturePath.isEmpty()) resolvedMaterial.setMetallicMap(texturePath);
		}
		else if (texture.type == "roughnessMap")
		{
			resolvedMaterial.setRoughnessTextureId(texture.id);
			if (!texturePath.isEmpty()) resolvedMaterial.setRoughnessMap(texturePath);
		}
		else if (texture.type == "normalMap" || texture.type == "texture_normal")
		{
			resolvedMaterial.setNormalTextureId(texture.id);
			if (!texturePath.isEmpty()) resolvedMaterial.setNormalMap(texturePath);
		}
		else if (texture.type == "aoMap" || texture.type == "occlusionMap" || texture.type == "occlusion")
		{
			resolvedMaterial.setOcclusionTextureId(texture.id);
			if (!texturePath.isEmpty()) resolvedMaterial.setAOMap(texturePath);
		}
		else if (texture.type == "opacityMap" || texture.type == "texture_opacity")
		{
			resolvedMaterial.setOpacityTextureId(texture.id);
			if (!texturePath.isEmpty()) resolvedMaterial.setOpacityMap(texturePath);
		}
		else if (texture.type == "heightMap" || texture.type == "texture_height")
		{
			resolvedMaterial.setHeightTextureId(texture.id);
			if (!texturePath.isEmpty()) resolvedMaterial.setHeightMap(texturePath);
		}
		else if (texture.type == "emissiveMap" || texture.type == "texture_emissive")
		{
			resolvedMaterial.setEmissiveTextureId(texture.id);
			if (!texturePath.isEmpty()) resolvedMaterial.setEmissiveMap(texturePath);
		}
		else if (texture.type == "transmissionMap")
		{
			resolvedMaterial.setTransmissionTextureId(texture.id);
			if (!texturePath.isEmpty()) resolvedMaterial.setTransmissionMap(texturePath);
		}
		else if (texture.type == "iorMap")
		{
			resolvedMaterial.setIORTextureId(texture.id);
			if (!texturePath.isEmpty()) resolvedMaterial.setIORMap(texturePath);
		}
		else if (texture.type == "sheenColorMap")
		{
			resolvedMaterial.setSheenColorTextureId(texture.id);
			if (!texturePath.isEmpty()) resolvedMaterial.setSheenColorMap(texturePath);
		}
		else if (texture.type == "sheenRoughnessMap")
		{
			resolvedMaterial.setSheenRoughnessTextureId(texture.id);
			if (!texturePath.isEmpty()) resolvedMaterial.setSheenRoughnessMap(texturePath);
		}
		else if (texture.type == "clearcoatColorMap")
		{
			resolvedMaterial.setClearcoatColorTextureId(texture.id);
			if (!texturePath.isEmpty()) resolvedMaterial.setClearcoatColorMap(texturePath);
		}
		else if (texture.type == "clearcoatRoughnessMap")
		{
			resolvedMaterial.setClearcoatRoughnessTextureId(texture.id);
			if (!texturePath.isEmpty()) resolvedMaterial.setClearcoatRoughnessMap(texturePath);
		}
		else if (texture.type == "clearcoatNormalMap")
		{
			resolvedMaterial.setClearcoatNormalTextureId(texture.id);
			if (!texturePath.isEmpty()) resolvedMaterial.setClearcoatNormalMap(texturePath);
		}
		else if (texture.type == "iridescenceMap")
		{
			resolvedMaterial.setIridescenceTextureId(texture.id);
			if (!texturePath.isEmpty()) resolvedMaterial.setIridescenceMap(texturePath);
		}
		else if (texture.type == "iridescenceThicknessMap")
		{
			resolvedMaterial.setIridescenceThicknessTextureId(texture.id);
			if (!texturePath.isEmpty()) resolvedMaterial.setIridescenceThicknessMap(texturePath);
		}
		else if (texture.type == "specularFactorMap")
		{
			resolvedMaterial.setSpecularFactorTextureId(texture.id);
			if (!texturePath.isEmpty()) resolvedMaterial.setSpecularFactorMap(texturePath);
		}
		else if (texture.type == "specularColorMap")
		{
			resolvedMaterial.setSpecularColorTextureId(texture.id);
			if (!texturePath.isEmpty()) resolvedMaterial.setSpecularColorMap(texturePath);
		}
		else if (texture.type == "anisotropyMap")
		{
			resolvedMaterial.setAnisotropyTextureId(texture.id);
			if (!texturePath.isEmpty()) resolvedMaterial.setAnisotropyMap(texturePath);
		}
		else if (texture.type == "thicknessMap")
		{
			resolvedMaterial.setThicknessTextureId(texture.id);
			if (!texturePath.isEmpty()) resolvedMaterial.setThicknessMap(texturePath);
		}
		else if (texture.type == "diffuseMap")
		{
			resolvedMaterial.setDiffuseTextureId(texture.id);
			if (!texturePath.isEmpty()) resolvedMaterial.setDiffuseMap(texturePath);
		}
		else if (texture.type == "diffuseTransmissionMap")
		{
			resolvedMaterial.setDiffuseTransmissionTextureId(texture.id);
			if (!texturePath.isEmpty()) resolvedMaterial.setDiffuseTransmissionMap(texturePath);
		}
		else if (texture.type == "diffuseTransmissionColorMap")
		{
			resolvedMaterial.setDiffuseTransmissionColorTextureId(texture.id);
			if (!texturePath.isEmpty()) resolvedMaterial.setDiffuseTransmissionColorMap(texturePath);
		}
		else if (texture.type == "specularGlossinessMap")
		{
			resolvedMaterial.setSpecularGlossinessTextureId(texture.id);
			if (!texturePath.isEmpty()) resolvedMaterial.setSpecularGlossinessMap(texturePath);
		}
	}

	// Ensure ADS values are recalculated after copy and texture assignment
	// (copy assignment operator at line 6506 doesn't call updateConsistency)
	resolvedMaterial.updateConsistency();

	auto* mesh = new AssImpMesh(_fgShader.get(),
		meshData.name,
		meshData.vertices,
		meshData.indices,
		textures,
		resolvedMaterial,
		!meshData.morphTargets.isEmpty());
	mesh->setHasNegativeScale(meshData.hasNegativeScale);
	mesh->setPrimitiveMode(meshData.primitiveMode);
	mesh->setSceneIndex(meshData.sceneIndex);
	mesh->setOriginalMaterialIndex(meshData.originalMaterialIndex);
	mesh->setSourceFile(meshData.sourceFile);
	mesh->setSourceNodeName(meshData.sourceNodeName);
	mesh->setSkinJoints(meshData.skinJoints);
	mesh->setMorphTargets(meshData.morphTargets, meshData.defaultMorphWeights);
	if (!meshData.variantMappings.isEmpty())
	{
		mesh->setVariantMappings(meshData.variantMappings);
		mesh->setAllVariantMaterials(meshData.allVariantMaterials);
	}

	// Always run the final material through the same GLMaterial-based texture
	// resolution path that variant switching uses. The earlier reconstruction
	// from meshData.textures is useful for preserving imported metadata, but it
	// is not the authoritative runtime path for every texture slot.
	GLMaterial runtimeResolved = resolveMaterialTextures(this, mesh->getMaterial());
	mesh->setMaterial(runtimeResolved);
	mesh->setTextureMaps(runtimeResolved);
	mesh->invertOpacityADSMap(runtimeResolved.isOpacityMapInverted());
	mesh->invertOpacityPBRMap(runtimeResolved.isOpacityMapInverted());

	return mesh;
}

void GLWidget::syncFileNodeTransforms(const QString& sourceFile)
{
	if (!_viewer || !_viewer->sceneGraph())
		return;

	SceneGraph* sceneGraph = _viewer->sceneGraph();
	SceneNode* fileNode = sceneGraph->findFileNode(sourceFile);
	if (!fileNode)
		return;

	const SceneGraphWorldTransforms evaluatedWorlds = sceneGraph->evaluateWorldTransformsForFile(sourceFile);

	RuntimeAnimationFileState& runtime = _runtimeAnimationsByFile[sourceFile];
	runtime.data = sceneGraph->animationDataForFile(sourceFile);
	runtime.defaultNodeTransformsByUuid.clear();
	runtime.defaultNodeMorphWeightsByUuid.clear();
	runtime.defaultMeshMaterials.clear();
	runtime.meshUuidsByMaterialIndex.clear();
	runtime.nodeUuidByName.clear();
	runtime.nodeUuidByIndex.clear();
	runtime.nodeIndexByUuid.clear();

	SceneNode* aiRootNode = fileNode->children.isEmpty() ? nullptr : fileNode->children.first();
	for (const GltfAnimationNodeBinding& binding : runtime.data.nodeBindings)
	{
		SceneNode* targetNode = nullptr;
		if (binding.hasAiChildPath)
			targetNode = findSceneNodeByAiChildPath(aiRootNode, binding.aiChildPath);

		if (!targetNode && !binding.nodeName.isEmpty())
		{
			std::function<SceneNode*(SceneNode*)> findByName = [&](SceneNode* node) -> SceneNode*
			{
				if (!node)
					return nullptr;
				if (node->name == binding.nodeName)
					return node;
				for (SceneNode* child : node->children)
				{
					if (SceneNode* match = findByName(child))
						return match;
				}
				return nullptr;
			};

			for (SceneNode* child : fileNode->children)
			{
				targetNode = findByName(child);
				if (targetNode)
					break;
			}
		}

		if (targetNode && binding.nodeIndex >= 0 && !runtime.nodeUuidByIndex.contains(binding.nodeIndex))
		{
			runtime.nodeUuidByIndex.insert(binding.nodeIndex, targetNode->nodeUuid);
			runtime.nodeIndexByUuid.insert(targetNode->nodeUuid, binding.nodeIndex);
		}
	}

	std::function<void(SceneNode*)> collect = [&](SceneNode* node)
	{
		if (!node)
			return;

		runtime.defaultNodeTransformsByUuid.insert(node->nodeUuid, decomposeNodeTransform(node->localTransform));
		if (!node->name.isEmpty() && !runtime.nodeUuidByName.contains(node->name))
			runtime.nodeUuidByName.insert(node->name, node->nodeUuid);
		for (const QUuid& uuid : node->meshUuids)
		{
			if (TriangleMesh* mesh = getMeshByUuid(uuid))
			{
				if (evaluatedWorlds.meshWorldByUuid.contains(uuid))
					mesh->setSceneRenderTransform(evaluatedWorlds.meshWorldByUuid.value(uuid));
				if (mesh->hasMorphTargets())
				{
					// Assimp appends "_node" to mesh-owning nodes when re-importing a glTF/GLB
					// whose node name matches its mesh name (e.g. "AnimatedMorphCube" → child
					// "AnimatedMorphCube_node" owns the mesh).  The morph-weight animation
					// targets the PARENT ("AnimatedMorphCube") in the JSON.  Use the parent
					// UUID so that the animation lookup resolves correctly.
					const SceneNode* morphNode = node;
					if (node->parent
					    && node->name.endsWith(QStringLiteral("_node"))
					    && node->parent->name == node->name.left(node->name.length() - 5))
					{
						morphNode = node->parent;
					}
					if (!runtime.defaultNodeMorphWeightsByUuid.contains(morphNode->nodeUuid))
						runtime.defaultNodeMorphWeightsByUuid.insert(morphNode->nodeUuid, mesh->defaultMorphWeights());
				}
				runtime.defaultMeshMaterials.insert(uuid, mesh->getMaterial());
				if (mesh->getOriginalMaterialIndex() >= 0)
					runtime.meshUuidsByMaterialIndex.insert(mesh->getOriginalMaterialIndex(), uuid);
			}
		}

		for (SceneNode* child : node->children)
			collect(child);
	};

	for (SceneNode* child : fileNode->children)
		collect(child);
}

void GLWidget::setActiveAnimation(const QString& sourceFile, int clipIndex)
{
	if (!_viewer || !_viewer->sceneGraph())
		return;

	const GltfAnimationData data = _viewer->sceneGraph()->animationDataForFile(sourceFile);
	if (clipIndex < 0 || clipIndex >= data.clips.size())
		return;

	// Rebuild runtime defaults from the authoritative SceneGraph state before
	// sampling frame 0. Newly-created clips can otherwise be applied against a
	// stale runtime cache, which makes meshes appear to "stick" in the wrong pose.
	syncFileNodeTransforms(sourceFile);

	_activeAnimationFile = sourceFile;
	_activeAnimationClip = clipIndex;
	_animationCurrentTimeSeconds = 0.0;
	_animationPlaying = false;
	_animationTimer->stop();
	_viewer->sceneGraph()->setActiveAnimationClip(sourceFile, clipIndex);
	applyAnimationPose(sourceFile, clipIndex, 0.0);
	emit animationStateChanged();
}

void GLWidget::clearAnimationRuntimeForFile(const QString& sourceFile)
{
	_runtimeAnimationsByFile.remove(sourceFile);
	if (_activeAnimationFile == sourceFile)
	{
		_animationTimer->stop();
		_animationPlaying = false;
		_activeAnimationFile.clear();
		_activeAnimationClip = -1;
		_animationCurrentTimeSeconds = 0.0;
		emit animationStateChanged();
	}
}

void GLWidget::syncRuntimeNodeTransforms(const QString& sourceFile)
{
	syncFileNodeTransforms(sourceFile);
}

void GLWidget::setAnimationPlaying(bool playing)
{
	_animationPlaying = playing;
	if (_animationPlaying)
	{
		_animationElapsed.restart();
		_animationTimer->start();
	}
	else
	{
		_animationTimer->stop();
	}
	emit animationStateChanged();
}

void GLWidget::seekAnimation(double timeSeconds)
{
	if (_activeAnimationFile.isEmpty() || _activeAnimationClip < 0)
		return;

	_animationCurrentTimeSeconds = std::max(0.0, timeSeconds);
	applyAnimationPose(_activeAnimationFile, _activeAnimationClip, _animationCurrentTimeSeconds);
	emit animationStateChanged();
}

void GLWidget::setAnimationLooping(bool looping)
{
	_animationLooping = looping;
	emit animationStateChanged();
}

void GLWidget::setAnimationPlaybackSpeed(double speed)
{
	_animationPlaybackSpeed = std::clamp(speed, 0.25, 4.0);
	if (_animationPlaying)
		_animationElapsed.restart();
	emit animationStateChanged();
}

// ---------------------------------------------------------------------------
// glTF camera switching
// ---------------------------------------------------------------------------

void GLWidget::activateGltfCamera(const QString& sourceFile, int cameraIndex)
{
	if (!_viewer || !_primaryCamera)
		return;

	const GltfCameraData cd = _viewer->sceneGraph()->gltfCameraDataForFile(sourceFile);
	if (cameraIndex < 0 || cameraIndex >= cd.cameras.size())
		return;

	const GltfCameraEntry& cam = cd.cameras[cameraIndex];

	// Save the current system camera state before the first glTF switch so the
	// user can get back to exactly where they were.
	if (!_systemCameraStateSaved)
	{
		_savedCameraPos      = _primaryCamera->getPosition();
		_savedCameraDir      = _primaryCamera->getViewDir();
		_savedCameraUp       = _primaryCamera->getUpVector();
		_savedCameraRight    = _primaryCamera->getRightVector();
		_savedProjectionType = _primaryCamera->getProjectionType();
		_savedCameraFOV      = _primaryCamera->getFOV();
		_savedCameraViewRange = _primaryCamera->getViewRange();
		_systemCameraStateSaved = true;
	}

	_activeGltfCameraFile  = sourceFile;
	_activeGltfCameraIndex = cameraIndex;

	// Always apply the static camera position/FOV from the parsed glTF camera
	// entry.  This handles the common case of cameras with no animation
	// (e.g. AnimationPointerUVs, where cameras have static matrix transforms
	// but are not targeted by any animation channel).
	applyGltfCameraEntryTransform(cam);

	// If an animation clip is active for this file, re-run applyAnimationPose
	// so that animated cameras (e.g. DiffuseTransmissionPlant) get overridden
	// by the animation's worldTransforms.  For static (non-animated) cameras,
	// applyAnimationPose's worldTransformsByNodeUuid won't contain the camera
	// node UUID and the view set above is preserved unchanged.
	const bool animationOwnsThisFile =
		_activeAnimationFile == sourceFile && _activeAnimationClip >= 0;
	if (animationOwnsThisFile)
	{
		applyAnimationPose(sourceFile, _activeAnimationClip, _animationCurrentTimeSeconds);
	}

	update();
}

void GLWidget::resetToSystemCamera()
{
	if (_systemCameraStateSaved)
	{
		_primaryCamera->setView(_savedCameraPos,
		                        _savedCameraDir,
		                        _savedCameraUp,
		                        _savedCameraRight);
		_primaryCamera->setProjectionType(_savedProjectionType);
		_primaryCamera->setFOV(_savedCameraFOV);
		_primaryCamera->setViewRange(_savedCameraViewRange);
		_viewRange = _savedCameraViewRange;
		_currentViewRange = _viewRange;
		_projection = (_savedProjectionType == GLCamera::ProjectionType::PERSPECTIVE)
			? ViewProjection::PERSPECTIVE
			: ViewProjection::ORTHOGRAPHIC;
		_previousProjection = _savedProjectionType;
		_systemCameraStateSaved = false;
	}

	_activeGltfCameraFile.clear();
	_activeGltfCameraIndex = -1;

	update();
}

GltfCameraData GLWidget::cameraDataForMvfSave(const GltfCameraData& source) const
{
	GltfCameraData result = source;

	// Bake the user model-level transform (if any) of the cameras' own file —
	// the same matrix the meshes and lights follow.
	QMatrix4x4 userTransform;
	const bool hasUserTransform =
		userModelTransformForFile(result.sourceFile, userTransform);

	for (GltfCameraEntry& cam : result.cameras)
	{
		QVector3D worldPos = cam.worldPosition;
		QVector3D worldDir = cam.worldDirection.normalized();
		QVector3D worldUp = cam.worldUp.normalized();

		// Mirror the same gate used by applyGltfCameraEntryTransform:
		// only bake the model-transform compensation when the flag is set.
		// Without this guard, each save/reload cycle bakes another rotation
		// layer that activation never applies back (flag is already false after
		// the first round-trip), causing cameras to drift on every save.
		if (cam.needsModelTransformCompensation && hasUserTransform)
		{
			worldPos = userTransform.map(worldPos);
			worldDir = userTransform.mapVector(worldDir).normalized();
			worldUp  = userTransform.mapVector(worldUp).normalized();
		}

		cam.worldPosition = worldPos;
		cam.worldDirection = worldDir;
		cam.worldUp = worldUp;
		cam.needsModelTransformCompensation = false;
	}

	return result;
}

void GLWidget::applyGltfCameraEntryTransform(const GltfCameraEntry& cam)
{
	if (!_primaryCamera)
		return;

	QVector3D worldPos = cam.worldPosition;
	QVector3D worldDir = cam.worldDirection.normalized();
	QVector3D worldUp  = cam.worldUp.normalized();

	// Follow the user model-level transform of the camera's own file — the
	// same matrix the meshes and lights follow.  modelScale also widens the
	// orthographic view range below so the framing tracks a scaled model.
	float modelScale = 1.0f;
	{
		QMatrix4x4 userTransform;
		if (userModelTransformForFile(_activeGltfCameraFile, userTransform))
		{
			modelScale = uniformScaleOf(userTransform);
			if (cam.needsModelTransformCompensation)
			{
				worldPos = userTransform.map(worldPos);
				worldDir = userTransform.mapVector(worldDir).normalized();
				worldUp  = userTransform.mapVector(worldUp).normalized();
			}
		}
	}

	if (cam.type == GltfCameraType::Perspective)
	{
		_primaryCamera->setProjectionType(GLCamera::ProjectionType::PERSPECTIVE);
		_primaryCamera->setFOV(qRadiansToDegrees(cam.fovYRadians));

		// Set _viewRange so the orbit pivot lands at the scene centre and
		// navigation feel matches the glTF author's framing intent.
		// orbitDist ≈ viewRange * 1.25 (maxShiftFactor clamp in computeViewShift),
		// so: viewRange = distToScene / 1.25.
		const QVector3D sceneCenter(
			static_cast<float>(_boundingSphere.getCenter().x()),
			static_cast<float>(_boundingSphere.getCenter().y()),
			static_cast<float>(_boundingSphere.getCenter().z()));
		const float distToScene = QVector3D::dotProduct(sceneCenter - worldPos, worldDir);
		// Clamp to at least scene radius so we never zoom in past the model.
		const float clampedDist = std::max(distToScene, _boundingSphere.getRadius());
		_primaryCamera->setViewRange(clampedDist / 1.25f);
		_projection = ViewProjection::PERSPECTIVE;
		_previousProjection = GLCamera::ProjectionType::PERSPECTIVE;
	}
	else
	{
		_primaryCamera->setProjectionType(GLCamera::ProjectionType::ORTHOGRAPHIC);
		const float orthoRange = std::max(cam.xMag, cam.yMag) * 2.0f * modelScale;
		_primaryCamera->setViewRange(std::max(orthoRange, 0.0001f));
		_projection = ViewProjection::ORTHOGRAPHIC;
		_previousProjection = GLCamera::ProjectionType::ORTHOGRAPHIC;
	}

	_viewRange = _primaryCamera->getViewRange();
	_currentViewRange = _viewRange;

	const QVector3D right = QVector3D::crossProduct(worldDir, worldUp).normalized();
	const QVector3D pivotPos = (_primaryCamera->getMode() == GLCamera::CameraMode::Orbit)
		? worldPos + worldDir * (_primaryCamera->getProjectionType() == GLCamera::ProjectionType::ORTHOGRAPHIC
			? _primaryCamera->getOrthoViewDistance()
			: _primaryCamera->getOrbitDistance())
		: worldPos;
	_primaryCamera->setView(pivotPos, worldDir, worldUp, right);
}

void GLWidget::refreshAnimationMaterialState(const QString& sourceFile)
{
	RuntimeAnimationFileState& runtime = _runtimeAnimationsByFile[sourceFile];
	if (runtime.data.sourceFile.isEmpty())
		runtime.data = _viewer && _viewer->sceneGraph()
			? _viewer->sceneGraph()->animationDataForFile(sourceFile)
			: GltfAnimationData();

	runtime.defaultMeshMaterials.clear();

	const std::vector<TriangleMesh*>& meshes = getMeshStore();
	for (TriangleMesh* mesh : meshes)
	{
		if (!mesh || mesh->getSourceFile() != sourceFile)
			continue;

		runtime.defaultMeshMaterials.insert(mesh->uuid(), mesh->getMaterial());
	}

	if (_activeAnimationFile == sourceFile && _activeAnimationClip >= 0)
		applyAnimationPose(sourceFile, _activeAnimationClip, _animationCurrentTimeSeconds);
}

void GLWidget::onAnimationTick()
{
	if (!_animationPlaying || _activeAnimationFile.isEmpty() || _activeAnimationClip < 0)
		return;

	const RuntimeAnimationFileState runtime = _runtimeAnimationsByFile.value(_activeAnimationFile);
	if (_activeAnimationClip >= runtime.data.clips.size())
		return;

	const double deltaSeconds = _animationElapsed.isValid()
		? static_cast<double>(_animationElapsed.restart()) / 1000.0
		: 0.016;
	const GltfAnimationClip& clip = runtime.data.clips[_activeAnimationClip];
	if (clip.durationSeconds <= 0.0)
		return;

	_animationCurrentTimeSeconds += deltaSeconds * _animationPlaybackSpeed;
	if (_animationCurrentTimeSeconds >= clip.durationSeconds)
	{
		if (_animationLooping)
			_animationCurrentTimeSeconds = std::fmod(_animationCurrentTimeSeconds, clip.durationSeconds);
		else
		{
			_animationCurrentTimeSeconds = clip.durationSeconds;
			_animationPlaying = false;
			_animationTimer->stop();
		}
	}

	applyAnimationPose(_activeAnimationFile, _activeAnimationClip, _animationCurrentTimeSeconds);
	emit animationStateChanged();
}

void GLWidget::resetAnimationPose(const QString& sourceFile)
{
	if (!_viewer || !_viewer->sceneGraph())
		return;

	SceneNode* fileNode = _viewer->sceneGraph()->findFileNode(sourceFile);
	if (!fileNode)
		return;

	const RuntimeAnimationFileState runtime = _runtimeAnimationsByFile.value(sourceFile);
	const bool needsRuntimeNodeTransforms =
		runtime.data.hasNodeAnimations || runtime.data.hasSkinning;
	const std::vector<TriangleMesh*>& meshes = getMeshStore();

	if (needsRuntimeNodeTransforms)
	{
		std::function<void(SceneNode*, const QMatrix4x4&)> applyNode =
			[&](SceneNode* node, const QMatrix4x4& parentWorld)
		{
			if (!node)
				return;

			const QMatrix4x4 local = aiToQMatrix(node->localTransform);
			const QMatrix4x4 world = parentWorld * local;
			for (const QUuid& uuid : node->meshUuids)
			{
				if (TriangleMesh* mesh = getMeshByUuid(uuid))
				{
					if (!mesh->hasSkinning())
						mesh->setSceneRenderTransformFast(world);
					else
						mesh->setJointPalette({});
				}
			}

			for (SceneNode* child : node->children)
				applyNode(child, world);
		};

		for (SceneNode* child : fileNode->children)
			applyNode(child, QMatrix4x4());
	}

	for (auto it = runtime.defaultMeshMaterials.constBegin(); it != runtime.defaultMeshMaterials.constEnd(); ++it)
	{
		if (TriangleMesh* mesh = getMeshByUuid(it.key()))
		{
			if (mesh->hasMorphTargets())
				mesh->resetMorphTargets();
			mesh->setMaterial(it.value());
		}
	}

	for (TriangleMesh* mesh : meshes)
	{
		if (!mesh || mesh->getSourceFile() != sourceFile || !mesh->hasMorphTargets())
			continue;
		mesh->resetMorphTargets();
	}

	if (!runtime.data.lightBindings.isEmpty())
	{
		clearAnimatedLightTransformState(sourceFile);
		QVector<bool> visibleByParsedLight(runtime.data.lightBindings.size(), true);
		QHash<int, bool> explicitVisibility;
		for (const GltfAnimationNodeVisibilityState& nodeState : runtime.data.nodeVisibilityStates)
			explicitVisibility.insert(nodeState.nodeIndex, nodeState.defaultVisible);

		QHash<int, bool> effectiveCache;
		std::function<bool(int)> evalVisible = [&](int nodeIndex) -> bool
		{
			if (effectiveCache.contains(nodeIndex))
				return effectiveCache.value(nodeIndex);
			if (nodeIndex < 0 || nodeIndex >= runtime.data.nodeVisibilityStates.size())
				return true;

			const GltfAnimationNodeVisibilityState& nodeState = runtime.data.nodeVisibilityStates[nodeIndex];
			const bool localVisible = explicitVisibility.value(nodeIndex, true);
			const bool effectiveVisible = localVisible &&
				(nodeState.parentNodeIndex < 0 || evalVisible(nodeState.parentNodeIndex));
			effectiveCache.insert(nodeIndex, effectiveVisible);
			return effectiveVisible;
		};

		for (const GltfAnimationLightBinding& binding : runtime.data.lightBindings)
		{
			if (binding.parsedLightIndex >= 0 && binding.parsedLightIndex < visibleByParsedLight.size())
				visibleByParsedLight[binding.parsedLightIndex] = evalVisible(binding.nodeIndex);
		}
		setAnimatedLightVisibilityState(sourceFile, visibleByParsedLight);
	}
	else
	{
		clearAnimatedLightTransformState(sourceFile);
		clearAnimatedLightVisibilityState(sourceFile);
	}

	if (!runtime.data.nodeVisibilityStates.isEmpty())
	{
		QHash<int, bool> explicitVisibility;
		for (const GltfAnimationNodeVisibilityState& nodeState : runtime.data.nodeVisibilityStates)
			explicitVisibility.insert(nodeState.nodeIndex, nodeState.defaultVisible);

		QHash<int, bool> effectiveCache;
		std::function<bool(int)> evalVisible = [&](int nodeIndex) -> bool
		{
			if (effectiveCache.contains(nodeIndex))
				return effectiveCache.value(nodeIndex);
			if (nodeIndex < 0 || nodeIndex >= runtime.data.nodeVisibilityStates.size())
				return true;

			const GltfAnimationNodeVisibilityState& nodeState = runtime.data.nodeVisibilityStates[nodeIndex];
			const bool localVisible = explicitVisibility.value(nodeIndex, true);
			const bool effectiveVisible = localVisible &&
				(nodeState.parentNodeIndex < 0 || evalVisible(nodeState.parentNodeIndex));
			effectiveCache.insert(nodeIndex, effectiveVisible);
			return effectiveVisible;
		};

		QSet<QUuid> hiddenMeshUuids;
		std::function<void(SceneNode*)> collectHidden = [&](SceneNode* node)
		{
			if (!node)
				return;

			const int nodeIndex = runtime.nodeIndexByUuid.value(node->nodeUuid, -1);

			const bool visible = nodeIndex < 0 ? true : evalVisible(nodeIndex);
			if (!visible)
			{
				for (const QUuid& uuid : node->meshUuids)
					hiddenMeshUuids.insert(uuid);
			}

			for (SceneNode* child : node->children)
				collectHidden(child);
		};

		for (SceneNode* child : fileNode->children)
			collectHidden(child);
		setAnimatedMeshVisibilityState(sourceFile, hiddenMeshUuids);
	}
	else
	{
		clearAnimatedMeshVisibilityState(sourceFile);
	}

	update();
}

void GLWidget::updateAnimatedMeshState(const QString& sourceFile,
	const QHash<QUuid, QMatrix4x4>& worldTransformsByNodeUuid)
{
	if (!_viewer || !_viewer->sceneGraph())
		return;

	SceneNode* fileNode = _viewer->sceneGraph()->findFileNode(sourceFile);
	if (!fileNode)
		return;

	const RuntimeAnimationFileState runtime = _runtimeAnimationsByFile.value(sourceFile);
	if (!runtime.data.hasNodeAnimations && !runtime.data.hasSkinning)
		return;

	std::function<void(SceneNode*)> applyToMeshes = [&](SceneNode* node)
	{
		if (!node)
			return;

		const QMatrix4x4 world = worldTransformsByNodeUuid.value(node->nodeUuid, aiToQMatrix(node->localTransform));
		for (const QUuid& uuid : node->meshUuids)
		{
			if (TriangleMesh* mesh = getMeshByUuid(uuid))
			{
				if (!mesh->hasSkinning())
				{
					mesh->setSceneRenderTransformFast(world);
				}
				else
				{
					QVector<QMatrix4x4> palette;
					palette.reserve(mesh->skinJoints().size());
					const QMatrix4x4 meshWorldInverse = world.inverted();
					for (const GltfSkinJoint& joint : mesh->skinJoints())
					{
						const QUuid jointNodeUuid = resolveRuntimeNodeUuid(runtime, -1, joint.nodeName);
						const QMatrix4x4 jointWorld = worldTransformsByNodeUuid.value(jointNodeUuid, world);
						palette.append(meshWorldInverse * jointWorld * aiToQMatrix(joint.inverseBindMatrix));
					}
					mesh->setJointPalette(palette);
					mesh->setSceneRenderTransformFast(world);
				}
			}
		}

		for (SceneNode* child : node->children)
			applyToMeshes(child);
	};

	for (SceneNode* child : fileNode->children)
		applyToMeshes(child);
}

void GLWidget::applyAnimationPose(const QString& sourceFile, int clipIndex, double timeSeconds)
{
	if (!_viewer || !_viewer->sceneGraph())
		return;
	SceneGraph* sceneGraph = _viewer->sceneGraph();

	RuntimeAnimationFileState& runtime = _runtimeAnimationsByFile[sourceFile];
	if (runtime.data.sourceFile.isEmpty())
		runtime.data = sceneGraph->animationDataForFile(sourceFile);

	if (clipIndex < 0 || clipIndex >= runtime.data.clips.size())
	{
		resetAnimationPose(sourceFile);
		return;
	}

	SceneNode* fileNode = sceneGraph->findFileNode(sourceFile);
	if (!fileNode)
		return;

	QHash<QUuid, RuntimeNodeTransform> sampled = runtime.defaultNodeTransformsByUuid;
	QHash<QUuid, RuntimeNodeTransform> sampledMeshTransforms;
	QHash<QUuid, QVector<float>> sampledMorphWeightsByUuid = runtime.defaultNodeMorphWeightsByUuid;
	QHash<QUuid, GLMaterial> animatedMaterials = runtime.defaultMeshMaterials;
	QHash<int, bool> sampledNodeVisibility;
	for (const GltfAnimationNodeVisibilityState& nodeState : runtime.data.nodeVisibilityStates)
		sampledNodeVisibility.insert(nodeState.nodeIndex, nodeState.defaultVisible);
	const GltfAnimationClip& clip = runtime.data.clips[clipIndex];
	const auto resolveChannelNodeUuid = [&](const GltfAnimationChannel& channel) -> QUuid
	{
		return resolveRuntimeNodeUuid(runtime, channel.targetNodeIndex, channel.targetNodeName);
	};
	QHash<QUuid, bool> nodeAffectsShadowCache;
	std::function<bool(const QUuid&)> nodeAffectsShadow = [&](const QUuid& nodeUuid) -> bool
	{
		if (nodeUuid.isNull())
			return false;
		if (nodeAffectsShadowCache.contains(nodeUuid))
			return nodeAffectsShadowCache.value(nodeUuid);

		const SceneNode* node = sceneGraph->findNodeByUuid(nodeUuid);
		if (!node)
		{
			nodeAffectsShadowCache.insert(nodeUuid, false);
			return false;
		}

		std::function<bool(const SceneNode*)> hasMeshInSubtree = [&](const SceneNode* current) -> bool
		{
			if (!current)
				return false;
			// Camera/helper nodes can animate without affecting casters. Only
			// nodes that own a mesh subtree should invalidate the shadow map.
			if (!current->meshUuids.isEmpty())
				return true;
			for (const SceneNode* child : current->children)
			{
				if (hasMeshInSubtree(child))
					return true;
			}
			return false;
		};

		const bool affects = hasMeshInSubtree(node);
		nodeAffectsShadowCache.insert(nodeUuid, affects);
		return affects;
	};
	bool animationAffectsShadowCasters = false;
	for (const GltfAnimationChannel& channel : clip.channels)
	{
		if (channel.targetPath == GltfAnimationTargetPath::Pointer)
		{
			if (channel.pointerTargetKind == GltfAnimationPointerTargetKind::MaterialTextureTransform)
			{
				if (channel.targetMaterialIndex < 0)
					continue;

				const QList<QUuid> affectedMeshes = runtime.meshUuidsByMaterialIndex.values(channel.targetMaterialIndex);
				for (const QUuid& meshUuid : affectedMeshes)
				{
					auto materialIt = animatedMaterials.find(meshUuid);
					if (materialIt == animatedMaterials.end())
						continue;

					if (channel.pointerProperty == GltfAnimationPointerProperty::BaseColorFactor)
					{
						const QVector4D vec4Value = sampleVec4Keys(channel.vec4Keys,
							timeSeconds,
							QVector4D(materialIt.value().albedoColor(), materialIt.value().opacity()));
						applyMaterialFactorPointerValue(materialIt.value(),
							channel.pointerProperty,
							vec4Value);
						continue;
					}

					const QVector2D vec2Value = channel.pointerProperty == GltfAnimationPointerProperty::Rotation
						? QVector2D()
						: sampleVec2Keys(channel.vec2Keys, timeSeconds, QVector2D());
					const float scalarValue = channel.pointerProperty == GltfAnimationPointerProperty::Rotation
						? sampleFloatKeys(channel.floatKeys, timeSeconds, 0.0f)
						: 0.0f;
					applyTexturePointerValue(materialIt.value(),
						channel.textureTarget,
						channel.pointerProperty,
						vec2Value,
						scalarValue);
				}
			}
				else if (channel.pointerTargetKind == GltfAnimationPointerTargetKind::NodeVisibility)
			{
				if (channel.targetNodeIndex >= 0)
				{
					const QUuid resolvedNodeUuid = resolveRuntimeNodeUuid(runtime, channel.targetNodeIndex);
					animationAffectsShadowCasters =
						animationAffectsShadowCasters || nodeAffectsShadow(resolvedNodeUuid);
					sampledNodeVisibility.insert(channel.targetNodeIndex,
						sampleBoolKeys(channel.boolKeys,
							timeSeconds,
							sampledNodeVisibility.value(channel.targetNodeIndex, true)));
				}
			}
			continue;
		}

		if (channel.targetKind == GltfAnimationBindingTargetKind::Mesh)
		{
			if (channel.targetMeshUuid.isNull())
				continue;

			if (channel.targetPath == GltfAnimationTargetPath::Translation ||
				channel.targetPath == GltfAnimationTargetPath::Rotation ||
				channel.targetPath == GltfAnimationTargetPath::Scale ||
				channel.targetPath == GltfAnimationTargetPath::Weights)
			{
				animationAffectsShadowCasters = true;
			}

			RuntimeNodeTransform meshTransform = sampledMeshTransforms.value(channel.targetMeshUuid);
			if (meshTransform.rotation.isNull())
				meshTransform.rotation = QQuaternion(1.0f, 0.0f, 0.0f, 0.0f);
			switch (channel.targetPath)
			{
			case GltfAnimationTargetPath::Translation:
				meshTransform.translation = sampleVec3Keys(channel.vec3Keys, timeSeconds, meshTransform.translation);
				break;
			case GltfAnimationTargetPath::Rotation:
				meshTransform.rotation = sampleQuatKeys(channel.quatKeys, timeSeconds, meshTransform.rotation);
				break;
			case GltfAnimationTargetPath::Scale:
				meshTransform.scale = sampleVec3Keys(channel.vec3Keys, timeSeconds,
					meshTransform.scale.isNull() ? QVector3D(1.0f, 1.0f, 1.0f) : meshTransform.scale);
				break;
			case GltfAnimationTargetPath::Weights:
			case GltfAnimationTargetPath::Pointer:
				continue;
			}
			if (meshTransform.scale.isNull())
				meshTransform.scale = QVector3D(1.0f, 1.0f, 1.0f);
			sampledMeshTransforms.insert(channel.targetMeshUuid, meshTransform);
			continue;
		}

		const QUuid resolvedNodeUuid = resolveChannelNodeUuid(channel);
		if (resolvedNodeUuid.isNull())
			continue;
		// Skinned clips are a special case: joint nodes usually do not own meshes
		// directly, but their TRS still deforms skinned vertices and must refresh
		// shadows every tick.
		if ((channel.targetPath == GltfAnimationTargetPath::Translation ||
		     channel.targetPath == GltfAnimationTargetPath::Rotation ||
		     channel.targetPath == GltfAnimationTargetPath::Scale ||
		     channel.targetPath == GltfAnimationTargetPath::Weights) &&
		    (runtime.data.hasSkinning || nodeAffectsShadow(resolvedNodeUuid)))
		{
			animationAffectsShadowCasters = true;
		}

		RuntimeNodeTransform node = sampled.value(resolvedNodeUuid);
		switch (channel.targetPath)
		{
		case GltfAnimationTargetPath::Translation:
			node.translation = sampleVec3Keys(channel.vec3Keys, timeSeconds, node.translation);
			break;
		case GltfAnimationTargetPath::Rotation:
			node.rotation = sampleQuatKeys(channel.quatKeys, timeSeconds, node.rotation);
			break;
		case GltfAnimationTargetPath::Scale:
			node.scale = sampleVec3Keys(channel.vec3Keys, timeSeconds, node.scale);
			break;
		case GltfAnimationTargetPath::Weights:
			sampledMorphWeightsByUuid.insert(resolvedNodeUuid,
				sampleWeightKeys(channel.weightKeys, timeSeconds, sampledMorphWeightsByUuid.value(resolvedNodeUuid)));
			continue;
		case GltfAnimationTargetPath::Pointer:
			continue;
		}
		sampled.insert(resolvedNodeUuid, node);
	}

	const std::vector<TriangleMesh*>& meshes = getMeshStore();
	for (TriangleMesh* mesh : meshes)
	{
		if (!mesh || mesh->getSourceFile() != sourceFile || !mesh->hasMorphTargets())
			continue;

		const SceneNode* ownerNode = _viewer->sceneGraph()->findNodeForMesh(mesh->uuid());
		if (!ownerNode)
		{
			mesh->resetMorphTargets();
			continue;
		}

		// Assimp appends "_node" to mesh-owning nodes on glTF/GLB re-import when the
		// node name matches the mesh name (e.g. node "AnimatedMorphCube" becomes a
		// parent and "AnimatedMorphCube_node" is the actual mesh-owning child).
		// The morph-weight animation channel targets the parent, so resolve via
		// the same parent-node logic used in defaultNodeMorphWeightsByUuid init.
		const SceneNode* lookupNode = ownerNode;
		if (ownerNode->parent
		    && ownerNode->name.endsWith(QStringLiteral("_node"))
		    && ownerNode->parent->name == ownerNode->name.left(ownerNode->name.length() - 5))
		{
			lookupNode = ownerNode->parent;
		}
		const QVector<float> weights = sampledMorphWeightsByUuid.value(lookupNode->nodeUuid);
		if (!weights.isEmpty())
			mesh->applyMorphWeights(weights);
		else
			mesh->resetMorphTargets();
	}

	if (runtime.data.hasNodeAnimations || runtime.data.hasSkinning)
	{
		QHash<QUuid, QMatrix4x4> worldTransformsByNodeUuid;
		std::function<void(SceneNode*, const QMatrix4x4&)> evalNode =
			[&](SceneNode* node, const QMatrix4x4& parentWorld)
		{
			if (!node)
				return;

			const RuntimeNodeTransform nodeTransform =
				sampled.value(node->nodeUuid, decomposeNodeTransform(node->localTransform));
			const QMatrix4x4 local = composeNodeTransform(nodeTransform);
			const QMatrix4x4 world = parentWorld * local;
			worldTransformsByNodeUuid.insert(node->nodeUuid, world);

			for (SceneNode* child : node->children)
				evalNode(child, world);
		};

		for (SceneNode* child : fileNode->children)
			evalNode(child, QMatrix4x4());

		updateAnimatedMeshState(sourceFile, worldTransformsByNodeUuid);

		for (auto meshIt = sampledMeshTransforms.cbegin(); meshIt != sampledMeshTransforms.cend(); ++meshIt)
		{
			TriangleMesh* mesh = getMeshByUuid(meshIt.key());
			const SceneNode* ownerNode = mesh ? _viewer->sceneGraph()->findNodeForMesh(meshIt.key()) : nullptr;
			if (!mesh || !ownerNode || mesh->getSourceFile() != sourceFile)
				continue;

			const QMatrix4x4 ownerWorld = worldTransformsByNodeUuid.value(ownerNode->nodeUuid,
				aiToQMatrix(ownerNode->localTransform));
			QMatrix4x4 meshLocal;
			meshLocal.translate(meshIt->translation);
			meshLocal.rotate(meshIt->rotation);
			meshLocal.scale(meshIt->scale);
			mesh->setSceneRenderTransformFast(ownerWorld * meshLocal);
		}

		// Determine how many lights belong to sourceFile and where they sit in
		// _originalParsedLights.  _lightFileIndexMap is populated by
		// onSceneLightDataChanged() and covers the multi-model case; fall back to
		// the whole vector when there is only one file loaded (map still empty).
		int fileLightOffset = 0;
		int fileLightCount  = static_cast<int>(_originalParsedLights.size()); // fallback
		if (!_lightFileIndexMap.isEmpty())
		{
			int foundOffset = -1, foundCount = 0;
			for (int k = 0; k < _lightFileIndexMap.size(); ++k)
			{
				if (_lightFileIndexMap[k].file == sourceFile)
				{
					if (foundCount == 0) foundOffset = k;
					++foundCount;
				}
			}
			if (foundOffset >= 0)
			{
				fileLightOffset = foundOffset;
				fileLightCount  = foundCount;
			}
		}

		// importCorrection is stored as fileNode->localTransform.  evalNode()
		// starts from identity (so the rendering path can apply fileNode's
		// transform separately for meshes), which means the world matrices it
		// produces are in model-space.  For lights we need world-space positions
		// that match _originalParsedLights (which already have importCorrection
		// baked in by AssImpModelLoader), so we pre-compose the correction here.
		const QMatrix4x4 importCorrection = aiToQMatrix(fileNode->localTransform);

		if (!runtime.data.lightBindings.isEmpty() &&
			fileLightCount == static_cast<int>(runtime.data.lightBindings.size()))
		{
			std::vector<GPULight> animatedLights = _originalParsedLights;
			bool resolvedAnyAnimatedLightTransform = false;
			for (const GltfAnimationLightBinding& binding : runtime.data.lightBindings)
			{
				// binding.parsedLightIndex is file-relative; map it to the global
				// index inside animatedLights (which mirrors _originalParsedLights).
				const int globalIndex = fileLightOffset + binding.parsedLightIndex;
				if (binding.parsedLightIndex < 0 ||
					globalIndex >= static_cast<int>(animatedLights.size()))
				{
					continue;
				}

				QMatrix4x4 modelSpaceWorld;
				bool hasWorldTransform = false;
				if (binding.nodeIndex >= 0 && runtime.nodeUuidByIndex.contains(binding.nodeIndex))
				{
					const QUuid nodeUuid = resolveRuntimeNodeUuid(runtime, binding.nodeIndex);
					if (worldTransformsByNodeUuid.contains(nodeUuid))
					{
						modelSpaceWorld = worldTransformsByNodeUuid.value(nodeUuid);
						hasWorldTransform = true;
					}
				}
				else if (!binding.nodeName.isEmpty())
				{
					const QUuid nodeUuid = resolveRuntimeNodeUuid(runtime, -1, binding.nodeName);
					if (!nodeUuid.isNull() && worldTransformsByNodeUuid.contains(nodeUuid))
					{
						modelSpaceWorld = worldTransformsByNodeUuid.value(nodeUuid);
						hasWorldTransform = true;
					}
				}

				if (!hasWorldTransform)
					continue;

				// Apply importCorrection so the resulting position is in the same
				// coordinate space as _originalParsedLights (world space).
				const QMatrix4x4 worldSpace = importCorrection * modelSpaceWorld;

				GPULight& light = animatedLights[globalIndex];
				light.position = glm::vec3(worldSpace(0, 3), worldSpace(1, 3), worldSpace(2, 3));

				const QVector3D localDir(0.0f, 0.0f, -1.0f);
				const QVector3D worldDir = (worldSpace.mapVector(localDir)).normalized();
				light.direction = glm::vec3(worldDir.x(), worldDir.y(), worldDir.z());
				resolvedAnyAnimatedLightTransform = true;
			}
			if (resolvedAnyAnimatedLightTransform)
				setAnimatedLightTransformState(sourceFile, animatedLights);
			else
				clearAnimatedLightTransformState(sourceFile);
		}
		else
		{
			clearAnimatedLightTransformState(sourceFile);
		}

		// Per-frame glTF camera update: when node animations are present the
		// camera's hosting node moves every frame.  Read its current world
		// transform from worldTransforms and drive the primary camera so that
		// animated cameras (e.g. the firefly-chasing cameras in
		// DiffuseTransmissionPlant) stay in sync with the animation timeline.
		if (_activeGltfCameraFile == sourceFile && _activeGltfCameraIndex >= 0 && _primaryCamera)
		{
			const GltfCameraData camData = _viewer->sceneGraph()->gltfCameraDataForFile(sourceFile);
			if (_activeGltfCameraIndex < camData.cameras.size())
			{
				const GltfCameraEntry& cam = camData.cameras[_activeGltfCameraIndex];
				const QUuid cameraNodeUuid = resolveRuntimeNodeUuid(runtime, cam.nodeIndex, cam.nodeName);
				if (!cameraNodeUuid.isNull() && worldTransformsByNodeUuid.contains(cameraNodeUuid))
				{
					const QMatrix4x4& world = worldTransformsByNodeUuid.value(cameraNodeUuid);
					// Position = translation column of the world matrix.
					const QVector3D worldPos(world(0, 3), world(1, 3), world(2, 3));
					// glTF cameras look along local -Z; +Y is up.
					const QVector3D worldDir   = world.mapVector(QVector3D(0.0f, 0.0f, -1.0f)).normalized();
					const QVector3D worldUp    = world.mapVector(QVector3D(0.0f, 1.0f,  0.0f)).normalized();
					GltfCameraEntry runtimeCam = cam;
					runtimeCam.worldPosition = worldPos;
					runtimeCam.worldDirection = worldDir;
					runtimeCam.worldUp = worldUp;
					applyGltfCameraEntryTransform(runtimeCam);
				}
			}
		}
	}
	else
	{
		clearAnimatedLightTransformState(sourceFile);
	}

	if (!runtime.data.lightBindings.isEmpty())
	{
		QVector<bool> visibleByParsedLight(runtime.data.lightBindings.size(), true);
		QHash<int, bool> effectiveCache;
		std::function<bool(int)> evalVisible = [&](int nodeIndex) -> bool
		{
			if (effectiveCache.contains(nodeIndex))
				return effectiveCache.value(nodeIndex);
			if (nodeIndex < 0 || nodeIndex >= runtime.data.nodeVisibilityStates.size())
				return true;

			const GltfAnimationNodeVisibilityState& nodeState = runtime.data.nodeVisibilityStates[nodeIndex];
			const bool localVisible = sampledNodeVisibility.value(nodeIndex, nodeState.defaultVisible);
			const bool effectiveVisible = localVisible &&
				(nodeState.parentNodeIndex < 0 || evalVisible(nodeState.parentNodeIndex));
			effectiveCache.insert(nodeIndex, effectiveVisible);
			return effectiveVisible;
		};

		for (const GltfAnimationLightBinding& binding : runtime.data.lightBindings)
		{
			if (binding.parsedLightIndex >= 0 && binding.parsedLightIndex < visibleByParsedLight.size())
				visibleByParsedLight[binding.parsedLightIndex] = evalVisible(binding.nodeIndex);
		}
		setAnimatedLightVisibilityState(sourceFile, visibleByParsedLight);
	}
	else
	{
		clearAnimatedLightVisibilityState(sourceFile);
	}

	if (!runtime.data.nodeVisibilityStates.isEmpty())
	{
		QHash<int, bool> effectiveCache;
		std::function<bool(int)> evalVisible = [&](int nodeIndex) -> bool
		{
			if (effectiveCache.contains(nodeIndex))
				return effectiveCache.value(nodeIndex);
			if (nodeIndex < 0 || nodeIndex >= runtime.data.nodeVisibilityStates.size())
				return true;

			const GltfAnimationNodeVisibilityState& nodeState = runtime.data.nodeVisibilityStates[nodeIndex];
			const bool localVisible = sampledNodeVisibility.value(nodeIndex, nodeState.defaultVisible);
			const bool effectiveVisible = localVisible &&
				(nodeState.parentNodeIndex < 0 || evalVisible(nodeState.parentNodeIndex));
			effectiveCache.insert(nodeIndex, effectiveVisible);
			return effectiveVisible;
		};

		QSet<QUuid> hiddenMeshUuids;
		std::function<void(SceneNode*)> collectHidden = [&](SceneNode* node)
		{
			if (!node)
				return;

			const int nodeIndex = runtime.nodeIndexByUuid.value(node->nodeUuid, -1);

			const bool visible = nodeIndex < 0 ? true : evalVisible(nodeIndex);
			if (!visible)
			{
				for (const QUuid& uuid : node->meshUuids)
					hiddenMeshUuids.insert(uuid);
			}

			for (SceneNode* child : node->children)
				collectHidden(child);
		};

		for (SceneNode* child : fileNode->children)
			collectHidden(child);
		setAnimatedMeshVisibilityState(sourceFile, hiddenMeshUuids);
	}
	else
	{
		clearAnimatedMeshVisibilityState(sourceFile);
	}
	for (auto it = animatedMaterials.constBegin(); it != animatedMaterials.constEnd(); ++it)
	{
		if (TriangleMesh* mesh = getMeshByUuid(it.key()))
			mesh->setMaterial(it.value());
	}
	if (animationAffectsShadowCasters)
		_shadowMapNeedsInitialization = true;
	update();
}

void GLWidget::onMeshBatchReady(const std::vector<AssImpMeshData>& batch)
{
	makeCurrent();
	for (const AssImpMeshData& meshData : batch)
	{
		AssImpMesh* mesh = createMeshFromData(meshData);
		addToDisplay(mesh);
		_pendingSceneUuids.append(mesh->uuid());
	}
	_viewer->updateDisplayList();
}

UVMethod GLWidget::promptLargeModelUVDecision(int totalTriangles, UVMethod currentMethod)
{
	QMessageBox msgBox(this);
	msgBox.setWindowTitle(tr("Performance Warning!"));
	msgBox.setText(tr("The model contains more than %1 triangles and the current method of UV generation is \"Smart UV\" which is time consuming.\nDo you want to continue generating the UV?")
		.arg(totalTriangles));
	msgBox.setIcon(QMessageBox::Question);

	QPushButton* yesButton = msgBox.addButton(QMessageBox::Yes);
	QPushButton* noButton = msgBox.addButton(QMessageBox::No);
	QPushButton* changeSettingsButton = msgBox.addButton(tr("Change Settings"), QMessageBox::ActionRole);

	msgBox.setDefaultButton(QMessageBox::Yes);
	msgBox.exec();

	if (msgBox.clickedButton() == noButton)
	{
		qDebug() << "User chose not to generate UVs, using None method.";
		return UVMethod::None;
	}

	if (msgBox.clickedButton() == changeSettingsButton)
	{
		return ModelViewer::askUserForUVMethod(this).method;
	}

	Q_UNUSED(yesButton);
	return currentMethod;
}

GLuint GLWidget::createGPUTextureFromImage(const QImage& image, const TextureSamplerSettings& samplers)
{
	if (image.isNull())
	{
		return 0;
	}

	GLenum internalFormat = GL_RGBA8;
	GLenum dataFormat = GL_RGBA;
	GLenum dataType = GL_UNSIGNED_BYTE;

	QImage glImage;

	switch (image.format())
	{
	case QImage::Format_RGB888:
		glImage = image;
		internalFormat = GL_RGB8;
		dataFormat = GL_RGB;
		break;

	case QImage::Format_RGBA8888:
	case QImage::Format_RGBA8888_Premultiplied:
		glImage = image;
		internalFormat = GL_RGBA8;
		dataFormat = GL_RGBA;
		break;

	case QImage::Format_Grayscale8:
		// Expand to RGBA so all three colour channels are populated.
		// Uploading as GL_RED leaves G and B at 0, making the texture appear red.
		glImage = image.convertToFormat(QImage::Format_RGBA8888);
		internalFormat = GL_RGBA8;
		dataFormat = GL_RGBA;
		break;

	case QImage::Format_Indexed8:
		glImage = image.convertToFormat(QImage::Format_RGBA8888);
		internalFormat = GL_RGBA8;
		dataFormat = GL_RGBA;
		break;

	default:
		glImage = image.convertToFormat(QImage::Format_RGBA8888);
		internalFormat = GL_RGBA8;
		dataFormat = GL_RGBA;
		break;
	}

	GLuint textureID;
	glGenTextures(1, &textureID);

	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, glImage.width(), glImage.height(), 0,
		dataFormat, dataType, glImage.constBits());
	glGenerateMipmap(GL_TEXTURE_2D);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, samplers.wrapS);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, samplers.wrapT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, samplers.minFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, samplers.magFilter);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, _anisotropicFilteringLevel);

	return textureID;
}

GLuint GLWidget::uploadDecodedTexture(GLMaterial::Texture& texture, const QImage& image)
{
	TextureSamplerSettings samplers{ texture.wrapS, texture.wrapT, texture.minFilter, texture.magFilter };
	GLuint textureId = uploadDecodedTextureImage(image, samplers);
	texture.id = textureId;
	return textureId;
}

GLuint GLWidget::uploadDecodedTextureImage(const QImage& image, const TextureSamplerSettings& samplers)
{
	makeCurrent();
	return createGPUTextureFromImage(image, samplers);
}

GLuint GLWidget::uploadKtx2TextureImage(const QString& path, const std::string& mapType, const TextureSamplerSettings& samplers)
{
	if (path.isEmpty())
	{
		return 0;
	}

	makeCurrent();

	TranscodedTexture transcodedTexture;
	if (!_ktx2Loader.loadKTX2(path.toStdString(), transcodedTexture, _gpuCapabilities, mapType))
	{
		qWarning() << "GLWidget::uploadKtx2Texture - Failed to load KTX2 file:" << path;
		return 0;
	}

	GLuint textureId = _ktx2Loader.uploadToGPU(transcodedTexture);
	if (textureId == 0)
	{
		qWarning() << "GLWidget::uploadKtx2Texture - Failed to upload KTX2 texture:" << path;
		return 0;
	}

	glBindTexture(GL_TEXTURE_2D, textureId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, samplers.minFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, samplers.magFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, samplers.wrapS);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, samplers.wrapT);
	glBindTexture(GL_TEXTURE_2D, 0);

	return textureId;
}

GLuint GLWidget::uploadKtx2Texture(const QString& path, const std::string& mapType, GLMaterial::Texture& texture)
{
	TextureSamplerSettings samplers{ texture.wrapS, texture.wrapT, texture.minFilter, texture.magFilter };
	GLuint textureId = uploadKtx2TextureImage(path, mapType, samplers);
	texture.id = textureId;
	return textureId;
}

unsigned int GLWidget::getOrCreateTextureCached(
	const QString& cacheKey,
	const QImage& image,
	const TextureSamplerSettings& samplers)
{
	if (cacheKey.isEmpty() || image.isNull())
	{
		return 0;
	}

	auto it = _texCache.find(cacheKey);
	if (it != _texCache.end())
	{
		CachedTextureEntry& entry = it->second;
		if (entry.lastGPUTexture != 0 && entry.lastSamplerSettings == samplers)
		{
			retainTexture(entry.lastGPUTexture);
			return entry.lastGPUTexture;
		}

		if (!entry.image.isNull())
		{
			makeCurrent();
			GLuint newTexID = createGPUTextureFromImage(entry.image, samplers);
			if (newTexID != 0)
			{
				entry.lastGPUTexture = newTexID;
				entry.lastSamplerSettings = samplers;
				_texRefCount[newTexID] = 1;
				return newTexID;
			}
		}
	}

	makeCurrent();
	GLuint texID = createGPUTextureFromImage(image, samplers);
	if (texID == 0)
	{
		return 0;
	}

	CachedTextureEntry newEntry;
	newEntry.image = image;
	newEntry.lastGPUTexture = texID;
	newEntry.lastSamplerSettings = samplers;
	newEntry.imageWidth = image.width();
	newEntry.imageHeight = image.height();

	_texCache[cacheKey] = newEntry;
	_texRefCount[texID] = 1;
	return texID;
}

unsigned int GLWidget::getOrLoadKtx2TextureCached(
	const QString& path,
	const std::string& mapType,
	const TextureSamplerSettings& samplers)
{
	if (path.isEmpty())
	{
		return 0;
	}

	const QString cacheKey = QStringLiteral("ktx2://%1::%2")
		.arg(path, QString::fromStdString(mapType));

	auto it = _texCache.find(cacheKey);
	if (it != _texCache.end())
	{
		CachedTextureEntry& entry = it->second;
		if (entry.lastGPUTexture != 0 && entry.lastSamplerSettings == samplers)
		{
			retainTexture(entry.lastGPUTexture);
			return entry.lastGPUTexture;
		}
	}

	GLuint texID = uploadKtx2TextureImage(path, mapType, samplers);
	if (texID == 0)
	{
		return 0;
	}

	CachedTextureEntry& entry = _texCache[cacheKey];
	entry.lastGPUTexture = texID;
	entry.lastSamplerSettings = samplers;
	_texRefCount[texID] = 1;
	return texID;
}

unsigned int GLWidget::getOrLoadTextureCached(
	const QString& path,
	const TextureSamplerSettings& samplers)
{
	if (path.isEmpty()) return 0;

	auto it = _texCache.find(path);

	// Cache hit - image exists
	if (it != _texCache.end())
	{
		CachedTextureEntry& entry = it->second;

		// Exact match (same image + same samplers)
		if (entry.lastGPUTexture != 0 && entry.lastSamplerSettings == samplers)
		{
			retainTexture(entry.lastGPUTexture);
			return entry.lastGPUTexture;
		}

		// Same image, different samplers - create new GPU texture from cached image
		if (!entry.image.isNull())
		{
			makeCurrent();
			GLuint newTexID = createGPUTextureFromImage(entry.image, samplers);
			if (newTexID != 0)
			{
				// CRITICAL FIX: Release the old texture before replacing it
				// Without this, orphaned GPU texture IDs cause context corruption
				GLuint oldTexID = entry.lastGPUTexture;
				if (oldTexID != 0)
				{
					releaseTexture(oldTexID);
				}

				entry.lastGPUTexture = newTexID;
				entry.lastSamplerSettings = samplers;
				_texRefCount[newTexID] = 1;
				return newTexID;
			}
		}
	}

	// Cache miss - image not cached, load from disk
	makeCurrent();
	GLuint texID = loadTextureFromFile(
		path.toStdString().c_str(),
		samplers.wrapS,
		samplers.wrapT,
		samplers.minFilter,
		samplers.magFilter,
		false);

	if (texID == 0) return 0;

	// Cache the image for future use with different samplers
	CachedTextureEntry newEntry;
	newEntry.image = QImage(path);  // Cache the image
	newEntry.lastGPUTexture = texID;
	newEntry.lastSamplerSettings = samplers;
	newEntry.imageWidth = newEntry.image.width();
	newEntry.imageHeight = newEntry.image.height();

	_texCache[path] = newEntry;
	_texRefCount[texID] = 1;

	return texID;
}

void GLWidget::retainTexture(unsigned int texId)
{
	if (texId == 0) return;
	auto it = _texRefCount.find(texId);
	if (it != _texRefCount.end()) it->second++;
	else _texRefCount[texId] = 1;
}

void GLWidget::releaseTexture(unsigned int texId)
{
	if (texId == 0) return;
	auto it = _texRefCount.find(texId);
	if (it == _texRefCount.end()) return;
	if (--(it->second) <= 0)
	{
		// remove from path map too
		for (auto pit = _texCache.begin(); pit != _texCache.end(); )
		{
			if (pit->second.lastGPUTexture == texId) pit = _texCache.erase(pit); else ++pit;
		}
		glDeleteTextures(1, &texId);
		_texRefCount.erase(texId);
	}
}

GLMaterial GLWidget::resolveMaterialTextures(GLWidget* w, const GLMaterial& src)
{
	GLMaterial m = src;
	auto resolveTexturePath = [w](const QString& path,
		const std::string& mapType,
		const TextureSamplerSettings& samplers) -> unsigned int
	{
		if (path.isEmpty())
			return 0;
		if (path.endsWith(".ktx2", Qt::CaseInsensitive))
			return w->getOrLoadKtx2TextureCached(path, mapType, samplers);
		return w->getOrLoadTextureCached(path, samplers);
	};
	auto resolveTexturePathOrKeepId = [&](const QString& path,
		const std::string& mapType,
		const TextureSamplerSettings& samplers,
		int currentId) -> unsigned int
	{
		if (path.isEmpty())
			return static_cast<unsigned int>(std::max(0, currentId));
		return resolveTexturePath(path, mapType, samplers);
	};

	if (m.hasAlbedoMap())
	{
		const GLMaterial::Texture& tex = m.texture(GLMaterial::TextureType::Albedo);
		TextureSamplerSettings samplers{ tex.wrapS, tex.wrapT, tex.minFilter, tex.magFilter };
		m.setAlbedoTextureId(resolveTexturePathOrKeepId(m.albedoMapPath(), "albedoMap", samplers, m.albedoTextureId()));
	}
	if (m.hasMetallicMap())
	{
		const GLMaterial::Texture& tex = m.texture(GLMaterial::TextureType::Metallic);
		TextureSamplerSettings samplers{ tex.wrapS, tex.wrapT, tex.minFilter, tex.magFilter };
		m.setMetallicTextureId(resolveTexturePathOrKeepId(m.metallicMapPath(), "metallicMap", samplers, m.metallicTextureId()));
	}
	if (m.hasRoughnessMap())
	{
		const GLMaterial::Texture& tex = m.texture(GLMaterial::TextureType::Roughness);
		TextureSamplerSettings samplers{ tex.wrapS, tex.wrapT, tex.minFilter, tex.magFilter };
		m.setRoughnessTextureId(resolveTexturePathOrKeepId(m.roughnessMapPath(), "roughnessMap", samplers, m.roughnessTextureId()));
	}
	if (m.hasNormalMap())
	{
		const GLMaterial::Texture& tex = m.texture(GLMaterial::TextureType::Normal);
		TextureSamplerSettings samplers{ tex.wrapS, tex.wrapT, tex.minFilter, tex.magFilter };
		m.setNormalTextureId(resolveTexturePathOrKeepId(m.normalMapPath(), "normalMap", samplers, m.normalTextureId()));
	}
	if (m.hasAOMap())
	{
		const GLMaterial::Texture& tex = m.texture(GLMaterial::TextureType::AmbientOcclusion);
		TextureSamplerSettings samplers{ tex.wrapS, tex.wrapT, tex.minFilter, tex.magFilter };
		m.setOcclusionTextureId(resolveTexturePathOrKeepId(m.aoMapPath(), "aoMap", samplers, m.occlusionTextureId()));
	}
	if (m.hasOpacityMap())
	{
		const GLMaterial::Texture& tex = m.texture(GLMaterial::TextureType::Opacity);
		TextureSamplerSettings samplers{ tex.wrapS, tex.wrapT, tex.minFilter, tex.magFilter };
		m.setOpacityTextureId(resolveTexturePathOrKeepId(m.opacityMapPath(), "opacityMap", samplers, m.opacityTextureId()));
	}
	if (m.hasHeightMap())
	{
		const GLMaterial::Texture& tex = m.texture(GLMaterial::TextureType::Height);
		TextureSamplerSettings samplers{ tex.wrapS, tex.wrapT, tex.minFilter, tex.magFilter };
		m.setHeightTextureId(resolveTexturePathOrKeepId(m.heightMapPath(), "heightMap", samplers, m.heightTextureId()));
	}
	if (m.hasEmissiveMap())
	{
		const GLMaterial::Texture& tex = m.texture(GLMaterial::TextureType::Emissive);
		TextureSamplerSettings samplers{ tex.wrapS, tex.wrapT, tex.minFilter, tex.magFilter };
		m.setEmissiveTextureId(resolveTexturePathOrKeepId(m.emissiveMapPath(), "emissiveMap", samplers, m.emissiveTextureId()));
	}
	if (m.hasTransmissionMap())
	{
		const GLMaterial::Texture& tex = m.texture(GLMaterial::TextureType::Transmission);
		TextureSamplerSettings samplers{ tex.wrapS, tex.wrapT, tex.minFilter, tex.magFilter };
		m.setTransmissionTextureId(resolveTexturePathOrKeepId(m.transmissionMapPath(), "transmissionMap", samplers, m.transmissionTextureId()));
	}
	if (m.hasIORMap())
	{
		const GLMaterial::Texture& tex = m.texture(GLMaterial::TextureType::IOR);
		TextureSamplerSettings samplers{ tex.wrapS, tex.wrapT, tex.minFilter, tex.magFilter };
		m.setIORTextureId(resolveTexturePathOrKeepId(m.iorMapPath(), "iorMap", samplers, m.iorTextureId()));
	}
	if (m.hasSheenColorMap())
	{
		const GLMaterial::Texture& tex = m.texture(GLMaterial::TextureType::SheenColor);
		TextureSamplerSettings samplers{ tex.wrapS, tex.wrapT, tex.minFilter, tex.magFilter };
		m.setSheenColorTextureId(resolveTexturePathOrKeepId(m.sheenColorMapPath(), "sheenColorMap", samplers, m.sheenColorTextureId()));
	}
	if (m.hasSheenRoughnessMap())
	{
		const GLMaterial::Texture& tex = m.texture(GLMaterial::TextureType::SheenRoughness);
		TextureSamplerSettings samplers{ tex.wrapS, tex.wrapT, tex.minFilter, tex.magFilter };
		m.setSheenRoughnessTextureId(resolveTexturePathOrKeepId(m.sheenRoughnessMapPath(), "sheenRoughnessMap", samplers, m.sheenRoughnessTextureId()));
	}
	if (m.hasClearcoatColorMap())
	{
		const GLMaterial::Texture& tex = m.texture(GLMaterial::TextureType::ClearcoatColor);
		TextureSamplerSettings samplers{ tex.wrapS, tex.wrapT, tex.minFilter, tex.magFilter };
		m.setClearcoatColorTextureId(resolveTexturePath(m.clearcoatColorMapPath(), "clearcoatColorMap", samplers));
	}
	if (m.hasClearcoatRoughnessMap())
	{
		const GLMaterial::Texture& tex = m.texture(GLMaterial::TextureType::ClearcoatRoughness);
		TextureSamplerSettings samplers{ tex.wrapS, tex.wrapT, tex.minFilter, tex.magFilter };
		m.setClearcoatRoughnessTextureId(resolveTexturePath(m.clearcoatRoughnessMapPath(), "clearcoatRoughnessMap", samplers));
	}
	if (m.hasClearcoatNormalMap())
	{
		const GLMaterial::Texture& tex = m.texture(GLMaterial::TextureType::ClearcoatNormal);
		TextureSamplerSettings samplers{ tex.wrapS, tex.wrapT, tex.minFilter, tex.magFilter };
		m.setClearcoatNormalTextureId(resolveTexturePath(m.clearcoatNormalMapPath(), "clearcoatNormalMap", samplers));
	}
	if (m.hasIridescenceMap())
	{
		const GLMaterial::Texture& tex = m.texture(GLMaterial::TextureType::Iridescence);
		TextureSamplerSettings samplers{ tex.wrapS, tex.wrapT, tex.minFilter, tex.magFilter };
		m.setIridescenceTextureId(resolveTexturePath(m.iridescenceMap(), "iridescenceMap", samplers));
	}
	if (m.hasIridescenceThicknessMap())
	{
		const GLMaterial::Texture& tex = m.texture(GLMaterial::TextureType::IridescenceThickness);
		TextureSamplerSettings samplers{ tex.wrapS, tex.wrapT, tex.minFilter, tex.magFilter };
		m.setIridescenceThicknessTextureId(resolveTexturePath(m.iridescenceThicknessMap(), "iridescenceThicknessMap", samplers));
	}
	if (m.hasSpecularColorMap())
	{
		const GLMaterial::Texture& tex = m.texture(GLMaterial::TextureType::SpecularColor);
		TextureSamplerSettings samplers{ tex.wrapS, tex.wrapT, tex.minFilter, tex.magFilter };
		m.setSpecularColorTextureId(resolveTexturePath(m.specularColorMap(), "specularColorMap", samplers));
	}
	if (m.hasSpecularFactorMap())
	{
		const GLMaterial::Texture& tex = m.texture(GLMaterial::TextureType::SpecularFactor);
		TextureSamplerSettings samplers{ tex.wrapS, tex.wrapT, tex.minFilter, tex.magFilter };
		m.setSpecularFactorTextureId(resolveTexturePath(m.specularFactorMap(), "specularFactorMap", samplers));
	}
	if (m.hasAnisotropyMap())
	{
		const GLMaterial::Texture& tex = m.texture(GLMaterial::TextureType::Anisotropy);
		TextureSamplerSettings samplers{ tex.wrapS, tex.wrapT, tex.minFilter, tex.magFilter };
		m.setAnisotropyTextureId(resolveTexturePath(m.anisotropyMap(), "anisotropyMap", samplers));
	}
	if (m.hasThicknessMap())
	{
		const GLMaterial::Texture& tex = m.texture(GLMaterial::TextureType::Thickness);
		TextureSamplerSettings samplers{ tex.wrapS, tex.wrapT, tex.minFilter, tex.magFilter };
		m.setThicknessTextureId(resolveTexturePath(m.thicknessMap(), "thicknessMap", samplers));
	}
	if (m.hasDiffuseMap())
	{
		const GLMaterial::Texture& tex = m.texture(GLMaterial::TextureType::Diffuse);
		TextureSamplerSettings samplers{ tex.wrapS, tex.wrapT, tex.minFilter, tex.magFilter };
		m.setDiffuseTextureId(resolveTexturePath(m.diffuseMap(), "diffuseMap", samplers));
	}
	if (m.hasDiffuseTransmissionMap())
	{
		const GLMaterial::Texture& tex = m.texture(GLMaterial::TextureType::DiffuseTransmission);
		TextureSamplerSettings samplers{ tex.wrapS, tex.wrapT, tex.minFilter, tex.magFilter };
		m.setDiffuseTransmissionTextureId(resolveTexturePath(m.diffuseTransmissionMap(), "diffuseTransmissionMap", samplers));
	}
	if (m.hasDiffuseTransmissionColorMap())
	{
		const GLMaterial::Texture& tex = m.texture(GLMaterial::TextureType::DiffuseTransmissionColor);
		TextureSamplerSettings samplers{ tex.wrapS, tex.wrapT, tex.minFilter, tex.magFilter };
		m.setDiffuseTransmissionColorTextureId(resolveTexturePath(m.diffuseTransmissionColorMap(), "diffuseTransmissionColorMap", samplers));
	}
	if (m.hasSpecularGlossinessMap())
	{
		const GLMaterial::Texture& tex = m.texture(GLMaterial::TextureType::SpecularGlossiness);
		TextureSamplerSettings samplers{ tex.wrapS, tex.wrapT, tex.minFilter, tex.magFilter };
		m.setSpecularGlossinessTextureId(resolveTexturePath(m.specularGlossinessMap(), "specularGlossinessMap", samplers));
	}

	m.syncTextureParameters();

	// Ensure ADS values are recalculated after copy and texture resolution
	// (copy assignment operator at line 6998 doesn't call updateConsistency)
	m.updateConsistency();

	return m;
}

void GLWidget::initTransmissionBuffer()
{
	// Called once during widget initialization (e.g., in initializeGL or constructor)
	_transmissionTextureWidth = width();
	_transmissionTextureHeight = height();

	if (_transmissionFBO != 0)
		glDeleteFramebuffers(1, &_transmissionFBO);
	if (_transmissionColorTexture != 0)
		glDeleteTextures(1, &_transmissionColorTexture);
	if (_transmissionDepthTexture != 0)
		glDeleteTextures(1, &_transmissionDepthTexture);

	// Create FBO
	glGenFramebuffers(1, &_transmissionFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, _transmissionFBO);

	// --- Create COLOR texture (RGBA32F for precision) ---
	glGenTextures(1, &_transmissionColorTexture);
	glBindTexture(GL_TEXTURE_2D, _transmissionColorTexture);
	// Allocate storage with mipmaps
	// Calculate number of mip levels: log2(max(width, height)) + 1
	int maxDim = std::max(_transmissionTextureWidth, _transmissionTextureHeight);
	int numMips = (int)std::floor(std::log2(maxDim)) + 1;

	// Allocate texture storage with mipmaps
	glTexStorage2D(GL_TEXTURE_2D, numMips, GL_RGBA32F,
		_transmissionTextureWidth, _transmissionTextureHeight);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, numMips - 1);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		GL_TEXTURE_2D, _transmissionColorTexture, 0);

	// --- Create DEPTH texture (DEPTH32F) ---
	glGenTextures(1, &_transmissionDepthTexture);
	glBindTexture(GL_TEXTURE_2D, _transmissionDepthTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F,
		_transmissionTextureWidth, _transmissionTextureHeight,
		0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
		GL_TEXTURE_2D, _transmissionDepthTexture, 0);

	GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, drawBuffers);

	// --- Verify FBO is complete ---
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		qWarning() << "Transmission FBO incomplete! Status:" << status;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void GLWidget::resizeTransmissionBuffer(int width, int height)
{
	// Called from resizeGL() whenever window size changes
	if (_transmissionTextureWidth == width && _transmissionTextureHeight == height)
		return; // No resize needed

	_transmissionTextureWidth = width;
	_transmissionTextureHeight = height;

	initTransmissionBuffer();
}

void GLWidget::createWhiteTexture()
{
	unsigned char white[] = { 255, 255, 255, 255 };
	glGenTextures(1, &_whiteTexture);
	glBindTexture(GL_TEXTURE_2D, _whiteTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void GLWidget::generateCubemapMipmaps(GLuint cubemapTexture)
{
	// Calculate mip count based on environment map resolution
	// Query actual cubemap resolution at runtime
	GLint baseSize = 0;
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
	glGetTextureLevelParameteriv(cubemapTexture, 0, GL_TEXTURE_WIDTH, &baseSize);

	int maxMipLevels = static_cast<int>(std::log2(baseSize)) + 1;

	// Create temporary FBO for rendering to mip levels
	GLuint mipmapFBO;
	GLuint mipmapRBO;
	glGenFramebuffers(1, &mipmapFBO);
	glGenRenderbuffers(1, &mipmapRBO);

	glBindFramebuffer(GL_FRAMEBUFFER, mipmapFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, mipmapRBO);

	// Setup projection matrix (90 degree FOV for cubemap, 1:1 aspect ratio)
	QMatrix4x4 captureProjection;
	captureProjection.perspective(90.0f, 1.0f, 0.1f, 10.0f);

	// Setup view matrices for each cubemap face (must match environment capture order)
	QMatrix4x4 captureViews[] = {
		// +X face
		[]() {
			QMatrix4x4 m;
			m.lookAt(QVector3D(0.0f, 0.0f, 0.0f), QVector3D(1.0f, 0.0f, 0.0f), QVector3D(0.0f, -1.0f, 0.0f));
			return m;
		}(),
			// -X face
			[]() {
				QMatrix4x4 m;
				m.lookAt(QVector3D(0.0f, 0.0f, 0.0f), QVector3D(-1.0f, 0.0f, 0.0f), QVector3D(0.0f, -1.0f, 0.0f));
				return m;
			}(),
				// +Y face
				[]() {
					QMatrix4x4 m;
					m.lookAt(QVector3D(0.0f, 0.0f, 0.0f), QVector3D(0.0f, 1.0f, 0.0f), QVector3D(0.0f, 0.0f, 1.0f));
					return m;
				}(),
					// -Y face
					[]() {
						QMatrix4x4 m;
						m.lookAt(QVector3D(0.0f, 0.0f, 0.0f), QVector3D(0.0f, -1.0f, 0.0f), QVector3D(0.0f, 0.0f, -1.0f));
						return m;
					}(),
						// +Z face
						[]() {
							QMatrix4x4 m;
							m.lookAt(QVector3D(0.0f, 0.0f, 0.0f), QVector3D(0.0f, 0.0f, 1.0f), QVector3D(0.0f, -1.0f, 0.0f));
							return m;
						}(),
							// -Z face
							[]() {
								QMatrix4x4 m;
								m.lookAt(QVector3D(0.0f, 0.0f, 0.0f), QVector3D(0.0f, 0.0f, -1.0f), QVector3D(0.0f, -1.0f, 0.0f));
								return m;
							}()
	};

	_skyBox->setProg(_downsampleShader.get());

	// Bind shader and set static uniforms
	_downsampleShader->bind();
	_downsampleShader->setUniformValue("projection", captureProjection);

	// Bind source cubemap to texture unit 0
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
	_downsampleShader->setUniformValue("sourceMap", 0);

	// Generate each mip level
	for (int mip = 1; mip < maxMipLevels; ++mip)
	{
		int mipSize = baseSize >> mip;  // Bitshift divide by 2^mip

		// Resize renderbuffer for depth attachment
		glBindRenderbuffer(GL_RENDERBUFFER, mipmapRBO);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipSize, mipSize);

		// Set viewport to target mip size
		glViewport(0, 0, mipSize, mipSize);

		// Set which source mip level to sample from
		_downsampleShader->setUniformValue("currentMipLevel", mip - 1);

		// Render to all 6 faces of this mip level
		for (int face = 0; face < 6; ++face)
		{
			// Attach this mip level of this face to framebuffer
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, cubemapTexture, mip);

			// Verify framebuffer is complete
			GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			if (status != GL_FRAMEBUFFER_COMPLETE)
			{
				qWarning() << "Framebuffer incomplete at mip" << mip << "face" << face;
				continue;
			}

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// Set view matrix for this face
			_downsampleShader->setUniformValue("view", captureViews[face]);

			// Render the cube
			_skyBox->render();
		}
	}

	// Restore state - MORE AGGRESSIVE
	glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glUseProgram(0);

	// Reset viewport to current widget size
	glViewport(0, 0, width(), height());

	// Force rebind environment map to ensure it's in correct state
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

}

QString GLWidget::generateUniqueMeshName(const QString& baseName)
{
	// Check if base name already exists
	bool nameExists = false;
	for (const TriangleMesh* mesh : _meshStore)
	{
		if (mesh->getName() == baseName)
		{
			nameExists = true;
			break;
		}
	}

	// If base name doesn't exist, use it as-is
	if (!nameExists)
		return baseName;

	// Find the next available number suffix
	int counter = 2;
	QString uniqueName;

	while (true)
	{
		uniqueName = QString("%1 (%2)").arg(baseName).arg(counter);

		bool exists = false;
		for (const TriangleMesh* mesh : _meshStore)
		{
			if (mesh->getName() == uniqueName)
			{
				exists = true;
				break;
			}
		}

		if (!exists)
			break;

		counter++;
	}

	return uniqueName;
}

void GLWidget::renderToTransmissionBuffer(GLCamera* camera, const QColor& topColor, const QColor& botColor)
{
	if (!_transmissionEnabled)
		return;

	resizeTransmissionBuffer(width(), height());

	// --- SETUP STATE ---
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glFrontFace(GL_CCW);
	glDisable(GL_POLYGON_OFFSET_FILL);
	glDepthMask(GL_TRUE);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glStencilMask(0xFF);
	glDisable(GL_STENCIL_TEST);

	// --- BIND FBO ---
	glBindFramebuffer(GL_FRAMEBUFFER, _transmissionFBO);
	glViewport(0, 0, _transmissionTextureWidth, _transmissionTextureHeight);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// --- Setup matrices ---
	_viewMatrix.setToIdentity();
	_viewMatrix = _primaryCamera->getViewMatrix();
	_projectionMatrix = _primaryCamera->getProjectionMatrix();
	_modelViewMatrix = _viewMatrix * _modelMatrix;

	// --- RENDER 1: BACKGROUND (gradient or skybox) ---
	if (_skyBoxEnabled)
	{
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		drawSkyBox();
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
	}
	else
	{
		// Render gradient background (same as main framebuffer)
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		gradientBackground(topColor.redF(), topColor.greenF(), topColor.blueF(), topColor.alphaF(),
			botColor.redF(), botColor.greenF(), botColor.blueF(), botColor.alphaF(), _gradientStyle);
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
	}

	// --- RENDER 2: OPAQUE MESHES (with clipping) ---
	// Units 32/33: prevent feedback — the transmission FBO is currently the render target,
	// so bind white instead of the real transmission textures.
	glActiveTexture(GL_TEXTURE0 + 32);
	glBindTexture(GL_TEXTURE_2D, _whiteTexture);
	glActiveTexture(GL_TEXTURE0 + 33);
	glBindTexture(GL_TEXTURE_2D, _whiteTexture);
	// Units 37/38: SSS irradiance/depth from the SSS capture pass.
	glActiveTexture(GL_TEXTURE0 + 37);
	glBindTexture(GL_TEXTURE_2D, _sssCaptureTexture != 0 ? _sssCaptureTexture : _whiteTexture);
	glActiveTexture(GL_TEXTURE0 + 38);
	glBindTexture(GL_TEXTURE_2D, _sssDepthTexture != 0 ? _sssDepthTexture : _whiteTexture);
	glActiveTexture(GL_TEXTURE0);

	_fgShader->bind();
	setCommonUniforms(_fgShader.get(), _primaryCamera);
	drawMeshesWithClipping(_fgShader.get(), false); // opaque pass only
	_fgShader->release();

	// --- RENDER 3: SECTION CAPS ---
	if (_cappingEnabled &&
		!_sectionCapsSuppressedDuringInteraction &&
		(_clipYZEnabled || _clipZXEnabled || _clipXYEnabled))
	{
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(1.0f, 1.0f);
		drawSectionCapping();
		glDisable(GL_POLYGON_OFFSET_FILL);
	}

	// --- RENDER 4: GROUND ---
	if (_displayMode == DisplayMode::REALSHADED &&
		_groundMode != GroundMode::None && !_cappingEnabled &&
		!_meshStore.empty() &&
		camera != _orthoViewsCamera)
	{
		if (_groundMode == GroundMode::Floor)
		{
			// Avoid sampling from the transmission render target while it is bound
			// as the current framebuffer color attachment.
			glActiveTexture(GL_TEXTURE0 + 32);
			glBindTexture(GL_TEXTURE_2D, _whiteTexture);
			glActiveTexture(GL_TEXTURE0 + 33);
			glBindTexture(GL_TEXTURE_2D, _whiteTexture);
			glActiveTexture(GL_TEXTURE0);
			drawFloor(false);
		}
		else if (_groundMode == GroundMode::Grid)
		{
			drawGrid();
		}
	}

	// IMPORTANT: After rendering, generate mipmaps
	glBindTexture(GL_TEXTURE_2D, _transmissionColorTexture);
	glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);

	// --- UNBIND FBO ---
	glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
	glViewport(0, 0, width(), height());
}

// ============================================================================
// SSS capture pass
// Renders only hasVolumeScattering meshes into _sssFBO with sssCapture=true,
// which makes the shader output raw linear diffuse irradiance.
// The result in _sssCaptureTexture feeds the blur passes in Sequence 4.
// ============================================================================

void GLWidget::renderToSSSBuffer(GLCamera* camera)
{
	if (!sceneHasVisibleSSSMaterials())
		return;

	resizeSSSBuffer(width(), height());

	// --- SETUP STATE ---
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glFrontFace(GL_CCW);
	glDisable(GL_POLYGON_OFFSET_FILL);
	glDepthMask(GL_TRUE);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glStencilMask(0xFF);
	glDisable(GL_STENCIL_TEST);

	// --- BIND FBO ---
	glBindFramebuffer(GL_FRAMEBUFFER, _sssFBO);
	glViewport(0, 0, _sssTextureWidth, _sssTextureHeight);

	// Black background — non-SSS pixels are discarded by the shader so nothing
	// writes to them; black is the correct additive identity for the blur.
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// --- Setup matrices ---
	_viewMatrix.setToIdentity();
	_viewMatrix = camera->getViewMatrix();
	_projectionMatrix = camera->getProjectionMatrix();
	_modelViewMatrix = _viewMatrix * _modelMatrix;

	// Bind white dummy textures on the SSS sampler slots (units 37/38) so the shader's
	// sampleCapturedSSSDiffuse() sees a valid, neutral value during the capture pass itself.
	glActiveTexture(GL_TEXTURE0 + 37);
	glBindTexture(GL_TEXTURE_2D, _whiteTexture);
	glActiveTexture(GL_TEXTURE0 + 38);
	glBindTexture(GL_TEXTURE_2D, _whiteTexture);
	glActiveTexture(GL_TEXTURE0);

	// --- RENDER: SSS opaque meshes only ---
	// sssCapture=true makes the shader discard non-SSS fragments and output
	// raw linear diffuse (baseDirectDiffuse + baseDiffuseIBL) for SSS ones.
	_fgShader->bind();
	setCommonUniforms(_fgShader.get(), camera);
	_fgShader->setUniformValue("sssCapture", true);
	drawMeshesWithClipping(_fgShader.get(), false); // opaque pass only
	_fgShader->setUniformValue("sssCapture", false); // reset before release
	_fgShader->release();

	// No mipmaps needed — the blur passes sample at full resolution.

	// --- UNBIND FBO ---
	glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
	glViewport(0, 0, width(), height());
}

void GLWidget::cleanupTransmissionBuffer()
{
	if (_transmissionFBO != 0)
	{
		glDeleteFramebuffers(1, &_transmissionFBO);
		_transmissionFBO = 0;
	}
	if (_transmissionColorTexture != 0)
	{
		glDeleteTextures(1, &_transmissionColorTexture);
		_transmissionColorTexture = 0;
	}
	if (_transmissionDepthTexture != 0)
	{
		glDeleteTextures(1, &_transmissionDepthTexture);
		_transmissionDepthTexture = 0;
	}
}

// ============================================================================
// SSS (Subsurface Scattering) Buffer
// Two-FBO ping-pong layout:
//   _sssFBO    + _sssCaptureTexture  — capture pass output / V-blur output
//   _sssBlurFBO + _sssBlurTexture    — H-blur output / V-blur input
// Both are RGBA16F (no mipmaps needed — they are blur intermediates).
// _sssDepthTexture is shared by the capture FBO for correct depth occlusion.
// ============================================================================

void GLWidget::initSSSBuffer()
{
	_sssTextureWidth  = width();
	_sssTextureHeight = height();

	// Clean up any pre-existing resources first
	if (_sssFBO != 0)            { glDeleteFramebuffers(1, &_sssFBO);            _sssFBO            = 0; }
	if (_sssCaptureTexture != 0) { glDeleteTextures(1, &_sssCaptureTexture);     _sssCaptureTexture = 0; }
	if (_sssDepthTexture != 0)   { glDeleteTextures(1, &_sssDepthTexture);       _sssDepthTexture   = 0; }
	if (_sssBlurFBO != 0)        { glDeleteFramebuffers(1, &_sssBlurFBO);        _sssBlurFBO        = 0; }
	if (_sssBlurTexture != 0)    { glDeleteTextures(1, &_sssBlurTexture);        _sssBlurTexture    = 0; }

	// ---- Capture FBO --------------------------------------------------------
	glGenFramebuffers(1, &_sssFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, _sssFBO);

	// Color: RGBA16F — SSS diffuse capture, no mipmaps (blur input)
	glGenTextures(1, &_sssCaptureTexture);
	glBindTexture(GL_TEXTURE_2D, _sssCaptureTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
		_sssTextureWidth, _sssTextureHeight,
		0, GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		GL_TEXTURE_2D, _sssCaptureTexture, 0);

	// Depth: DEPTH32F — correct occlusion ordering during the capture pass
	glGenTextures(1, &_sssDepthTexture);
	glBindTexture(GL_TEXTURE_2D, _sssDepthTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F,
		_sssTextureWidth, _sssTextureHeight,
		0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
		GL_TEXTURE_2D, _sssDepthTexture, 0);

	{
		GLenum drawBufs[] = { GL_COLOR_ATTACHMENT0 };
		glDrawBuffers(1, drawBufs);
		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE)
			qWarning() << "SSS capture FBO incomplete! Status:" << status;
	}

	// ---- Blur FBO -----------------------------------------------------------
	// No depth attachment needed — blur passes are fullscreen quads.
	glGenFramebuffers(1, &_sssBlurFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, _sssBlurFBO);

	glGenTextures(1, &_sssBlurTexture);
	glBindTexture(GL_TEXTURE_2D, _sssBlurTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
		_sssTextureWidth, _sssTextureHeight,
		0, GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		GL_TEXTURE_2D, _sssBlurTexture, 0);

	{
		GLenum drawBufs[] = { GL_COLOR_ATTACHMENT0 };
		glDrawBuffers(1, drawBufs);
		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE)
			qWarning() << "SSS blur FBO incomplete! Status:" << status;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void GLWidget::resizeSSSBuffer(int width, int height)
{
	if (_sssTextureWidth == width && _sssTextureHeight == height)
		return;

	_sssTextureWidth  = width;
	_sssTextureHeight = height;

	initSSSBuffer();
}

void GLWidget::cleanupSSSBuffer()
{
	if (_sssFBO != 0)            { glDeleteFramebuffers(1, &_sssFBO);            _sssFBO            = 0; }
	if (_sssCaptureTexture != 0) { glDeleteTextures(1, &_sssCaptureTexture);     _sssCaptureTexture = 0; }
	if (_sssDepthTexture != 0)   { glDeleteTextures(1, &_sssDepthTexture);       _sssDepthTexture   = 0; }
	if (_sssBlurFBO != 0)        { glDeleteFramebuffers(1, &_sssBlurFBO);        _sssBlurFBO        = 0; }
	if (_sssBlurTexture != 0)    { glDeleteTextures(1, &_sssBlurTexture);        _sssBlurTexture    = 0; }
}

void GLWidget::checkAndStopTimers()
{
	if (_animateViewTimer->isActive())
	{
		_animateViewTimer->stop();
		// Set all defaults
		_currentRotation = QQuaternion::fromRotationMatrix(_primaryCamera->getViewMatrix().toGenericMatrix<3, 3>());
		_currentTranslation = _primaryCamera->getPosition();
		_currentViewRange = _viewRange;
		_slerpStep = 0.0f;
		_customViewAnimationActive = false;
		emit rotationsSet();
	}
	if (_animateFitAllTimer->isActive())
	{
		_animateFitAllTimer->stop();
		// Set all defaults
		_currentTranslation = _primaryCamera->getPosition();
		_currentViewRange = _viewRange;
		_slerpStep = 0.0f;
		emit zoomAndPanSet();
	}
	if (_animateWindowZoomTimer->isActive())
	{
		_animateWindowZoomTimer->stop();
		_animateFitAllTimer->stop();
		// Set all defaults
		_currentTranslation = _primaryCamera->getPosition();
		_currentViewRange = _viewRange;
		_slerpStep = 0.0f;
		emit zoomAndPanSet();
	}
	if (_animateCenterScreenTimer->isActive())
	{
		_animateCenterScreenTimer->stop();
		_animateFitAllTimer->stop();
		// Set all defaults
		_currentTranslation = _primaryCamera->getPosition();
		_currentViewRange = _viewRange;
		_slerpStep = 0.0f;
		emit zoomAndPanSet();
	}
}

void GLWidget::disableLowRes()
{
	_lowResEnabled = false;
	update();
}

void GLWidget::disableSectionCapsInteractionSuppression()
{
	setSectionCapsInteractionSuppressed(false);
}

void GLWidget::setSectionCapsInteractionSuppressed(bool suppressed)
{
	bool actual = suppressed && _dynamicCappingEnabled;
	if (_sectionCapsSuppressedDuringInteraction == actual)
		return;

	_sectionCapsSuppressedDuringInteraction = actual;
	update();
}

void GLWidget::setSectionCapsDynamicEnabled(bool enabled)
{
	_dynamicCappingEnabled = enabled;
	if (!enabled && _sectionCapsSuppressedDuringInteraction)
		setSectionCapsInteractionSuppressed(false);
}

void GLWidget::resizeEvent(QResizeEvent* event)
{
	if (_viewToolbar)
	{
		_viewToolbar->reposition(width(), height()); // Move completely below widget
	}
	QOpenGLWidget::resizeEvent(event);
	if (_viewer)
	{
		_viewer->updateNavigationOverlayGeometry();
		QMetaObject::invokeMethod(this, [this]()
		{
			if (_viewer)
				_viewer->updateNavigationOverlayGeometry();
		}, Qt::QueuedConnection);
	}
}

void GLWidget::mousePressEvent(QMouseEvent* e)
{
	setFocus();
	checkAndStopTimers();

	// Reset inertia on new mouse press
	_inertiaPanVelocity = QVector2D();
	_inertiaZoomVelocity = 0.0f;
	_inertiaRotateVelocity = QVector2D();
	if (_inertiaTimer) _inertiaTimer->stop();

	// Reset movement tracking
	_mouseMovedSincePress = false;
	_lastMouseMoveTime = 0;
	_lastMousePos  = e->pos();
	_lastMouseTime = e->timestamp();

	if (e->button() & Qt::LeftButton)
	{
		const QPoint clickPoint(e->position().x(), e->position().y());
		const BoundingSphere selectionSphere = computeTransformGizmoSelectionSphere();
		const float selectionRadius = selectionSphere.getRadius() > 0.0f
			? selectionSphere.getRadius()
			: _boundingSphere.getRadius();
		const float gizmoScale = (std::max)(selectionRadius * 0.9f, 0.01f);
		if (!(e->modifiers() & Qt::ControlModifier) &&
			_transformGizmoRequested && _transformGizmo &&
			_transformGizmo->activateHandleAt(clickPoint, _primaryCamera, _viewMatrix, _projectionMatrix,
				QRect(0, 0, width(), height()), gizmoScale))
		{
			if (beginTransformGizmoDrag(_transformGizmo->activeHandle(), clickPoint))
			{
				update();
				return;
			}
		}
		if (_transformGizmo)
			_transformGizmo->clearInteraction();
		if (!(e->modifiers() & Qt::ControlModifier) && !(e->modifiers() & Qt::ShiftModifier)
			&& !_windowZoomActive && !_viewRotating && !_viewPanning && !_viewZooming
			&& handleViewCubeClick(clickPoint))
		{
			return;
		}

		_leftButtonPoint.setX(e->position().x());
		_leftButtonPoint.setY(e->position().y());

		// Track if Shift is held for drag selection mode
		_shiftDragActive = (e->modifiers() & Qt::ShiftModifier) != 0;
		_sweepStartPoint = QPoint(e->position().x(), e->position().y());

		if (_viewPanning || _viewZooming || _viewRotating)
		{
			_navigationViewportLocked = true;
			_navigationLockedViewport = getViewportFromPoint(e->pos());
			_navigationLockedClientRect = getClientRectFromPoint(e->pos());
		}

		if (!(e->modifiers() & Qt::ControlModifier) && !(e->modifiers() & Qt::ShiftModifier)
			&& !_windowZoomActive && !_viewRotating && !_viewPanning && !_viewZooming)
		{
			// Selection
			_selectionManager->clickSelect(clickPoint);
		}


		_rubberBand->setGeometry(QRect(_leftButtonPoint, QSize()));
		_rubberBand->show();
	}

	if ((e->button() & Qt::RightButton) || ((e->button() & Qt::LeftButton) && _viewPanning))
	{
		_rightButtonPoint.setX(e->position().x());
		_rightButtonPoint.setY(e->position().y());
		_lastPanPoint = e->pos();
		_navigationViewportLocked = true;
		_navigationLockedViewport = getViewportFromPoint(e->pos());
		_navigationLockedClientRect = getClientRectFromPoint(e->pos());
	}

	if (e->button() & Qt::MiddleButton || ((e->button() & Qt::LeftButton) && _viewRotating))
	{
		_middleButtonPoint.setX(e->position().x());
		_middleButtonPoint.setY(e->position().y());
		if (e->button() & Qt::MiddleButton)
		{
			_navigationViewportLocked = true;
			_navigationLockedViewport = getViewportFromPoint(e->pos());
			_navigationLockedClientRect = getClientRectFromPoint(e->pos());
		}
	}
}

void GLWidget::mouseReleaseEvent(QMouseEvent* e)
{
	if ((e->button() & Qt::LeftButton) && _transformGizmoTranslating)
	{
		finishTransformGizmoTranslationDrag(true);
		update();
		return;
	}
	if ((e->button() & Qt::LeftButton) && _transformGizmoScaling)
	{
		finishTransformGizmoScaleDrag(true);
		update();
		return;
	}
	if ((e->button() & Qt::LeftButton) && _transformGizmoRotating)
	{
		finishTransformGizmoRotationDrag(true);
		update();
		return;
	}

	if (e->button() & Qt::LeftButton)
	{
        _rubberBand->hide();
		if (_windowZoomActive)
		{
			performWindowZoom();
		}
		else if (!(e->modifiers() & Qt::ControlModifier) && !_viewRotating && !_viewPanning && !_viewZooming)
		{
			// Sweep select: check shift status at release time to determine if we should add to selection
			bool shiftHeldAtRelease = (e->modifiers() & Qt::ShiftModifier) != 0;
			// Prefer the current shift state at release time over the state at press time
			bool addToSelection = shiftHeldAtRelease || _shiftDragActive;

			sweepSelect(e->pos(), addToSelection);
			_shiftDragActive = false;  // Reset the flag
		}
	}

	if (e->button() & Qt::RightButton)
	{
		_lastPanPoint = e->pos();
	}

	if (e->button() & Qt::MiddleButton)
	{
		if (e->modifiers() == Qt::NoModifier)
		{
			if (!_multiViewActive)
			{
				QPoint o(width() / 2, height() / 2);
				QPoint p = e->pos();

				QVector3D OP = get3dTranslationVectorFromMousePoints(o, p);
				_primaryCamera->move(OP.x(), OP.y(), OP.z());
				_currentTranslation = _primaryCamera->getPosition();
				update();
			}
		}
	}

	_lowResEnabled = false;
	if (!_viewRotating && !_viewPanning && !_viewZooming)
	{
		setCursor(QCursor(Qt::ArrowCursor));
	}

	// Only start inertia if mouse was moving recently
	qint64 now = e->timestamp();
	const qint64 maxIdleMs = 50; // adjust as needed
	bool recentMove = (_lastMouseMoveTime > 0) && ((now - _lastMouseMoveTime) < maxIdleMs);

	// Start inertia if velocity is significant
	if (_mouseMovedSincePress && recentMove &&
		(_inertiaPanVelocity.lengthSquared() > 1.0f ||
			std::abs(_inertiaZoomVelocity) > 0.01f ||
			_inertiaRotateVelocity.lengthSquared() > 1.0f))
	{
		if (_inertiaTimer) _inertiaTimer->start();
	}
	else if (_sectionCapsSuppressedDuringInteraction)
	{
		QTimer::singleShot(100, this, &GLWidget::disableSectionCapsInteractionSuppression);
	}

	if (e->buttons() == Qt::NoButton)
	{
		_navigationViewportLocked = false;
		_navigationLockedViewport = QRect();
		_navigationLockedClientRect = QRect();
	}

	update();
}

void GLWidget::mouseMoveEvent(QMouseEvent* e)
{
	_mouseMovedSincePress = true;
	_lastMouseMoveTime = e->timestamp();
	QPoint currentPos = e->pos();
	qint64 currentTime = e->timestamp();
	QPoint delta = currentPos - _lastMousePos;
	float dt = (currentTime - _lastMouseTime) / 1000.0f; // seconds

	QPoint downPoint(e->position().x(), e->position().y());
	if (_transformGizmoTranslating && (e->buttons() & Qt::LeftButton))
	{
		updateTransformGizmoTranslationDrag(e->pos());
		_lastMousePos = currentPos;
		_lastMouseTime = currentTime;
		return;
	}
	if (_transformGizmoScaling && (e->buttons() & Qt::LeftButton))
	{
		updateTransformGizmoScaleDrag(e->pos());
		_lastMousePos = currentPos;
		_lastMouseTime = currentTime;
		return;
	}
	if (_transformGizmoRotating && (e->buttons() & Qt::LeftButton))
	{
		updateTransformGizmoRotationDrag(e->pos());
		_lastMousePos = currentPos;
		_lastMouseTime = currentTime;
		return;
	}

	if (e->buttons() == Qt::LeftButton && !_viewPanning && !_viewZooming)
	{
		if (!(e->modifiers() & Qt::ControlModifier) && !_viewRotating && !_viewPanning && !_viewZooming)
		{
            _rubberBand->setGeometry(QRect(_leftButtonPoint, e->pos()).normalized());
		}
		if (_windowZoomActive)
		{
			setCursor(QCursor(QPixmap(":/icons/res/window-zoom-cursor.png"), 12, 12));
		}
		else if (((e->modifiers() & Qt::ControlModifier) || _viewRotating) && !isGltfCameraActive())
		{
			if (_displayedObjectsMemSize > MAX_MODEL_SIZE_BYTES)
				_lowResEnabled = true;
			setSectionCapsInteractionSuppressed(true);
			QPoint rotate = _leftButtonPoint - downPoint;

			if (_primaryCamera->getMode() == GLCamera::CameraMode::Orbit)
			{
				_primaryCamera->rotateX(rotate.y() / 2.0);
				_primaryCamera->rotateY(rotate.x() / 2.0);
			}
			else if (_primaryCamera->getMode() == GLCamera::CameraMode::Fly || _primaryCamera->getMode() == GLCamera::CameraMode::FirstPerson)
			{
				_primaryCamera->getYaw() += rotate.x() * 0.2f;
				_primaryCamera->getPitch() += rotate.y() * 0.2f;

				if (_primaryCamera->getMode() == GLCamera::CameraMode::FirstPerson)
					_primaryCamera->getPitch() = std::clamp(_primaryCamera->getPitch(), -60.0f, 60.0f);
				else
					_primaryCamera->getPitch() = std::clamp(_primaryCamera->getPitch(), -89.0f, 89.0f);

				_primaryCamera->updateFlyView();
			}


			_currentRotation = QQuaternion::fromRotationMatrix(_primaryCamera->getViewMatrix().toGenericMatrix<3, 3>());
			_leftButtonPoint = downPoint;
			setCursor(QCursor(QPixmap(":/icons/res/rotatecursor.png")));
			_viewMode = ViewMode::NONE;

			const float maxInertiaVelocity = 10.0f; // Adjust as needed
			if (dt > 0) {
				_inertiaRotateVelocity = -QVector2D(delta) / dt;
				if (_inertiaRotateVelocity.length() > maxInertiaVelocity)
					_inertiaRotateVelocity = _inertiaRotateVelocity.normalized() * maxInertiaVelocity;
			}
		}

		update();
	}
	else if (e->buttons() == Qt::RightButton && !(e->modifiers() & Qt::ControlModifier) &&
		(_primaryCamera->getMode() == GLCamera::CameraMode::Fly ||
		 _primaryCamera->getMode() == GLCamera::CameraMode::FirstPerson) &&
		!isGltfCameraActive())
	{
		// Free-look in Fly/FP mode: RMB drag rotates the view via yaw/pitch
		QPoint look = _rightButtonPoint - downPoint;
		_primaryCamera->getYaw()   += look.x() * 0.2f;
		_primaryCamera->getPitch() += look.y() * 0.2f;

		if (_primaryCamera->getMode() == GLCamera::CameraMode::FirstPerson)
			_primaryCamera->getPitch() = std::clamp(_primaryCamera->getPitch(), -60.0f, 60.0f);
		else
			_primaryCamera->getPitch() = std::clamp(_primaryCamera->getPitch(), -89.0f, 89.0f);

		_primaryCamera->updateFlyView();
		_currentRotation = QQuaternion::fromRotationMatrix(_primaryCamera->getViewMatrix().toGenericMatrix<3, 3>());
		_rightButtonPoint = downPoint;
		setCursor(QCursor(QPixmap(":/icons/res/rotatecursor.png")));

		if (dt > 0) {
			_inertiaRotateVelocity = -QVector2D(look) / dt;
			const float maxVel = 10.0f;
			if (_inertiaRotateVelocity.length() > maxVel)
				_inertiaRotateVelocity = _inertiaRotateVelocity.normalized() * maxVel;
		}

		update();
	}
	else if (((e->buttons() == Qt::RightButton && e->modifiers() & Qt::ControlModifier) || (e->buttons() == Qt::LeftButton && _viewPanning)) && !isGltfCameraActive())
	{
		if (_displayedObjectsMemSize > MAX_MODEL_SIZE_BYTES)
			_lowResEnabled = true;
		setSectionCapsInteractionSuppressed(true);
		QVector3D OP = get3dTranslationVectorFromMousePoints(downPoint, _rightButtonPoint);
		_primaryCamera->move(OP.x(), OP.y(), OP.z());
		_currentTranslation = _primaryCamera->getPosition();

		_rightButtonPoint = downPoint;
		setCursor(QCursor(QPixmap(":/icons/res/pancursor.png")));

		// Clamp pan inertia velocity
		const float maxPanInertiaVelocity = 20.0f; // Adjust as needed
		if (dt > 0) {
			_inertiaPanVelocity = QVector2D(delta) / dt;
			if (_inertiaPanVelocity.length() > maxPanInertiaVelocity)
				_inertiaPanVelocity = _inertiaPanVelocity.normalized() * maxPanInertiaVelocity;

			_inertiaZoomPanVelocity = OP;
		}

		update();
	}
	else if (((e->buttons() == Qt::MiddleButton && e->modifiers() & Qt::ControlModifier) || (e->buttons() == Qt::LeftButton && _viewZooming)) && !isGltfCameraActive())
	{
		if (_displayedObjectsMemSize > MAX_MODEL_SIZE_BYTES)
			_lowResEnabled = true;
		setSectionCapsInteractionSuppressed(true);
		// Zoom
		if (downPoint.x() > _middleButtonPoint.x() || downPoint.y() < _middleButtonPoint.y()) {
			_viewRange /= 1.05f;
			_lastZoomDirection = 1;
		}
		else {
			_viewRange *= 1.05f;
			_lastZoomDirection = -1;
		}
		
		// Perspective: floor is the focused sub-mesh radius × 0.5 so the orbit
		// distance stays outside whatever mesh is currently fully in view.
		// As the user zooms in, large meshes leave the frustum and the floor
		// shrinks automatically to match the focused geometry.
		// Ortho: the near/far floor in GLCamera handles tearing without any
		// zoom restriction, so only a minimal absolute floor is applied.
		updateZoomInLimit();
		const float focusRadius = _zoomInLimit;
		const float minVR = (_projection == ViewProjection::PERSPECTIVE)
		    ? std::max(focusRadius * 0.5f, 0.0001f)
		    : std::max(focusRadius / 100.0f, 0.00001f);
		if (_viewRange < minVR)
			_viewRange = minVR;
		if (_viewRange > _boundingSphere.getRadius() * 100.0f)
			_viewRange = _boundingSphere.getRadius() * 100.0f;
		_currentViewRange = _viewRange;

		// Translate to focus on mouse center
		QPoint cen = getClientRectFromPoint(downPoint).center();
		float sign = (downPoint.x() > _middleButtonPoint.x() || downPoint.y() < _middleButtonPoint.y()) ? 1.0f : -1.0f;
		QVector3D OP = get3dTranslationVectorFromMousePoints(cen, _middleButtonPoint);
		OP *= sign * 0.05f;
		_primaryCamera->move(OP.x(), OP.y(), OP.z());
		_currentTranslation = _primaryCamera->getPosition();
		_lastZoomPanVector = OP; // Store for inertia

		if (dt > 0) {
			_inertiaZoomVelocity = _lastZoomDirection; // +1 or -1
		}

		resizeGL(width(), height());

		_middleButtonPoint = downPoint;
		setCursor(QCursor(QPixmap(":/icons/res/zoomcursor.png")));

		update();
	}
	else
	{
		_lowResEnabled = false;
	}

	updateViewCubeHover(e->pos(), e->buttons());


	// Auto-hide/show the view toolbar
	if (_viewToolbar && e->buttons() == Qt::NoButton)
	{
		const int revealMargin = 30; // e.g., 30 px threshold

		QRect hidden = _viewToolbar->hiddenRect();
		QRect revealArea(hidden.left(), hidden.top() - revealMargin, hidden.width(), revealMargin * 2);

		if (revealArea.contains(e->pos()) || _viewToolbar->underMouse())
		{
			_viewToolbar->showAnimated();
		}
		else
		{
			// Store the timer as a member (optional) to manage it better
			auto timer = new QTimer(this);
			timer->setSingleShot(true);
			connect(timer, &QTimer::timeout, this, [this, timer]() {
				if (!_viewToolbar)
				{
					timer->deleteLater(); // Clean up the timer
					return; // Exit safely
				}

				QPoint globalPos = QCursor::pos();
				QPoint localPos = mapFromGlobal(globalPos);
				QRect hidden = _viewToolbar->hiddenRect();
				QRect revealArea(hidden.left(), hidden.top() - 30, hidden.width(), 60);

				bool isFlyoutVisible = _viewToolbar->isFlyoutMenuVisible();

				if (!revealArea.contains(localPos) &&
					!_viewToolbar->underMouse() &&
					!isFlyoutVisible)
				{
					_viewToolbar->hideAnimated();
				}

				timer->deleteLater(); // Clean up the timer
				});

			// Start the timer
			timer->start(2000);

			// Ensure proper cleanup of the timer if the toolbar is deleted
			connect(_viewToolbar, &QObject::destroyed, timer, [timer]() {
				timer->stop();
				timer->deleteLater();
				});
		}
	}

	// Hover highlight feedback for the transform gizmo.
	bool gizmoHovered = false;
	if (e->buttons() == Qt::NoButton)
	{
		const BoundingSphere selectionSphere = computeTransformGizmoSelectionSphere();
		const float selectionRadius = selectionSphere.getRadius() > 0.0f
			? selectionSphere.getRadius()
			: _boundingSphere.getRadius();
		const float gizmoScale = (std::max)(selectionRadius * 0.9f, 0.01f);
		if (!(e->modifiers() & Qt::ControlModifier) &&
			_transformGizmoRequested && _transformGizmo &&
			_transformGizmo->updateHover(e->pos(), _primaryCamera, _viewMatrix, _projectionMatrix,
				QRect(0, 0, width(), height()), gizmoScale))
		{
			gizmoHovered = true;
			update();
		}
		else if (_transformGizmo)
		{
			const bool hadHover = (_transformGizmo->hoveredHandle() != TransformGizmo::Handle::None);
			_transformGizmo->updateHover(QPoint(-1, -1), _primaryCamera, _viewMatrix, _projectionMatrix,
				QRect(0, 0, width(), height()), gizmoScale);
			if (hadHover)
				update();
		}
	}

	// Hover highlight feedback (visual preview, independent of actual selection)
	if (e->buttons() == Qt::NoButton && _selectionManager->getHoverMode() != HoverHighlightMode::Disabled)
	{
		if (!gizmoHovered && (!_showViewCubeOverride || !viewCubeScreenRect().contains(e->pos())))
		{
			// Compute hovered mesh (SelectionManager will emit hoverChanged signal if it changed)
			_selectionManager->hoverSelect(e->pos());
		}
	}

	update();

	_lastMousePos  = currentPos;
	_lastMouseTime = currentTime;
}

void GLWidget::wheelEvent(QWheelEvent* e)
{
	// Stop any ongoing inertia when wheel zooming
	_inertiaRotateVelocity = QVector2D(0, 0);
	_inertiaPanVelocity = QVector2D(0, 0);
	_inertiaZoomVelocity = 0.0f;
	if (_inertiaTimer && _inertiaTimer->isActive())
		_inertiaTimer->stop();

	// Scroll-wheel zoom is disabled when a glTF camera is active (read-only view).
	if (isGltfCameraActive())
		return;

	if (_displayedObjectsMemSize > MAX_MODEL_SIZE_BYTES)
		_lowResEnabled = true;
	setSectionCapsInteractionSuppressed(true);

	if (_primaryCamera->getMode() == GLCamera::CameraMode::Fly ||
		_primaryCamera->getMode() == GLCamera::CameraMode::FirstPerson)
	{
		QPoint numDegrees = e->angleDelta() / 8;
		QPoint numSteps = numDegrees / 30;
		float zoomStep = numSteps.y();
		if (zoomStep != 0.0f)
		{
			const float moveDist = _viewRange * 0.08f * std::abs(zoomStep);
			_primaryCamera->moveForward(zoomStep > 0.0f ? moveDist : -moveDist);
			_currentTranslation = _primaryCamera->getPosition();
			_inertiaZoomVelocity = 0.0f;
			resizeGL(width(), height());
			update();
		}
		return;
	}

	// Zoom
	QPoint numDegrees = e->angleDelta() / 8;
	QPoint numSteps = numDegrees / 30;
	float zoomStep = numSteps.y();
	float zoomFactor = abs(zoomStep) + 0.05;
	const float oldViewRange = _viewRange;

	if (zoomStep < 0)
		_viewRange *= zoomFactor;
	else
		_viewRange /= zoomFactor;

	{
		updateZoomInLimit();
		const float focusRadius = _zoomInLimit;
		const float minVR = (_projection == ViewProjection::PERSPECTIVE)
		    ? std::max(focusRadius * 0.5f, 0.0001f)
		    : std::max(focusRadius / 100.0f, 0.00001f);
		if (_viewRange < minVR)
			_viewRange = minVR;
		if (_viewRange > _boundingSphere.getRadius() * 100.0f)
			_viewRange = _boundingSphere.getRadius() * 100.0f;
	}

	_currentViewRange = _viewRange;

	// Translate to focus on mouse center
	QPoint cen = getClientRectFromPoint(e->position().toPoint()).center();
	QVector3D OP = get3dTranslationVectorFromMousePoints(cen, e->position().toPoint());
	const float rangeScale = (oldViewRange > 0.0f) ? (_viewRange / oldViewRange) : 1.0f;
	OP *= (1.0f - rangeScale);
	_primaryCamera->move(OP.x(), OP.y(), OP.z());
	_currentTranslation = _primaryCamera->getPosition();

	// Add inertia for wheel zoom
	_inertiaZoomVelocity = (e->angleDelta().y() / 120.0f) * 0.05f; // scale as needed
	if (_inertiaTimer) _inertiaTimer->start();

	_inertiaZoomPanVelocity += OP;

	resizeGL(width(), height());
	update();
}

void GLWidget::keyPressEvent(QKeyEvent* event)
{
	QWidget::keyPressEvent(event);

	const auto key = event->key();

	if (key == Qt::Key_Escape)
	{
		_viewRotating = false;
		_viewPanning = false;
		_viewZooming = false;
		_windowZoomActive = false;
		setCursor(QCursor(Qt::ArrowCursor));
		MainWindow::showStatusMessage("");

		// Deactivate navigation mode buttons in toolbar
		if (_viewToolbar)
			_viewToolbar->deactivateAllNavigationModes();

		_selectedIDs.clear();               // Clear GLWidget's internal list immediately
		_viewer->deselectAllWithUndo();     // Clear viewer selection and push an undo entry
	}
	else if (key == Qt::Key_F)
	{
		fitAll();
	}
	else if (key == Qt::Key_Delete)
		_viewer->deleteSelectedItems();
	else if (key == Qt::Key_Space)
	{
		if (event->modifiers() & Qt::ShiftModifier)
			_viewer->showOnlySelectedItems();
		else
			_visibleSwapped ? _viewer->showSelectedItems() : _viewer->hideSelectedItems();
	}
	else if (key == Qt::Key_S && (event->modifiers() & Qt::AltModifier))
	{
		swapVisible(!_visibleSwapped);
	}
	else
		_keys.insert(key);

	// Camera mode switching
	if (key == Qt::Key_1) setCameraMode(GLCamera::CameraMode::Orbit);
	if (key == Qt::Key_2) setCameraMode(GLCamera::CameraMode::Fly);
	if (key == Qt::Key_3) setCameraMode(GLCamera::CameraMode::FirstPerson);


	update();
}

void GLWidget::keyReleaseEvent(QKeyEvent* event)
{
	_keys.remove(event->key());
	QWidget::keyReleaseEvent(event);
}

void GLWidget::performKeyboardNav()
{
	// Keyboard navigation is disabled when a glTF camera is active (read-only view).
	if (isGltfCameraActive())
		return;

	const Qt::KeyboardModifiers modifiers = QApplication::keyboardModifiers();
	const bool allowGameplayModifiers = (modifiers == Qt::NoModifier || modifiers == Qt::ShiftModifier);

	if (_keys.empty() == false && allowGameplayModifiers)
	{
		const float sceneScale = std::max(_boundingSphere.getRadius(), 0.001f);
		float factor = std::max(sceneScale * 0.02f, _viewRange * 0.01f);
		if (modifiers & Qt::ShiftModifier)
			factor *= 3.0f;

		// https://forum.qt.io/topic/28327/big-issue-with-qt-key-inputs-for-gaming/4
		if (_primaryCamera->getMode() == GLCamera::CameraMode::Fly || _primaryCamera->getMode() == GLCamera::CameraMode::FirstPerson)
		{
			const bool firstPerson = _primaryCamera->getMode() == GLCamera::CameraMode::FirstPerson;
			if (firstPerson)
			{
				if (_keys.contains(Qt::Key_W) || _keys.contains(Qt::Key_Up))
					_primaryCamera->moveForwardPlanar(factor);
				if (_keys.contains(Qt::Key_S) || _keys.contains(Qt::Key_Down))
					_primaryCamera->moveForwardPlanar(-factor);
				if (_keys.contains(Qt::Key_A) || _keys.contains(Qt::Key_Left))
					_primaryCamera->moveAcrossPlanar(-factor);
				if (_keys.contains(Qt::Key_D) || _keys.contains(Qt::Key_Right))
					_primaryCamera->moveAcrossPlanar(factor);
			}
			else
			{
				if (_keys.contains(Qt::Key_W) || _keys.contains(Qt::Key_Up))
					_primaryCamera->moveForward(factor);
				if (_keys.contains(Qt::Key_S) || _keys.contains(Qt::Key_Down))
					_primaryCamera->moveForward(-factor);
				if (_keys.contains(Qt::Key_A) || _keys.contains(Qt::Key_Left))
					_primaryCamera->moveAcross(-factor);
				if (_keys.contains(Qt::Key_D) || _keys.contains(Qt::Key_Right))
					_primaryCamera->moveAcross(factor);
				if (_keys.contains(Qt::Key_Q))
					_primaryCamera->moveWorldUp(-factor);
				if (_keys.contains(Qt::Key_E))
					_primaryCamera->moveWorldUp(factor);
			}
		}
		else
		{
			// Use Orbit-style orthographic nav (as before)
			if (_keys.contains(Qt::Key_A) || _keys.contains(Qt::Key_Left))
				_primaryCamera->moveAcross(factor);
			if (_keys.contains(Qt::Key_D) || _keys.contains(Qt::Key_Right))
				_primaryCamera->moveAcross(-factor);
			if (_keys.contains(Qt::Key_W) || _keys.contains(Qt::Key_Up))
				_primaryCamera->moveUpward(-factor);
			if (_keys.contains(Qt::Key_S) || _keys.contains(Qt::Key_Down))
				_primaryCamera->moveUpward(factor);
		}

		if (_primaryCamera->getMode() == GLCamera::CameraMode::Fly ||
			_primaryCamera->getMode() == GLCamera::CameraMode::FirstPerson)
		{
			bool updatedLook = false;
			if (_keys.contains(Qt::Key_J))
			{
				_primaryCamera->getYaw() += 2.0f;
				updatedLook = true;
			}
			if (_keys.contains(Qt::Key_L))
			{
				_primaryCamera->getYaw() -= 2.0f;
				updatedLook = true;
			}
			if (_keys.contains(Qt::Key_I))
			{
				_primaryCamera->getPitch() += 2.0f;
				updatedLook = true;
			}
			if (_keys.contains(Qt::Key_K))
			{
				_primaryCamera->getPitch() -= 2.0f;
				updatedLook = true;
			}

			if (updatedLook)
			{
				const float pitchLimit = (_primaryCamera->getMode() == GLCamera::CameraMode::FirstPerson) ? 60.0f : 89.0f;
				_primaryCamera->getPitch() = std::clamp(_primaryCamera->getPitch(), -pitchLimit, pitchLimit);
				_primaryCamera->updateFlyView();
			}
		}
		else
		{
			if (_keys.contains(Qt::Key_J))
				_primaryCamera->rotateY(2.0f);
			if (_keys.contains(Qt::Key_L))
				_primaryCamera->rotateY(-2.0f);
			if (_keys.contains(Qt::Key_I))
				_primaryCamera->rotateX(2.0f);
			if (_keys.contains(Qt::Key_K))
				_primaryCamera->rotateX(-2.0f);
			if (_keys.contains(Qt::Key_M))
				_primaryCamera->rotateZ(2.0f);
			if (_keys.contains(Qt::Key_N))
				_primaryCamera->rotateZ(-2.0f);
		}
		if (_keys.contains(Qt::Key_X) || _keys.contains(Qt::Key_Z))
		{
			if(_primaryCamera->getMode() == GLCamera::CameraMode::Orbit)
			{
				// Zoom only if Orbit camera mode
				if (_keys.contains(Qt::Key_X))
					_viewRange /= 1.05f;
				else
					_viewRange *= 1.05f;
				{
					updateZoomInLimit();
		const float focusRadius = _zoomInLimit;
					const float minVR = (_projection == ViewProjection::PERSPECTIVE)
					    ? std::max(focusRadius * 0.5f, 0.0001f)
					    : std::max(focusRadius / 100.0f, 0.00001f);
					if (_viewRange < minVR) _viewRange = minVR;
					if (_viewRange > _boundingSphere.getRadius() * 100.0f)
						_viewRange = _boundingSphere.getRadius() * 100.0f;
				}
				// Translate to focus on mouse center
				QPoint pos = mapFromGlobal(QCursor::pos());
				QPoint cen = getClientRectFromPoint(pos).center();
				float sign = (pos.x() > cen.x() || pos.y() < cen.y() ||
					(pos.x() < cen.x() && pos.y() > cen.y())) && _keys.contains(Qt::Key_Q) ? 1.0f : -1.0f;
				QVector3D OP = get3dTranslationVectorFromMousePoints(cen, pos);
				OP *= sign * 0.05f;
				_primaryCamera->move(OP.x(), OP.y(), OP.z());
			}
		}

		_currentViewRange = _viewRange;
		_currentTranslation = _primaryCamera->getPosition();
		_currentRotation = QQuaternion::fromRotationMatrix(_primaryCamera->getViewMatrix().toGenericMatrix<3, 3>());
		resizeGL(width(), height());
		update();
	}
}

void GLWidget::animateViewChange()
{
	setSectionCapsInteractionSuppressed(true);
	if (_displayedObjectsMemSize > MAX_MODEL_SIZE_BYTES)
		_lowResEnabled = true;
	if (_customViewAnimationActive)
	{
		animateToRotation(_customViewTargetRotation);
		resizeGL(width(), height());
		return;
	}
	if (_viewMode == ViewMode::TOP)
	{
		setRotations(0.0f, 0.0f, 0.0f);
	}
	if (_viewMode == ViewMode::BOTTOM)
	{
		setRotations(0.0f, 180.0f, 0.0f);
	}
	if (_viewMode == ViewMode::LEFT)
	{
		setRotations(0.0f, -90.0f, 90.0f);
	}
	if (_viewMode == ViewMode::RIGHT)
	{
		setRotations(0.0f, -90.0f, -90.0f);
	}
	if (_viewMode == ViewMode::FRONT)
	{
		setRotations(0.0f, -90.0f, 0.0f);
	}
	if (_viewMode == ViewMode::BACK)
	{
		setRotations(0.0f, -90.0f, 180.0f);
	}
	if (_viewMode == ViewMode::ISOMETRIC)
	{
        setRotations(-45.0f, -54.7356f, 0.0f);
	}
	if (_viewMode == ViewMode::DIMETRIC)
	{
        setRotations(-20.7048f, -70.5288f, 0.0f);
	}
	if (_viewMode == ViewMode::TRIMETRIC)
	{
        setRotations(-30.0f, -55.0f, 0.0f);
	}

	resizeGL(width(), height());
}

void GLWidget::animateFitAll()
{
	setSectionCapsInteractionSuppressed(true);
	if (_displayedObjectsMemSize > MAX_MODEL_SIZE_BYTES)
		_lowResEnabled = true;

	setZoomAndPan(_viewBoundingSphereDia, -_currentTranslation + _boundingSphere.getCenter());
	//fitBoxToScreen(_boundingBox);

	resizeGL(width(), height());
}

void GLWidget::animateWindowZoom()
{
	setSectionCapsInteractionSuppressed(true);
	if (_displayedObjectsMemSize > MAX_MODEL_SIZE_BYTES)
		_lowResEnabled = true;
	setZoomAndPan(_currentViewRange / _rubberBandZoomRatio, _rubberBandPan);
	resizeGL(width(), height());
}

void GLWidget::animateCenterScreen()
{
	setSectionCapsInteractionSuppressed(true);
	setZoomAndPan(_selectionBoundingSphere.getRadius() * 2, -_currentTranslation + _selectionBoundingSphere.getCenter());
	resizeGL(width(), height());
}

void GLWidget::onInertiaTimer()
{
	// Inertia effects are suppressed when a glTF camera is active (read-only view).
	if (isGltfCameraActive())
		return;

	bool active = false;

	// --- Pan inertia ---
	if (_inertiaPanVelocity.lengthSquared() > 0.01f) {
		// Apply pan inertia from the last pan point, in the same way as interactive panning
		QPointF panDelta(-_inertiaPanVelocity.x(), -_inertiaPanVelocity.y());
		QPoint newPanPoint = _lastPanPoint + panDelta.toPoint();
		QVector3D OP = get3dTranslationVectorFromMousePoints(_lastPanPoint, newPanPoint);
		_primaryCamera->move(OP.x(), OP.y(), OP.z());
		_currentTranslation = _primaryCamera->getPosition();
		_lastPanPoint = newPanPoint; // Update for next frame
		_inertiaPanVelocity *= _inertiaDamping;
		active = true;
	}

	// --- Zoom inertia ---
	if (std::abs(_inertiaZoomVelocity) > 0.001f) {
		float zoomFactor = 1.005f;
		if (_inertiaZoomVelocity > 0)
			_viewRange /= zoomFactor;
		else
			_viewRange *= zoomFactor;

		QPoint cen = getViewportFromPoint(mapFromGlobal(QCursor::pos())).center();
		QVector3D OP = get3dTranslationVectorFromMousePoints(cen, cen);
		OP *= -_inertiaZoomPanVelocity * 0.05f;
		_primaryCamera->move(OP.x(), OP.y(), OP.z());
		_currentTranslation = _primaryCamera->getPosition();

		// Decay inertia
		_inertiaZoomVelocity *= _inertiaDamping * 0.1f;
		_inertiaZoomPanVelocity *= _inertiaDamping * 0.1f;

		if (std::abs(_inertiaZoomVelocity) > 0.001f)
			active = true;
		else
			_inertiaZoomVelocity = 0.0f;

		resizeGL(width(), height());

		updateZoomInLimit();
		const float focusRadius = _zoomInLimit;
		const float minRange = (_projection == ViewProjection::PERSPECTIVE)
		    ? std::max(focusRadius * 0.5f, 0.0001f)
		    : std::max(focusRadius / 100.0f, 0.00001f);
		const float maxRange = _boundingSphere.getRadius() * 100.0f;
		if (_viewRange < minRange) _viewRange = minRange;
		if (_viewRange > maxRange) _viewRange = maxRange;
		_currentViewRange = _viewRange;

		active = true;
	}

	// --- Rotation inertia ---
	if (_inertiaRotateVelocity.lengthSquared() > 0.01f) {
		if (_primaryCamera->getMode() == GLCamera::CameraMode::Fly ||
		    _primaryCamera->getMode() == GLCamera::CameraMode::FirstPerson)
		{
			_primaryCamera->getYaw()   += _inertiaRotateVelocity.x() / 2.0f;
			_primaryCamera->getPitch() += _inertiaRotateVelocity.y() / 2.0f;
			if (_primaryCamera->getMode() == GLCamera::CameraMode::FirstPerson)
				_primaryCamera->getPitch() = std::clamp(_primaryCamera->getPitch(), -60.0f, 60.0f);
			else
				_primaryCamera->getPitch() = std::clamp(_primaryCamera->getPitch(), -89.0f, 89.0f);
			_primaryCamera->updateFlyView();
		}
		else
		{
			_primaryCamera->rotateX(_inertiaRotateVelocity.y() / 2.0);
			_primaryCamera->rotateY(_inertiaRotateVelocity.x() / 2.0);
		}
		_currentRotation = QQuaternion::fromRotationMatrix(_primaryCamera->getViewMatrix().toGenericMatrix<3, 3>());
		_inertiaRotateVelocity *= _inertiaDamping;
		active = true;
	}

	if (!active) {
		_inertiaTimer->stop();
		_inertiaPanVelocity = QVector2D();
		_inertiaZoomVelocity = 0.0f;
		_inertiaRotateVelocity = QVector2D();
		QTimer::singleShot(100, this, &GLWidget::disableSectionCapsInteractionSuppression);
	}

	update();
}

void GLWidget::stopAnimations()
{
	_animateViewTimer->stop();
	_animateFitAllTimer->stop();
	_animateWindowZoomTimer->stop();
	_animateCenterScreenTimer->stop();
	_keyboardNavTimer->start();
	QTimer::singleShot(100, this, &GLWidget::disableLowRes);
	QTimer::singleShot(100, this, &GLWidget::disableSectionCapsInteractionSuppression);
}


// Note: convertClickToRay is now implemented in SelectionManager
// Keeping getViewportFromPoint and getClientRectFromPoint as they're still used by GLWidget

GLCamera* GLWidget::getCameraForPoint(const QPoint& pixel)
{
	if (!_multiViewActive)
		return _primaryCamera;

	// Determine which ortho view contains this pixel (same quadrant logic as getViewportFromPoint).
	// Isometric viewport (bottom-right) uses the primary camera unchanged.
	ViewMode viewMode;
	if (pixel.x() < width() / 2 && pixel.y() > height() / 2)
		viewMode = ViewMode::TOP;
	else if (pixel.x() < width() / 2 && pixel.y() <= height() / 2)
		viewMode = ViewMode::FRONT;
	else if (pixel.x() >= width() / 2 && pixel.y() < height() / 2)
		viewMode = ViewMode::LEFT;
	else
		return _primaryCamera; // Isometric viewport

	// Configure the shared ortho camera exactly as the matching pane was rendered.
	const std::vector<QVector3D> multiViewCorners = collectVisibleCorners();
	const QVector3D sharedMultiViewCenter = _primaryCamera->getPosition();
	const float zoomScale = (_viewBoundingSphereDia > 0.0f)
		? (_viewRange / _viewBoundingSphereDia) : 1.0f;
	configureOrthoSubviewCamera(
		viewMode,
		multiViewCorners,
		width() / 2,
		height() / 2,
		sharedMultiViewCenter,
		computeSharedOrthographicMultiViewRange(multiViewCorners, width() / 2, height() / 2, sharedMultiViewCenter) * zoomScale);
	return _orthoViewsCamera;
}

QRect GLWidget::getViewportFromPoint(const QPoint& pixel)
{
	QRect viewport;
	if (_multiViewActive)
	{
		// top view
		if (pixel.x() < width() / 2 && pixel.y() > height() / 2)
			viewport = QRect(0, 0, width() / 2, height() / 2);
		// front view
		else if (pixel.x() < width() / 2 && pixel.y() <= height() / 2)
			viewport = QRect(0, height() / 2, width() / 2, height() / 2);
		// left view
		else if (pixel.x() >= width() / 2 && pixel.y() < height() / 2)
			viewport = QRect(width() / 2, height() / 2, width() / 2, height() / 2);
		// isometric (also catches pixels exactly on the dividing lines)
		else
			viewport = QRect(width() / 2, 0, width() / 2, height() / 2);
	}
	else
	{
		// single viewport
		viewport = QRect(0, 0, width(), height());
	}

	return viewport;
}

QRect GLWidget::getClientRectFromPoint(const QPoint& pixel)
{
	QRect clientRect;
	if (_multiViewActive)
	{
		// top view
		if (pixel.x() < width() / 2 && pixel.y() > height() / 2)
			clientRect = QRect(0, height() / 2, width() / 2, height() / 2);
		// front view
		else if (pixel.x() < width() / 2 && pixel.y() <= height() / 2)
			clientRect = QRect(0, 0, width() / 2, height() / 2);
		// left view
		else if (pixel.x() >= width() / 2 && pixel.y() < height() / 2)
			clientRect = QRect(width() / 2, 0, width() / 2, height() / 2);
		// isometric (also catches pixels exactly on the dividing lines)
		else
			clientRect = QRect(width() / 2, height() / 2, width() / 2, height() / 2);
	}
	else
	{
		// single viewport
		clientRect = QRect(0, 0, width(), height());
	}

	return clientRect;
}


QVector3D GLWidget::get3dTranslationVectorFromMousePoints(const QPoint& start, const QPoint& end)
{
	if (width() <= 0 || height() <= 0)
		return QVector3D(0, 0, 0);

	auto clampPointToRect = [](const QPoint& point, const QRect& rect) {
		if (rect.width() <= 0 || rect.height() <= 0)
			return point;

		return QPoint(
			std::clamp(point.x(), rect.left(), rect.right()),
			std::clamp(point.y(), rect.top(), rect.bottom()));
	};

	// Determine viewport and camera
	const QRect widgetRect(0, 0, width(), height());
	const QPoint safeStart = clampPointToRect(start, widgetRect);
	QRect viewport = _navigationViewportLocked ? _navigationLockedViewport : getViewportFromPoint(safeStart);
	QRect clientRect = _navigationViewportLocked ? _navigationLockedClientRect : getClientRectFromPoint(safeStart);
	if (viewport.width() <= 0 || viewport.height() <= 0)
		return QVector3D(0, 0, 0);

	const QPoint clampedStart = clampPointToRect(safeStart, clientRect);
	const QPoint clampedEnd = clampPointToRect(clampPointToRect(end, widgetRect), clientRect);
	GLCamera* camera = _multiViewActive && (viewport.x() != viewport.width() || viewport.y() != 0)
		? _orthoViewsCamera
		: _primaryCamera;

	QVector3D viewCenter = (camera->getMode() == GLCamera::CameraMode::Orbit)
		? camera->getPosition()
		: _boundingSphere.getCenter();
	// Get view and projection matrices
	QMatrix4x4 view = camera->getViewMatrix();
	QMatrix4x4 projection = camera->getProjectionMatrix();
	QMatrix4x4 inv = (projection * view).inverted();

	if (camera->getProjectionType() == GLCamera::ProjectionType::ORTHOGRAPHIC) {
		const float viewRange = std::max(camera->getViewRange(), 0.0001f);
		const float viewportWidth = static_cast<float>(std::max(viewport.width(), 1));
		const float viewportHeight = static_cast<float>(std::max(viewport.height(), 1));

		float halfWidth = 0.0f;
		float halfHeight = 0.0f;
		if (viewportWidth <= viewportHeight)
		{
			halfWidth = viewRange * 0.5f;
			halfHeight = halfWidth * (viewportHeight / viewportWidth);
		}
		else
		{
			halfHeight = viewRange * 0.5f;
			halfWidth = halfHeight * (viewportWidth / viewportHeight);
		}

		const float unitsPerPixelX = (2.0f * halfWidth) / viewportWidth;
		const float unitsPerPixelY = (2.0f * halfHeight) / viewportHeight;
		const float dxPixels = static_cast<float>(clampedEnd.x() - clampedStart.x());
		const float dyPixels = static_cast<float>(clampedEnd.y() - clampedStart.y());

		const QVector3D right = camera->getRightVector().normalized();
		const QVector3D up = camera->getUpVector().normalized();
		return right * (dxPixels * unitsPerPixelX) - up * (dyPixels * unitsPerPixelY);
	}
	else {
		auto sampleSceneDepth = [&](const QPoint& point) {
			makeCurrent();

			float rawDepth = 1.0f;
			const QPoint clampedPoint = clampPointToRect(point, viewport);
			const int cx = clampedPoint.x();
			const int cy = height() - clampedPoint.y() - 1;
			glReadPixels(cx, cy, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &rawDepth);

			if (rawDepth >= 1.0f)
			{
				const int halfGrid = 4;
				const int x0 = std::max(0, cx - halfGrid);
				const int y0 = std::max(0, cy - halfGrid);
				const int x1 = std::min(width() - 1, cx + halfGrid);
				const int y1 = std::min(height() - 1, cy + halfGrid);
				const int sw = x1 - x0 + 1;
				const int sh = y1 - y0 + 1;
				if (sw > 0 && sh > 0)
				{
					std::vector<float> depthBuf(sw * sh, 1.0f);
					glReadPixels(x0, y0, sw, sh, GL_DEPTH_COMPONENT, GL_FLOAT, depthBuf.data());

					float minDepth = 1.0f;
					for (float d : depthBuf)
					{
						if (d < minDepth)
							minDepth = d;
					}

					if (minDepth < 1.0f)
						rawDepth = minDepth;
				}
			}

			if (rawDepth >= 1.0f)
			{
				const QVector3D projectedCenter = viewCenter.project(view, projection, viewport);
				rawDepth = projectedCenter.z();
			}

			return rawDepth;
		};

		const float depthZ = sampleSceneDepth(clampedStart);
		QVector3D worldStart(clampedStart.x(), height() - clampedStart.y(), depthZ);
		QVector3D worldEnd(clampedEnd.x(), height() - clampedEnd.y(), depthZ);

		const QVector3D startWorld = worldStart.unproject(view, projection, viewport);
		const QVector3D endWorld = worldEnd.unproject(view, projection, viewport);
		return endWorld - startWorld;
	}
}


unsigned int GLWidget::loadTextureFromFile(
	const char* path,
	GLenum wrapS, GLenum wrapT,
	GLenum minFilter, GLenum magFilter,
	bool flipY)
{
	GLuint textureID = 0;

	// Load image using Qt
	QImageReader reader(path);
	reader.setAutoTransform(true); // respects EXIF orientation

	QImage image = reader.read();
	if (image.isNull())
	{
		qWarning() << "Texture failed to load:" << path
			<< reader.errorString();
		return 0;
	}

	// Optional vertical flip (OpenGL vs Qt coordinate difference)
	if (flipY)
		image = image.mirrored(false, true);

	GLenum internalFormat = GL_RGBA8;
	GLenum dataFormat = GL_RGBA;
	GLenum dataType = GL_UNSIGNED_BYTE;

	QImage glImage;

	switch (image.format())
	{
	case QImage::Format_RGB888:
		glImage = image;
		internalFormat = GL_RGB8;
		dataFormat = GL_RGB;
		break;

	case QImage::Format_RGBA8888:
	case QImage::Format_RGBA8888_Premultiplied:
		glImage = image;
		internalFormat = GL_RGBA8;
		dataFormat = GL_RGBA;
		break;

	case QImage::Format_Grayscale8:
		// Expand to RGBA so all three colour channels are populated.
		// Uploading as GL_RED leaves G and B at 0, making the texture appear red.
		glImage = image.convertToFormat(QImage::Format_RGBA8888);
		internalFormat = GL_RGBA8;
		dataFormat = GL_RGBA;
		break;

	case QImage::Format_Indexed8:
		glImage = image.convertToFormat(QImage::Format_RGBA8888);
		internalFormat = GL_RGBA8;
		dataFormat = GL_RGBA;
		break;

	default:
		// Fallback for uncommon formats (ARGB32, RGB32, etc.)
		glImage = image.convertToFormat(QImage::Format_RGBA8888);
		internalFormat = GL_RGBA8;
		dataFormat = GL_RGBA;
		break;
	}

	// Create OpenGL texture
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);

	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		internalFormat,
		glImage.width(),
		glImage.height(),
		0,
		dataFormat,
		dataType,
		glImage.constBits()
	);

	glGenerateMipmap(GL_TEXTURE_2D);

	// Texture parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);

	// Anisotropic filtering (if supported)
	bool hasAnisotropy =
		context()->hasExtension("GL_EXT_texture_filter_anisotropic");

	if (hasAnisotropy)
	{
		GLfloat maxAniso = 0.0f;
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);

		GLfloat aniso = qMin(_anisotropicFilteringLevel, maxAniso);

		glTexParameterf(
			GL_TEXTURE_2D,
			GL_TEXTURE_MAX_ANISOTROPY_EXT,
			aniso
		);
	}

	glBindTexture(GL_TEXTURE_2D, 0);
	return textureID;
}

QList<int> GLWidget::sweepSelect(const QPoint& pixel, bool addToSelection)
{
	const auto& ids = _visibleSwapped ? _hiddenObjectsIds : _displayedObjectsIds;

	// Check if there's actually a rubber band with non-null geometry (actual drag)
	bool hasRubberBand = !ids.empty() && _rubberBand && !_rubberBand->geometry().isNull();

	// If no rubber band, return without modifying selection (click without drag case)
	if (!hasRubberBand) {
		return _selectedIDs;
	}

	// Only clear selection if NOT adding to existing selection (Shift not held)
	if (!addToSelection) {
		_selectedIDs.clear();  // Regular sweep: replace selection
	}
	// If Shift is held (addToSelection=true), DON'T clear - preserve existing selection

	const QRect rubberRect = _rubberBand->geometry();
	const QRect viewport(0, 0, width(), height());
	const QMatrix4x4 projMatrix = _projectionMatrix;
	const QMatrix4x4 viewMatrix = _viewMatrix * _modelMatrix;
	constexpr float SELECTION_THRESHOLD = 0.5f;

	QApplication::setOverrideCursor(Qt::WaitCursor);
	_selectedIDs.reserve(ids.size());

	for (int i : ids) // Iterate through each ID in the 'ids' collection.
	{
		TriangleMesh* mesh = _meshStore.at(i);

		BoundingSphere sphere = mesh->getBoundingSphere();
		QVector3D center = sphere.getCenter();
		float radius = sphere.getRadius();

		// Convert the 3D center into a 4D vector (homogeneous coordinates).
		QVector4D center4(center, 1.0f);
		// Project the center from 3D object space into clip space.
		QVector4D projectedCenter = projMatrix * viewMatrix * center4;

		// Check if the projected center is behind the camera (negative w-coordinate).
		if (projectedCenter.w() <= 0.0f)
			continue;

		// Convert the center from clip space to normalized device coordinates (NDC).
		QVector3D ndcCenter = projectedCenter.toVector3DAffine();
		QPointF screenCenter(
			(ndcCenter.x() * 0.5f + 0.5f) * viewport.width(), // Map NDC to screen coordinates along the X-axis.
			(1.0f - (ndcCenter.y() * 0.5f + 0.5f)) * viewport.height() // Map NDC to screen coordinates along the Y-axis (invert Y for screen space).
		);

		// Project the edge of the bounding sphere in the X direction to determine its radius in screen space.
		QVector4D edge4 = projMatrix * viewMatrix * QVector4D(center + QVector3D(radius, 0, 0), 1.0f);
		if (edge4.w() <= 0.0f) // Check if the projected edge is behind the camera.
			continue;

		// Convert the edge from clip space to normalized device coordinates (NDC).
		QVector3D ndcEdge = edge4.toVector3DAffine();
		QPointF screenEdge(
			(ndcEdge.x() * 0.5f + 0.5f) * viewport.width(), // Map NDC to screen coordinates along the X-axis for the edge.
			(1.0f - (ndcEdge.y() * 0.5f + 0.5f)) * viewport.height() // Map NDC to screen coordinates along the Y-axis for the edge.
		);

		// Calculate the radius in pixels based on the distance between the center and edge in screen space.
		float radiusPixels = QLineF(screenCenter, screenEdge).length();
		QRectF projectedRect(
			screenCenter.x() - radiusPixels, // Top-left X coordinate of the rectangle.
			screenCenter.y() - radiusPixels, // Top-left Y coordinate of the rectangle.
			2 * radiusPixels, // Width of the rectangle (2 * radius).
			2 * radiusPixels  // Height of the rectangle (2 * radius).
		);

		// Check if the projected rectangle is completely within the rubber rectangle.
		if (rubberRect.contains(projectedRect.toRect()))
		{
			// Add ID if not already selected (avoid duplicates)
			if (!_selectedIDs.contains(i))
				_selectedIDs.push_back(i);
		}
		else if (rubberRect.intersects(projectedRect.toRect())) // Check if the projected rectangle intersects the rubber rectangle.
		{
			QRectF intersected = rubberRect.intersected(projectedRect.toRect()); // Calculate the intersection rect between the two rectangles.
			float intersectArea = intersected.width() * intersected.height(); // Compute the area of the intersection.
			float projectedArea = projectedRect.width() * projectedRect.height(); // Compute the area of the projected rectangle.

			// Select the ID if the intersection area is significant enough.
			if (projectedArea > 0 && (intersectArea / projectedArea) >= SELECTION_THRESHOLD) {
				// Add ID if not already selected (avoid duplicates)
				if (!_selectedIDs.contains(i))
					_selectedIDs.push_back(i);
			}
		}
	}

	QApplication::restoreOverrideCursor();

	// Sync SelectionManager internal state without emitting signal (avoids feedback loops)
	_selectionManager->syncSelectedIds(_selectedIDs);

	// Emit sweepSelectionDone with the complete accumulated selection
	emit sweepSelectionDone(_selectedIDs);

	return _selectedIDs;
}



unsigned int GLWidget::colorToIndex(const QColor& color)
{
	int alpha = color.alpha();
	int red = color.red();
	int green = color.green();
	int blue = color.blue();
	unsigned int index =  ((alpha << 24) | (red << 16) | (green << 8) | (blue));
	return index;
}

QColor GLWidget::indexToColor(const unsigned int& index)
{
	int red = ((index >> 16) & 0xFF);
	int green = ((index >> 8) & 0xFF);
	int blue = (index & 0xFF);
	int alpha = ((index >> 24) & 0xFF);
	return QColor(red, green, blue, alpha);
}

void GLWidget::setView(QVector3D viewPos, QVector3D viewDir, QVector3D upDir, QVector3D rightDir)
{
	_primaryCamera->setView(viewPos, viewDir, upDir, rightDir);
	emit viewSet();
}

// Collect a representative set of world-space vertex positions for every
// visible mesh.  Using actual vertices (sampled for large meshes) gives a
// genuinely tight projected silhouette — no phantom corners that arise when
// an AABB combines, say, the maximum-X from the arm tip with the maximum-Y
// from the lamp body at a point that never exists in the geometry.
// Sampling cap: at most MAX_SAMPLES_PER_MESH positions per mesh so that
// fitting remains fast even for high-poly scenes.
std::vector<QVector3D> GLWidget::collectVisibleCorners() const
{
	constexpr int MAX_SAMPLES_PER_MESH = 1024;

	const auto& ids = _visibleSwapped ? _hiddenObjectsIds : _displayedObjectsIds;
	std::vector<QVector3D> points;
	points.reserve(ids.size() * MAX_SAMPLES_PER_MESH);

	for (int i : ids)
	{
		try
		{
			TriangleMesh* mesh = _meshStore.at(i);
			const std::vector<float>& pts = mesh->getTrsfPoints();
			const int nVerts = static_cast<int>(pts.size()) / 3;

			const QVector3D expOff = mesh->explosionOffset();

			if (nVerts <= 0)
			{
				// Fallback: use the 8 AABB corners if vertex data is absent
				for (const QVector3D& c : mesh->getBoundingBox().getCorners())
					points.push_back(c + expOff);
				continue;
			}

			// Uniform stride so we always inspect ≤ MAX_SAMPLES_PER_MESH vertices
			// while still touching the full extent of the mesh (first + last are
			// always included, then evenly-spaced interior samples).
			const int stride = std::max(1, nVerts / MAX_SAMPLES_PER_MESH);
			for (int j = 0; j < nVerts; j += stride)
			{
				const int b = j * 3;
				points.emplace_back(pts[b] + expOff.x(), pts[b + 1] + expOff.y(), pts[b + 2] + expOff.z());
			}
			// Always include the last vertex so we never miss a boundary point
			if (nVerts > 0)
			{
				const int b = (nVerts - 1) * 3;
				points.emplace_back(pts[b] + expOff.x(), pts[b + 1] + expOff.y(), pts[b + 2] + expOff.z());
			}
		}
		catch (const std::out_of_range&) {}
	}

    // Fallback: if somehow empty, use visible mesh AABBs with explosion offsets.
    if (points.empty())
    {
        for (int i : ids)
        {
            try
            {
                TriangleMesh* mesh = _meshStore.at(i);
                if (!mesh)
                    continue;

                const QVector3D expOff = mesh->explosionOffset();
                for (const QVector3D& c : mesh->getBoundingBox().getCorners())
                    points.push_back(c + expOff);
            }
            catch (const std::out_of_range&) {}
        }
    }

    // Final fallback: scene AABB if no visible mesh points could be gathered.
    if (points.empty())
        return _boundingBox.getCorners();
    return points;
}

// Convenience: read axes from the current view matrix, then delegate.
float GLWidget::computeFitViewRange(QVector3D* outCenter) const
{
	const QMatrix4x4 V = _primaryCamera->getViewMatrix();
	return computeFitViewRange(
		 V.row(0).toVector3D().normalized(),
		 V.row(1).toVector3D().normalized(),
		-V.row(2).toVector3D().normalized(),
		outCenter);
}

// Convenience: collect visible corners, then delegate to the core.
// Used by setViewMode() with the destination quaternion's axes so that
// rotation and zoom can animate concurrently.
float GLWidget::computeFitViewRange(
	const QVector3D& right, const QVector3D& up, const QVector3D& viewDir,
	QVector3D* outCenter) const
{
	return computeFitViewRange(collectVisibleCorners(), right, up, viewDir, outCenter);
}

float GLWidget::computeOrthographicFitViewRangeForViewport(
	const std::vector<QVector3D>& corners,
	const QVector3D& right,
	const QVector3D& up,
	const QVector3D& viewDir,
	int viewportWidth,
	int viewportHeight,
	QVector3D* outCenter,
	const QVector3D& eyePos) const
{
	if (corners.empty())
	{
		if (outCenter) *outCenter = _boundingSphere.getCenter();
		return std::max(_boundingSphere.getRadius() * 2.0f, 0.0001f);
	}

	float xMin_v = std::numeric_limits<float>::max();
	float xMax_v = -std::numeric_limits<float>::max();
	float yMin_v = std::numeric_limits<float>::max();
	float yMax_v = -std::numeric_limits<float>::max();
	float zMin_v = std::numeric_limits<float>::max();
	float zMax_v = -std::numeric_limits<float>::max();

	for (const QVector3D& c : corners)
	{
		// Project relative to eyePos so the range is the actual screen-space extent
		// from the ortho camera's origin (the orbit center / look-at point).
		const QVector3D rel = c - eyePos;
		const float xc = QVector3D::dotProduct(rel, right);
		const float yc = QVector3D::dotProduct(rel, up);
		const float zc = QVector3D::dotProduct(rel, viewDir);
		xMin_v = std::min(xMin_v, xc);  xMax_v = std::max(xMax_v, xc);
		yMin_v = std::min(yMin_v, yc);  yMax_v = std::max(yMax_v, yc);
		zMin_v = std::min(zMin_v, zc);  zMax_v = std::max(zMax_v, zc);
	}

	// The ortho window is symmetric around the eye, so the required half-extent
	// is the farthest corner distance from the eye on each screen axis.
	const float halfX = std::max(std::abs(xMin_v), std::abs(xMax_v));
	const float halfY = std::max(std::abs(yMin_v), std::abs(yMax_v));
	const float cx = (xMin_v + xMax_v) * 0.5f;
	const float cy = (yMin_v + yMax_v) * 0.5f;
	const float cz = (zMin_v + zMax_v) * 0.5f;
	if (outCenter)
		*outCenter = eyePos + right * cx + up * cy + viewDir * cz;

	const int safeWidth = std::max(viewportWidth, 1);
	const int safeHeight = std::max(viewportHeight, 1);
	const float aspect = static_cast<float>(safeWidth) / static_cast<float>(safeHeight);
	constexpr float margin = 1.05f;

	float halfRange;
	if (safeWidth > safeHeight)
		halfRange = std::max(halfX / std::max(aspect, 0.001f), halfY);
	else
		halfRange = std::max(halfX, halfY * aspect);

	return std::max(halfRange * 2.0f * margin, 0.0001f);
}

QVector3D GLWidget::computeVisibleWorldCenter(const std::vector<QVector3D>& corners) const
{
	if (corners.empty())
	{
		return QVector3D(
			static_cast<float>((_boundingBox.xMin() + _boundingBox.xMax()) * 0.5),
			static_cast<float>((_boundingBox.yMin() + _boundingBox.yMax()) * 0.5),
			static_cast<float>((_boundingBox.zMin() + _boundingBox.zMax()) * 0.5));
	}

	float minX = std::numeric_limits<float>::max();
	float minY = std::numeric_limits<float>::max();
	float minZ = std::numeric_limits<float>::max();
	float maxX = -std::numeric_limits<float>::max();
	float maxY = -std::numeric_limits<float>::max();
	float maxZ = -std::numeric_limits<float>::max();

	for (const QVector3D& c : corners)
	{
		minX = std::min(minX, c.x());
		minY = std::min(minY, c.y());
		minZ = std::min(minZ, c.z());
		maxX = std::max(maxX, c.x());
		maxY = std::max(maxY, c.y());
		maxZ = std::max(maxZ, c.z());
	}

	return QVector3D(
		(minX + maxX) * 0.5f,
		(minY + maxY) * 0.5f,
		(minZ + maxZ) * 0.5f);
}

float GLWidget::computeSharedOrthographicMultiViewRange(
	const std::vector<QVector3D>& corners,
	int viewportWidth,
	int viewportHeight,
	const QVector3D& eyePos) const
{
	float sharedRange = 0.0001f;
	for (const ViewMode viewMode : { ViewMode::TOP, ViewMode::FRONT, ViewMode::LEFT })
	{
		QVector3D viewDir;
		QVector3D upDir;
		QVector3D rightDir;
		standardViewBasis(viewMode, viewDir, upDir, rightDir);
		sharedRange = std::max(
			sharedRange,
			computeOrthographicFitViewRangeForViewport(
				corners, rightDir, upDir, viewDir, viewportWidth, viewportHeight, nullptr, eyePos));
	}
	return sharedRange;
}

void GLWidget::configureOrthoSubviewCamera(
	ViewMode viewMode,
	const std::vector<QVector3D>& corners,
	int viewportWidth,
	int viewportHeight,
	const QVector3D& sharedCenter,
	float sharedViewRange)
{
	Q_UNUSED(corners);

	QVector3D viewDir;
	QVector3D upDir;
	QVector3D rightDir;
	standardViewBasis(viewMode, viewDir, upDir, rightDir);

	_orthoViewsCamera->setScreenSize(viewportWidth, viewportHeight);
	_orthoViewsCamera->setViewRange(sharedViewRange);
	_orthoViewsCamera->setSceneRadius(_boundingSphere.getRadius());
	_orthoViewsCamera->setProjectionType(GLCamera::ProjectionType::ORTHOGRAPHIC);
	_orthoViewsCamera->setView(sharedCenter, viewDir, upDir, rightDir);
}

// Core implementation: fits an arbitrary set of world-space corners given
// explicit view axes.  Analytical for both ortho and perspective.
float GLWidget::computeFitViewRange(const std::vector<QVector3D>& corners,
	const QVector3D& right, const QVector3D& up, const QVector3D& viewDir,
	QVector3D* outCenter) const
{
	if (corners.empty())
	{
		if (outCenter) *outCenter = _boundingSphere.getCenter();
		return _boundingSphere.getRadius() * 2.0f;
	}

	// Project every corner onto the view axes using ABSOLUTE dot products.
	// The midpoint of the resulting intervals is the "visual centre" of the
	// scene for this orientation — the point that should appear at screen centre
	// so that equal margins surround the geometry on every side.
	float xMin_v =  std::numeric_limits<float>::max();
	float xMax_v = -std::numeric_limits<float>::max();
	float yMin_v =  std::numeric_limits<float>::max();
	float yMax_v = -std::numeric_limits<float>::max();
	float zMin_v =  std::numeric_limits<float>::max();
	float zMax_v = -std::numeric_limits<float>::max();

	for (const QVector3D& c : corners)
	{
		const float xc = QVector3D::dotProduct(c, right);
		const float yc = QVector3D::dotProduct(c, up);
		const float zc = QVector3D::dotProduct(c, viewDir);
		xMin_v = std::min(xMin_v, xc);  xMax_v = std::max(xMax_v, xc);
		yMin_v = std::min(yMin_v, yc);  yMax_v = std::max(yMax_v, yc);
		zMin_v = std::min(zMin_v, zc);  zMax_v = std::max(zMax_v, zc);
	}

	// Half-spans: these are the minimum extents required on each side of the
	// projected centre — independent of the old bounding-sphere centre.
	const float halfX = (xMax_v - xMin_v) * 0.5f;
	const float halfY = (yMax_v - yMin_v) * 0.5f;

	if (halfX <= 0.0f && halfY <= 0.0f)
	{
		if (outCenter) *outCenter = _boundingSphere.getCenter();
		return _boundingSphere.getRadius() * 2.0f;
	}

	// Projected visual centre — the point in 3-D whose view-space coordinates
	// are the midpoints of the extent intervals.  Callers use this as the new
	// orbit/pan target so the scene is centred on screen after a fit operation.
	const float cx = (xMin_v + xMax_v) * 0.5f;
	const float cy = (yMin_v + yMax_v) * 0.5f;
	const float cz = (zMin_v + zMax_v) * 0.5f;
	const QVector3D projCenter = right * cx + up * cy + viewDir * cz;
	if (outCenter) *outCenter = projCenter;

	const float aspect = static_cast<float>(width()) / static_cast<float>(height());
	constexpr float margin = 1.05f;
	float viewRange = 0.0f;

	if (_projection == ViewProjection::ORTHOGRAPHIC)
	{
		// The ortho projection maps halfRange to the shorter screen dimension:
		//   landscape (w > h): half-height = halfRange, half-width = halfRange * aspect
		//   portrait  (w ≤ h): half-width  = halfRange, half-height = halfRange / aspect
		// Using halfX = xSpan/2 (relative to the projected centre) ensures
		// equal margins on both sides and no wasted screen space.
		float halfRange;
		if (width() > height())
			halfRange = std::max(halfX / aspect, halfY);
		else
			halfRange = std::max(halfX, halfY * aspect);

		viewRange = halfRange * 2.0f * margin;
	}
	else // PERSPECTIVE
	{
		// For each corner at view-space offset (xc_rel, yc_rel, dc) from the
		// projected centre:
		//   shiftFactor * viewRange ≥ max(|xc_rel|/tan_half_x, |yc_rel|/tan_half_y) − dc
		// Near-side corners (dc < 0) increase the requirement; far corners reduce it.
		const float fovRad      = qDegreesToRadians(_FOV);
		const float tanHalfFov  = std::tan(fovRad * 0.5f);
		const float sinHalfFov  = std::sin(fovRad * 0.5f);
		const float shiftFactor = std::min(1.05f / sinHalfFov, 1.25f);

		float maxReq = 0.0f;
		for (const QVector3D& c : corners)
		{
			const float xc_rel = QVector3D::dotProduct(c, right)   - cx;
			const float yc_rel = QVector3D::dotProduct(c, up)      - cy;
			const float dc     = QVector3D::dotProduct(c, viewDir) - cz;

			float req;
			if (aspect >= 1.0f) // landscape: tan_half_x = tanHalfFov * aspect
				req = std::max(std::abs(xc_rel) / aspect, std::abs(yc_rel)) / tanHalfFov - dc;
			else               // portrait:  tan_half_x = tanHalfFov
				req = std::max(std::abs(xc_rel), std::abs(yc_rel) * aspect) / tanHalfFov - dc;

			maxReq = std::max(maxReq, req);
		}

		viewRange = maxReq / shiftFactor * margin;
	}

	return std::max(viewRange, 0.0001f);
}

// Improved approach based on rubberband zoom technique
void GLWidget::fitBoxToScreen(const BoundingBox& box)
{			
	// Project bounding box corners to screen space
	std::vector<Point> corners = box.corners();
	std::vector<QVector3D> vcorners =	
	{
	QVector3D(corners[0].getX(), corners[0].getY(), corners[0].getZ()),
	QVector3D(corners[1].getX(), corners[1].getY(), corners[1].getZ()),
	QVector3D(corners[2].getX(), corners[2].getY(), corners[2].getZ()),
	QVector3D(corners[3].getX(), corners[3].getY(), corners[3].getZ()),
	QVector3D(corners[4].getX(), corners[4].getY(), corners[4].getZ()),
	QVector3D(corners[5].getX(), corners[5].getY(), corners[5].getZ()),
	QVector3D(corners[6].getX(), corners[6].getY(), corners[6].getZ()),
	QVector3D(corners[7].getX(), corners[7].getY(), corners[7].getZ())
	};

	QRect screenBounds;
	bool firstPoint = true;

	for (const auto& corner : vcorners)
	{
		// Project point to screen coordinates
		QVector4D clipCoords = _projectionMatrix * _viewMatrix * QVector4D(corner, 1.0f);

		QVector3D screenPoint(clipCoords.x() / clipCoords.w(),
                           clipCoords.y() / clipCoords.w(), 
                           clipCoords.z() / clipCoords.w());
				
		QPoint pixelPoint(
			static_cast<int>((clipCoords.x() + 1.0f) * 0.5f * width()),
			static_cast<int>(height() - (clipCoords.y() + 1.0f) * 0.5f * height())
		);

		// Update screen bounds
		if (firstPoint) {
			screenBounds = QRect(pixelPoint, QSize(1, 1));
			firstPoint = false;
		}
		else {
			screenBounds = screenBounds.united(QRect(pixelPoint, QSize(1, 1)));
		}
	}

	// Calculate client rect (full viewport)
	QRect clientRect(0, 0, width(), height());

	// Calculate zoom ratio using the same approach as window zoom
	double widthRatio = static_cast<double>(clientRect.width()) / screenBounds.width();
	double heightRatio = static_cast<double>(clientRect.height()) / screenBounds.height();

	// Use the smaller ratio to ensure the box fits in both dimensions
	// Apply a factor of 0.95 to leave a small margin around the object
	double zoomRatio = std::min(widthRatio, heightRatio) * 0.95;
		
	// Get center points for screen and box
	QPoint screenCenter = screenBounds.center();
	QPoint viewportCenter = clientRect.center();

	// Convert pan offset to 3D space
	// First, get world coordinates at screen center with current Z
	QVector3D screenCenterWorld(screenCenter.x(), height() - screenCenter.y(), 0.5);
	QVector3D viewportCenterWorld(viewportCenter.x(), height() - viewportCenter.y(), 0.5);

	// Unproject both points to get world coordinates
	QVector3D screenCenterPoint = screenCenterWorld.unproject(_viewMatrix * _modelMatrix, _projectionMatrix, QRect(0, 0, width(), height()));
	QVector3D viewportCenterPoint = viewportCenterWorld.unproject(_viewMatrix * _modelMatrix, _projectionMatrix, QRect(0, 0, width(), height()));

	// Calculate the pan vector
	QVector3D panVector = screenCenterPoint - viewportCenterPoint;

	
	setZoomAndPan(_currentViewRange / zoomRatio, panVector);
}


void GLWidget::animateToRotation(const QQuaternion& targetRotation)
{
	QQuaternion curRot = QQuaternion::slerp(_currentRotation, targetRotation, _slerpStep += _slerpFrac);

	QMatrix4x4 rotMat = QMatrix4x4(curRot.toRotationMatrix());
	QVector3D viewDir = -rotMat.row(2).toVector3D();
	QVector3D upDir = rotMat.row(1).toVector3D();
	QVector3D rightDir = rotMat.row(0).toVector3D();

	float scaleStep = (_currentViewRange - _viewBoundingSphereDia) * _slerpFrac;
	_viewRange -= scaleStep;

	QVector3D curPos;
	if (_primaryCamera->getMode() == GLCamera::CameraMode::Fly ||
		_primaryCamera->getMode() == GLCamera::CameraMode::FirstPerson)
	{
		QMatrix4x4 targetRotMat(targetRotation.toRotationMatrix());
		QVector3D targetViewDir = -targetRotMat.row(2).toVector3D().normalized();
		const float fovRad = qDegreesToRadians(_FOV);
		const float sinHalfFov = std::max(std::sin(fovRad * 0.5f), 0.001f);
		const float shiftFactor = std::min(1.05f / sinHalfFov, 1.25f);
		const float targetDistance = shiftFactor * _viewBoundingSphereDia;
		const QVector3D targetEye = _boundingSphere.getCenter() - targetViewDir * targetDistance;
		curPos = _currentTranslation - (_slerpStep * _currentTranslation) + (targetEye * _slerpStep);
	}
	else
	{
		curPos = _currentTranslation - (_slerpStep * _currentTranslation) + (_boundingSphere.getCenter() * _slerpStep);
	}

	_primaryCamera->setView(curPos, viewDir, upDir, rightDir);
	if (_primaryCamera->getMode() == GLCamera::CameraMode::Fly ||
		_primaryCamera->getMode() == GLCamera::CameraMode::FirstPerson)
	{
		_primaryCamera->setYawPitchFromViewDir();
	}

	if (qFuzzyCompare(_slerpStep, 1.0f))
	{
		if (_primaryCamera->getMode() == GLCamera::CameraMode::Fly ||
			_primaryCamera->getMode() == GLCamera::CameraMode::FirstPerson)
		{
			QMatrix4x4 targetRotMat(targetRotation.toRotationMatrix());
			QVector3D targetViewDir = -targetRotMat.row(2).toVector3D().normalized();
			QVector3D targetUpDir = targetRotMat.row(1).toVector3D().normalized();
			QVector3D targetRightDir = targetRotMat.row(0).toVector3D().normalized();
			const float fovRad = qDegreesToRadians(_FOV);
			const float sinHalfFov = std::max(std::sin(fovRad * 0.5f), 0.001f);
			const float shiftFactor = std::min(1.05f / sinHalfFov, 1.25f);
			const float targetDistance = shiftFactor * _viewBoundingSphereDia;
			const QVector3D targetEye = _boundingSphere.getCenter() - targetViewDir * targetDistance;
			_primaryCamera->setView(targetEye, targetViewDir, targetUpDir, targetRightDir);
			_primaryCamera->setYawPitchFromViewDir();
			_primaryCamera->updateFlyView();
		}
		else
		{
			_primaryCamera->setView(curPos, viewDir, upDir, rightDir);
		}

		_currentRotation = QQuaternion::fromRotationMatrix(_primaryCamera->getViewMatrix().toGenericMatrix<3, 3>());
		_currentTranslation = _primaryCamera->getPosition();
		_currentViewRange = _viewRange;
		_slerpStep = 0.0f;
		_customViewAnimationActive = false;

		emit rotationsSet();
	}
}

void GLWidget::setRotations(float xRot, float yRot, float zRot)
{
	QQuaternion targetRotation = QQuaternion::fromEulerAngles(yRot, zRot, xRot); //Pitch, Yaw, Roll
	animateToRotation(targetRotation);
}

void GLWidget::setZoomAndPan(float zoom, QVector3D pan)
{
	_slerpStep += _slerpFrac;

	// Translation
	QVector3D curPos = pan * _slerpFrac;
	_primaryCamera->move(curPos.x(), curPos.y(), curPos.z());

	// Set zoom
	float scaleStep = (_currentViewRange - zoom) * _slerpFrac;
	_viewRange -= scaleStep;

	if (qFuzzyCompare(_slerpStep, 1.0f))
	{
		// Set all defaults
		_currentTranslation = _primaryCamera->getPosition();
		_currentViewRange = _viewRange;
		_slerpStep = 0.0f;

		emit zoomAndPanSet();
	}
}

void GLWidget::closeEvent(QCloseEvent* event)
{
	event->accept();
}

bool GLWidget::areLightsShown() const
{
	return _showLights;
}

void GLWidget::showLights(bool showLights)
{
	_showLights = showLights;
	update();
}

void GLWidget::useDefaultLights(bool useDefaultLights)
{
	_useDefaultLights = useDefaultLights;	
	update();
}

void GLWidget::usePunctualLights(bool usePunctualLights)
{
	_usePunctualLights = usePunctualLights;
	update();
}

void GLWidget::applyEnabledLightList(const std::vector<GPULight>& enabledLights)
{
	makeCurrent();
	glLights->setLights(enabledLights);
	syncPunctualLightUniforms(static_cast<int>(enabledLights.size()),
	                          !enabledLights.empty());
}

void GLWidget::useIBL(bool useIBL)
{
	_useIBL = useIBL;
	update();
}

float GLWidget::getScreenGamma() const
{
	return _screenGamma;
}

void GLWidget::setScreenGamma(double screenGamma)
{
	_screenGamma = static_cast<float>(screenGamma);
	update();
}

void GLWidget::setHDRToneMappingMode(HDRToneMapMode mode)
{
	_toneMappingMode = mode;
	update();
}

void GLWidget::setEnvMapExposure(double exposure)
{
	_envMapExposure = pow(2.0f, exposure);
	update();
}

void GLWidget::setIBLExposure(double exposure)
{
	_iblExposure = pow(2.0f, exposure);
	update();
}

bool GLWidget::getGammaCorrection() const
{
	return _gammaCorrection;
}

void GLWidget::enableGammaCorrection(bool gammaCorrection)
{
	_gammaCorrection = gammaCorrection;
	update();
}

bool GLWidget::getHdrToneMapping() const
{
	return _hdrToneMapping;
}

void GLWidget::enableHDRToneMapping(bool hdrToneMapping)
{
	_hdrToneMapping = hdrToneMapping;
	update();
}

RenderingMode GLWidget::getRenderingMode() const
{
	return _renderingMode;
}

void GLWidget::setRenderingMode(const RenderingMode& renderingMode)
{
	_renderingMode = renderingMode;
	_fgShader->bind();
	_fgShader->setUniformValue("renderingMode", static_cast<int>(_renderingMode));

	// Mark textures as dirty to ensure they are reloaded
	for (auto mesh : _meshStore)
	{
		mesh->markTexturesDirty();
		mesh->markUniformsDirty();
	}

	_fgShader->release();
	update();
	emit renderingModeChanged(static_cast<int>(_renderingMode));
}

void GLWidget::setFloorTexRepeatT(double floorTexRepeatT)
{
	_floorTexRepeatT = static_cast<float>(floorTexRepeatT);
	updateFloorPlane();
	update();
}

void GLWidget::setFloorTexRepeatS(double floorTexRepeatS)
{
	_floorTexRepeatS = static_cast<float>(floorTexRepeatS);
	updateFloorPlane();
	update();
}

void GLWidget::setFloorOffsetPercent(double value)
{
	_floorOffsetPercent = static_cast<float>(value / 100.0f);
	updateFloorPlane();
	update();
}

void GLWidget::setSkyBoxFOV(double fov)
{
	_skyBoxFOV = static_cast<float>(fov);
	update();
}

void GLWidget::setPerspFOV(int fovDegrees)
{
	_FOV = static_cast<float>(qBound(1, fovDegrees, 179));
	_primaryCamera->setFOV(_FOV);
	resizeGL(width(), height());
	update();
}

void GLWidget::setSkyBoxZRotation(int index)
{
	// Map combo index to Y-axis rotation angle (OpenGL Y = world Z-up)
	// X+ → 0°, X- → 180°, Y-Z+ → 90°, Y+ → 270°
	static constexpr float angles[] = { 0.0f, 180.0f, 90.0f, 270.0f };
	_skyBoxZRotation = angles[index % 4];
	updateEnvMapRotationMatrix();
	update();
}

void GLWidget::updateEnvMapRotationMatrix()
{
	// _fgShader is null before initializeGL() — the call from the constructor
	// (settings-based setCameraUpAxisZUp) is a no-op; initializeGL() picks up
	// the correct state at line 1729 after createShaderPrograms().
	if (!_fgShader)
		return;

	// Build Ry(-theta) · Rx(-90°) using Qt post-multiply (M = M*R):
	//
	//   envMapRotationMatrix = Ry(-theta) · Rx(-90°)
	//
	// This converts a Z-up world direction into a cubemap sample direction:
	//   1. Rx(-90°)  — maps world Z-up to cubemap Y-up (Z-up correction)
	//   2. Ry(-theta)— rotates horizontally around the (now corrected) Y axis,
	//                   matching the user's Z-up world rotation
	//
	// Ordering matters: Rx(-90°) must be the INNER transform and Ry(-theta)
	// the OUTER, so that Ry acts in cubemap (Y-up) space, not raw Z-up space.
	// Reversing the order would tilt the sky axis as the environment rotates.
	QMatrix4x4 envMapRot;
	envMapRot.rotate(-_skyBoxZRotation, 0, 1, 0); // Qt post-mul: M = Ry(-theta)
	envMapRot.rotate(-90.0f, 1, 0, 0);            // Qt post-mul: M = Ry(-theta) · Rx(-90°)
	envMapRot.rotate(cameraUpAxisConventionRotation().inverted());
	_fgShader->bind();
	_fgShader->setUniformValue("envMapRotationMatrix", envMapRot.toGenericMatrix<3, 3>());
}

void GLWidget::setSkyBoxTextureHDRI(bool hdrSet)
{
	_skyBoxTextureHDRI = hdrSet;
	update();
}

QColor GLWidget::getBgBotColor() const
{
	return _bgBotColor;
}

void GLWidget::updateOverlayEditorTheme()
{
	emit backgroundColorChanged(_bgTopColor, _bgBotColor);

	const QColor averageBackgroundColor(
		(_bgTopColor.red() + _bgBotColor.red()) / 2,
		(_bgTopColor.green() + _bgBotColor.green()) / 2,
		(_bgTopColor.blue() + _bgBotColor.blue()) / 2,
		(_bgTopColor.alpha() + _bgBotColor.alpha()) / 2);
	const QColor contrastColor = (averageBackgroundColor.lightnessF() < 0.5)
		? QColor(255, 255, 255)
		: QColor(0, 0, 0);

	if (QTabWidget* tabs = _viewer ? _viewer->findChild<QTabWidget*>("tabWidget") : nullptr) {
		const QString tabStyleSheet = QString("color: rgb(%1, %2, %3);")
									  .arg(contrastColor.red())
									  .arg(contrastColor.green())
									  .arg(contrastColor.blue()) +
									  "background-color: rgba(255, 255, 255, 0);";
		tabs->setStyleSheet(tabStyleSheet);
	}
}

void GLWidget::setBgBotColor(const QColor& bgBotColor)
{
	_bgBotColor = bgBotColor;
	updateOverlayEditorTheme();
	refreshNavigationOverlayStyle();
	update();
}

QColor GLWidget::getBgTopColor() const
{
	return _bgTopColor;
}

void GLWidget::setBgTopColor(const QColor& bgTopColor)
{
	_bgTopColor = bgTopColor;
	updateOverlayEditorTheme();
	refreshNavigationOverlayStyle();
	update();
}

BoundingSphere GLWidget::getBoundingSphere() const
{
	return _boundingSphere;
}

std::vector<int> GLWidget::getDisplayedObjectsIds() const
{
	return _displayedObjectsIds;
}

bool GLWidget::isVisibleSwapped() const
{
	return _visibleSwapped;
}

void GLWidget::setShowFaceNormals(bool showFaceNormals)
{
	_showFaceNormals = showFaceNormals;
}

bool GLWidget::isFaceNormalsShown() const
{
	return _showFaceNormals;
}

bool GLWidget::isVertexNormalsShown() const
{
	return _showVertexNormals;
}

void GLWidget::setShowVertexNormals(bool showVertexNormals)
{
	_showVertexNormals = showVertexNormals;
}

bool GLWidget::isShaded() const
{
	return _displayMode == DisplayMode::SHADED;
}

DisplayMode GLWidget::getDisplayMode() const
{
	return _displayMode;
}

void GLWidget::setDisplayMode(DisplayMode mode)
{
	_displayMode = mode;

	if (_viewToolbar)
		_viewToolbar->setDefaultDisplayModeAction(static_cast<DisplayModeActions>(_displayMode));
		
	_fgShader->bind();
	_fgShader->setUniformValue("displayMode", static_cast<int>(_displayMode));
	_fgShader->release();
	emit displayModeChanged(static_cast<int>(_displayMode));
}

void GLWidget::setTransmissionEnabled(const bool& enabled)
{
	_transmissionEnabled = enabled;
	if (_transmissionEnabled)
		initTransmissionBuffer();
	update();
}

float GLWidget::getZScale() const
{
	return _zScale;
}

void GLWidget::setZScale(const float& zScale)
{
	_zScale = zScale;
}

float GLWidget::getYScale() const
{
	return _yScale;
}

void GLWidget::setYScale(const float& yScale)
{
	_yScale = yScale;
}

float GLWidget::getXScale() const
{
	return _xScale;
}

void GLWidget::setXScale(const float& xScale)
{
	_xScale = xScale;
}

float GLWidget::getZRot() const
{
	return _zRot;
}

void GLWidget::setZRot(const float& zRot)
{
	_zRot = zRot;
}

float GLWidget::getYRot() const
{
	return _yRot;
}

void GLWidget::setYRot(const float& yRot)
{
	_yRot = yRot;
}

float GLWidget::getXRot() const
{
	return _xRot;
}

void GLWidget::setXRot(const float& xRot)
{
	_xRot = xRot;
}

float GLWidget::getZTran() const
{
	return _zTran;
}

void GLWidget::setZTran(const float& zTran)
{
	_zTran = zTran;
}

float GLWidget::getYTran() const
{
	return _yTran;
}

void GLWidget::setYTran(const float& yTran)
{
	_yTran = yTran;
}


float GLWidget::getXTran() const
{
	return _xTran;
}

void GLWidget::setXTran(const float& xTran)
{
	_xTran = xTran;
}

float GLWidget::highestModelZ()
{
	return _visibleHighestZ;
}

float GLWidget::lowestModelZ()
{
	return _visibleLowestZ;
}

void GLWidget::showContextMenu(const QPoint& pos)
{
	if (QApplication::keyboardModifiers() != Qt::ControlModifier)
	{
		// Create menu and insert some actions
		QMenu contextMenu;
		SceneTreeWidget* treeWidgetModel = _viewer->getTreeModel();
		if (treeWidgetModel->hasMeshSelection() &&
			(_visibleSwapped ? _hiddenObjectsIds.size() != 0 : _displayedObjectsIds.size() != 0))
		{
			contextMenu.addAction(tr("Center Screen"), _viewer, &ModelViewer::centerScreen);
			QList<QUuid> selUuids = treeWidgetModel->selectedMeshUuids();
			if (selUuids.count() <= 1)
			{
				// Show "Center Object List" only when the selected mesh is visible
				QSet<QUuid> visibleUuids = treeWidgetModel->getVisibleUuids();
				if (selUuids.isEmpty() || visibleUuids.contains(selUuids.first()))
					contextMenu.addAction(tr("Center Object List"), this, &GLWidget::centerDisplayList);
			}
			contextMenu.addSeparator();
			if (_visibleSwapped)
				contextMenu.addAction(tr("Show"), _viewer, &ModelViewer::showSelectedItems);
			else
				contextMenu.addAction(tr("Hide"), _viewer, &ModelViewer::hideSelectedItems);
			if (_displayedObjectsIds.size() > 1)
				contextMenu.addAction(tr("Show Only"), _viewer, &ModelViewer::showOnlySelectedItems);
			contextMenu.addSeparator();
			contextMenu.addAction(tr("Transformations"), _viewer, &ModelViewer::showTransformationsPage);
			contextMenu.addAction(tr("Edit Material"), _viewer, &ModelViewer::editMeshMaterial);
			contextMenu.addSeparator();			
			contextMenu.addAction(tr("Generate UVs"), _viewer, &ModelViewer::generateUVsForSelectedItems);
			contextMenu.addSeparator();
			contextMenu.addAction(tr("Copy"),   _viewer, &ModelViewer::copySelectedItems);
			contextMenu.addAction(tr("Cut"),    _viewer, &ModelViewer::cutSelectedItems);
			contextMenu.addAction(tr("Delete"), _viewer, &ModelViewer::deleteSelectedItems);			
			contextMenu.addSeparator();
			contextMenu.addAction(tr("Mesh Info"), _viewer, &ModelViewer::displaySelectedMeshInfo);
		}
		else
		{
			QAction* action = nullptr;
			if ((!_visibleSwapped && _displayedObjectsIds.size() != 0) || (_visibleSwapped && _hiddenObjectsIds.size() != 0))
			{				
				action = contextMenu.addAction(QIcon(":/icons/res/fit-all.png"), tr("Fit All"), this, &GLWidget::fitAll);
				action->setShortcut(QKeySequence(Qt::Key_F));

				action = contextMenu.addAction(QIcon(":/icons/res/window-zoom.png"), tr("Zoom Area"));
				action->setShortcut(QKeySequence(Qt::ALT | Qt::Key_W));
				action->setCheckable(true);
				connect(action, &QAction::triggered, this, &GLWidget::beginWindowZoom);

				// View manipulation actions				
				contextMenu.addSeparator();

				// If any of the view modes are active, add a menu item named select to disable them
				if (_viewZooming || _viewPanning || _viewRotating)
				{
					contextMenu.addSeparator();
					action = contextMenu.addAction(QIcon(":/icons/res/select.png"), tr("Select"));
					connect(action, &QAction::triggered, this, [this]() {
						setZoomingActive(false);
						setPanningActive(false);
						setRotationActive(false);
						setCursor(QCursor(Qt::ArrowCursor));
						});
				}
				action = contextMenu.addAction(QIcon(":/icons/res/zoomview.png"), tr("Zoom"));
				action->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Z));
				connect(action, &QAction::triggered, this, [this]() {
					setZoomingActive(true);
					});

				action = contextMenu.addAction(QIcon(":/icons/res/panview.png"), tr("Pan"));
				action->setShortcut(QKeySequence(Qt::ALT | Qt::Key_P));
				connect(action, &QAction::triggered, this, [this]() {
					setPanningActive(true);
					});

				action = contextMenu.addAction(QIcon(":/icons/res/rotateview.png"), tr("Rotate"));
				action->setShortcut(QKeySequence(Qt::ALT | Qt::Key_R));
				connect(action, &QAction::triggered, this, [this]() {
					setRotationActive(true);
					});								

				contextMenu.addSeparator();
			}			

			if (_hiddenObjectsIds.size() != 0)
			{
				action = contextMenu.addAction(QIcon(":/icons/res/showall.png"), tr("Show All"), _viewer, &ModelViewer::showAllItems);
				action->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_A));
			}
			if (_displayedObjectsIds.size() != 0)
			{
				action = contextMenu.addAction(QIcon(":/icons/res/hideall.png"), tr("Hide All"), _viewer, &ModelViewer::hideAllItems);
				action->setShortcut(QKeySequence(Qt::ALT | Qt::Key_A));
			}
			if (_hiddenObjectsIds.size() != 0)
			{
				action = contextMenu.addAction(QIcon(":/icons/res/swapvisible.png"), tr("Swap Visible"));
				action->setCheckable(true);
				action->setShortcut(QKeySequence(Qt::ALT | Qt::Key_S));
				action->setChecked(_visibleSwapped);				
				connect(action, &QAction::triggered, this, [this](bool enabled) {
					swapVisible(enabled);
					});
			}
			contextMenu.addSeparator();
			contextMenu.addAction(QIcon(":/icons/res/environment.png"), tr("Environment Settings"), _viewer, &ModelViewer::showVisualizationModelPage);
			contextMenu.addAction(QIcon(":/icons/res/bg_color.png"), tr("Background Color"), this, &GLWidget::setBackgroundColor);
		}
		// Show context menu at handling position
		contextMenu.exec(mapToGlobal(pos));
	}
}

void GLWidget::centerDisplayList()
{
	SceneTreeWidget* treeWidgetModel = _viewer->getTreeModel();
	if (treeWidgetModel && treeWidgetModel->hasMeshSelection())
		treeWidgetModel->scrollFirstSelectedToCenter();
}

#include "BackgroundColor.h"
void GLWidget::setBackgroundColor()
{
	BackgroundColor bgCol(this);
	bgCol.exec();
}

// ---------------------------------------------------------------------------
// MVF3 mesh loader
// ---------------------------------------------------------------------------

#include "MvfDocument.h"
#include <QFile>
#include <QTemporaryDir>
#include <QUuid>
#include <cstring>

namespace
{
// Read count*componentsOf(type) floats from geometryChunk via an accessor index.
static std::vector<float> readFloatStream(const QByteArray& chunk,
                                           const QJsonArray& accessors,
                                           const QJsonArray& bufferViews,
                                           int accessorIndex)
{
    if (accessorIndex < 0 || accessorIndex >= accessors.size())
        return {};
    const QJsonObject acc = accessors[accessorIndex].toObject();
    const int bvIdx = acc[QStringLiteral("bufferView")].toInt(-1);
    if (bvIdx < 0 || bvIdx >= bufferViews.size())
        return {};
    const QJsonObject bv = bufferViews[bvIdx].toObject();
    if (bv[QStringLiteral("buffer")].toInt(-1) != 0)
        return {};  // not the GEOM buffer

    const int bvOffset   = (int)bv[QStringLiteral("byteOffset")].toDouble(0);
    const int accOffset  = (int)acc[QStringLiteral("byteOffset")].toDouble(0);
    const int byteOffset = bvOffset + accOffset;
    const int count      = (int)acc[QStringLiteral("count")].toDouble(0);

    const QString type = acc[QStringLiteral("type")].toString();
    int components = 1;
    if      (type == QLatin1String("VEC2")) components = 2;
    else if (type == QLatin1String("VEC3")) components = 3;
    else if (type == QLatin1String("VEC4")) components = 4;

    const int totalFloats = count * components;
    if (byteOffset + totalFloats * (int)sizeof(float) > chunk.size())
        return {};

    std::vector<float> result(totalFloats);
    std::memcpy(result.data(), chunk.constData() + byteOffset,
                totalFloats * sizeof(float));
    return result;
}

// Read count unsigned ints from geometryChunk via an accessor index.
static std::vector<unsigned int> readUIntStream(const QByteArray& chunk,
                                                 const QJsonArray& accessors,
                                                 const QJsonArray& bufferViews,
                                                 int accessorIndex)
{
    if (accessorIndex < 0 || accessorIndex >= accessors.size())
        return {};
    const QJsonObject acc = accessors[accessorIndex].toObject();
    const int bvIdx = acc[QStringLiteral("bufferView")].toInt(-1);
    if (bvIdx < 0 || bvIdx >= bufferViews.size())
        return {};
    const QJsonObject bv = bufferViews[bvIdx].toObject();
    if (bv[QStringLiteral("buffer")].toInt(-1) != 0)
        return {};

    const int bvOffset   = (int)bv[QStringLiteral("byteOffset")].toDouble(0);
    const int accOffset  = (int)acc[QStringLiteral("byteOffset")].toDouble(0);
    const int byteOffset = bvOffset + accOffset;
    const int count      = (int)acc[QStringLiteral("count")].toDouble(0);

    if (byteOffset + count * (int)sizeof(unsigned int) > chunk.size())
        return {};

    std::vector<unsigned int> result(count);
    std::memcpy(result.data(), chunk.constData() + byteOffset,
                count * sizeof(unsigned int));
    return result;
}

// Apply a texture info JSON object to the right GLMaterial path setter.
static void applyTextureRef(GLMaterial& mat,
                             GLMaterial::TextureType type,
                             const QJsonObject& texInfo,
                             const QHash<int, QString>& imagePaths,
                             const QJsonArray& textures,
                             const QJsonArray& samplers)
{
    const int texIndex = texInfo[QStringLiteral("index")].toInt(-1);
    if (texIndex < 0 || texIndex >= textures.size())
        return;
    const QJsonObject texObj   = textures[texIndex].toObject();
    const int imgIndex         = texObj[QStringLiteral("image")].toInt(-1);
    const int samplerIndex     = texObj[QStringLiteral("sampler")].toInt(-1);
    const QString path         = imagePaths.value(imgIndex);
    if (path.isEmpty())
        return;

    switch (type)
    {
    case GLMaterial::TextureType::Albedo:                   mat.setAlbedoMap(path); break;
    case GLMaterial::TextureType::Normal:                   mat.setNormalMap(path); break;
    case GLMaterial::TextureType::AmbientOcclusion:         mat.setAOMap(path); break;
    case GLMaterial::TextureType::Emissive:                 mat.setEmissiveMap(path); break;
    case GLMaterial::TextureType::Metallic:                 mat.setMetallicMap(path); break;
    case GLMaterial::TextureType::Roughness:                mat.setRoughnessMap(path); break;
    case GLMaterial::TextureType::Transmission:             mat.setTransmissionMap(path); break;
    case GLMaterial::TextureType::IOR:                      mat.setIORMap(path); break;
    case GLMaterial::TextureType::SheenColor:               mat.setSheenColorMap(path); break;
    case GLMaterial::TextureType::SheenRoughness:           mat.setSheenRoughnessMap(path); break;
    case GLMaterial::TextureType::ClearcoatColor:           mat.setClearcoatColorMap(path); break;
    case GLMaterial::TextureType::ClearcoatRoughness:       mat.setClearcoatRoughnessMap(path); break;
    case GLMaterial::TextureType::ClearcoatNormal:          mat.setClearcoatNormalMap(path); break;
    case GLMaterial::TextureType::Iridescence:              mat.setIridescenceMap(path); break;
    case GLMaterial::TextureType::IridescenceThickness:     mat.setIridescenceThicknessMap(path); break;
    case GLMaterial::TextureType::SpecularFactor:           mat.setSpecularFactorMap(path); break;
    case GLMaterial::TextureType::SpecularColor:            mat.setSpecularColorMap(path); break;
    case GLMaterial::TextureType::Anisotropy:               mat.setAnisotropyMap(path); break;
    case GLMaterial::TextureType::Thickness:                mat.setThicknessMap(path); break;
    case GLMaterial::TextureType::Diffuse:                  mat.setDiffuseMap(path); break;
    case GLMaterial::TextureType::DiffuseTransmission:      mat.setDiffuseTransmissionMap(path); break;
    case GLMaterial::TextureType::DiffuseTransmissionColor: mat.setDiffuseTransmissionColorMap(path); break;
    case GLMaterial::TextureType::SpecularGlossiness:       mat.setSpecularGlossinessMap(path); break;
    case GLMaterial::TextureType::Opacity:                  mat.setOpacityMap(path); break;
    case GLMaterial::TextureType::Height:                   mat.setHeightMap(path); break;
    default: return;
    }

    GLMaterial::Texture tex = mat.texture(type);
    tex.path = path.toStdString();
    tex.texCoordIndex = texInfo[QStringLiteral("texCoord")].toInt(0);

    if (samplerIndex >= 0 && samplerIndex < samplers.size())
    {
        const QJsonObject samp = samplers[samplerIndex].toObject();
        tex.magFilter = static_cast<GLenum>(samp[QStringLiteral("magFilter")].toInt(GL_LINEAR));
        tex.minFilter = static_cast<GLenum>(samp[QStringLiteral("minFilter")].toInt(GL_LINEAR_MIPMAP_LINEAR));
        tex.wrapS     = static_cast<GLenum>(samp[QStringLiteral("wrapS")].toInt(GL_REPEAT));
        tex.wrapT     = static_cast<GLenum>(samp[QStringLiteral("wrapT")].toInt(GL_REPEAT));
    }

    const QJsonObject extensions = texInfo[QStringLiteral("extensions")].toObject();
    const QJsonObject transform = extensions[QStringLiteral("KHR_texture_transform")].toObject();
    if (!transform.isEmpty())
    {
        const QJsonArray scale = transform[QStringLiteral("scale")].toArray();
        if (scale.size() >= 2)
            tex.scale = glm::vec2(static_cast<float>(scale[0].toDouble(1.0)),
                                  static_cast<float>(scale[1].toDouble(1.0)));

        const QJsonArray offset = transform[QStringLiteral("offset")].toArray();
        if (offset.size() >= 2)
            tex.offset = glm::vec2(static_cast<float>(offset[0].toDouble(0.0)),
                                   static_cast<float>(offset[1].toDouble(0.0)));

        tex.rotation = static_cast<float>(transform[QStringLiteral("rotation")].toDouble(0.0));
    }

    mat.setTexture(type, tex);
}

// Reconstruct a GLMaterial from an MVF3 material JSON object.
static GLMaterial reconstructMvfMaterial(const QJsonObject& matObj,
                                         const QHash<int, QString>& imagePaths,
                                         const QJsonArray& textures,
                                         const QJsonArray& samplers)
{
    const QString materialName = matObj[QStringLiteral("name")].toString();
    const QJsonObject exts = matObj[QStringLiteral("extensions")].toObject();
    const bool hasRuntimeMaterial =
        !exts[QStringLiteral("MVF_material_runtime")].toObject().isEmpty();

    GLMaterial mat = hasRuntimeMaterial
        ? GLMaterial::fromVariantMap(exts[QStringLiteral("MVF_material_runtime")].toObject().toVariantMap())
        : GLMaterial();
    mat.setName(materialName);

    if (!hasRuntimeMaterial)
    {
        const QString shadingModel = matObj[QStringLiteral("shadingModel")].toString();
        if      (shadingModel == QLatin1String("PBR"))        mat.setShadingModel(GLMaterial::ShadingModel::PBR);
        else if (shadingModel == QLatin1String("BlinnPhong")) mat.setShadingModel(GLMaterial::ShadingModel::BlinnPhong);
        else if (shadingModel == QLatin1String("Unlit"))      mat.setShadingModel(GLMaterial::ShadingModel::Unlit);
        else if (shadingModel == QLatin1String("Toon"))       mat.setShadingModel(GLMaterial::ShadingModel::Toon);

        const QString blendMode = matObj[QStringLiteral("blendMode")].toString();
        if      (blendMode == QLatin1String("Opaque"))   mat.setBlendMode(GLMaterial::BlendMode::Opaque);
        else if (blendMode == QLatin1String("Masked"))   mat.setBlendMode(GLMaterial::BlendMode::Masked);
        else if (blendMode == QLatin1String("Alpha"))    mat.setBlendMode(GLMaterial::BlendMode::Alpha);
        else if (blendMode == QLatin1String("Additive")) mat.setBlendMode(GLMaterial::BlendMode::Additive);
        else if (blendMode == QLatin1String("Multiply")) mat.setBlendMode(GLMaterial::BlendMode::Multiply);

        mat.setTwoSided(matObj[QStringLiteral("doubleSided")].toBool(false));
        mat.setAlphaThreshold((float)matObj[QStringLiteral("alphaCutoff")].toDouble(0.5));
        mat.setOpacity((float)matObj[QStringLiteral("opacity")].toDouble(1.0));

        const QJsonObject pbr = matObj[QStringLiteral("pbr")].toObject();
        const QJsonArray bc = pbr[QStringLiteral("baseColorFactor")].toArray();
        if (bc.size() >= 3)
            mat.setAlbedoColor(QVector3D((float)bc[0].toDouble(),
                                          (float)bc[1].toDouble(),
                                          (float)bc[2].toDouble()));
        mat.setMetalness((float)pbr[QStringLiteral("metallicFactor")].toDouble(0.0));
        mat.setRoughness((float)pbr[QStringLiteral("roughnessFactor")].toDouble(1.0));
    }

    if (!hasRuntimeMaterial && exts.contains(QStringLiteral("MVF_material_ads")))
    {
        const QJsonObject ads = exts[QStringLiteral("MVF_material_ads")].toObject();
        auto v3 = [](const QJsonArray& a, const QVector3D& def = {}) -> QVector3D {
            return a.size() >= 3
                ? QVector3D((float)a[0].toDouble(), (float)a[1].toDouble(), (float)a[2].toDouble())
                : def;
        };
        mat.setAmbient (v3(ads[QStringLiteral("ambient")].toArray()));
        mat.setDiffuse (v3(ads[QStringLiteral("diffuse")].toArray()));
        mat.setSpecular(v3(ads[QStringLiteral("specular")].toArray()));
        mat.setEmissive(v3(ads[QStringLiteral("emissive")].toArray()));
        mat.setShininess((float)ads[QStringLiteral("shininess")].toDouble(32.0));
    }

    if (exts.contains(QStringLiteral("MVF_material_pbr")))
    {
        const QJsonObject mvfPbr = exts[QStringLiteral("MVF_material_pbr")].toObject();

        if (!hasRuntimeMaterial)
        {
            mat.setIOR((float)mvfPbr[QStringLiteral("ior")].toDouble(1.5));
            mat.setTransmission((float)mvfPbr[QStringLiteral("transmission")].toDouble(0.0));
            mat.setClearcoat((float)mvfPbr[QStringLiteral("clearcoat")].toDouble(0.0));
            mat.setClearcoatRoughness((float)mvfPbr[QStringLiteral("clearcoatRoughness")].toDouble(0.0));
            const QJsonArray sc = mvfPbr[QStringLiteral("sheenColor")].toArray();
            if (sc.size() >= 3)
                mat.setSheenColor(QVector3D((float)sc[0].toDouble(),
                                             (float)sc[1].toDouble(),
                                             (float)sc[2].toDouble()));
            mat.setSheenRoughness((float)mvfPbr[QStringLiteral("sheenRoughness")].toDouble(0.0));
        }

        static const struct { const char* key; GLMaterial::TextureType type; } kTexKeys[] = {
            {"baseColorTexture",                GLMaterial::TextureType::Albedo},
            {"normalTexture",                   GLMaterial::TextureType::Normal},
            {"occlusionTexture",                GLMaterial::TextureType::AmbientOcclusion},
            {"emissiveTexture",                 GLMaterial::TextureType::Emissive},
            {"metallicTexture",                 GLMaterial::TextureType::Metallic},
            {"roughnessTexture",                GLMaterial::TextureType::Roughness},
            {"transmissionTexture",             GLMaterial::TextureType::Transmission},
            {"iorTexture",                      GLMaterial::TextureType::IOR},
            {"sheenColorTexture",               GLMaterial::TextureType::SheenColor},
            {"sheenRoughnessTexture",           GLMaterial::TextureType::SheenRoughness},
            {"clearcoatTexture",                GLMaterial::TextureType::ClearcoatColor},
            {"clearcoatRoughnessTexture",       GLMaterial::TextureType::ClearcoatRoughness},
            {"clearcoatNormalTexture",          GLMaterial::TextureType::ClearcoatNormal},
            {"iridescenceTexture",              GLMaterial::TextureType::Iridescence},
            {"iridescenceThicknessTexture",     GLMaterial::TextureType::IridescenceThickness},
            {"specularTexture",                 GLMaterial::TextureType::SpecularFactor},
            {"specularColorTexture",            GLMaterial::TextureType::SpecularColor},
            {"anisotropyTexture",               GLMaterial::TextureType::Anisotropy},
            {"thicknessTexture",                GLMaterial::TextureType::Thickness},
            {"diffuseTexture",                  GLMaterial::TextureType::Diffuse},
            {"diffuseTransmissionTexture",      GLMaterial::TextureType::DiffuseTransmission},
            {"diffuseTransmissionColorTexture", GLMaterial::TextureType::DiffuseTransmissionColor},
            {"specularGlossinessTexture",       GLMaterial::TextureType::SpecularGlossiness},
            {"opacityTexture",                  GLMaterial::TextureType::Opacity},
            {"heightTexture",                   GLMaterial::TextureType::Height},
        };

        for (const auto& entry : kTexKeys)
        {
            const QString key = QLatin1String(entry.key);
            if (mvfPbr.contains(key))
                applyTextureRef(mat, entry.type, mvfPbr[key].toObject(),
                                imagePaths, textures, samplers);
        }
    }

    // Mirror any MVF-rebound texture slots back into the material's canonical
    // per-map path/id fields before runtime texture resolution.
    mat.syncTextureParameters();
    mat.updateConsistency();
    return mat;
}

static QVector<int> jsonArrayToIntVector(const QJsonArray& array)
{
    QVector<int> values;
    values.reserve(array.size());
    for (const QJsonValue& value : array)
        values.append(value.toInt(-1));
    return values;
}

static QVector<GltfVariantMapping> parseVariantMappings(const QJsonArray& array)
{
    QVector<GltfVariantMapping> mappings;
    mappings.reserve(array.size());
    for (const QJsonValue& value : array)
    {
        const QJsonObject obj = value.toObject();
        GltfVariantMapping mapping;
        mapping.materialIndex = obj[QStringLiteral("materialIndex")].toInt(-1);
        mapping.variantIndices = jsonArrayToIntVector(obj[QStringLiteral("variantIndices")].toArray());
        mappings.append(mapping);
    }
    return mappings;
}
} // anonymous namespace

// ---------------------------------------------------------------------------
// prepareMvfMeshes — CPU-only, thread-safe
// ---------------------------------------------------------------------------
QVector<GLWidget::PreparedMvfMesh> GLWidget::prepareMvfMeshes(
    const Mvf::Document& document,
    const QByteArray& geometryChunk,
    const QByteArray& imageChunk)
{
    // Build image index -> resolved file path.
    QHash<int, QString> imagePaths;
    static QTemporaryDir s_embeddedImageDir;

    for (int i = 0; i < document.images.size(); ++i)
    {
        const QJsonObject imgObj  = document.images[i].toObject();
        const int bvIndex         = imgObj[QStringLiteral("bufferView")].toInt(-1);
        const QString origUri     = imgObj[QStringLiteral("originalUri")].toString();
        const QString mimeType    = imgObj[QStringLiteral("mimeType")].toString();

        if (bvIndex >= 0 && bvIndex < document.bufferViews.size() && !imageChunk.isEmpty())
        {
            const QJsonObject bv = document.bufferViews[bvIndex].toObject();
            if (bv[QStringLiteral("buffer")].toInt(-1) == 1)
            {
                const int offset = (int)bv[QStringLiteral("byteOffset")].toDouble(0);
                const int length = (int)bv[QStringLiteral("byteLength")].toDouble(0);
                if (length > 0 && offset + length <= imageChunk.size()
                    && s_embeddedImageDir.isValid())
                {
                    const QByteArray embeddedBytes = QByteArray(
                        imageChunk.constData() + offset, length);

                    QString ext = QStringLiteral(".bin");
                    if      (mimeType == QLatin1String("image/png"))  ext = QStringLiteral(".png");
                    else if (mimeType == QLatin1String("image/jpeg")) ext = QStringLiteral(".jpg");
                    else if (mimeType == QLatin1String("image/webp")) ext = QStringLiteral(".webp");
                    else if (mimeType == QLatin1String("image/bmp"))  ext = QStringLiteral(".bmp");

                    const QString tempPath = s_embeddedImageDir.filePath(
                        QStringLiteral("img_%1_%2%3")
                            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces))
                            .arg(i)
                            .arg(ext));
                    QFile f(tempPath);
                    if (f.open(QIODevice::WriteOnly))
                    {
                        f.write(embeddedBytes);
                        f.close();
                        imagePaths[i] = tempPath;
                        continue;
                    }
                }
            }
        }

        if (!origUri.isEmpty())
            imagePaths[i] = origUri;
    }

    QVector<PreparedMvfMesh> result;
    result.reserve(document.meshes.size());

    for (int meshIdx = 0; meshIdx < document.meshes.size(); ++meshIdx)
    {
        const QJsonObject meshObj  = document.meshes[meshIdx].toObject();
        const QJsonArray primitives = meshObj[QStringLiteral("primitives")].toArray();
        if (primitives.isEmpty())
            continue;

        const QJsonObject prim    = primitives[0].toObject();
        const QJsonObject attribs = prim[QStringLiteral("attributes")].toObject();
        const QJsonObject extras  = prim[QStringLiteral("extras")].toObject();

        const std::vector<float> positions = readFloatStream(
            geometryChunk, document.accessors, document.bufferViews,
            attribs[QStringLiteral("POSITION")].toInt(-1));
        if (positions.empty())
            continue;

        const std::vector<unsigned int> indices = readUIntStream(geometryChunk, document.accessors,
            document.bufferViews, prim[QStringLiteral("indices")].toInt(-1));
        if (indices.empty())
            continue;

        const std::vector<float> normals  = readFloatStream(geometryChunk, document.accessors,
            document.bufferViews, attribs[QStringLiteral("NORMAL")].toInt(-1));
        const int tangentAccessorIndex = attribs[QStringLiteral("TANGENT")].toInt(-1);
        const std::vector<float> tangents = readFloatStream(geometryChunk, document.accessors,
            document.bufferViews, tangentAccessorIndex);
        const std::vector<float> uv0      = readFloatStream(geometryChunk, document.accessors,
            document.bufferViews, attribs[QStringLiteral("TEXCOORD_0")].toInt(-1));
        const std::vector<float> uv1      = readFloatStream(geometryChunk, document.accessors,
            document.bufferViews, attribs[QStringLiteral("TEXCOORD_1")].toInt(-1));
        const std::vector<float> uv2      = readFloatStream(geometryChunk, document.accessors,
            document.bufferViews, attribs[QStringLiteral("TEXCOORD_2")].toInt(-1));
        const std::vector<float> uv3      = readFloatStream(geometryChunk, document.accessors,
            document.bufferViews, attribs[QStringLiteral("TEXCOORD_3")].toInt(-1));
        const std::vector<float> colors   = readFloatStream(geometryChunk, document.accessors,
            document.bufferViews, attribs[QStringLiteral("COLOR_0")].toInt(-1));
        const std::vector<float> joints0  = readFloatStream(geometryChunk, document.accessors,
            document.bufferViews, attribs[QStringLiteral("JOINTS_0")].toInt(-1));
        const std::vector<float> weights0 = readFloatStream(geometryChunk, document.accessors,
            document.bufferViews, attribs[QStringLiteral("WEIGHTS_0")].toInt(-1));

        const size_t vertexCount = positions.size() / 3;
        std::vector<Vertex> vertices(vertexCount);
        const bool tangentAccessorIsVec4 =
            tangentAccessorIndex >= 0 &&
            tangentAccessorIndex < document.accessors.size() &&
            document.accessors[tangentAccessorIndex].toObject()[QStringLiteral("type")].toString() == QLatin1String("VEC4");
        const int tangentStride = tangentAccessorIsVec4 ? 4 : 3;
        for (size_t vi = 0; vi < vertexCount; ++vi)
        {
            Vertex& v = vertices[vi];
            v.Position = glm::vec3(positions[vi*3], positions[vi*3+1], positions[vi*3+2]);

            if (normals.size()  >= vi*3+3) v.Normal  = glm::vec3(normals [vi*3], normals [vi*3+1], normals [vi*3+2]);
            if (tangents.size() >= static_cast<size_t>(vi * tangentStride + 3))
            {
                v.Tangent = glm::vec3(
                    tangents[vi * tangentStride],
                    tangents[vi * tangentStride + 1],
                    tangents[vi * tangentStride + 2]);

                if (glm::length(v.Normal) > 0.0001f && glm::length(v.Tangent) > 0.0001f)
                {
                    float handedness = 1.0f;
                    if (tangentAccessorIsVec4 && tangents.size() > static_cast<size_t>(vi * tangentStride + 3))
                    {
                        handedness = tangents[vi * tangentStride + 3] >= 0.0f ? 1.0f : -1.0f;
                    }
                    v.Bitangent = glm::normalize(glm::cross(v.Normal, v.Tangent)) * handedness;
                }
            }

            if (uv0.size() >= vi*2+2) v.TexCoords[0] = glm::vec2(uv0[vi*2], uv0[vi*2+1]);
            if (uv1.size() >= vi*2+2) v.TexCoords[1] = glm::vec2(uv1[vi*2], uv1[vi*2+1]);
            if (uv2.size() >= vi*2+2) v.TexCoords[2] = glm::vec2(uv2[vi*2], uv2[vi*2+1]);
            if (uv3.size() >= vi*2+2) v.TexCoords[3] = glm::vec2(uv3[vi*2], uv3[vi*2+1]);

            v.Color = colors.size() >= vi*4+4
                      ? glm::vec4(colors[vi*4], colors[vi*4+1], colors[vi*4+2], colors[vi*4+3])
                      : glm::vec4(1.0f);

            if (joints0.size() >= vi*4+4)
                v.JointIndices = glm::vec4(joints0[vi*4], joints0[vi*4+1], joints0[vi*4+2], joints0[vi*4+3]);
            if (weights0.size() >= vi*4+4)
                v.JointWeights = glm::vec4(weights0[vi*4], weights0[vi*4+1], weights0[vi*4+2], weights0[vi*4+3]);
        }

        const bool hasNormals = normals.size() >= vertexCount * 3;
        const bool hasUv0 = uv0.size() >= vertexCount * 2;
        const bool hasTangents = tangents.size() >= vertexCount * tangentStride;
        if (!hasTangents && hasNormals && hasUv0 && prim[QStringLiteral("mode")].toInt(GL_TRIANGLES) == GL_TRIANGLES)
        {
            TangentGenerator::generateMikkTSpaceTangentsForMesh(vertices, indices);
        }

        const int materialIndex = prim[QStringLiteral("material")].toInt(-1);
        GLMaterial material;
        if (materialIndex >= 0 && materialIndex < document.materials.size())
            material = reconstructMvfMaterial(document.materials[materialIndex].toObject(),
                                              imagePaths, document.textures, document.samplers);

        PreparedMvfMesh prepared;
        prepared.name          = meshObj[QStringLiteral("name")].toString();
        prepared.primitiveMode = static_cast<GLenum>(prim[QStringLiteral("mode")].toInt(GL_TRIANGLES));
        prepared.sceneIndex    = extras[QStringLiteral("sceneIndex")].toInt(-1);
        prepared.hasNegativeScale = extras[QStringLiteral("hasNegativeScale")].toBool(false);
        prepared.originalMaterialIndex = extras[QStringLiteral("originalMaterialIndex")].toInt(-1);
        prepared.sourceFile = extras[QStringLiteral("sourceFile")].toString();
        prepared.sourceNodeName = extras[QStringLiteral("sourceNodeName")].toString();
        prepared.variantMappings = parseVariantMappings(extras[QStringLiteral("variantMappings")].toArray());
        const QString uuidStr  = extras[QStringLiteral("meshUuid")].toString();
        prepared.uuid          = uuidStr.isEmpty()
                                 ? QUuid::fromString(meshObj[QStringLiteral("id")].toString())
                                 : QUuid::fromString(uuidStr);
        prepared.vertices      = std::move(vertices);
        prepared.indices       = std::move(indices);
        prepared.material      = std::move(material);

        // Restore skin joints saved in primitiveExtras so the re-created mesh can
        // drive skeletal animation after MVF load.
        for (const QJsonValue& jointValue : extras[QStringLiteral("skinJoints")].toArray())
        {
            const QJsonObject jointObj = jointValue.toObject();
            GltfSkinJoint joint;
            joint.nodeName = jointObj[QStringLiteral("nodeName")].toString();
            const QJsonArray matArr = jointObj[QStringLiteral("inverseBindMatrix")].toArray();
            if (matArr.size() == 16)
            {
                aiMatrix4x4& m = joint.inverseBindMatrix;
                m.a1 = static_cast<float>(matArr[0].toDouble());  m.a2 = static_cast<float>(matArr[1].toDouble());
                m.a3 = static_cast<float>(matArr[2].toDouble());  m.a4 = static_cast<float>(matArr[3].toDouble());
                m.b1 = static_cast<float>(matArr[4].toDouble());  m.b2 = static_cast<float>(matArr[5].toDouble());
                m.b3 = static_cast<float>(matArr[6].toDouble());  m.b4 = static_cast<float>(matArr[7].toDouble());
                m.c1 = static_cast<float>(matArr[8].toDouble());  m.c2 = static_cast<float>(matArr[9].toDouble());
                m.c3 = static_cast<float>(matArr[10].toDouble()); m.c4 = static_cast<float>(matArr[11].toDouble());
                m.d1 = static_cast<float>(matArr[12].toDouble()); m.d2 = static_cast<float>(matArr[13].toDouble());
                m.d3 = static_cast<float>(matArr[14].toDouble()); m.d4 = static_cast<float>(matArr[15].toDouble());
            }
            prepared.skinJoints.append(joint);
        }

        // Restore per-mesh user transform (gizmo TRS saved by MvfSceneBuilder).
        const QJsonObject meshTrs = extras[QStringLiteral("meshTrs")].toObject();
        if (!meshTrs.isEmpty())
        {
            prepared.meshTranslation  = QVector3D(
                (float)meshTrs[QStringLiteral("tx")].toDouble(0.0),
                (float)meshTrs[QStringLiteral("ty")].toDouble(0.0),
                (float)meshTrs[QStringLiteral("tz")].toDouble(0.0));
            prepared.meshRotationQuat = QQuaternion(
                (float)meshTrs[QStringLiteral("qw")].toDouble(1.0),
                (float)meshTrs[QStringLiteral("qx")].toDouble(0.0),
                (float)meshTrs[QStringLiteral("qy")].toDouble(0.0),
                (float)meshTrs[QStringLiteral("qz")].toDouble(0.0));
            prepared.meshRotation     = QVector3D(
                (float)meshTrs[QStringLiteral("rx")].toDouble(0.0),
                (float)meshTrs[QStringLiteral("ry")].toDouble(0.0),
                (float)meshTrs[QStringLiteral("rz")].toDouble(0.0));
            prepared.meshScale        = QVector3D(
                (float)meshTrs[QStringLiteral("sx")].toDouble(1.0),
                (float)meshTrs[QStringLiteral("sy")].toDouble(1.0),
                (float)meshTrs[QStringLiteral("sz")].toDouble(1.0));
        }

        const QJsonArray variantMaterialsArray = extras[QStringLiteral("variantMaterials")].toArray();
        for (const QJsonValue& variantMaterialValue : variantMaterialsArray)
        {
            const QJsonObject variantMaterialObj = variantMaterialValue.toObject();
            const int key = variantMaterialObj[QStringLiteral("key")].toInt(-1);
            const QJsonObject materialObj = variantMaterialObj[QStringLiteral("material")].toObject();
            if (key < 0 || materialObj.isEmpty())
                continue;

            prepared.allVariantMaterials.insert(
                key,
                reconstructMvfMaterial(materialObj, imagePaths, document.textures, document.samplers));

        }

        // Restore morph target delta geometry saved by MvfSceneBuilder.
        // Each entry in the "targets" array of the primitive carries accessor
        // indices for POSITION, NORMAL, and TANGENT deltas.
        {
            const QJsonArray targetsArray = prim[QStringLiteral("targets")].toArray();
            if (!targetsArray.isEmpty())
            {
                QVector<MorphTargetData> morphTargets;
                morphTargets.reserve(targetsArray.size());
                for (const QJsonValue& targetVal : targetsArray)
                {
                    const QJsonObject targetObj = targetVal.toObject();
                    MorphTargetData mt;

                    // POSITION deltas
                    const std::vector<float> posD = readFloatStream(
                        geometryChunk, document.accessors, document.bufferViews,
                        targetObj[QStringLiteral("POSITION")].toInt(-1));
                    if (!posD.empty())
                    {
                        mt.positionDeltas.reserve(posD.size() / 3);
                        for (size_t i = 0; i + 2 < posD.size(); i += 3)
                            mt.positionDeltas.push_back(glm::vec3(posD[i], posD[i+1], posD[i+2]));
                    }

                    // NORMAL deltas
                    const std::vector<float> norD = readFloatStream(
                        geometryChunk, document.accessors, document.bufferViews,
                        targetObj[QStringLiteral("NORMAL")].toInt(-1));
                    if (!norD.empty())
                    {
                        mt.normalDeltas.reserve(norD.size() / 3);
                        for (size_t i = 0; i + 2 < norD.size(); i += 3)
                            mt.normalDeltas.push_back(glm::vec3(norD[i], norD[i+1], norD[i+2]));
                    }

                    // TANGENT deltas
                    const std::vector<float> tanD = readFloatStream(
                        geometryChunk, document.accessors, document.bufferViews,
                        targetObj[QStringLiteral("TANGENT")].toInt(-1));
                    if (!tanD.empty())
                    {
                        mt.tangentDeltas.reserve(tanD.size() / 3);
                        for (size_t i = 0; i + 2 < tanD.size(); i += 3)
                            mt.tangentDeltas.push_back(glm::vec3(tanD[i], tanD[i+1], tanD[i+2]));
                    }

                    morphTargets.append(std::move(mt));
                }
                prepared.morphTargets = std::move(morphTargets);

                // Restore default weights if saved.
                const QJsonArray defWeightsJson = extras[QStringLiteral("defaultMorphWeights")].toArray();
                QVector<float> defWeights;
                defWeights.reserve(defWeightsJson.size());
                for (const QJsonValue& wv : defWeightsJson)
                    defWeights.append(static_cast<float>(wv.toDouble(0.0)));
                prepared.defaultMorphWeights = std::move(defWeights);
            }
        }

        result.append(std::move(prepared));
    }

    return result;
}

// ---------------------------------------------------------------------------
// uploadPreparedMvfMeshes — GL-only, must be on main thread
// ---------------------------------------------------------------------------
bool GLWidget::uploadPreparedMvfMeshes(const QVector<PreparedMvfMesh>& meshes)
{
    makeCurrent();

    if (!_fgShader)
    {
        update();
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        makeCurrent();
    }
    if (!_fgShader)
        return false;

    for (TriangleMesh* m : _meshStore)
        delete m;
    _meshStore.clear();
    _displayedObjectsIds.clear();
    _hiddenObjectsIds.clear();
    if (_visibleSwapped)
    {
        _visibleSwapped = false;
        emit visibleSwapped(_visibleSwapped);
    }

    const int totalMeshes = meshes.size();
    QElapsedTimer yieldTimer;
    yieldTimer.start();

    for (int i = 0; i < totalMeshes; ++i)
    {
        const PreparedMvfMesh& pm = meshes[i];

        AssImpMesh* mesh = new AssImpMesh(_fgShader.get(), pm.name,
                                          {}, {}, {}, pm.material);
        mesh->setUuid(pm.uuid);
        mesh->setPrimitiveMode(pm.primitiveMode);
        mesh->setSceneIndex(pm.sceneIndex);
        mesh->setHasNegativeScale(pm.hasNegativeScale);
        mesh->setOriginalMaterialIndex(pm.originalMaterialIndex);
        mesh->setSourceFile(pm.sourceFile);
        mesh->setSourceNodeName(pm.sourceNodeName);
        mesh->setMeshData(pm.vertices, pm.indices);
        mesh->setVariantMappings(pm.variantMappings);
        mesh->setAllVariantMaterials(pm.allVariantMaterials);

        // Restore skeletal skinning data so bone animations work after MVF reload.
        if (!pm.skinJoints.isEmpty())
            mesh->setSkinJoints(pm.skinJoints);

        // Restore morph target geometry so blend-shape animations work after MVF reload.
        if (!pm.morphTargets.isEmpty())
            mesh->setMorphTargets(pm.morphTargets, pm.defaultMorphWeights);

        const GLMaterial resolved = resolveMaterialTextures(this, pm.material);
        mesh->setMaterial(resolved);
        mesh->setTextureMaps(resolved);
        mesh->invertOpacityADSMap(resolved.isOpacityMapInverted());
        mesh->invertOpacityPBRMap(resolved.isOpacityMapInverted());

        // Restore per-mesh user transform (gizmo TRS) saved in the MVF file.
        // Apply non-default values only to avoid clobbering the identity state
        // for meshes that were never transformed by the user.
        {
            const QVector3D defaultScale(1.0f, 1.0f, 1.0f);
            const QVector3D defaultTranslation(0.0f, 0.0f, 0.0f);
            const bool hasTranslation  = !pm.meshTranslation.isNull();
            const bool hasScale        = (pm.meshScale != defaultScale);
            const bool hasRotation     = !pm.meshRotationQuat.isIdentity();
            if (hasTranslation || hasRotation || hasScale)
            {
                mesh->setTranslation(pm.meshTranslation);
                mesh->setRotationQuaternion(pm.meshRotationQuat, pm.meshRotation);
                mesh->setScaling(pm.meshScale);
            }
        }

        addToDisplay(mesh);

        // Yield periodically so the progress bar and event loop stay alive.
        if (yieldTimer.elapsed() >= 8)
        {
            const int pct = 50 + (i + 1) * 40 / totalMeshes;   // 50-90%
            MainWindow::setProgressValue(pct);
            MainWindow::showStatusMessage(
                tr("Uploading mesh %1 / %2").arg(i + 1).arg(totalMeshes));

            doneCurrent();
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            makeCurrent();
            yieldTimer.restart();
        }
    }

    updateView();
    return !_meshStore.empty();
}

// ---------------------------------------------------------------------------
// clearMeshStore — delete all meshes and clear display list
// ---------------------------------------------------------------------------
void GLWidget::clearMeshStore()
{
    makeCurrent();

    for (TriangleMesh* m : _meshStore)
        delete m;
    _meshStore.clear();
    _displayedObjectsIds.clear();
    _hiddenObjectsIds.clear();
    if (_visibleSwapped)
    {
        _visibleSwapped = false;
        emit visibleSwapped(_visibleSwapped);
    }
}

// ---------------------------------------------------------------------------
// uploadOneMvfMesh — single-mesh GL upload for BlockingQueuedConnection
// ---------------------------------------------------------------------------
void GLWidget::uploadOneMvfMesh(const PreparedMvfMesh& pm)
{
    makeCurrent();

    // Create mesh on main thread (GL context required)
    AssImpMesh* mesh = new AssImpMesh(_fgShader.get(), pm.name,
                                      {}, {}, {}, pm.material);
    mesh->setUuid(pm.uuid);
    mesh->setPrimitiveMode(pm.primitiveMode);
    mesh->setSceneIndex(pm.sceneIndex);
    mesh->setHasNegativeScale(pm.hasNegativeScale);
    mesh->setOriginalMaterialIndex(pm.originalMaterialIndex);
    mesh->setSourceFile(pm.sourceFile);
    mesh->setSourceNodeName(pm.sourceNodeName);
    mesh->setVariantMappings(pm.variantMappings);
    mesh->setAllVariantMaterials(pm.allVariantMaterials);

    // Upload VBO data
    mesh->setMeshData(pm.vertices, pm.indices);

    // Restore skeletal skinning data so bone animations work after MVF reload.
    if (!pm.skinJoints.isEmpty())
        mesh->setSkinJoints(pm.skinJoints);

    // Restore morph target geometry so blend-shape animations work after MVF reload.
    if (!pm.morphTargets.isEmpty())
        mesh->setMorphTargets(pm.morphTargets, pm.defaultMorphWeights);

    // Resolve textures and set material
    const GLMaterial resolved = resolveMaterialTextures(this, pm.material);
    mesh->setMaterial(resolved);
    mesh->setTextureMaps(resolved);
    mesh->invertOpacityADSMap(resolved.isOpacityMapInverted());
    mesh->invertOpacityPBRMap(resolved.isOpacityMapInverted());

    // Restore per-mesh user transform (gizmo TRS) saved in the MVF file.
    {
        const QVector3D defaultScale(1.0f, 1.0f, 1.0f);
        const bool hasTranslation = !pm.meshTranslation.isNull();
        const bool hasScale       = (pm.meshScale != defaultScale);
        const bool hasRotation    = !pm.meshRotationQuat.isIdentity();
        if (hasTranslation || hasRotation || hasScale)
        {
            mesh->setTranslation(pm.meshTranslation);
            mesh->setRotationQuaternion(pm.meshRotationQuat, pm.meshRotation);
            mesh->setScaling(pm.meshScale);
        }
    }

    // Add to display list and track in pending UUIDs (like AssImp's onMeshBatchReady)
    addToDisplay(mesh);
    _pendingSceneUuids.append(mesh->uuid());
}

// ---------------------------------------------------------------------------
// loadMvfMeshes — legacy combined entry point
// ---------------------------------------------------------------------------
bool GLWidget::loadMvfMeshes(const Mvf::Document& document,
                               const QByteArray& geometryChunk,
                               const QByteArray& imageChunk)
{
    QVector<PreparedMvfMesh> prepared = prepareMvfMeshes(document, geometryChunk, imageChunk);
    return uploadPreparedMvfMeshes(prepared);
}

void GLWidget::setParsedLights(const GltfLightData& lightData)
{
    _pendingLightData = lightData;

    // If this model carries no punctual lights, leave the existing GPU light
    // state intact.  Another model already loaded may have punctual lights
    // registered in SceneGraph; wiping _originalParsedLights here would make
    // those gizmos vanish and incorrectly trigger the fallback-light path.
    // onSceneLightDataChanged() (connected to SceneGraph::lightDataChanged)
    // is the authoritative place to rebuild from all registered files.
    if (lightData.isEmpty())
        return;

    // Extract the flat GPU list for animation / repositioning code that
    // still works in terms of std::vector<GPULight>.
    // _lightFileIndexMap is cleared here and rebuilt by onSceneLightDataChanged
    // (fired via setLightData in appendFromSceneAsync) once the data is in SceneGraph.
    _originalParsedLights.clear();
    _lightFileIndexMap.clear();
    for (const GltfLightEntry& entry : lightData.lights)
        _originalParsedLights.push_back(entry.gpuLight);

    _currentRepositionedLights.clear();
    _animatedLightTransformSourceFile.clear();
    _animatedParsedLights.clear();
    _animatedLightVisibilitySourceFile.clear();
    _animatedLightVisibilityMask.clear();
    _animatedMeshVisibilitySourceFile.clear();
    _animatedHiddenMeshUuids.clear();

    syncPunctualLightUniforms(static_cast<int>(_originalParsedLights.size()),
                              !_originalParsedLights.empty());
}

// ---------------------------------------------------------------------------
// onSceneLightDataChanged
//
// Slot connected to SceneGraph::lightDataChanged.  Called whenever a file's
// light data is registered (setLightData) or removed (clearLightData).
// Rebuilds _originalParsedLights from the full set of SceneGraph-registered
// lights so that gizmos and the repositioning system always reflect the
// current multi-model scene, not just the last model that was loaded.
// ---------------------------------------------------------------------------
void GLWidget::onSceneLightDataChanged()
{
    if (!_viewer || !_viewer->sceneGraph())
        return;

    // Collect ALL GPULights across all files (ignoring per-entry enabled flag
    // so the repositioning baseline always covers the complete original set).
    // Also build _lightFileIndexMap so updatePunctualLights() can filter the
    // repositioned list by each light's current enabled state before GPU upload.
    _originalParsedLights.clear();
    _lightFileIndexMap.clear();
    const QStringList files = _viewer->sceneGraph()->filesWithLights();
    for (const QString& file : files)
    {
        const GltfLightData& ld = _viewer->sceneGraph()->lightDataForFile(file);
        for (int i = 0; i < static_cast<int>(ld.lights.size()); ++i)
        {
            _originalParsedLights.push_back(ld.lights[i].gpuLight);
            _lightFileIndexMap.append({file, i});
        }
    }

    if (_originalParsedLights.empty())
    {
        // All lights have been removed — clear the GPU buffer immediately.
        _currentRepositionedLights.clear();
        makeCurrent();
        glLights->setLights({});
        syncPunctualLightUniforms(0, false);
        return;
    }

    // Rebuild the working light set from the new per-file registrations.
    // Positions derive from each file's parsed lights plus that file's own
    // user model transform — adding/removing other files cannot move them.
    makeCurrent();
    updatePunctualLights();
}

// ---------------------------------------------------------------------------
// setDebugTextureEnabled / clearDebugTextureOverrides
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Per-unit scalar override helpers
//
// When a texture is disabled the replacement is white (1,1,1) — a neutral
// value that leaves all multiplicative channels visible on a lit mesh.
// That is the right neutral for modulating channels such as AO, roughness,
// metallic and normal: their effect can only be seen if the base mesh remains
// lit and visible.
//
// Emissive is the exception: it is purely additive, so a white replacement
// drives full-strength emission via the scalar factor.  Unit 12 therefore
// gets _debugBlackTex instead of white, which silences ADS directly
// (matEmissive = sample(black) = vec3(0)).  Scalar uniforms are also zeroed
// so PBR (emissiveStrength) is suppressed without relying on a bool override.
//
//  Unit 12 – emissive (black texture + scalar zeroing)
//    ADS: matEmissive = sample(black) = vec3(0)        — silenced by texture
//    PBR: pbrLighting.emissiveStrength = 0             — silenced by scalar
//
// All other units (including albedo / diffuse) receive only the white texture
// replacement — their scalar factors are left at their real values so the mesh
// stays normally lit, which is necessary for channels like AO to be visible.
// ---------------------------------------------------------------------------
namespace
{
void setScalarOverridesForUnit(TriangleMesh* mesh, int unit)
{
	switch (unit)
	{
	case 12: // emissive — additive channel, must be fully suppressed
		mesh->setDebugUniformOverride("pbrLighting.emissiveStrength",
		    QVariant::fromValue<float>(0.0f));
		mesh->setDebugUniformOverride("material.emission",
		    QVariant::fromValue(QVector3D(0.0f, 0.0f, 0.0f)));
		// ADS: hasEmissiveTexture suppression via bool uniform is unreliable (QVariant
		// type matching). Silence ADS by substituting a black texture for unit 12 instead
		// (matEmissive = sample(black) = vec3(0)). PBR is covered by emissiveStrength=0.
		break;
	default:
		break;
	}
}

void clearScalarOverridesForUnit(TriangleMesh* mesh, int unit)
{
	switch (unit)
	{
	case 12:
		mesh->clearDebugUniformOverride("pbrLighting.emissiveStrength");
		mesh->clearDebugUniformOverride("material.emission");
		mesh->markUniformsDirty();  // force setupUniforms to restore the real values
		break;
	default:
		break;
	}
}
} // anonymous namespace

void GLWidget::setDebugTextureEnabled(int meshId, int unitIndex, bool enabled)
{
	if (meshId < 0 || meshId >= static_cast<int>(_meshStore.size()) || !_meshStore[meshId])
		return;

	TriangleMesh* mesh = _meshStore[meshId];

	if (enabled)
	{
		mesh->clearDebugTextureOverride(unitIndex);
		clearScalarOverridesForUnit(mesh, unitIndex);
	}
	else
	{
		// Normal-map units get flat tangent-space normal (0,0,1).
		// Emissive unit gets black (0,0,0) so the ADS path (which overwrites the
		// scalar directly from the sample) contributes nothing without needing a
		// bool-uniform override. PBR is covered by emissiveStrength=0 scalar.
		// All other units get neutral white (1,1,1).
		const bool isNormalUnit   = (unitIndex == 13 || unitIndex == 20);
		const bool isEmissiveUnit = (unitIndex == 12);
		const GLuint replaceTex = isNormalUnit   ? _debugNormalTex :
		                          isEmissiveUnit ? _debugBlackTex  : _debugNeutralTex;
		mesh->setDebugTextureOverride(unitIndex, replaceTex);
		setScalarOverridesForUnit(mesh, unitIndex);
	}
	update();
}

void GLWidget::clearDebugTextureOverrides(int meshId)
{
	if (meshId >= 0 && meshId < static_cast<int>(_meshStore.size()) && _meshStore[meshId])
		_meshStore[meshId]->clearAllDebugTextureOverrides();
	update();
}

void GLWidget::clearAllDebugOverrides(int meshId)
{
	if (meshId >= 0 && meshId < static_cast<int>(_meshStore.size()) && _meshStore[meshId])
	{
		TriangleMesh* mesh = _meshStore[meshId];
		mesh->clearAllDebugTextureOverrides();
		mesh->clearAllDebugUniformOverrides();
		// Re-write the current global channel so debugChannelOutput stays consistent
		// after the override map was wiped.  If the panel is closing, the caller
		// follows up with setGlobalDebugChannel(0) which resets all meshes.
		mesh->setDebugUniformOverride("debugChannelOutput",
		    QVariant::fromValue<int>(_globalDebugChannel));
		mesh->markUniformsDirty();
	}
	update();
}

// ---------------------------------------------------------------------------
// applyDebugTextureState
// ---------------------------------------------------------------------------
// Full-state replacement for the per-toggle setDebugTextureEnabled path.
// Called by TextureDebugPanel whenever any checkbox changes so the entire
// enabled/disabled set can be evaluated at once.
// NOTE: does NOT touch debugChannelOutput — that uniform is owned exclusively
// by setGlobalDebugChannel.
void GLWidget::applyDebugTextureState(int meshId,
                                       const QSet<int>& enabledUnits,
                                       const QSet<int>& allUnits)
{
	if (meshId < 0 || meshId >= static_cast<int>(_meshStore.size()) || !_meshStore[meshId])
		return;
	TriangleMesh* mesh = _meshStore[meshId];

	// All textures active → clear all per-mesh overrides; no replacements needed.
	if (enabledUnits == allUnits)
	{
		for (int unit : allUnits)
		{
			mesh->clearDebugTextureOverride(unit);
			clearScalarOverridesForUnit(mesh, unit);
		}
		mesh->markUniformsDirty();
		update();
		return;
	}

	// Partial selection: replace disabled slots with neutral textures.
	for (int unit : allUnits)
	{
		if (enabledUnits.contains(unit))
		{
			mesh->clearDebugTextureOverride(unit);
			clearScalarOverridesForUnit(mesh, unit);
		}
		else
		{
			const bool isNormalUnit   = (unit == 13 || unit == 20);
			const bool isEmissiveUnit = (unit == 12);
			const GLuint replaceTex = isNormalUnit   ? _debugNormalTex :
			                          isEmissiveUnit ? _debugBlackTex  : _debugNeutralTex;
			mesh->setDebugTextureOverride(unit, replaceTex);
			setScalarOverridesForUnit(mesh, unit);
		}
	}
	mesh->markUniformsDirty();
	update();
}

// ---------------------------------------------------------------------------
// setGlobalDebugChannel
// ---------------------------------------------------------------------------
// Activates or clears single-channel isolation for the channel dropdown.
// Applied to every mesh in _meshStore — no mesh selection required.
// channelId == 0 restores normal rendering on all meshes.
void GLWidget::setGlobalDebugChannel(int channelId)
{
	_globalDebugChannel = channelId;
	makeCurrent();
	for (TriangleMesh* mesh : _meshStore)
	{
		if (!mesh) continue;
		if (channelId != 0)
		{
			// Channel isolation must ignore all checkbox/extension override state.
			// Clear every per-mesh debug override first, then install only the
			// requested debugChannelOutput override below.
			mesh->clearAllDebugTextureOverrides();
			mesh->clearAllDebugUniformOverrides();
		}
		mesh->setDebugUniformOverride("debugChannelOutput",
		    QVariant::fromValue<int>(channelId));
		mesh->markUniformsDirty();
	}
	doneCurrent();
	update();
}

// ---------------------------------------------------------------------------
// setDebugExtensionEnabled / clearDebugExtensionOverrides
// ---------------------------------------------------------------------------
// Extension key → { float uniform overrides, vec3 uniform overrides, texture units }
namespace
{
struct ExtOverrideDef
{
	QVector<QPair<QString, float>>      floatUniforms;
	QVector<QPair<QString, QVector3D>>  vec3Uniforms;
	QVector<QPair<QString, bool>>       boolUniforms;
	QVector<int>                        textureUnits;
};

const QMap<QString, ExtOverrideDef>& extensionOverrideDefs()
{
	// Build once, return by const ref.  Uses explicit qMakePair everywhere
	// to avoid MSVC brace-init ambiguity with QPair<QString,T> from const char*.
	static QMap<QString, ExtOverrideDef> defs;
	if (!defs.isEmpty())
		return defs;

	// Sheen
	{
		ExtOverrideDef d;
		d.floatUniforms << qMakePair(QString("pbrLighting.sheenRoughness"), 0.0f);
		d.vec3Uniforms  << qMakePair(QString("pbrLighting.sheenColor"),     QVector3D(0,0,0));
		d.textureUnits  << 26 << 27;
		defs["Sheen"] = d;
	}
	// Clearcoat
	{
		ExtOverrideDef d;
		d.floatUniforms << qMakePair(QString("pbrLighting.clearcoat"), 0.0f);
		d.textureUnits  << 18 << 19 << 20;
		defs["Clearcoat"] = d;
	}
	// Iridescence
	{
		ExtOverrideDef d;
		d.floatUniforms << qMakePair(QString("pbrLighting.iridescenceFactor"), 0.0f);
		d.textureUnits  << 24 << 25;
		defs["Iridescence"] = d;
	}
	// Volume / SSS
	{
		ExtOverrideDef d;
		d.floatUniforms << qMakePair(QString("pbrLighting.thicknessFactor"), 0.0f);
		d.textureUnits  << 30;
		defs["Volume / SSS"] = d;
	}
	// Specular
	{
		ExtOverrideDef d;
		d.floatUniforms << qMakePair(QString("pbrLighting.specularFactor"), 0.0f);
		d.textureUnits  << 21 << 22;
		defs["Specular"] = d;
	}
	// Anisotropy
	{
		ExtOverrideDef d;
		d.floatUniforms << qMakePair(QString("pbrLighting.anisotropyStrength"), 0.0f);
		d.textureUnits  << 23;
		defs["Anisotropy"] = d;
	}
	// Transmission
	{
		ExtOverrideDef d;
		d.floatUniforms << qMakePair(QString("pbrLighting.transmission"), 0.0f);
		d.textureUnits  << 28;
		defs["Transmission"] = d;
	}
	// Diffuse Transmission
	{
		ExtOverrideDef d;
		d.floatUniforms << qMakePair(QString("pbrLighting.diffuseTransmissionFactor"), 0.0f);
		d.textureUnits  << 34 << 35;
		defs["Diffuse Transmission"] = d;
	}
	// IOR — revert to glTF default (1.5) when disabled
	{
		ExtOverrideDef d;
		d.floatUniforms << qMakePair(QString("pbrLighting.ior"), 1.5f);
		d.textureUnits  << 29;
		defs["IOR"] = d;
	}
	// Emissive Strength — revert to neutral multiplier (1.0) when disabled
	{
		ExtOverrideDef d;
		d.floatUniforms << qMakePair(QString("pbrLighting.emissiveStrength"), 1.0f);
		defs["Emissive Strength"] = d;
	}
	// Dispersion — zero out chromatic dispersion when disabled
	{
		ExtOverrideDef d;
		d.floatUniforms << qMakePair(QString("pbrLighting.dispersion"), 0.0f);
		defs["Dispersion"] = d;
	}
	// Volume Scattering — disable the Burley SSS pass when disabled
	{
		ExtOverrideDef d;
		d.boolUniforms << qMakePair(QString("hasVolumeScattering"), false);
		defs["Volume Scattering"] = d;
	}
	return defs;
}
} // anonymous namespace

void GLWidget::setDebugExtensionEnabled(int meshId, const QString& extensionKey, bool enabled)
{
	if (meshId < 0 || meshId >= static_cast<int>(_meshStore.size()) || !_meshStore[meshId])
		return;

	TriangleMesh* mesh = _meshStore[meshId];
	const auto& defs = extensionOverrideDefs();
	auto it = defs.constFind(extensionKey);
	if (it == defs.constEnd())
		return;

	const ExtOverrideDef& def = it.value();

	if (enabled)
	{
		// Remove overrides; force uniforms to re-run so originals are restored.
		for (const auto& kv : def.floatUniforms)
			mesh->clearDebugUniformOverride(kv.first);
		for (const auto& kv : def.vec3Uniforms)
			mesh->clearDebugUniformOverride(kv.first);
		for (const auto& kv : def.boolUniforms)
			mesh->clearDebugUniformOverride(kv.first);
		for (int unit : def.textureUnits)
			mesh->clearDebugTextureOverride(unit);
		mesh->markUniformsDirty();
	}
	else
	{
		// Suppress the extension's contribution via uniform overrides.
		for (const auto& kv : def.floatUniforms)
			mesh->setDebugUniformOverride(kv.first, QVariant::fromValue<float>(kv.second));
		for (const auto& kv : def.vec3Uniforms)
			mesh->setDebugUniformOverride(kv.first, QVariant::fromValue(kv.second));
		for (const auto& kv : def.boolUniforms)
			mesh->setDebugUniformOverride(kv.first, QVariant::fromValue<bool>(kv.second));
		// Neutral-bind the extension's texture units.
		for (int unit : def.textureUnits)
		{
			const bool isNormalUnit = (unit == 13 || unit == 20);
			mesh->setDebugTextureOverride(unit, isNormalUnit ? _debugNormalTex : _debugNeutralTex);
		}
	}
	update();
}

void GLWidget::clearDebugExtensionOverrides(int meshId)
{
	if (meshId < 0 || meshId >= static_cast<int>(_meshStore.size()) || !_meshStore[meshId])
		return;

	TriangleMesh* mesh = _meshStore[meshId];
	mesh->clearAllDebugUniformOverrides();
	mesh->markUniformsDirty();
	update();
}

// ---------------------------------------------------------------------------
// requestTextureReadback
// Reads back every per-mesh texture slot for the given _meshStore index and
// emits textureReadbackReady() with one TextureSlotInfo per slot.
// Inactive slots (textureId == 0) are included with isActive = false and a
// null thumbnail so the debug panel can show a placeholder if desired.
// ---------------------------------------------------------------------------
void GLWidget::requestTextureReadback(int meshId)
{
	if (meshId < 0 || meshId >= static_cast<int>(_meshStore.size()) || !_meshStore[meshId])
	{
		emit textureReadbackReady({}, {});
		return;
	}

	makeCurrent();

	TriangleMesh*    mesh    = _meshStore[meshId];
	const GLMaterial& mat    = mesh->getMaterial();
	const QString    meshName = mesh->getName();

	// baseColorTex mirrors the logic in TriangleMesh::setupTextures() so the
	// debug panel shows what is actually bound on unit 10.
	const GLuint baseColorTex = mat.hasAlbedoMap()
		? static_cast<GLuint>(mat.albedoTextureId())
		: (mat.hasDiffuseMap() ? static_cast<GLuint>(mat.diffuseTextureId()) : 0U);

	// Pre-compute extension active flags from the material.
	// These are true whenever the KHR extension is in use — even if no texture
	// is bound (e.g. sheen colour factor set but no sheen texture).
	// specularFactor defaults to 1.0 in glTF, so we consider KHR_materials_specular
	// active when it deviates from the default or a specular texture is present.
	const bool extSheen      = mat.hasSheen()
	                           || mat.hasSheenColorMap()
	                           || mat.hasSheenRoughnessMap();
	const bool extClearcoat  = mat.hasClearcoat()
	                           || mat.hasClearcoatColorMap()
	                           || mat.hasClearcoatRoughnessMap()
	                           || mat.hasClearcoatNormalMap();
	const bool extIridescence= mat.iridescenceFactor() > 0.0f
	                           || mat.hasIridescenceMap()
	                           || mat.hasIridescenceThicknessMap();
	const bool extVolume     = mat.hasVolumeScattering()
	                           || mat.thicknessFactor() > 0.0f
	                           || mat.hasThicknessMap();
	const bool extSpecular   = mat.hasSpecularFactorMap() || mat.hasSpecularColorMap()
	                           || mat.specularFactor() != 1.0f
	                           || mat.specularColorFactor() != QVector3D(1.0f, 1.0f, 1.0f);
	const bool extAnisotropy = mat.anisotropyStrength() != 0.0f || mat.hasAnisotropyMap();
	const bool extTransmission = mat.hasTransmission()
	                             || mat.hasTransmissionMap();
	const bool extDiffuseTrans = mat.diffuseTransmissionFactor() > 0.0f
	                             || mat.hasDiffuseTransmissionMap()
	                             || mat.hasDiffuseTransmissionColorMap();
	// IOR defaults to 1.5 for every material (glTF spec) so mat.ior() > 0 is
	// always true.  Use deviation from 1.5 (explicit extension value) or a
	// texture as the activity signal; scalar marker slot 203 carries this flag.
	const bool extIOR          = mat.hasIORMap();                      // real unit 29
	const bool extIORScalar    = (mat.ior() != 1.5f) || mat.hasIORMap(); // marker unit 203

	struct SlotDef
	{
		QString name;
		int     unit;
		GLuint  texId;
		bool    extEnabled;   // parent KHR extension is active (with or without texture)
	};

	const QVector<SlotDef> defs = {
		{ "albedo / diffuse",         10, baseColorTex,                                                                                    false          },
		{ "metallicMap",              11, mat.hasMetallicMap()            ? static_cast<GLuint>(mat.metallicTextureId())            : 0U,  false          },
		{ "emissiveMap",              12, mat.hasEmissiveMap()             ? static_cast<GLuint>(mat.emissiveTextureId())             : 0U, false          },
		{ "normalMap",                13, mat.hasNormalMap()               ? static_cast<GLuint>(mat.normalTextureId())               : 0U, false          },
		{ "heightMap",                14, mat.hasHeightMap()               ? static_cast<GLuint>(mat.heightTextureId())               : 0U, false          },
		{ "opacityMap",               15, mat.hasOpacityMap()              ? static_cast<GLuint>(mat.opacityTextureId())              : 0U, false          },
		{ "roughnessMap",             16, mat.hasRoughnessMap()            ? static_cast<GLuint>(mat.roughnessTextureId())            : 0U, false          },
		{ "aoMap",                    17, mat.hasAOMap()                   ? static_cast<GLuint>(mat.occlusionTextureId())            : 0U, false          },
		{ "clearcoatColorMap",        18, mat.hasClearcoatColorMap()       ? static_cast<GLuint>(mat.clearcoatColorTextureId())       : 0U, extClearcoat   },
		{ "clearcoatRoughnessMap",    19, mat.hasClearcoatRoughnessMap()   ? static_cast<GLuint>(mat.clearcoatRoughnessTextureId())   : 0U, extClearcoat   },
		{ "clearcoatNormalMap",       20, mat.hasClearcoatNormalMap()      ? static_cast<GLuint>(mat.clearcoatNormalTextureId())      : 0U, extClearcoat   },
		{ "specularFactorMap",        21, mat.hasSpecularFactorMap()       ? static_cast<GLuint>(mat.specularFactorTextureId())       : 0U, extSpecular    },
		{ "specularColorMap",         22, mat.hasSpecularColorMap()        ? static_cast<GLuint>(mat.specularColorTextureId())        : 0U, extSpecular    },
		{ "anisotropyMap",            23, mat.hasAnisotropyMap()           ? static_cast<GLuint>(mat.anisotropyTextureId())           : 0U, extAnisotropy  },
		{ "iridescenceMap",           24, mat.hasIridescenceMap()          ? static_cast<GLuint>(mat.iridescenceTextureId())          : 0U, extIridescence },
		{ "iridescenceThicknessMap",  25, mat.hasIridescenceThicknessMap() ? static_cast<GLuint>(mat.iridescenceThicknessTextureId()) : 0U, extIridescence },
		{ "sheenColorMap",            26, mat.hasSheenColorMap()           ? static_cast<GLuint>(mat.sheenColorTextureId())           : 0U, extSheen       },
		{ "sheenRoughnessMap",        27, mat.hasSheenRoughnessMap()       ? static_cast<GLuint>(mat.sheenRoughnessTextureId())       : 0U, extSheen       },
		{ "transmissionMap",          28, mat.hasTransmissionMap()         ? static_cast<GLuint>(mat.transmissionTextureId())         : 0U, extTransmission},
		{ "iorMap",                   29, mat.hasIORMap()                  ? static_cast<GLuint>(mat.iorTextureId())                  : 0U, extIOR         },
		{ "diffuseTransmissionMap",   34, mat.hasDiffuseTransmissionMap()  ? static_cast<GLuint>(mat.diffuseTransmissionTextureId())  : 0U, extDiffuseTrans},
		{ "diffuseTransmissionColor", 35, mat.hasDiffuseTransmissionColorMap() ? static_cast<GLuint>(mat.diffuseTransmissionColorTextureId()) : 0U, extDiffuseTrans},
		{ "thicknessMap",             30, mat.hasThicknessMap()            ? static_cast<GLuint>(mat.thicknessTextureId())            : 0U, extVolume      },
		// Scalar-activity markers (units 200-203): no real GL texture — used only
		// to drive the extension-panel activity dot for extensions that have no
		// dedicated texture slot.  isMarker is set to true in the loop below.
		{ "ior",                     203, 0U, extIORScalar                                       },
		{ "emissiveStrength",        200, 0U, mat.emissiveStrength() != 1.0f                     },
		{ "dispersion",              201, 0U, mat.dispersion() > 0.0f                            },
		{ "volumeScattering",        202, 0U, mat.hasVolumeScattering()                          },
	};

	constexpr int ThumbSize = 64;
	QVector<TextureSlotInfo> result;
	result.reserve(defs.size());

	for (const auto& d : defs)
	{
		TextureSlotInfo info;
		info.slotName         = d.name;
		info.unitIndex        = d.unit;
		info.textureId        = d.texId;
		info.isActive         = (d.texId != 0U);
		info.extensionEnabled = d.extEnabled;
		info.isMarker         = (d.unit >= 200);   // scalar-activity markers have no real GL unit

		if (info.isActive)
		{
			glBindTexture(GL_TEXTURE_2D, d.texId);
			GLint w = 0, h = 0;
			glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH,  &w);
			glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);

			if (w > 0 && h > 0)
			{
				QByteArray buf(w * h * 4, '\0');
				glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE,
				              reinterpret_cast<void*>(buf.data()));
				QImage img(reinterpret_cast<const uchar*>(buf.constData()),
				           w, h, w * 4, QImage::Format_RGBA8888);
				// Deep-copy before the buffer goes out of scope
				info.thumbnail = QPixmap::fromImage(
				    img.copy().scaled(ThumbSize, ThumbSize,
				                     Qt::KeepAspectRatio, Qt::SmoothTransformation));
			}
		}

		result.push_back(std::move(info));
	}

	doneCurrent();
	emit textureReadbackReady(result, meshName);
}
