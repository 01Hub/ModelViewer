#include "config.h"
#include "Point.h"
#include "TriangleBaldwinWeber.h"
#include "TriangleMesh.h"
#include "TriangleMollerTrumbore.h"
#include "Utils.h"
#include <algorithm>
#include <atomic>
#include <iostream>
#include <QApplication>
#include <QQuaternion>
#include <QVector3D>

namespace
{
QQuaternion meshEulerToQuaternion(const QVector3D& rotation)
{
	QMatrix4x4 matrix;
	matrix.setToIdentity();
	matrix.rotate(rotation.x(), QVector3D(1.0f, 0.0f, 0.0f));
	matrix.rotate(rotation.y(), QVector3D(0.0f, 1.0f, 0.0f));
	matrix.rotate(rotation.z(), QVector3D(0.0f, 0.0f, 1.0f));
	return QQuaternion::fromRotationMatrix(matrix.toGenericMatrix<3, 3>()).normalized();
}
}

TriangleMesh::TriangleMesh(QOpenGLShaderProgram* prog, const QString name) : Drawable(prog),
_nVerts(0),
_fallbackTexture(0),
_sMax(1),
_tMax(1),
_hasTextureAlpha(false),
_hasVertexColors(false),
_baseThicknessFactor(0.0f),
_baseAttenuationDistance(std::numeric_limits<float>::infinity())
{
	setAutoIncrName(name);
	_memorySize = 0;
	_transX = _transY = _transZ = 0.0f;
	_rotateX = _rotateY = _rotateZ = 0.0f;
	_scaleX = _scaleY = _scaleZ = 1.0f;
	_rotationQuat = QQuaternion();
	_transformation.setToIdentity();
	_sceneRenderTransform.setToIdentity();

	_indexBuffer = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
	_positionBuffer = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
	_normalBuffer = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
	_colorBuffer = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
	_texCoord0Buffer = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
	_texCoord1Buffer = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
	_texCoord2Buffer = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
	_texCoord3Buffer = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
	_tangentBuf = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
	_bitangentBuf = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
	_jointIndexBuffer = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
	_jointWeightBuffer = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);

	_indexBuffer.create();
	_positionBuffer.create();
	_normalBuffer.create();
	_colorBuffer.create();
	_texCoord0Buffer.create();
	_texCoord1Buffer.create();
	_texCoord2Buffer.create();
	_texCoord3Buffer.create();
	_tangentBuf.create();
	_bitangentBuf.create();
	_jointIndexBuffer.create();
	_jointWeightBuffer.create();

	_vertexArrayObject.create();

	QImage dummy(128, 128, QImage::Format_ARGB32);
	dummy.fill(Qt::white);
	_fallbackTextureBuffer = dummy;
	_fallbackTextureImage = convertToGLFormat(_fallbackTextureBuffer);

	glGenTextures(1, &_fallbackTexture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, _fallbackTexture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
}

void TriangleMesh::cacheBaseVolumeProperties()
{
	_baseThicknessFactor = _material.thicknessFactor();
	_baseAttenuationDistance = _material.attenuationDistance();
}

void TriangleMesh::applyScaledVolumeProperties()
{
	// Runtime mesh scaling is already applied in the shader through modelMatrix
	// when evaluating volume thickness/refraction. Keep the authored material
	// values here to avoid double-applying user scale on the CPU.
	_material.setThicknessFactor(_baseThicknessFactor);
	_material.setAttenuationDistance(_baseAttenuationDistance);
}

void TriangleMesh::initBuffers(
	std::vector<unsigned int>* indices,
	std::vector<float>* points,
	std::vector<float>* normals,
	std::vector<float>* colors,
	std::vector<float>* texCoords,
	std::vector<float>* tangents,
	std::vector<float>* bitangents,
	std::vector<float>* jointIndices,
	std::vector<float>* jointWeights)
{
	// Must have data for indices, points, and normals
	if (indices == nullptr || points == nullptr || normals == nullptr)
		return;

	_indices = *indices;
	_points = *points;
	_trsfPoints = _points;
	_normals = *normals;
	_trsfNormals = _normals;
	_trsfTangents.clear();
	_trsfBitangents.clear();

	// Compute the local-space bounding box from the raw (untransformed) points.
	// This is used by setSceneRenderTransformFast() to cheaply keep _boundingBox
	// correct for animated meshes without re-transforming every vertex.
	if (!_points.empty())
	{
		float lxMin =  std::numeric_limits<float>::max();
		float lyMin =  std::numeric_limits<float>::max();
		float lzMin =  std::numeric_limits<float>::max();
		float lxMax = -std::numeric_limits<float>::max();
		float lyMax = -std::numeric_limits<float>::max();
		float lzMax = -std::numeric_limits<float>::max();
		for (size_t i = 0; i < _points.size(); i += 3)
		{
			lxMin = std::min(lxMin, _points[i]);
			lyMin = std::min(lyMin, _points[i + 1]);
			lzMin = std::min(lzMin, _points[i + 2]);
			lxMax = std::max(lxMax, _points[i]);
			lyMax = std::max(lyMax, _points[i + 1]);
			lzMax = std::max(lzMax, _points[i + 2]);
		}
		_localBoundingBox.setLimits(
			static_cast<double>(lxMin), static_cast<double>(lxMax),
			static_cast<double>(lyMin), static_cast<double>(lyMax),
			static_cast<double>(lzMin), static_cast<double>(lzMax));
	}

	// build the triangles for selection
	buildTriangles();

	if (colors)
	{
		_colors = *colors;
		_hasVertexColors = true;
	}

	if (texCoords)
		_texCoords = *texCoords;
	if (tangents)
	{
		_tangents = *tangents;
		_trsfTangents = _tangents;
	}
	if (bitangents)
	{
		_bitangents = *bitangents;
		_trsfBitangents = _bitangents;
	}
	if (jointIndices)
		_jointIndices = *jointIndices;
	if (jointWeights)
		_jointWeights = *jointWeights;

	_memorySize = 0;
	_memorySize = (_points.size() + _normals.size() + _indices.size()) * sizeof(float);

	_nVerts = indices->empty()
		? static_cast<unsigned int>(points->size() / 3)
		: static_cast<unsigned int>(indices->size());

	_buffers.push_back(_indexBuffer);
	_indexBuffer.bind();
	_indexBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
	_indexBuffer.allocate(indices->data(), static_cast<int>(indices->size() * sizeof(unsigned int)));

	_buffers.push_back(_positionBuffer);
	_positionBuffer.bind();
	_positionBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
	_positionBuffer.allocate(points->data(), static_cast<int>(points->size() * sizeof(float)));

	_buffers.push_back(_normalBuffer);
	_normalBuffer.bind();
	_normalBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
	_normalBuffer.allocate(normals->data(), static_cast<int>(normals->size() * sizeof(float)));


	if(_colors.size())
	{
		_buffers.push_back(_colorBuffer);
		_colorBuffer.bind();
		_colorBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
		_colorBuffer.allocate(_colors.data(), static_cast<int>(_colors.size() * sizeof(float)));
		_memorySize += _colors.size() * sizeof(float);
	}

	if (_tangents.size())
	{
		_buffers.push_back(_tangentBuf);
		_tangentBuf.bind();
		_tangentBuf.setUsagePattern(QOpenGLBuffer::StaticDraw);
		_tangentBuf.allocate(_tangents.data(), static_cast<int>(_tangents.size() * sizeof(float)));
		_memorySize += _tangents.size() * sizeof(float);
	}

	if (_bitangents.size())
	{
		_buffers.push_back(_bitangentBuf);
		_bitangentBuf.bind();
		_bitangentBuf.setUsagePattern(QOpenGLBuffer::StaticDraw);
		_bitangentBuf.allocate(_bitangents.data(), static_cast<int>(_bitangents.size() * sizeof(float)));
		_memorySize += _bitangents.size() * sizeof(float);
	}

	if (_jointIndices.size())
	{
		_buffers.push_back(_jointIndexBuffer);
		_jointIndexBuffer.bind();
		_jointIndexBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
		_jointIndexBuffer.allocate(_jointIndices.data(), static_cast<int>(_jointIndices.size() * sizeof(float)));
		_memorySize += _jointIndices.size() * sizeof(float);
	}

	if (_jointWeights.size())
	{
		_buffers.push_back(_jointWeightBuffer);
		_jointWeightBuffer.bind();
		_jointWeightBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
		_jointWeightBuffer.allocate(_jointWeights.data(), static_cast<int>(_jointWeights.size() * sizeof(float)));
		_memorySize += _jointWeights.size() * sizeof(float);
	}

	_vertexArrayObject.bind();

	_indexBuffer.bind();

	// _position
	_positionBuffer.bind();
	//glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	//glEnableVertexAttribArray(0);  // Vertex position
	_prog->enableAttributeArray("vertexPosition");
	_prog->setAttributeBuffer("vertexPosition", GL_FLOAT, 0, 3);

	// Normal
	_normalBuffer.bind();
	//glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
	//glEnableVertexAttribArray(1);  // Normal
	_prog->enableAttributeArray("vertexNormal");
	_prog->setAttributeBuffer("vertexNormal", GL_FLOAT, 0, 3);

	if (_hasVertexColors)
	{
		_colorBuffer.bind();
		_prog->enableAttributeArray("vertexColor");
		_prog->setAttributeBuffer("vertexColor", GL_FLOAT, 0, 4);
	}

	if (_tangents.size())
	{
		_tangentBuf.bind();
		//glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 0, 0);
		//glEnableVertexAttribArray(3);  // Tangents
		_prog->enableAttributeArray("vertexTangent");
		_prog->setAttributeBuffer("vertexTangent", GL_FLOAT, 0, 3);
	}

	if (_bitangents.size())
	{
		_bitangentBuf.bind();
		_prog->enableAttributeArray("vertexBitangent");
		_prog->setAttributeBuffer("vertexBitangent", GL_FLOAT, 0, 3);
	}

	if (_jointIndices.size())
	{
		_jointIndexBuffer.bind();
		_prog->enableAttributeArray("jointIndices");
		_prog->setAttributeBuffer("jointIndices", GL_FLOAT, 0, 4);
	}

	if (_jointWeights.size())
	{
		_jointWeightBuffer.bind();
		_prog->enableAttributeArray("jointWeights");
		_prog->setAttributeBuffer("jointWeights", GL_FLOAT, 0, 4);
	}

	// Tex coords
	if (_texCoords.size())
	{
		size_t numVertices = _indices.size() > 0 ? _indices.size() : 0;
		// If we're here, texCoords was populated with 8 floats per vertex

		// Extract 4 separate texCoord sets from the interleaved buffer
		std::vector<float> texCoord0, texCoord1, texCoord2, texCoord3;
		texCoord0.reserve(numVertices * 2);
		texCoord1.reserve(numVertices * 2);
		texCoord2.reserve(numVertices * 2);
		texCoord3.reserve(numVertices * 2);

		// Deinterleave: every 8 floats = one vertex's 4 sets
		for (size_t i = 0; i < _texCoords.size(); i += 8)
		{
			// Set 0
			texCoord0.push_back(_texCoords[i + 0]);
			texCoord0.push_back(_texCoords[i + 1]);
			// Set 1
			texCoord1.push_back(_texCoords[i + 2]);
			texCoord1.push_back(_texCoords[i + 3]);
			// Set 2
			texCoord2.push_back(_texCoords[i + 4]);
			texCoord2.push_back(_texCoords[i + 5]);
			// Set 3
			texCoord3.push_back(_texCoords[i + 6]);
			texCoord3.push_back(_texCoords[i + 7]);
		}

		// Create separate VBOs for each texCoord set
		_buffers.push_back(_texCoord0Buffer);
		_texCoord0Buffer.bind();
		_texCoord0Buffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
		_texCoord0Buffer.allocate(texCoord0.data(), static_cast<int>(texCoord0.size() * sizeof(float)));
		_memorySize += texCoord0.size() * sizeof(float);

		_buffers.push_back(_texCoord1Buffer);
		_texCoord1Buffer.bind();
		_texCoord1Buffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
		_texCoord1Buffer.allocate(texCoord1.data(), static_cast<int>(texCoord1.size() * sizeof(float)));
		_memorySize += texCoord1.size() * sizeof(float);

		_buffers.push_back(_texCoord2Buffer);
		_texCoord2Buffer.bind();
		_texCoord2Buffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
		_texCoord2Buffer.allocate(texCoord2.data(), static_cast<int>(texCoord2.size() * sizeof(float)));
		_memorySize += texCoord2.size() * sizeof(float);

		_buffers.push_back(_texCoord3Buffer);
		_texCoord3Buffer.bind();
		_texCoord3Buffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
		_texCoord3Buffer.allocate(texCoord3.data(), static_cast<int>(texCoord3.size() * sizeof(float)));
		_memorySize += texCoord3.size() * sizeof(float);

		// Set up vertex attributes (stride = 0 for each, since they're separate)
		_texCoord0Buffer.bind();
		_prog->enableAttributeArray("texCoord0");
		_prog->setAttributeBuffer("texCoord0", GL_FLOAT, 0, 2, 0);

		_texCoord1Buffer.bind();
		_prog->enableAttributeArray("texCoord1");
		_prog->setAttributeBuffer("texCoord1", GL_FLOAT, 0, 2, 0);

		_texCoord2Buffer.bind();
		_prog->enableAttributeArray("texCoord2");
		_prog->setAttributeBuffer("texCoord2", GL_FLOAT, 0, 2, 0);

		_texCoord3Buffer.bind();
		_prog->enableAttributeArray("texCoord3");
		_prog->setAttributeBuffer("texCoord3", GL_FLOAT, 0, 2, 0);

	}

	_vertexArrayObject.release();
}

void TriangleMesh::buildTriangles()
{
	if (_triangles.size())
	{
		for (Triangle* t : _triangles)
			delete t;
		_memorySize -= _triangles.size() * sizeof(Triangle);
		_triangles.clear();
	}
	try
	{
		size_t offset = 3; // each index points to 3 floats
		for (size_t i = 0; i < _indices.size();)
		{
			// Vertex 1
			QVector3D v1(_trsfPoints.at(offset * _indices.at(i) + 0), // x coordinate
				_trsfPoints.at(offset * _indices.at(i) + 1),          // y coordinate
				_trsfPoints.at(offset * _indices.at(i) + 2));         // z coordinate
			i++;

			// Vertex 2
			QVector3D v2(_trsfPoints.at(offset * _indices.at(i) + 0), // x coordinate
				_trsfPoints.at(offset * _indices.at(i) + 1),          // y coordinate
				_trsfPoints.at(offset * _indices.at(i) + 2));         // z coordinate
			i++;

			// Vertex 3
			QVector3D v3(_trsfPoints[offset * _indices.at(i) + 0], // x coordinate
				_trsfPoints.at(offset * _indices.at(i) + 1),          // y coordinate
				_trsfPoints.at(offset * _indices.at(i) + 2));         // z coordinate
			i++;

			_triangles.push_back(new TriangleMollerTrumbore(v1, v2, v3, this));
		}
		_memorySize += _triangles.size() * sizeof(Triangle);
	}
	catch (const std::exception& ex)
	{
		std::cout << "Exception raised in TriangleMesh::buildTriangles\n" << ex.what() << std::endl;
	}
}

void TriangleMesh::setProg(QOpenGLShaderProgram* prog)
{
	_prog = prog;

	_vertexArrayObject.bind();

	//_indexBuffer.bind();

	// Position
	_positionBuffer.bind();
	_prog->enableAttributeArray("vertexPosition");
	_prog->setAttributeBuffer("vertexPosition", GL_FLOAT, 0, 3);

	// Normal
	_normalBuffer.bind();
	_prog->enableAttributeArray("vertexNormal");
	_prog->setAttributeBuffer("vertexNormal", GL_FLOAT, 0, 3);

	// Color
	if (_colors.size())
	{
		_hasVertexColors = true;
		_colorBuffer.bind();
		_prog->enableAttributeArray("vertexColor");
		_prog->setAttributeBuffer("vertexColor", GL_FLOAT, 0, 4);
	}

	// Tex coords
	if (_texCoords.size())
	{
		_texCoord0Buffer.bind();
		_prog->enableAttributeArray("texCoord0");
		_prog->setAttributeBuffer("texCoord0", GL_FLOAT, 0, 2);

		_texCoord1Buffer.bind();
		_prog->enableAttributeArray("texCoord1");
		_prog->setAttributeBuffer("texCoord1", GL_FLOAT, 0, 2);

		_texCoord2Buffer.bind();
		_prog->enableAttributeArray("texCoord2");
		_prog->setAttributeBuffer("texCoord2", GL_FLOAT, 0, 2);
		
		_texCoord3Buffer.bind();
		_prog->enableAttributeArray("texCoord3");
		_prog->setAttributeBuffer("texCoord3", GL_FLOAT, 0, 2);
	}

	// Tangents
	if (_tangents.size())
	{
		_tangentBuf.bind();
		_prog->enableAttributeArray("vertexTangent");
		_prog->setAttributeBuffer("vertexTangent", GL_FLOAT, 0, 3);
	}

	// Bitangents
	if (_bitangents.size())
	{
		_bitangentBuf.bind();
		_prog->enableAttributeArray("vertexBitangent");
		_prog->setAttributeBuffer("vertexBitangent", GL_FLOAT, 0, 3);
	}

	if (_jointIndices.size())
	{
		_jointIndexBuffer.bind();
		_prog->enableAttributeArray("jointIndices");
		_prog->setAttributeBuffer("jointIndices", GL_FLOAT, 0, 4);
	}

	if (_jointWeights.size())
	{
		_jointWeightBuffer.bind();
		_prog->enableAttributeArray("jointWeights");
		_prog->setAttributeBuffer("jointWeights", GL_FLOAT, 0, 4);
	}

	_vertexArrayObject.release();
	markUniformsDirty();
}

void TriangleMesh::setupTextures()
{
	const bool hasClearcoatColorTex = _material.hasClearcoatColorMap() || _material.clearcoatColorTextureId() != 0;
	const bool hasClearcoatRoughnessTex = _material.hasClearcoatRoughnessMap() || _material.clearcoatRoughnessTextureId() != 0;
	const bool hasClearcoatNormalTex = _material.hasClearcoatNormalMap() || _material.clearcoatNormalTextureId() != 0;
	const bool hasSpecularFactorTex = _material.hasSpecularFactorMap() || _material.specularFactorTextureId() != 0;
	const bool hasSpecularColorTex = _material.hasSpecularColorMap() || _material.specularColorTextureId() != 0;
	const bool hasAnisotropyTex = _material.hasAnisotropyMap() || _material.anisotropyTextureId() != 0;
	const bool hasIridescenceTex = _material.hasIridescenceMap() || _material.iridescenceTextureId() != 0;
	const bool hasIridescenceThicknessTex = _material.hasIridescenceThicknessMap() || _material.iridescenceThicknessTextureId() != 0;
	const bool hasSheenColorTex = _material.hasSheenColorMap() || _material.sheenColorTextureId() != 0;
	const bool hasSheenRoughnessTex = _material.hasSheenRoughnessMap() || _material.sheenRoughnessTextureId() != 0;
	const bool hasTransmissionTex = _material.hasTransmissionMap() || _material.transmissionTextureId() != 0;
	const bool hasIORTex = _material.hasIORMap() || _material.iorTextureId() != 0;
	const bool hasThicknessTex = _material.hasThicknessMap() || _material.thicknessTextureId() != 0;
	const bool hasDiffuseTransmissionTex = _material.hasDiffuseTransmissionMap() || _material.diffuseTransmissionTextureId() != 0;
	const bool hasDiffuseTransmissionColorTex = _material.hasDiffuseTransmissionColorMap() || _material.diffuseTransmissionColorTextureId() != 0;

	// Unit 10 is shared by three shader paths — ADS (texture_diffuse / texture_specular /
	// etc.), PBR metallic-roughness (albedoMap), and PBR specular-glossiness (diffuseMap) —
	// all of which sample unit 10 for the base/diffuse/albedo colour.
	// Fall back to the legacy diffuse texture for specular-glossiness materials that carry
	// hasDiffuseMap but not hasAlbedoMap; using only hasAlbedoMap here would leave unit 10
	// as 0 and black out the diffuse colour for those materials.
	const GLuint baseColorTex = _material.hasAlbedoMap()
		? static_cast<GLuint>(_material.albedoTextureId())
		: (_material.hasDiffuseMap() ? _material.diffuseTextureId() : 0U);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, _fallbackTexture);
	if (_textureBindingsDirty && !_fallbackTextureImage.isNull())
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _fallbackTextureImage.width(), _fallbackTextureImage.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, _fallbackTextureImage.bits());
		glGenerateMipmap(GL_TEXTURE_2D);
		_textureBindingsDirty = false;
	}

	// Texture maps (units 10–17): single unified bind — serves ADS, PBR metallic-roughness,
	// and PBR specular-glossiness simultaneously.  Previously there were two consecutive bind
	// blocks for the same units (ADS then PBR), with the PBR block overwriting the ADS one;
	// the ADS-only block obscured the specular-glossiness bug and has been removed.
	glActiveTexture(GL_TEXTURE10);
	glBindTexture(GL_TEXTURE_2D, baseColorTex);
	glActiveTexture(GL_TEXTURE11);
	glBindTexture(GL_TEXTURE_2D, _material.hasMetallicMap() ? static_cast<GLuint>(_material.metallicTextureId()) : 0U);
	glActiveTexture(GL_TEXTURE12);
	glBindTexture(GL_TEXTURE_2D, _material.hasEmissiveMap() ? static_cast<GLuint>(_material.emissiveTextureId()) : 0U);
	glActiveTexture(GL_TEXTURE13);
	glBindTexture(GL_TEXTURE_2D, _material.hasNormalMap() ? static_cast<GLuint>(_material.normalTextureId()) : 0U);
	glActiveTexture(GL_TEXTURE14);
	glBindTexture(GL_TEXTURE_2D, _material.hasHeightMap() ? static_cast<GLuint>(_material.heightTextureId()) : 0U);
	glActiveTexture(GL_TEXTURE15);
	glBindTexture(GL_TEXTURE_2D, _material.hasOpacityMap() ? static_cast<GLuint>(_material.opacityTextureId()) : 0U);
	glActiveTexture(GL_TEXTURE16);
	glBindTexture(GL_TEXTURE_2D, _material.hasRoughnessMap() ? static_cast<GLuint>(_material.roughnessTextureId()) : 0U);
	glActiveTexture(GL_TEXTURE17);
	glBindTexture(GL_TEXTURE_2D, _material.hasAOMap() ? static_cast<GLuint>(_material.occlusionTextureId()) : 0U);
	
	// Mesh-owned material block (units 10–29). These are the feature-complete
	// material slots we keep inside the guaranteed 0..31 range.
	glActiveTexture(GL_TEXTURE18);
	glBindTexture(GL_TEXTURE_2D, hasClearcoatColorTex ? static_cast<GLuint>(_material.clearcoatColorTextureId()) : 0U);
	glActiveTexture(GL_TEXTURE19);
	glBindTexture(GL_TEXTURE_2D, hasClearcoatRoughnessTex ? static_cast<GLuint>(_material.clearcoatRoughnessTextureId()) : 0U);
	glActiveTexture(GL_TEXTURE20);
	glBindTexture(GL_TEXTURE_2D, hasClearcoatNormalTex ? static_cast<GLuint>(_material.clearcoatNormalTextureId()) : 0U);
	glActiveTexture(GL_TEXTURE21);
	glBindTexture(GL_TEXTURE_2D, hasSpecularFactorTex ? static_cast<GLuint>(_material.specularFactorTextureId()) : 0U);
	glActiveTexture(GL_TEXTURE22);
	glBindTexture(GL_TEXTURE_2D, hasSpecularColorTex ? static_cast<GLuint>(_material.specularColorTextureId()) : 0U);
	glActiveTexture(GL_TEXTURE23);
	glBindTexture(GL_TEXTURE_2D, hasAnisotropyTex ? static_cast<GLuint>(_material.anisotropyTextureId()) : 0U);
	glActiveTexture(GL_TEXTURE24);
	glBindTexture(GL_TEXTURE_2D, hasIridescenceTex ? static_cast<GLuint>(_material.iridescenceTextureId()) : 0U);
	glActiveTexture(GL_TEXTURE25);
	glBindTexture(GL_TEXTURE_2D, hasIridescenceThicknessTex ? static_cast<GLuint>(_material.iridescenceThicknessTextureId()) : 0U);
	glActiveTexture(GL_TEXTURE26);
	glBindTexture(GL_TEXTURE_2D, hasSheenColorTex ? static_cast<GLuint>(_material.sheenColorTextureId()) : 0U);
	glActiveTexture(GL_TEXTURE27);
	glBindTexture(GL_TEXTURE_2D, hasSheenRoughnessTex ? static_cast<GLuint>(_material.sheenRoughnessTextureId()) : 0U);
	glActiveTexture(GL_TEXTURE28);
	glBindTexture(GL_TEXTURE_2D, hasTransmissionTex ? static_cast<GLuint>(_material.transmissionTextureId()) : 0U);
	glActiveTexture(GL_TEXTURE29);
	glBindTexture(GL_TEXTURE_2D, hasIORTex ? static_cast<GLuint>(_material.iorTextureId()) : 0U);

	// Overflow material bundles (units 34+).
	if (hasDiffuseTransmissionTex) {
		glActiveTexture(GL_TEXTURE0 + 34);
		glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(_material.diffuseTransmissionTextureId()));
	}
	if (hasDiffuseTransmissionColorTex) {
		glActiveTexture(GL_TEXTURE0 + 35);
		glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(_material.diffuseTransmissionColorTextureId()));
	}
	if (hasThicknessTex) {
		glActiveTexture(GL_TEXTURE30);
		glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(_material.thicknessTextureId()));
	}
}

void TriangleMesh::setupUniforms()
{
	if (!_uniformsDirty) return;
	_prog->bind();
	const bool hasTransmissionTex = _material.hasTransmissionMap() || _material.transmissionTextureId() != 0;
	const bool hasIORTex = _material.hasIORMap() || _material.iorTextureId() != 0;
	const bool hasSheenColorTex = _material.hasSheenColorMap() || _material.sheenColorTextureId() != 0;
	const bool hasSheenRoughnessTex = _material.hasSheenRoughnessMap() || _material.sheenRoughnessTextureId() != 0;
	const bool hasClearcoatColorTex = _material.hasClearcoatColorMap() || _material.clearcoatColorTextureId() != 0;
	const bool hasClearcoatRoughnessTex = _material.hasClearcoatRoughnessMap() || _material.clearcoatRoughnessTextureId() != 0;
	const bool hasClearcoatNormalTex = _material.hasClearcoatNormalMap() || _material.clearcoatNormalTextureId() != 0;
	const bool hasSpecularFactorTex = _material.hasSpecularFactorMap() || _material.specularFactorTextureId() != 0;
	const bool hasSpecularColorTex = _material.hasSpecularColorMap() || _material.specularColorTextureId() != 0;
	const bool hasAnisotropyTex = _material.hasAnisotropyMap() || _material.anisotropyTextureId() != 0;
	const bool hasIridescenceTex = _material.hasIridescenceMap() || _material.iridescenceTextureId() != 0;
	const bool hasIridescenceThicknessTex = _material.hasIridescenceThicknessMap() || _material.iridescenceThicknessTextureId() != 0;
	const bool hasThicknessTex = _material.hasThicknessMap() || _material.thicknessTextureId() != 0;
	const bool hasDiffuseTransmissionTex = _material.hasDiffuseTransmissionMap() || _material.diffuseTransmissionTextureId() != 0;
	const bool hasDiffuseTransmissionColorTex = _material.hasDiffuseTransmissionColorMap() || _material.diffuseTransmissionColorTextureId() != 0;
	GLint modeValue = 0;
	switch (_primitiveMode)
	{
	case GL_POINTS:         modeValue = 0; break;
	case GL_LINES:          modeValue = 1; break;
	case GL_LINE_LOOP:      modeValue = 2; break;
	case GL_LINE_STRIP:     modeValue = 3; break;
	case GL_TRIANGLES:      modeValue = 4; break;
	case GL_TRIANGLE_STRIP: modeValue = 5; break;
	case GL_TRIANGLE_FAN:   modeValue = 6; break;
	default:                modeValue = 4; break;
	}
	_prog->setUniformValue("primitiveMode", modeValue);
	_prog->setUniformValue("hasVertexColors", _hasVertexColors);
	_prog->setUniformValue("hasNegativeScale", _hasNegativeScale);
	_prog->setUniformValue("material.ambient", _material.ambient());
	// For specular-glossiness materials the authoritative diffuse colour is
	// diffuseColorFactor (stored in _diffuseColor / diffuseColor()), not the
	// albedo-derived _diffuse.  Mirror the same logic used for diffuseFactor in
	// the PBR path: use (1,1,1) when a diffuse texture is present (the texture
	// provides the colour), otherwise use the factor directly.  Without this,
	// ADS mode multiplies the correctly bound diffuse texture by the wrong colour
	// and the specular-glossiness material renders black.
	const QVector3D adsDiffuse = _material.getUseSpecularGlossiness()
		? (_material.hasDiffuseMap() ? QVector3D(1.0f, 1.0f, 1.0f) : _material.diffuseColor())
		: _material.diffuse();
	_prog->setUniformValue("material.diffuse", adsDiffuse);
	_prog->setUniformValue("material.specular", _material.specular());
	_prog->setUniformValue("material.emission", _material.emissive());
	_prog->setUniformValue("material.shininess", _material.shininess());
	_prog->setUniformValue("material.metallic", _material.metallic());
	_prog->setUniformValue("opacity", _material.opacity());
	// ADS light texture maps
	_prog->setUniformValue("hasDiffuseTexture", _material.hasAlbedoMap() || _material.hasDiffuseMap());
	_prog->setUniformValue("hasSpecularTexture", _material.hasMetallicMap());
	_prog->setUniformValue("hasEmissiveTexture", _material.hasEmissiveMap());
	_prog->setUniformValue("hasNormalTexture", _material.hasNormalMap());
	_prog->setUniformValue("hasHeightTexture", _material.hasHeightMap());
	_prog->setUniformValue("hasOpacityTexture", _material.hasOpacityMap());
	_prog->setUniformValue("opacityTextureInverted", _material.isOpacityMapInverted());

	_prog->setUniformValue("texture_diffuse", 10);
	_prog->setUniformValue("texture_specular", 11);
	_prog->setUniformValue("texture_emissive", 12);
	_prog->setUniformValue("texture_normal", 13);
	_prog->setUniformValue("texture_height", 14);
	_prog->setUniformValue("texture_opacity", 15);
	// PBR Direct Lighting
	_prog->setUniformValue("pbrLighting.albedo", _material.albedoColor());
	_prog->setUniformValue("pbrLighting.metallic", _material.metalness());
	_prog->setUniformValue("pbrLighting.roughness", _material.roughness());
	_prog->setUniformValue("pbrLighting.normalScale", _material.normalScale());
	_prog->setUniformValue("pbrLighting.ambientOcclusion", 1.0f);
	_prog->setUniformValue("pbrLighting.occlusionStrength", _material.occlusionStrength());
	_prog->setUniformValue("pbrLighting.transmission", _material.transmission());
	_prog->setUniformValue("pbrLighting.ior", _material.ior());
	_prog->setUniformValue("pbrLighting.sheenColor", _material.sheenColor());
	_prog->setUniformValue("pbrLighting.sheenRoughness", _material.sheenRoughness());
	_prog->setUniformValue("pbrLighting.clearcoat", _material.clearcoat());
	_prog->setUniformValue("pbrLighting.clearcoatRoughness", _material.clearcoatRoughness());

	// Alpha transparency mode and cuttoff
	_prog->setUniformValue("alphaThreshold", _material.alphaThreshold());
	_prog->setUniformValue("blendMode", static_cast<int>(_material.blendMode()));

	_prog->setUniformValue("twoSided", _material.twoSided());
	
	// PBR Texture Maps
	_prog->setUniformValue("albedoMap", 10);
	_prog->setUniformValue("metallicMap", 11);
	_prog->setUniformValue("emissiveMap", 12);
	_prog->setUniformValue("normalMap", 13);
	_prog->setUniformValue("heightMap", 14);
	_prog->setUniformValue("opacityMap", 15);
	_prog->setUniformValue("roughnessMap", 16);
	_prog->setUniformValue("aoMap", 17);

	// Upload channel-packing parameters so the shader reads the correct channel
	// with the correct scale/bias from each packed texture (e.g. ORM: R=AO, G=roughness, B=metallic).
	// GLMaterial initialises sensible defaults (roughness→G, metallic→B, AO→R, all scale=1)
	// and detectAndAssignPacking() updates them when multiple maps share the same file.
	// Without this upload the GLSL uniforms zero-initialise (scale=0) and texture
	// contributions are silently discarded, making all textured materials appear roughness≈0.
	{
		const auto& rp = _material.packingFor("roughness");
		_prog->setUniformValue("roughnessChannel", rp.channel);
		_prog->setUniformValue("roughnessInvert",  (int)rp.invert);
		_prog->setUniformValue("roughnessScale",   rp.scale);
		_prog->setUniformValue("roughnessBias",    rp.bias);

		const auto& mp = _material.packingFor("metallic");
		_prog->setUniformValue("metallicChannel", mp.channel);
		_prog->setUniformValue("metallicInvert",  (int)mp.invert);
		_prog->setUniformValue("metallicScale",   mp.scale);
		_prog->setUniformValue("metallicBias",    mp.bias);

		const auto& ap = _material.packingFor("ao");
		_prog->setUniformValue("aoChannel", ap.channel);
		_prog->setUniformValue("aoInvert",  (int)ap.invert);
		_prog->setUniformValue("aoScale",   ap.scale);
		_prog->setUniformValue("aoBias",    ap.bias);

		const auto& op = _material.packingFor("opacity");
		_prog->setUniformValue("opacityChannel", op.channel);
		_prog->setUniformValue("opacityInvert",  (int)op.invert);
		_prog->setUniformValue("opacityScale",   op.scale);
		_prog->setUniformValue("opacityBias",    op.bias);
	}
	// Advanced PBR Maps
	_prog->setUniformValue("clearcoatColorMap", 18);
	_prog->setUniformValue("clearcoatRoughnessMap", 19);
	_prog->setUniformValue("clearcoatNormalMap", 20);
	_prog->setUniformValue("specularFactorMap", 21);
	_prog->setUniformValue("specularColorMap", 22);
	_prog->setUniformValue("anisotropyMap", 23);
	_prog->setUniformValue("iridescenceMap", 24);
	_prog->setUniformValue("iridescenceThicknessMap", 25);
	_prog->setUniformValue("sheenColorMap", 26);
	_prog->setUniformValue("sheenRoughnessMap", 27);
	_prog->setUniformValue("transmissionMap", 28);
	_prog->setUniformValue("iorMap", 29);

	// KHR_materials_specular
	_prog->setUniformValue("hasSpecularFactorMap", hasSpecularFactorTex);
	_prog->setUniformValue("hasSpecularColorMap", hasSpecularColorTex);

	// KHR_materials_pbrSpecularGlossiness
	_prog->setUniformValue("diffuseMap", 10);
	_prog->setUniformValue("specularGlossinessMap", 21);
	_prog->setUniformValue("hasDiffuseMap", _material.hasDiffuseMap());
	_prog->setUniformValue("hasSpecularGlossinessMap", _material.hasSpecularGlossinessMap());
	_prog->setUniformValue("useSpecularGlossiness", _material.getUseSpecularGlossiness());

	// KHR_materials_pbrSpecularGlossiness - Factor values
	QVector3D diffuseFactor = _material.hasDiffuseMap() ?
		QVector3D(1.0f, 1.0f, 1.0f) : _material.diffuseColor();
	QVector3D specularFactor = _material.hasSpecularGlossinessMap() ?
		QVector3D(1.0f, 1.0f, 1.0f) : _material.specularColor();
	float glossinessFactor = _material.glossinessFactor();

	_prog->setUniformValue("diffuseFactor", diffuseFactor);
	_prog->setUniformValue("specularColor", specularFactor);
	_prog->setUniformValue("glossinessFactor", glossinessFactor);

	// KHR_materials_anisotropy
	_prog->setUniformValue("anisotropyMap", 23);
	_prog->setUniformValue("hasAnisotropyMap", hasAnisotropyTex);

	// KHR_materials_iridescence
	_prog->setUniformValue("iridescenceMap", 24);
	_prog->setUniformValue("hasIridescenceMap", hasIridescenceTex);

	_prog->setUniformValue("iridescenceThicknessMap", 25);
	_prog->setUniformValue("hasIridescenceThicknessMap", hasIridescenceThicknessTex);

	// KHR_materials_volume
	_prog->setUniformValue("thicknessMap", 30);
	_prog->setUniformValue("hasThicknessMap", hasThicknessTex);
	_prog->setUniformValue("hasThicknessAlpha", _material.hasThicknessAlpha());

	// KHR_materials_scattering
	_prog->setUniformValue("multiScatterColor", _material.multiScatterColor());
	_prog->setUniformValue("hasVolumeScattering", _material.hasVolumeScattering());

	// KHR_materials_specular (2 uniforms)
	_prog->setUniformValue("pbrLighting.specularFactor",
		_material.specularFactor());
	_prog->setUniformValue("pbrLighting.specularColorFactor",
		_material.specularColorFactor());

	// KHR_materials_anisotropy (2 uniforms)
	_prog->setUniformValue("pbrLighting.anisotropyStrength",
		_material.anisotropyStrength());
	_prog->setUniformValue("pbrLighting.anisotropyRotation",
		_material.anisotropyRotation());

	// KHR_materials_iridescence (4 uniforms)
	_prog->setUniformValue("pbrLighting.iridescenceFactor",
		_material.iridescenceFactor());
	_prog->setUniformValue("pbrLighting.iridescenceIor",
		_material.iridescenceIor());
	_prog->setUniformValue("pbrLighting.iridescenceThicknessMin",
		_material.iridescenceThicknessMin());
	_prog->setUniformValue("pbrLighting.iridescenceThicknessMax",
		_material.iridescenceThicknessMax());

	// KHR_materials_volume (3 uniforms)
	_prog->setUniformValue("pbrLighting.thicknessFactor",
		_material.thicknessFactor());
	_prog->setUniformValue("pbrLighting.attenuationDistance",
		_material.attenuationDistance());
	_prog->setUniformValue("pbrLighting.attenuationColor",
		_material.attenuationColor());

	// KHR_materials_dispersion (1 uniform)
	_prog->setUniformValue("pbrLighting.dispersion",
		_material.dispersion());

	// KHR_materials_unlit (1 uniform)
	_prog->setUniformValue("pbrLighting.unlit",
		_material.isUnlit());

	// KHR_materials_emissive_strength (1 uniform)
	_prog->setUniformValue("pbrLighting.emissiveStrength",
		_material.emissiveStrength());

	// KHR_materials_diffues_transmission
	_prog->setUniformValue("pbrLighting.diffuseTransmissionFactor",
		_material.diffuseTransmissionFactor());
	_prog->setUniformValue("pbrLighting.diffuseTransmissionColorFactor",
		_material.diffuseTransmissionColorFactor());
	// Diffuse Transmission Map
	_prog->setUniformValue("hasDiffuseTransmissionMap",
		hasDiffuseTransmissionTex);
	_prog->setUniformValue("diffuseTransmissionMap", 34);
	
	// Diffuse Transmission Color Map
	_prog->setUniformValue("hasDiffuseTransmissionColorMap",
		hasDiffuseTransmissionColorTex);
	_prog->setUniformValue("diffuseTransmissionColorMap", 35);

	// Texture transform uniforms
	_prog->setUniformValue("albedoTexTransform.texCoordIndex", _material.albedoTexCoord());
	_prog->setUniformValue("albedoTexTransform.offset", _material.albedoTexOffset());
	_prog->setUniformValue("albedoTexTransform.scale", _material.albedoTexScale());
	_prog->setUniformValue("albedoTexTransform.rotation", _material.albedoTexRotation());

	// Legacy diffuse map transform
	_prog->setUniformValue("diffuseTextureTransform.texCoordIndex", _material.albedoTexCoord());
	_prog->setUniformValue("diffuseTextureTransform.offset", _material.albedoTexOffset());
	_prog->setUniformValue("diffuseTextureTransform.scale", _material.albedoTexScale());
	_prog->setUniformValue("diffuseTextureTransform.rotation", _material.albedoTexRotation());

	// Metallic map transform
	_prog->setUniformValue("metallicTexTransform.texCoordIndex", _material.metallicTexCoord());
	_prog->setUniformValue("metallicTexTransform.offset", _material.metallicTexOffset());
	_prog->setUniformValue("metallicTexTransform.scale", _material.metallicTexScale());
	_prog->setUniformValue("metallicTexTransform.rotation", _material.metallicTexRotation());

	// Legacy specular map transform
	_prog->setUniformValue("specularTextureTransform.texCoordIndex", _material.metallicTexCoord());
	_prog->setUniformValue("specularTextureTransform.offset", _material.metallicTexOffset());
	_prog->setUniformValue("specularTextureTransform.scale", _material.metallicTexScale());
	_prog->setUniformValue("specularTextureTransform.rotation", _material.metallicTexRotation());

	// Roughness map transform
	_prog->setUniformValue("roughnessTexTransform.texCoordIndex", _material.roughnessTexCoord());
	_prog->setUniformValue("roughnessTexTransform.offset", _material.roughnessTexOffset());
	_prog->setUniformValue("roughnessTexTransform.scale", _material.roughnessTexScale());
	_prog->setUniformValue("roughnessTexTransform.rotation", _material.roughnessTexRotation());

	// Normal map transform
	_prog->setUniformValue("normalTexTransform.texCoordIndex", _material.normalTexCoord());
	_prog->setUniformValue("normalTexTransform.offset", _material.normalTexOffset());
	_prog->setUniformValue("normalTexTransform.scale", _material.normalTexScale());
	_prog->setUniformValue("normalTexTransform.rotation", _material.normalTexRotation());

	// Legacy normal map transform
	_prog->setUniformValue("normalTextureTransform.texCoordIndex", _material.normalTexCoord());
	_prog->setUniformValue("normalTextureTransform.offset", _material.normalTexOffset());
	_prog->setUniformValue("normalTextureTransform.scale", _material.normalTexScale());
	_prog->setUniformValue("normalTextureTransform.rotation", _material.normalTexRotation());

	// Occlusion map transform
	_prog->setUniformValue("aoTexTransform.texCoordIndex", _material.occlusionTexCoord());
	_prog->setUniformValue("aoTexTransform.offset", _material.occlusionTexOffset());
	_prog->setUniformValue("aoTexTransform.scale", _material.occlusionTexScale());
	_prog->setUniformValue("aoTexTransform.rotation", _material.occlusionTexRotation());

	// Emissive map transform
	_prog->setUniformValue("emissiveTexTransform.texCoordIndex", _material.emissiveTexCoord());
	_prog->setUniformValue("emissiveTexTransform.offset", _material.emissiveTexOffset());
	_prog->setUniformValue("emissiveTexTransform.scale", _material.emissiveTexScale());
	_prog->setUniformValue("emissiveTexTransform.rotation", _material.emissiveTexRotation());

	// Legacy emissive map transform
	_prog->setUniformValue("emissiveTextureTransform.texCoordIndex", _material.emissiveTexCoord());
	_prog->setUniformValue("emissiveTextureTransform.offset", _material.emissiveTexOffset());
	_prog->setUniformValue("emissiveTextureTransform.scale", _material.emissiveTexScale());
	_prog->setUniformValue("emissiveTextureTransform.rotation", _material.emissiveTexRotation());

	// Height map transform
	_prog->setUniformValue("heightTexTransform.texCoordIndex", _material.heightTexCoord());
	_prog->setUniformValue("heightTexTransform.offset", _material.heightTexOffset());
	_prog->setUniformValue("heightTexTransform.scale", _material.heightTexScale());
	_prog->setUniformValue("heightTexTransform.rotation", _material.heightTexRotation());

	// Legacy height map transform
	_prog->setUniformValue("heightTextureTransform.texCoordIndex", _material.heightTexCoord());
	_prog->setUniformValue("heightTextureTransform.offset", _material.heightTexOffset());
	_prog->setUniformValue("heightTextureTransform.scale", _material.heightTexScale());
	_prog->setUniformValue("heightTextureTransform.rotation", _material.heightTexRotation());

	// Opacity map transform
	_prog->setUniformValue("opacityTexTransform.texCoordIndex", _material.opacityTexCoord());
	_prog->setUniformValue("opacityTexTransform.offset", _material.opacityTexOffset());
	_prog->setUniformValue("opacityTexTransform.scale", _material.opacityTexScale());
	_prog->setUniformValue("opacityTexTransform.rotation", _material.opacityTexRotation());

	// Legacy opacity map transform
	_prog->setUniformValue("opacityTextureTransform.texCoordIndex", _material.opacityTexCoord());
	_prog->setUniformValue("opacityTextureTransform.offset", _material.opacityTexOffset());
	_prog->setUniformValue("opacityTextureTransform.scale", _material.opacityTexScale());
	_prog->setUniformValue("opacityTextureTransform.rotation", _material.opacityTexRotation());

	// Transmission map transform
	_prog->setUniformValue("transmissionTexTransform.texCoordIndex", _material.transmissionTexCoord());
	_prog->setUniformValue("transmissionTexTransform.offset", _material.transmissionTexOffset());
	_prog->setUniformValue("transmissionTexTransform.scale", _material.transmissionTexScale());
	_prog->setUniformValue("transmissionTexTransform.rotation", _material.transmissionTexRotation());

	// IOR map transform
	_prog->setUniformValue("iorTexTransform.texCoordIndex", _material.iorTexCoord());
	_prog->setUniformValue("iorTexTransform.offset", _material.iorTexOffset());
	_prog->setUniformValue("iorTexTransform.scale", _material.iorTexScale());
	_prog->setUniformValue("iorTexTransform.rotation", _material.iorTexRotation());

	// Sheen map transforms
	_prog->setUniformValue("sheenColorTexTransform.texCoordIndex", _material.sheenColorTexCoord());
	_prog->setUniformValue("sheenColorTexTransform.offset", _material.sheenColorTexOffset());
	_prog->setUniformValue("sheenColorTexTransform.scale", _material.sheenColorTexScale());
	_prog->setUniformValue("sheenColorTexTransform.rotation", _material.sheenColorTexRotation());

	// Sheen roughness map transform
	_prog->setUniformValue("sheenRoughnessTexTransform.texCoordIndex", _material.sheenRoughnessTexCoord());
	_prog->setUniformValue("sheenRoughnessTexTransform.offset", _material.sheenRoughnessTexOffset());
	_prog->setUniformValue("sheenRoughnessTexTransform.scale", _material.sheenRoughnessTexScale());
	_prog->setUniformValue("sheenRoughnessTexTransform.rotation", _material.sheenRoughnessTexRotation());

	// Clearcoat map transforms
	_prog->setUniformValue("clearcoatTexTransform.texCoordIndex", _material.clearcoatColorTexCoord());
	_prog->setUniformValue("clearcoatTexTransform.offset", _material.clearcoatColorTexOffset());
	_prog->setUniformValue("clearcoatTexTransform.scale", _material.clearcoatColorTexScale());
	_prog->setUniformValue("clearcoatTexTransform.rotation", _material.clearcoatColorTexRotation());

	// Clearcoat roughness map transform
	_prog->setUniformValue("clearcoatRoughnessTexTransform.texCoordIndex", _material.clearcoatRoughnessTexCoord());
	_prog->setUniformValue("clearcoatRoughnessTexTransform.offset", _material.clearcoatRoughnessTexOffset());
	_prog->setUniformValue("clearcoatRoughnessTexTransform.scale", _material.clearcoatRoughnessTexScale());
	_prog->setUniformValue("clearcoatRoughnessTexTransform.rotation", _material.clearcoatRoughnessTexRotation());

	// Clearcoat normal map transform
	_prog->setUniformValue("clearcoatNormalTexTransform.texCoordIndex", _material.clearcoatNormalTexCoord());
	_prog->setUniformValue("clearcoatNormalTexTransform.offset", _material.clearcoatNormalTexOffset());
	_prog->setUniformValue("clearcoatNormalTexTransform.scale", _material.clearcoatNormalTexScale());
	_prog->setUniformValue("clearcoatNormalTexTransform.rotation", _material.clearcoatNormalTexRotation());

	// KHR_materials_specular
	_prog->setUniformValue("specularFactorTexTransform.texCoordIndex", _material.specularFactorTexCoord());
	_prog->setUniformValue("specularFactorTexTransform.offset", _material.specularFactorTexOffset());
	_prog->setUniformValue("specularFactorTexTransform.scale", _material.specularFactorTexScale());
	_prog->setUniformValue("specularFactorTexTransform.rotation", _material.specularFactorTexRotation());
	
	// KHR_materials_specular
	_prog->setUniformValue("specularColorTexTransform.texCoordIndex", _material.specularColorTexCoord());
	_prog->setUniformValue("specularColorTexTransform.offset", _material.specularColorTexOffset());
	_prog->setUniformValue("specularColorTexTransform.scale", _material.specularColorTexScale());
	_prog->setUniformValue("specularColorTexTransform.rotation", _material.specularColorTexRotation());

	// KHR_materials_pbrSpecularGlossiness - Diffuse map transform
	_prog->setUniformValue("diffuseTexTransform.texCoordIndex", _material.diffuseTexCoord());
	_prog->setUniformValue("diffuseTexTransform.offset", _material.diffuseTexOffset());
	_prog->setUniformValue("diffuseTexTransform.scale", _material.diffuseTexScale());
	_prog->setUniformValue("diffuseTexTransform.rotation", _material.diffuseTexRotation());

	// KHR_materials_pbrSpecularGlossiness - Specular-Glossiness map transform
	_prog->setUniformValue("specularGlossinessTexTransform.texCoordIndex", _material.specularGlossinessTexCoord());
	_prog->setUniformValue("specularGlossinessTexTransform.offset", _material.specularGlossinessTexOffset());
	_prog->setUniformValue("specularGlossinessTexTransform.scale", _material.specularGlossinessTexScale());
	_prog->setUniformValue("specularGlossinessTexTransform.rotation", _material.specularGlossinessTexRotation());

	// KHR_materials_anisotropy
	_prog->setUniformValue("anisotropyTexTransform.texCoordIndex", _material.anisotropyTexCoord());
	_prog->setUniformValue("anisotropyTexTransform.offset", _material.anisotropyTexOffset());
	_prog->setUniformValue("anisotropyTexTransform.scale", _material.anisotropyTexScale());
	_prog->setUniformValue("anisotropyTexTransform.rotation", _material.anisotropyTexRotation());

	// KHR_materials_iridescence
	_prog->setUniformValue("iridescenceTexTransform.texCoordIndex", _material.iridescenceTexCoord());
	_prog->setUniformValue("iridescenceTexTransform.offset", _material.iridescenceTexOffset());
	_prog->setUniformValue("iridescenceTexTransform.scale", _material.iridescenceTexScale());
	_prog->setUniformValue("iridescenceTexTransform.rotation", _material.iridescenceTexRotation());
	
	// KHR_materials_iridescence
	_prog->setUniformValue("iridescenceThicknessTexTransform.texCoordIndex", _material.iridescenceThicknessTexCoord());
	_prog->setUniformValue("iridescenceThicknessTexTransform.offset", _material.iridescenceThicknessTexOffset());
	_prog->setUniformValue("iridescenceThicknessTexTransform.scale", _material.iridescenceThicknessTexScale());
	_prog->setUniformValue("iridescenceThicknessTexTransform.rotation", _material.iridescenceThicknessTexRotation());
	
	// KHR_materials_volume
	_prog->setUniformValue("thicknessTexTransform.texCoordIndex", _material.thicknessTexCoord());
	_prog->setUniformValue("thicknessTexTransform.offset", _material.thicknessTexOffset());
	_prog->setUniformValue("thicknessTexTransform.scale", _material.thicknessTexScale());
	_prog->setUniformValue("thicknessTexTransform.rotation", _material.thicknessTexRotation());

	// KHR_materials_diffues_transmission
	_prog->setUniformValue("diffuseTransmissionTexTransform.texCoordIndex", _material.diffuseTransmissionTexCoord());
	_prog->setUniformValue("diffuseTransmissionTexTransform.offset", _material.diffuseTransmissionTexOffset());
	_prog->setUniformValue("diffuseTransmissionTexTransform.scale", _material.diffuseTransmissionTexScale());
	_prog->setUniformValue("diffuseTransmissionTexTransform.rotation", _material.diffuseTransmissionTexRotation());
	_prog->setUniformValue("diffuseTransmissionColorTexTransform.texCoordIndex", _material.diffuseTransmissionColorTexCoord());
	_prog->setUniformValue("diffuseTransmissionColorTexTransform.offset", _material.diffuseTransmissionColorTexOffset());
	_prog->setUniformValue("diffuseTransmissionColorTexTransform.scale", _material.diffuseTransmissionColorTexScale());
	_prog->setUniformValue("diffuseTransmissionColorTexTransform.rotation", _material.diffuseTransmissionColorTexRotation());


	_prog->setUniformValue("heightScale", _material.heightScale());
	_prog->setUniformValue("clearcoatNormalScale", _material.clearcoatNormalScale());
	_prog->setUniformValue("hasAlbedoMap", _material.hasAlbedoMap());
	_prog->setUniformValue("hasMetallicMap", _material.hasMetallicMap());
	_prog->setUniformValue("hasEmissiveMap", _material.hasEmissiveMap());
	_prog->setUniformValue("hasRoughnessMap", _material.hasRoughnessMap());
	_prog->setUniformValue("hasNormalMap", _material.hasNormalMap());
	_prog->setUniformValue("hasAOMap", _material.hasAOMap());
	_prog->setUniformValue("hasOpacityMap", _material.hasOpacityMap());
	_prog->setUniformValue("opacityMapInverted", _material.isOpacityMapInverted());
	_prog->setUniformValue("hasHeightMap", _material.hasHeightMap());
	_prog->setUniformValue("hasTransmissionMap", hasTransmissionTex);
	_prog->setUniformValue("hasIORMap", hasIORTex);
	_prog->setUniformValue("hasSheenColorMap", hasSheenColorTex);
	_prog->setUniformValue("hasSheenRoughnessMap", hasSheenRoughnessTex);
	_prog->setUniformValue("hasClearcoatMap", hasClearcoatColorTex);
	_prog->setUniformValue("hasClearcoatRoughnessMap", hasClearcoatRoughnessTex);
	_prog->setUniformValue("hasClearcoatNormalMap", hasClearcoatNormalTex);

	// Extension-presence flags for debug channel gating (mirrors Khronos #ifdef MATERIAL_XXX).
	// True when the extension is active regardless of whether a texture is present.
	_prog->setUniformValue("extClearcoat",    _material.hasClearcoat()
	                                              || hasClearcoatColorTex
	                                              || hasClearcoatRoughnessTex
	                                              || hasClearcoatNormalTex);
	_prog->setUniformValue("extSheen",        _material.hasSheen()
	                                              || hasSheenColorTex
	                                              || hasSheenRoughnessTex);
	_prog->setUniformValue("extTransmission", _material.hasTransmission()
	                                              || hasTransmissionTex);
	_prog->setUniformValue("extSpecular",     hasSpecularFactorTex
	                                              || hasSpecularColorTex
	                                              || _material.specularFactor() != 1.0f
	                                              || _material.specularColorFactor() != QVector3D(1.0f, 1.0f, 1.0f));
	_prog->setUniformValue("extAnisotropy",   _material.anisotropyStrength() != 0.0f
	                                              || hasAnisotropyTex);
	_prog->setUniformValue("extIridescence",  _material.iridescenceFactor() > 0.0f
	                                              || hasIridescenceTex
	                                              || hasIridescenceThicknessTex);
	_prog->setUniformValue("extVolume",       _material.thicknessFactor() > 0.0f
	                                              || hasThicknessTex
	                                              || _material.hasVolumeScattering());
	_prog->setUniformValue("extDiffuseTrans", _material.diffuseTransmissionFactor() > 0.0f
	                                              || hasDiffuseTransmissionTex
	                                              || hasDiffuseTransmissionColorTex);

	_prog->setUniformValue("isGLTFMaterial", _material.isGLTFMaterial());

	// send channel-packing uniforms now that samplers are bound to units
	// Uniform naming: <base>Channel, <base>Invert, <base>Scale, <base>Bias
	auto sendPackingUniform = [this](const QString& key, const char* base) {
		GLMaterial::ChannelPacking p = _material.packingFor(key);
		int ch = p.channel;
		if (ch < -1) ch = -1;
		if (ch > 3) ch = 3;

		// Note: _prog must be bound at this point (it is in render())
		const QByteArray channelName = QString("%1Channel").arg(base).toUtf8();
		const QByteArray invertName = QString("%1Invert").arg(base).toUtf8();
		const QByteArray scaleName = QString("%1Scale").arg(base).toUtf8();
		const QByteArray biasName = QString("%1Bias").arg(base).toUtf8();

		int locChan = _prog->uniformLocation(channelName.constData());
		if (locChan != -1) _prog->setUniformValue(locChan, ch);

		int locInv = _prog->uniformLocation(invertName.constData());
		if (locInv != -1) _prog->setUniformValue(locInv, p.invert ? 1 : 0);

		int locScale = _prog->uniformLocation(scaleName.constData());
		if (locScale != -1) _prog->setUniformValue(locScale, p.scale);

		int locBias = _prog->uniformLocation(biasName.constData());
		if (locBias != -1) _prog->setUniformValue(locBias, p.bias);
		};

	// send for all packable maps
	sendPackingUniform("metallic", "metallic");
	sendPackingUniform("roughness", "roughness");
	sendPackingUniform("ao", "ao");
	sendPackingUniform("opacity", "opacity");

	_prog->setUniformValue("selected", _selected);
	_uniformsDirty = false;
}

void TriangleMesh::invertOpacityADSMap(bool invert)
{
	_material.setInvertOpacityMap(invert);
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::setOpacityADSMap(unsigned int opacityTex)
{
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::setHeightADSMap(unsigned int heightTex)
{
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::setNormalADSMap(unsigned int normalTex)
{
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::setSpecularADSMap(unsigned int specularTex)
{
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::setEmissiveADSMap(unsigned int emissiveTex)
{
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::setDiffuseADSMap(unsigned int diffuseTex)
{
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearDiffuseADSMap()
{
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearSpecularADSMap()
{
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearEmissiveADSMap()
{
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearNormalADSMap()
{
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearHeightADSMap()
{
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearOpacityADSMap()
{
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearAllADSMaps()
{
	markTexturesDirty();
	markUniformsDirty();
}

GLMaterial TriangleMesh::getMaterial() const
{
	return _material;
}

void TriangleMesh::setMaterial(const GLMaterial& material)
{
	_material = material;
	cacheBaseVolumeProperties();
	applyScaledVolumeProperties();
	markUniformsDirty();
}

void TriangleMesh::setTextureMaps(const GLMaterial& material)
{
	// Resolved GLMaterial instances can carry shared texture ids from GLWidget's
	// cache. Treat this call as a state sync only; deleting/recreating ids here
	// can invalidate the very cached textures we are about to keep using.
	_material = material;
	cacheBaseVolumeProperties();
	applyScaledVolumeProperties();

	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::render()
{
	if (!_vertexArrayObject.isCreated())
		return;

	const QVariant globalModelVar = _prog->property("globalModelMatrix");
	const QMatrix4x4 globalModelMatrix = globalModelVar.isValid()
		? globalModelVar.value<QMatrix4x4>()
		: QMatrix4x4();
	const QMatrix4x4 modelMatrix = globalModelMatrix * combinedRenderTransform();
	const QVariant viewVar = _prog->property("viewMatrix");
	const QMatrix4x4 viewMatrix = viewVar.isValid() ? viewVar.value<QMatrix4x4>() : QMatrix4x4();
	const QMatrix4x4 modelViewMatrix = viewMatrix * modelMatrix;

	if (_prog->uniformLocation("modelMatrix") >= 0)
		_prog->setUniformValue("modelMatrix", modelMatrix);
	if (_prog->uniformLocation("modelViewMatrix") >= 0)
		_prog->setUniformValue("modelViewMatrix", modelViewMatrix);
	if (_prog->uniformLocation("normalMatrix") >= 0)
		_prog->setUniformValue("normalMatrix", modelViewMatrix.normalMatrix());
	if (_prog->uniformLocation("hasSkinning") >= 0)
		_prog->setUniformValue("hasSkinning", hasSkinning());
	if (_prog->uniformLocation("jointCount") >= 0)
		_prog->setUniformValue("jointCount", static_cast<int>(_jointPalette.size()));
	if (hasSkinning() && !_jointPalette.isEmpty())
	{
		const int maxJoints = std::min(static_cast<int>(_jointPalette.size()), 128);
		for (int i = 0; i < maxJoints; ++i)
		{
			const QString uniformName = QStringLiteral("jointMatrices[%1]").arg(i);
			if (_prog->uniformLocation(uniformName.toUtf8().constData()) >= 0)
				_prog->setUniformValue(uniformName.toUtf8().constData(), _jointPalette[i]);
		}
	}

	setupTextures();
	applyDebugTextureOverrides();

	setupUniforms();
	applyDebugUniformOverrides();

	if(_material.opacity() < 1.0f ||
		_material.hasOpacityMap() || _material.hasTransmissionMap() ||
		_material.transmission() > 0.0f ||
		_material.blendMode() == GLMaterial::BlendMode::Alpha ||
		_material.alphaThreshold() > 0.0f || _hasTextureAlpha)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_LINE_SMOOTH);
		glEnable(GL_POLYGON_SMOOTH);
	}
	else
	{
		glDisable(GL_BLEND);
	}

	// Handle lighting normal for negative scaling
	if ((_scaleX < 0 && _scaleY > 0 && _scaleZ > 0) ||
		(_scaleX > 0 && _scaleY < 0 && _scaleZ > 0) ||
		(_scaleX > 0 && _scaleY > 0 && _scaleZ < 0) ||
		(_scaleX < 0 && _scaleY < 0 && _scaleZ < 0))
	{
		glFrontFace(GL_CW);
	}
	else
	{
		glFrontFace(GL_CCW);
	}
	_vertexArrayObject.bind();
	glDrawElements(GL_TRIANGLES, _nVerts, GL_UNSIGNED_INT, 0);
	_vertexArrayObject.release();	

	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_BLEND);
}

// Render the mesh for shadow mapping
void TriangleMesh::renderShadow()
{
	if (!_vertexArrayObject.isCreated())
		return;

	const QVariant globalModelVar = _prog->property("globalModelMatrix");
	const QMatrix4x4 globalModelMatrix = globalModelVar.isValid()
		? globalModelVar.value<QMatrix4x4>()
		: QMatrix4x4();
	if (_prog->uniformLocation("model") >= 0)
		_prog->setUniformValue("model", globalModelMatrix * combinedRenderTransform());
	if (_prog->uniformLocation("hasSkinning") >= 0)
		_prog->setUniformValue("hasSkinning", hasSkinning());
	if (_prog->uniformLocation("jointCount") >= 0)
		_prog->setUniformValue("jointCount", static_cast<int>(_jointPalette.size()));
	if (hasSkinning() && !_jointPalette.isEmpty())
	{
		const int maxJoints = std::min(static_cast<int>(_jointPalette.size()), 128);
		for (int i = 0; i < maxJoints; ++i)
		{
			const QString uniformName = QStringLiteral("jointMatrices[%1]").arg(i);
			if (_prog->uniformLocation(uniformName.toUtf8().constData()) >= 0)
				_prog->setUniformValue(uniformName.toUtf8().constData(), _jointPalette[i]);
		}
	}

	// Handle negative scaling (important for shadow mapping!)
	if ((_scaleX < 0 && _scaleY > 0 && _scaleZ > 0) ||
		(_scaleX > 0 && _scaleY < 0 && _scaleZ > 0) ||
		(_scaleX > 0 && _scaleY > 0 && _scaleZ < 0) ||
		(_scaleX < 0 && _scaleY < 0 && _scaleZ < 0))
	{
		glFrontFace(GL_CW);
	}
	else
	{
		glFrontFace(GL_CCW);
	}

	_vertexArrayObject.bind();
	glDrawElements(GL_TRIANGLES, _nVerts, GL_UNSIGNED_INT, 0);
	_vertexArrayObject.release();
}

void TriangleMesh::deleteTextures()
{
	if (_fallbackTexture != 0)
	{
		glDeleteTextures(1, &_fallbackTexture);
		_fallbackTexture = 0;
	}

	// Material texture IDs are resolved through GLWidget's shared texture cache
	// and may be referenced by multiple meshes or UI previews. Deleting them here
	// lets one mesh teardown invalidate another mesh's live bindings.
}

TriangleMesh::~TriangleMesh()
{
	deleteBuffers();
	deleteTextures();
	for (Triangle* t : _triangles)
		delete t;
}

void TriangleMesh::deleteBuffers()
{
	if (_buffers.size() > 0)
	{
		for (QOpenGLBuffer& buff : _buffers)
		{
			buff.destroy();
		}
		_buffers.clear();
	}

	if (_vertexArrayObject.isCreated())
	{
		_vertexArrayObject.destroy();
	}
}

void TriangleMesh::computeBounds()
{
	// Ritter's algorithm
	std::vector<QVector3D> aPoints;
	for (size_t i = 0; i < _trsfPoints.size(); i += 3)
	{
		aPoints.push_back(QVector3D(_trsfPoints[i], _trsfPoints[i + 1], _trsfPoints[i + 2]));
	}
	QVector3D xmin, xmax, ymin, ymax, zmin, zmax;
	xmin = ymin = zmin = QVector3D(1, 1, 1) * INFINITY;
	xmax = ymax = zmax = QVector3D(1, 1, 1) * -INFINITY;
	for (auto p : aPoints)
	{
		if (p.x() < xmin.x())
			xmin = p;
		if (p.x() > xmax.x())
			xmax = p;
		if (p.y() < ymin.y())
			ymin = p;
		if (p.y() > ymax.y())
			ymax = p;
		if (p.z() < zmin.z())
			zmin = p;
		if (p.z() > zmax.z())
			zmax = p;
	}
	// Stable bounding sphere: center at the bounding box midpoint, radius = max vertex
	// distance from that center.  This avoids the instability of Ritter's algorithm,
	// whose initial-pair vertex selection can flip on sub-epsilon position differences
	// (e.g. after an export/import world-transform round-trip), cascading into a
	// significantly different sphere.  The stable properties are:
	//   - center is derived from min/max coordinate VALUES, not vertex selection
	//   - radius is max() over all vertices, which varies by at most ε under perturbation
	// For sphere-shaped meshes the result is as tight as Ritter's (all vertices are
	// equidistant from the centroid).  For cubes it gives the circumscribed sphere
	// (half-diagonal from center to corner) — correct and minimal for box geometry.
	const QVector3D center(
		(xmin.x() + xmax.x()) * 0.5f,
		(ymin.y() + ymax.y()) * 0.5f,
		(zmin.z() + zmax.z()) * 0.5f
	);
	float sqRad = 0.0f;
	for (const auto& p : aPoints)
	{
		const float d = (p - center).lengthSquared();
		if (d > sqRad) sqRad = d;
	}

	_boundingSphere.setCenter(center);
	_boundingSphere.setRadius(std::sqrt(sqRad));

	_boundingBox.setLimits(
		xmin.x(), xmax.x(),
		ymin.y(), ymax.y(),
		zmin.z(), zmax.z()
	);
	Point cen = _boundingBox.center();
}

float TriangleMesh::getHighestXValue() const
{
	return _boundingBox.xMax();
}

float TriangleMesh::getLowestXValue() const
{
	return _boundingBox.xMin();
}

float TriangleMesh::getHighestYValue() const
{
	return _boundingBox.yMax();
}

float TriangleMesh::getLowestYValue() const
{
	return _boundingBox.yMin();
}

float TriangleMesh::getHighestZValue() const
{
	return _boundingBox.zMax();
}

float TriangleMesh::getLowestZValue() const
{
	return _boundingBox.zMin();
}

QRect TriangleMesh::projectedRect(const QMatrix4x4& modelView, const QMatrix4x4& projection, const QRect& viewport, const QRect& window) const
{
	float xMin = std::numeric_limits<float>::max();
	float xMax = std::numeric_limits<float>::lowest();
	float yMin = std::numeric_limits<float>::max();
	float yMax = std::numeric_limits<float>::lowest();

	for (size_t i = 0; i < _trsfPoints.size(); i += 3)
	{
		QVector3D point(_trsfPoints.at(i + 0), _trsfPoints.at(i + 1), _trsfPoints.at(i + 2));
		QVector3D projPoint = point.project(modelView, projection, viewport);

		xMin = std::min(xMin, projPoint.x());
		xMax = std::max(xMax, projPoint.x());
		yMin = std::min(yMin, projPoint.y());
		yMax = std::max(yMax, projPoint.y());
	}
	QRect rect(xMin, (window.height() - yMax), (xMax - xMin), (yMax - yMin));

	return rect;
}

std::vector<float> TriangleMesh::getNormals() const
{
	return _normals;
}

std::vector<float> TriangleMesh::getTexCoords() const
{
	return _texCoords;
}

const std::vector<float>& TriangleMesh::getTrsfPoints() const
{
	return _trsfPoints;
}

void TriangleMesh::resetTransformations()
{
	_transX = _transY = _transZ = 0.0f;
	_rotateX = _rotateY = _rotateZ = 0.0f;
	_scaleX = _scaleY = _scaleZ = 1.0f;
	_rotationQuat = QQuaternion();

	rebuildAbsoluteTransformation();

	_trsfPoints.clear();
	_trsfNormals.clear();
	_trsfTangents.clear();  
	_trsfBitangents.clear();

	_trsfPoints = _points;
	_trsfNormals = _normals;
	_trsfTangents = _tangents;     
	_trsfBitangents = _bitangents; 

	applyScaledVolumeProperties();
	updateRuntimeBounds();
}

std::vector<unsigned int> TriangleMesh::getIndices() const
{
	return _indices;
}

std::vector<float> TriangleMesh::getPoints() const
{
	return _points;
}

QVector3D TriangleMesh::getTranslation() const
{
	return QVector3D(_transX, _transY, _transZ);
}

void TriangleMesh::setTranslation(const QVector3D& trans)
{
	_transX = trans.x();
	_transY = trans.y();
	_transZ = trans.z();
	rebuildAbsoluteTransformation();
	setupTransformation();
}

QVector3D TriangleMesh::getRotation() const
{
	return QVector3D(_rotateX, _rotateY, _rotateZ);
}

void TriangleMesh::setRotation(const QVector3D& rota)
{
	_rotateX = rota.x();
	_rotateY = rota.y();
	_rotateZ = rota.z();
	_rotationQuat = meshEulerToQuaternion(rota);
	rebuildAbsoluteTransformation();
	setupTransformation();
}

QQuaternion TriangleMesh::getRotationQuaternion() const
{
	return _rotationQuat;
}

void TriangleMesh::setRotationQuaternion(const QQuaternion& quat, const QVector3D& displayEuler)
{
	_rotationQuat = quat.normalized();
	_rotateX = displayEuler.x();
	_rotateY = displayEuler.y();
	_rotateZ = displayEuler.z();
	rebuildAbsoluteTransformation();
	setupTransformation();
}

QVector3D TriangleMesh::getScaling() const
{
	return QVector3D(_scaleX, _scaleY, _scaleZ);
}

void TriangleMesh::setScaling(const QVector3D& scale)
{
	_scaleX = scale.x();
	_scaleY = scale.y();
	_scaleZ = scale.z();
	rebuildAbsoluteTransformation();
	setupTransformation();
	
	applyScaledVolumeProperties();
}

void TriangleMesh::rebuildAbsoluteTransformation()
{
	_transformation.setToIdentity();
	_transformation.translate(_transX, _transY, _transZ);
	_transformation.rotate(_rotationQuat);
	_transformation.scale(_scaleX, _scaleY, _scaleZ);
}

QMatrix4x4 TriangleMesh::getTransformation() const
{
	return _transformation;
}

QVector3D TriangleMesh::getStableTransformCenter() const
{
	const Point localCenter = _localBoundingBox.center();
	const QVector3D center(
		static_cast<float>(localCenter.getX()),
		static_cast<float>(localCenter.getY()),
		static_cast<float>(localCenter.getZ()));
	return combinedRenderTransform().map(center);
}

float TriangleMesh::getStableTransformRadius() const
{
	const Point localCenterPoint = _localBoundingBox.center();
	const QVector3D localCenter(
		static_cast<float>(localCenterPoint.getX()),
		static_cast<float>(localCenterPoint.getY()),
		static_cast<float>(localCenterPoint.getZ()));

	float localRadius = 0.0f;
	for (size_t i = 0; i < _points.size(); i += 3)
	{
		const QVector3D point(_points[i], _points[i + 1], _points[i + 2]);
		localRadius = (std::max)(localRadius, (point - localCenter).length());
	}

	const QMatrix4x4 combined = combinedRenderTransform();
	const QVector3D col0(combined(0, 0), combined(1, 0), combined(2, 0));
	const QVector3D col1(combined(0, 1), combined(1, 1), combined(2, 1));
	const QVector3D col2(combined(0, 2), combined(1, 2), combined(2, 2));
	const float maxScale = (std::max)(col0.length(), (std::max)(col1.length(), col2.length()));

	return localRadius * maxScale;
}

QMatrix4x4 TriangleMesh::getSceneRenderTransform() const
{
	return _sceneRenderTransform;
}

QMatrix4x4 TriangleMesh::combinedRenderTransform() const
{
	const QMatrix4x4 base = _transformation * _sceneRenderTransform;
	if (_explosionOffset.isNull())
		return base;
	QMatrix4x4 t;
	t.translate(_explosionOffset);
	return t * base;
}

void TriangleMesh::setSceneRenderTransform(const QMatrix4x4& trsf)
{
	_sceneRenderTransform = trsf;
	updateRuntimeBounds();
}

void TriangleMesh::setSceneRenderTransformFast(const QMatrix4x4& trsf)
{
	_sceneRenderTransform = trsf;

	// Update the world-space bounding box cheaply by transforming the 8 corners of the
	// local-space bounding box (from _points, no transform) through the current combined
	// render transform.  This keeps isMeshOutsideFrustum() correct for animated meshes
	// without the cost of re-transforming every vertex (as updateRuntimeBounds() would).
	if (!_points.empty())
	{
		const QMatrix4x4 combined = combinedRenderTransform();
		float xMin =  std::numeric_limits<float>::max();
		float yMin =  std::numeric_limits<float>::max();
		float zMin =  std::numeric_limits<float>::max();
		float xMax = -std::numeric_limits<float>::max();
		float yMax = -std::numeric_limits<float>::max();
		float zMax = -std::numeric_limits<float>::max();
		for (const QVector3D& corner : _localBoundingBox.getCorners())
		{
			const QVector3D tc = combined.map(corner);
			xMin = std::min(xMin, tc.x());
			yMin = std::min(yMin, tc.y());
			zMin = std::min(zMin, tc.z());
			xMax = std::max(xMax, tc.x());
			yMax = std::max(yMax, tc.y());
			zMax = std::max(zMax, tc.z());
		}
		_boundingBox.setLimits(
			static_cast<double>(xMin), static_cast<double>(xMax),
			static_cast<double>(yMin), static_cast<double>(yMax),
			static_cast<double>(zMin), static_cast<double>(zMax));
	}
}

void TriangleMesh::setupTransformation()
{
	updateRuntimeBounds();
}

// ---------------------------------------------------------------------------
// Fast TRS setters — for interactive gizmo drag
// ---------------------------------------------------------------------------
// These update _transformation and recompute the world-space bounding box from
// the 8 local AABB corners only (O(1)), skipping the full O(N) vertex transform.
// Call fullUpdateRuntimeBounds() once when the drag ends to resync _trsfPoints.

// Helper: update _boundingBox from _localBoundingBox corners via combinedRenderTransform.
void TriangleMesh::fastUpdateWorldBounds()
{
	const QMatrix4x4 combined = combinedRenderTransform();
	float xMin =  std::numeric_limits<float>::max();
	float yMin =  std::numeric_limits<float>::max();
	float zMin =  std::numeric_limits<float>::max();
	float xMax = -std::numeric_limits<float>::max();
	float yMax = -std::numeric_limits<float>::max();
	float zMax = -std::numeric_limits<float>::max();
	for (const QVector3D& corner : _localBoundingBox.getCorners())
	{
		const QVector3D tc = combined.map(corner);
		xMin = std::min(xMin, tc.x());
		yMin = std::min(yMin, tc.y());
		zMin = std::min(zMin, tc.z());
		xMax = std::max(xMax, tc.x());
		yMax = std::max(yMax, tc.y());
		zMax = std::max(zMax, tc.z());
	}
	_boundingBox.setLimits(
		static_cast<double>(xMin), static_cast<double>(xMax),
		static_cast<double>(yMin), static_cast<double>(yMax),
		static_cast<double>(zMin), static_cast<double>(zMax));
}

void TriangleMesh::setTranslationFast(const QVector3D& trans)
{
	_transX = trans.x();
	_transY = trans.y();
	_transZ = trans.z();
	rebuildAbsoluteTransformation();
	fastUpdateWorldBounds();
}

void TriangleMesh::setRotationFast(const QVector3D& rota)
{
	_rotateX = rota.x();
	_rotateY = rota.y();
	_rotateZ = rota.z();
	_rotationQuat = meshEulerToQuaternion(rota);
	rebuildAbsoluteTransformation();
	fastUpdateWorldBounds();
}

void TriangleMesh::setRotationQuaternionFast(const QQuaternion& quat, const QVector3D& displayEuler)
{
	_rotationQuat = quat.normalized();
	_rotateX = displayEuler.x();
	_rotateY = displayEuler.y();
	_rotateZ = displayEuler.z();
	rebuildAbsoluteTransformation();
	fastUpdateWorldBounds();
}

void TriangleMesh::setScalingFast(const QVector3D& scale)
{
	_scaleX = scale.x();
	_scaleY = scale.y();
	_scaleZ = scale.z();
	rebuildAbsoluteTransformation();
	fastUpdateWorldBounds();
}

void TriangleMesh::fullUpdateRuntimeBounds()
{
	updateRuntimeBounds();
}

void TriangleMesh::updateRuntimeBounds()
{
	// Compute the local-space bounding box from _points (before any transform).
	// This is used by setSceneRenderTransformFast() to cheaply derive the
	// world-space bounding box each animation frame without re-transforming every vertex.
	if (!_points.empty())
	{
		float lxMin =  std::numeric_limits<float>::max();
		float lyMin =  std::numeric_limits<float>::max();
		float lzMin =  std::numeric_limits<float>::max();
		float lxMax = -std::numeric_limits<float>::max();
		float lyMax = -std::numeric_limits<float>::max();
		float lzMax = -std::numeric_limits<float>::max();
		for (size_t i = 0; i < _points.size(); i += 3)
		{
			lxMin = std::min(lxMin, _points[i]);
			lyMin = std::min(lyMin, _points[i + 1]);
			lzMin = std::min(lzMin, _points[i + 2]);
			lxMax = std::max(lxMax, _points[i]);
			lyMax = std::max(lyMax, _points[i + 1]);
			lzMax = std::max(lzMax, _points[i + 2]);
		}
		_localBoundingBox.setLimits(
			static_cast<double>(lxMin), static_cast<double>(lxMax),
			static_cast<double>(lyMin), static_cast<double>(lyMax),
			static_cast<double>(lzMin), static_cast<double>(lzMax));
	}

	_trsfPoints.clear();
	_trsfNormals.clear();
	_trsfTangents.clear();
	_trsfBitangents.clear();
	const QMatrix4x4 combined = combinedRenderTransform();

	// ============================================================================
	// TRANSFORM POSITIONS
	// ============================================================================
	for (size_t i = 0; i < _points.size(); i += 3)
	{
		QVector3D p(_points[i + 0], _points[i + 1], _points[i + 2]);
		QVector3D tp = combined.map(p);
		_trsfPoints.push_back(tp.x());
		_trsfPoints.push_back(tp.y());
		_trsfPoints.push_back(tp.z());
	}

	// ============================================================================
	// TRANSFORM NORMALS
	// ============================================================================
	// Use inverse-transpose for proper normal transformation (handles non-uniform scaling)
	for (size_t i = 0; i < _normals.size(); i += 3)
	{
		QVector3D n(_normals[i + 0], _normals[i + 1], _normals[i + 2]);
		QMatrix4x4 rotMat = combined;
		// Extract rotation/scale part only (zero out translation)
		rotMat.setColumn(3, QVector4D(0, 0, 0, 1));
		QVector3D tn = rotMat.map(n);
		_trsfNormals.push_back(tn.x());
		_trsfNormals.push_back(tn.y());
		_trsfNormals.push_back(tn.z());
	}

	// ============================================================================
	// TRANSFORM TANGENTS using inverse-transpose
	// This is critical for normal mapping and anisotropy to work correctly
	// ============================================================================
	if (!_tangents.empty())
	{
		QMatrix4x4 rotMat = combined;
		rotMat.setColumn(3, QVector4D(0, 0, 0, 1));

		for (size_t i = 0; i < _tangents.size(); i += 3)
		{
			QVector3D t(_tangents[i + 0], _tangents[i + 1], _tangents[i + 2]);
			QVector3D tt = rotMat.map(t);

			// Renormalize after transformation to maintain unit length
			// This is especially important for non-uniform scaling
			float len = tt.length();
			if (len > 0.001f)
			{
				tt.normalize();
			}
			else
			{
				tt = QVector3D(1.0f, 0.0f, 0.0f); // Fallback to default if degenerate
			}

			_trsfTangents.push_back(tt.x());
			_trsfTangents.push_back(tt.y());
			_trsfTangents.push_back(tt.z());
		}
	}

	// ============================================================================
	// TRANSFORM BITANGENTS using inverse-transpose
	// ============================================================================
	if (!_bitangents.empty())
	{
		QMatrix4x4 rotMat = combined;
		rotMat.setColumn(3, QVector4D(0, 0, 0, 1));

		for (size_t i = 0; i < _bitangents.size(); i += 3)
		{
			QVector3D b(_bitangents[i + 0], _bitangents[i + 1], _bitangents[i + 2]);
			QVector3D tb = rotMat.map(b);

			// Renormalize after transformation
			float len = tb.length();
			if (len > 0.001f)
			{
				tb.normalize();
			}
			else
			{
				tb = QVector3D(0.0f, 1.0f, 0.0f); // Fallback to default if degenerate
			}

			_trsfBitangents.push_back(tb.x());
			_trsfBitangents.push_back(tb.y());
			_trsfBitangents.push_back(tb.z());
		}
	}

	buildTriangles();
	computeBounds();
}

float TriangleMesh::shininess() const
{
	return _material.shininess();
}

void TriangleMesh::setShininess(const float& shine)
{
	_material.setShininess(shine);
	markUniformsDirty();
}

float TriangleMesh::opacity() const
{
	return _material.opacity();
}

void TriangleMesh::setOpacity(const float& opacity)
{
	_material.setOpacity(opacity);	
	if (opacity < 1.0f)
		_material.setBlendMode(GLMaterial::BlendMode::Alpha);
	else
		_material.setBlendMode(GLMaterial::BlendMode::Opaque);
	markUniformsDirty();
}

QVector3D TriangleMesh::emmissiveMaterial() const
{
	return _material.emissive();
}

void TriangleMesh::setEmmissiveMaterial(const QVector3D& emissive)
{
	_material.setEmissive(emissive);
	markUniformsDirty();
}

QVector3D TriangleMesh::specularMaterial() const
{
	return _material.specular();
}

void TriangleMesh::setSpecularMaterial(const QVector3D& specular)
{
	_material.setSpecular(specular);
	markUniformsDirty();
}

QVector3D TriangleMesh::diffuseMaterial() const
{
	return _material.diffuse();
}

void TriangleMesh::setDiffuseMaterial(const QVector3D& diffuse)
{
	_material.setDiffuse(diffuse);
	markUniformsDirty();
}

QVector3D TriangleMesh::ambientMaterial() const
{
	return _material.ambient();
}

void TriangleMesh::setAmbientMaterial(const QVector3D& ambient)
{
	_material.setAmbient(ambient);
	markUniformsDirty();
}

bool TriangleMesh::isMetallic() const
{
	return _material.metallic();
}

void TriangleMesh::setMetallic(bool metallic)
{
	_material.setMetallic(metallic);
	_material.setMetalness(metallic ? 1.0f : 0.0f);
	markUniformsDirty();
}

void TriangleMesh::setPBRAlbedoColor(const float& r, const float& g, const float& b)
{
	_material.setAlbedoColor(QVector3D(r, g, b));
	markUniformsDirty();
}

void TriangleMesh::setPBRMetallic(const float& val)
{
	_material.setMetalness(val);
	markUniformsDirty();
}

void TriangleMesh::setPBRRoughness(const float& val)
{
	_material.setRoughness(val);
	markUniformsDirty();
}

QOpenGLVertexArrayObject& TriangleMesh::getVAO()
{
	return _vertexArrayObject;
}

unsigned long long TriangleMesh::memorySize() const
{
	return _memorySize + sizeof(TriangleMesh);
}


bool TriangleMesh::intersectsWithRay(const QVector3D& rayPos, const QVector3D& rayDir, QVector3D& outIntersectionPoint)
{
	// If the mesh is exploded, bring the ray into the non-exploded (base) space so
	// the triangle data in _triangles (which are at base world positions) can be
	// tested directly.  The hit point is then translated back to world space.
	const QVector3D adjustedRayPos = _explosionOffset.isNull()
		? rayPos : rayPos - _explosionOffset;

	float closestDistance = std::numeric_limits<float>::max();
	bool found = false;
	QVector3D bestIntersection;


	float localMinDist = std::numeric_limits<float>::max();
	QVector3D localIntersection;
	bool localFound = false;

	for (int i = 0; i < _triangles.size(); ++i)
	{
		QVector3D hitPoint;
		if (_triangles[i]->intersectsWithRay(adjustedRayPos, rayDir, hitPoint))
		{
			float dist = (hitPoint - adjustedRayPos).length();
			if (dist < localMinDist)
			{
				localMinDist = dist;
				localIntersection = hitPoint;
				localFound = true;
			}
		}
	}

	if (localFound && localMinDist < closestDistance)
	{
		closestDistance = localMinDist;
		bestIntersection = localIntersection + _explosionOffset;
		found = true;
	}

	if (found)
	{
		outIntersectionPoint = bestIntersection;
	}
	return found;
}



bool TriangleMesh::hasAlbedoPBRMap() const
{
	return _material.hasAlbedoMap();
}

void TriangleMesh::enableAlbedoPBRMap(bool hasAlbedoMap)
{
	if (!hasAlbedoMap)
		_material.setAlbedoTextureId(0);
	markUniformsDirty();
}

bool TriangleMesh::hasMetallicPBRMap() const
{
	return _material.hasMetallicMap();
}

void TriangleMesh::enableMetallicPBRMap(bool hasMetallicMap)
{
	if (!hasMetallicMap)
		_material.setMetallicTextureId(0);
	markUniformsDirty();
}

bool TriangleMesh::hasEmissivePBRMap() const
{
	return _material.hasEmissiveMap();
}

void TriangleMesh::enableEmissivePBRMap(bool hasEmissiveMap)
{
	if (!hasEmissiveMap)
		_material.setEmissiveTextureId(0);
	markUniformsDirty();
}

bool TriangleMesh::hasRoughnessPBRMap() const
{
	return _material.hasRoughnessMap();
}

void TriangleMesh::enableRoughnessPBRMap(bool hasRoughnessMap)
{
	if (!hasRoughnessMap)
		_material.setRoughnessTextureId(0);
	markUniformsDirty();
}

bool TriangleMesh::hasHeightPBRMap() const
{
	return _material.hasHeightMap();
}

void TriangleMesh::enableHeightPBRMap(bool hasHeightMap)
{
	if (!hasHeightMap)
		_material.setHeightTextureId(0);
	markUniformsDirty();
}

bool TriangleMesh::hasAOPBRMap() const
{
	return _material.hasAOMap();
}

void TriangleMesh::enableAOPBRMap(bool hasAOMap)
{
	if (!hasAOMap)
		_material.setOcclusionTextureId(0);
	markUniformsDirty();
}

bool TriangleMesh::hasNormalPBRMap() const
{
	return _material.hasNormalMap();
}

void TriangleMesh::enableNormalPBRMap(bool hasNormalMap)
{
	if (!hasNormalMap)
		_material.setNormalTextureId(0);
	markUniformsDirty();
}

bool TriangleMesh::hasOpacityPBRMap() const
{
	return _material.hasOpacityMap();
}

void TriangleMesh::enableOpacityPBRMap(bool hasOpacityMap)
{
	if (!hasOpacityMap)
		_material.setOpacityTextureId(0);
	markUniformsDirty();
}

bool TriangleMesh::hasTransmissionPBRMap() const
{
	return _material.hasTransmissionMap();
}

void TriangleMesh::enableTransmissionPBRMap(bool hasTransmissionMap)
{
	if (!hasTransmissionMap)
		_material.setTransmissionTextureId(0);
	markUniformsDirty();
}

void TriangleMesh::setTransmissionPBRMap(unsigned int transmissionMap)
{
	GLuint existing = static_cast<GLuint>(_material.transmissionTextureId());
	if (existing != 0)
		glDeleteTextures(1, &existing);
	_material.setTransmissionTextureId(transmissionMap);
	markTexturesDirty();
	markUniformsDirty();
}

bool TriangleMesh::hasIORPBRMap() const
{
	return _material.hasIORMap();
}

void TriangleMesh::enableIORPBRMap(bool hasIORMap)
{
	if (!hasIORMap)
		_material.setIORTextureId(0);
	markUniformsDirty();
}

void TriangleMesh::setIORPBRMap(unsigned int iorMap)
{
	GLuint existing = static_cast<GLuint>(_material.iorTextureId());
	if (existing != 0)
		glDeleteTextures(1, &existing);
	_material.setIORTextureId(iorMap);
	markTexturesDirty();
	markUniformsDirty();
}

bool TriangleMesh::hasSheenColorPBRMap() const
{
	return _material.hasSheenColorMap();
}

void TriangleMesh::enableSheenColorPBRMap(bool hasSheenColorMap)
{
	if (!hasSheenColorMap)
		_material.setSheenColorTextureId(0);
	markUniformsDirty();
}

void TriangleMesh::setSheenColorPBRMap(unsigned int sheenColorMap)
{
	GLuint existing = static_cast<GLuint>(_material.sheenColorTextureId());
	if (existing != 0)
		glDeleteTextures(1, &existing);
	_material.setSheenColorTextureId(sheenColorMap);
	markTexturesDirty();
	markUniformsDirty();
}

bool TriangleMesh::hasSheenRoughnessPBRMap() const
{
	return _material.hasSheenRoughnessMap();
}

void TriangleMesh::enableSheenRoughnessPBRMap(bool hasSheenRoughnessMap)
{
	if (!hasSheenRoughnessMap)
		_material.setSheenRoughnessTextureId(0);
	markUniformsDirty();
}

void TriangleMesh::setSheenRoughnessPBRMap(unsigned int sheenRoughnessMap)
{
	GLuint existing = static_cast<GLuint>(_material.sheenRoughnessTextureId());
	if (existing != 0)
		glDeleteTextures(1, &existing);
	_material.setSheenRoughnessTextureId(sheenRoughnessMap);
	markTexturesDirty();
	markUniformsDirty();
}

bool TriangleMesh::hasClearcoatPBRMap() const
{
	return _material.hasClearcoatColorMap();
}

void TriangleMesh::enableClearcoatPBRMap(bool hasClearcoatMap)
{
	if (!hasClearcoatMap)
		_material.setClearcoatColorTextureId(0);
	markUniformsDirty();
}

void TriangleMesh::setClearcoatPBRMap(unsigned int clearcoatColorMap)
{
	GLuint existing = static_cast<GLuint>(_material.clearcoatColorTextureId());
	if (existing != 0)
		glDeleteTextures(1, &existing);
	_material.setClearcoatColorTextureId(clearcoatColorMap);
	markTexturesDirty();
	markUniformsDirty();
}

bool TriangleMesh::hasClearcoatRoughnessPBRMap() const
{
	return _material.hasClearcoatRoughnessMap();
}

void TriangleMesh::enableClearcoatRoughnessPBRMap(bool hasClearcoatRoughnessMap)
{
	if (!hasClearcoatRoughnessMap)
		_material.setClearcoatRoughnessTextureId(0);
	markUniformsDirty();
}

void TriangleMesh::setClearcoatRoughnessPBRMap(unsigned int clearcoatRoughnessMap)
{
	GLuint existing = static_cast<GLuint>(_material.clearcoatRoughnessTextureId());
	if (existing != 0)
		glDeleteTextures(1, &existing);
	_material.setClearcoatRoughnessTextureId(clearcoatRoughnessMap);
	markTexturesDirty();
	markUniformsDirty();
}

bool TriangleMesh::hasClearcoatNormalPBRMap() const
{
	return _material.hasClearcoatNormalMap();
}

void TriangleMesh::enableClearcoatNormalPBRMap(bool hasClearcoatNormalMap)
{
	if (!hasClearcoatNormalMap)
		_material.setClearcoatNormalTextureId(0);
	markUniformsDirty();
}

void TriangleMesh::setClearcoatNormalPBRMap(unsigned int clearcoatNormalMap)
{
	GLuint existing = static_cast<GLuint>(_material.clearcoatNormalTextureId());
	if (existing != 0)
		glDeleteTextures(1, &existing);
	_material.setClearcoatNormalTextureId(clearcoatNormalMap);
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::setAlbedoPBRMap(unsigned int albedoMap)
{
	GLuint existing = static_cast<GLuint>(_material.albedoTextureId());
	if (existing != 0)
		glDeleteTextures(1, &existing);
	_material.setAlbedoTextureId(albedoMap);
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::setMetallicPBRMap(unsigned int metallicMap)
{
	GLuint existing = static_cast<GLuint>(_material.metallicTextureId());
	if (existing != 0)
		glDeleteTextures(1, &existing);
	_material.setMetallicTextureId(metallicMap);
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::setEmissivePBRMap(unsigned int emissiveMap)
{
	GLuint existing = static_cast<GLuint>(_material.emissiveTextureId());
	if (existing != 0)
		glDeleteTextures(1, &existing);
	_material.setEmissiveTextureId(emissiveMap);
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::setRoughnessPBRMap(unsigned int roughnessMap)
{
	GLuint existing = static_cast<GLuint>(_material.roughnessTextureId());
	if (existing != 0)
		glDeleteTextures(1, &existing);
	_material.setRoughnessTextureId(roughnessMap);
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::setNormalPBRMap(unsigned int normalMap)
{
	GLuint existing = static_cast<GLuint>(_material.normalTextureId());
	if (existing != 0)
		glDeleteTextures(1, &existing);
	_material.setNormalTextureId(normalMap);
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::setAOPBRMap(unsigned int aoMap)
{
	GLuint existing = static_cast<GLuint>(_material.occlusionTextureId());
	if (existing != 0)
		glDeleteTextures(1, &existing);
	_material.setOcclusionTextureId(aoMap);
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::setHeightPBRMap(unsigned int heightMap)
{
	GLuint existing = static_cast<GLuint>(_material.heightTextureId());
	if (existing != 0)
		glDeleteTextures(1, &existing);
	_material.setHeightTextureId(heightMap);
	markTexturesDirty();
	markUniformsDirty();
}

float TriangleMesh::getHeightPBRMapScale() const
{
	return _material.heightScale();
}

void TriangleMesh::setHeightPBRMapScale(float heightScale)
{
	_material.setHeightScale(heightScale);
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::setOpacityPBRMap(unsigned int opacityMap)
{
	GLuint existing = static_cast<GLuint>(_material.opacityTextureId());
	if (existing != 0)
		glDeleteTextures(1, &existing);
	_material.setOpacityTextureId(opacityMap);
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::invertOpacityPBRMap(bool invert)
{
	_material.setInvertOpacityMap(invert);
	markTexturesDirty();
	markUniformsDirty();
}

// KHR_materials_pbrSpecularGlossiness
void TriangleMesh::setSpecularGlossinessMap(unsigned int sgMap)
{
	GLuint existing = static_cast<GLuint>(_material.specularGlossinessTextureId());
	if (existing != 0)
		glDeleteTextures(1, &existing);
	_material.setSpecularGlossinessTextureId(sgMap);
	markTexturesDirty();
	markUniformsDirty();
}

bool TriangleMesh::isTransparent() const
{	
	// If it has transmission, it's ALWAYS transparent (exclude from FBO)
	if (_material.transmission() > 0.0f)
		return true;
	// If it's OPAQUE, it's NOT transparent
	if (_material.blendMode() == GLMaterial::BlendMode::Opaque)
		return false;
	// If it has BLEND mode (not MASK or OPAQUE), it's transparent
	if (_material.blendMode() == GLMaterial::BlendMode::Alpha)  // BLEND mode
		return true;
	// If it's masked, it's NOT transparent (exclude from FBO)
	if (_material.blendMode() == GLMaterial::BlendMode::Masked)
		return false;

	return (_material.opacity() < 0.999f) ||
		_hasTextureAlpha || _material.hasOpacityMap() ||
		_material.hasTransmissionMap() || _material.transmission() > 0.0f;
}

bool TriangleMesh::needsDepthMaskOff() const
{
	return (_material.opacity() < 0.999f);  // uniform-only transparency
}


void TriangleMesh::clearAlbedoPBRMap()
{
	GLuint existing = static_cast<GLuint>(_material.albedoTextureId());
	if (existing != 0)
		glDeleteTextures(1, &existing);
	_material.setAlbedoTextureId(0);
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearMetallicPBRMap()
{
	GLuint existing = static_cast<GLuint>(_material.metallicTextureId());
	if (existing != 0)
		glDeleteTextures(1, &existing);
	_material.setMetallicTextureId(0);
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearRoughnessPBRMap()
{
	GLuint existing = static_cast<GLuint>(_material.roughnessTextureId());
	if (existing != 0)
		glDeleteTextures(1, &existing);
	_material.setRoughnessTextureId(0);
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearNormalPBRMap()
{
	GLuint existing = static_cast<GLuint>(_material.normalTextureId());
	if (existing != 0)
		glDeleteTextures(1, &existing);
	_material.setNormalTextureId(0);
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearAOPBRMap()
{
	GLuint existing = static_cast<GLuint>(_material.occlusionTextureId());
	if (existing != 0)
		glDeleteTextures(1, &existing);
	_material.setOcclusionTextureId(0);
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearHeightPBRMap()
{
	GLuint existing = static_cast<GLuint>(_material.heightTextureId());
	if (existing != 0)
		glDeleteTextures(1, &existing);
	_material.setHeightTextureId(0);
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearOpacityPBRMap()
{
	GLuint existing = static_cast<GLuint>(_material.opacityTextureId());
	if (existing != 0)
		glDeleteTextures(1, &existing);
	_material.setOpacityTextureId(0);
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearTransmissionPBRMap()
{
	GLuint existing = static_cast<GLuint>(_material.transmissionTextureId());
	if (existing != 0)
		glDeleteTextures(1, &existing);
	_material.setTransmissionTextureId(0);
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearIORPBRMap()
{
	GLuint existing = static_cast<GLuint>(_material.iorTextureId());
	if (existing != 0)
		glDeleteTextures(1, &existing);
	_material.setIORTextureId(0);
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearSheenColorPBRMap()
{
	GLuint existing = static_cast<GLuint>(_material.sheenColorTextureId());
	if (existing != 0)
		glDeleteTextures(1, &existing);
	_material.setSheenColorTextureId(0);
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearSheenRoughnessPBRMap()
{
	GLuint existing = static_cast<GLuint>(_material.sheenRoughnessTextureId());
	if (existing != 0)
		glDeleteTextures(1, &existing);
	_material.setSheenRoughnessTextureId(0);
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearClearcoatPBRMap()
{
	GLuint existing = static_cast<GLuint>(_material.clearcoatColorTextureId());
	if (existing != 0)
		glDeleteTextures(1, &existing);
	_material.setClearcoatColorTextureId(0);
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearClearcoatRoughnessPBRMap()
{
	GLuint existing = static_cast<GLuint>(_material.clearcoatRoughnessTextureId());
	if (existing != 0)
		glDeleteTextures(1, &existing);
	_material.setClearcoatRoughnessTextureId(0);
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearClearcoatNormalPBRMap()
{
	GLuint existing = static_cast<GLuint>(_material.clearcoatNormalTextureId());
	if (existing != 0)
		glDeleteTextures(1, &existing);
	_material.setClearcoatNormalTextureId(0);
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearSpecularGlossinessMap()
{
	GLuint existing = static_cast<GLuint>(_material.specularGlossinessTextureId());
	if (existing != 0)
		glDeleteTextures(1, &existing);
	_material.setSpecularGlossinessTextureId(0);
	markTexturesDirty();
	markUniformsDirty();
}


void TriangleMesh::clearAllPBRMaps()
{
	GLuint pbrIds[] = {
		static_cast<GLuint>(_material.albedoTextureId()),
		static_cast<GLuint>(_material.metallicTextureId()),
		static_cast<GLuint>(_material.roughnessTextureId()),
		static_cast<GLuint>(_material.normalTextureId()),
		static_cast<GLuint>(_material.occlusionTextureId()),
		static_cast<GLuint>(_material.heightTextureId()),
		static_cast<GLuint>(_material.emissiveTextureId()),
		static_cast<GLuint>(_material.transmissionTextureId()),
		static_cast<GLuint>(_material.iorTextureId()),
		static_cast<GLuint>(_material.sheenColorTextureId()),
		static_cast<GLuint>(_material.sheenRoughnessTextureId()),
		static_cast<GLuint>(_material.clearcoatColorTextureId()),
		static_cast<GLuint>(_material.clearcoatRoughnessTextureId()),
		static_cast<GLuint>(_material.clearcoatNormalTextureId()),
		static_cast<GLuint>(_material.specularGlossinessTextureId())
	};
	for (GLuint id : pbrIds)
	{
		if (id != 0)
			glDeleteTextures(1, &id);
	}
	_material.setAlbedoTextureId(0);
	_material.setMetallicTextureId(0);
	_material.setRoughnessTextureId(0);
	_material.setNormalTextureId(0);
	_material.setOcclusionTextureId(0);
	_material.setHeightTextureId(0);
	_material.setEmissiveTextureId(0);
	_material.setTransmissionTextureId(0);
	_material.setIORTextureId(0);
	_material.setSheenColorTextureId(0);
	_material.setSheenRoughnessTextureId(0);
	_material.setClearcoatColorTextureId(0);
	_material.setClearcoatRoughnessTextureId(0);
	_material.setClearcoatNormalTextureId(0);
	_material.setSpecularGlossinessTextureId(0);

	markTexturesDirty();
	markUniformsDirty();
}

const GLMaterial* TriangleMesh::materialForVariant(int variantIndex) const
{
	if (_variantMappings.isEmpty())
		return nullptr;

	if (variantIndex < 0)
	{
		// Reset to file default
		auto it = _allVariantMaterials.find(_originalMaterialIndex);
		return (it != _allVariantMaterials.end()) ? &it.value() : nullptr;
	}

	for (const GltfVariantMapping& vm : _variantMappings)
	{
		if (vm.variantIndices.contains(variantIndex))
		{
			auto it = _allVariantMaterials.find(vm.materialIndex);
			return (it != _allVariantMaterials.end()) ? &it.value() : nullptr;
		}
	}

	return nullptr;  // no explicit mapping — caller keeps current material
}

// ---------------------------------------------------------------------------
// Debug texture overrides (TextureDebugPanel)
// ---------------------------------------------------------------------------
void TriangleMesh::setDebugTextureOverride(int unit, GLuint replaceTex)
{
	_debugTextureOverrides[unit] = replaceTex;
}

void TriangleMesh::clearDebugTextureOverride(int unit)
{
	_debugTextureOverrides.remove(unit);
}

void TriangleMesh::clearAllDebugTextureOverrides()
{
	_debugTextureOverrides.clear();
}

void TriangleMesh::applyDebugTextureOverrides()
{
	if (_debugTextureOverrides.isEmpty())
		return;

	for (auto it = _debugTextureOverrides.constBegin();
	     it != _debugTextureOverrides.constEnd(); ++it)
	{
		glActiveTexture(GL_TEXTURE0 + it.key());
		glBindTexture(GL_TEXTURE_2D, it.value());
	}
	// Restore the active texture to unit 0 so subsequent code is not confused.
	glActiveTexture(GL_TEXTURE0);
}

void TriangleMesh::setDebugUniformOverride(const QString& name, const QVariant& value)
{
	_debugUniformOverrides[name] = value;
}

void TriangleMesh::clearDebugUniformOverride(const QString& name)
{
	_debugUniformOverrides.remove(name);
}

void TriangleMesh::clearAllDebugUniformOverrides()
{
	_debugUniformOverrides.clear();
}

void TriangleMesh::applyDebugUniformOverrides()
{
	if (_debugUniformOverrides.isEmpty() || !_prog)
		return;

	for (auto it = _debugUniformOverrides.constBegin();
	     it != _debugUniformOverrides.constEnd(); ++it)
	{
		const QByteArray nameBytes = it.key().toUtf8();
		const char* uName = nameBytes.constData();

		const int typeId = it.value().userType();
		if (typeId == qMetaTypeId<QVector3D>())
			_prog->setUniformValue(uName, it.value().value<QVector3D>());
		else if (typeId == QMetaType::Bool)
			_prog->setUniformValue(uName, it.value().toBool());
		else if (typeId == QMetaType::Int)
			_prog->setUniformValue(uName, it.value().toInt());
		else
			_prog->setUniformValue(uName, it.value().toFloat());
	}
}
