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
#include <QVector3D>

TriangleMesh::TriangleMesh(QOpenGLShaderProgram* prog, const QString name) : Drawable(prog),
_nVerts(0),
_texture(0),
_diffuseADSMap(0),
_specularADSMap(0),
_emissiveADSMap(0),
_normalADSMap(0),
_heightADSMap(0),
_opacityADSMap(0),
_hasTexture(false),
_hasDiffuseADSMap(false),
_hasSpecularADSMap(false),
_hasEmissiveADSMap(false),
_hasNormalADSMap(false),
_hasHeightADSMap(false),
_hasOpacityADSMap(false),
_opacityADSMapInverted(false),
_sMax(1),
_tMax(1),
_albedoPBRMap(0),
_metallicPBRMap(0),
_emissivePBRMap(0),
_roughnessPBRMap(0),
_normalPBRMap(0),
_aoPBRMap(0),
_opacityPBRMap(0),
_heightPBRMap(0),
_transmissionPBRMap(0),
_IORPBRMap(0),
_sheenColorPBRMap(0),
_sheenRoughnessPBRMap(0),
_clearcoatPBRMap(0),
_clearcoatRoughnessPBRMap(0),
_clearcoatNormalPBRMap(0),
_hasAlbedoPBRMap(false),
_hasMetallicPBRMap(false),
_hasEmissivePBRMap(false),
_hasRoughnessPBRMap(false),
_hasNormalPBRMap(false),
_hasAOPBRMap(false),
_hasHeightPBRMap(false),
_heightPBRMapScale(0.02f),
_hasOpacityPBRMap(false),
_hasTransmissionPBRMap(false),
_hasIORPBRMap(false),
_hasSheenColorPBRMap(false),
_hasSheenRoughnessPBRMap(false),
_hasClearcoatPBRMap(false),
_hasClearcoatRoughnessPBRMap(false),
_hasClearcoatNormalPBRMap(false),
_opacityPBRMapInverted(false),
_hasTextureAlpha(false),
_hasVertexColors(false)
{
	setAutoIncrName(name);
	_memorySize = 0;
	_transX = _transY = _transZ = 0.0f;
	_rotateX = _rotateY = _rotateZ = 0.0f;
	_scaleX = _scaleY = _scaleZ = 1.0f;
	_transformation.setToIdentity();

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

	_vertexArrayObject.create();
		
	QImage dummy(128, 128, QImage::Format_ARGB32);
	dummy.fill(Qt::white);
	_texBuffer = dummy;
	_texImage = convertToGLFormat(_texBuffer);

	glGenTextures(1, &_texture);
	//std::cout << "TriangleMesh::TriangleMesh : _texture = " << _texture << std::endl;
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, _texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
}

void TriangleMesh::initBuffers(
	std::vector<unsigned int>* indices,
	std::vector<float>* points,
	std::vector<float>* normals,
	std::vector<float>* colors,
	std::vector<float>* texCoords,
	std::vector<float>* tangents,
	std::vector<float>* bitangents)
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

	_memorySize = 0;
	_memorySize = (_points.size() + _normals.size() + _indices.size()) * sizeof(float);

	_nVerts = (unsigned int)indices->size();

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

	_vertexArrayObject.release();
	markUniformsDirty();
}

void TriangleMesh::setupTextures()
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, _texture);
	// Only re-upload texture data when it has actually changed — avoids a full
	// glTexImage2D + mipmap regeneration every frame for every mesh.
	if (_textureBindingsDirty && !_texImage.isNull())
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _texImage.width(), _texImage.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, _texImage.bits());
		glGenerateMipmap(GL_TEXTURE_2D);
		_textureBindingsDirty = false;
	}

	// ADS light texture maps
	glActiveTexture(GL_TEXTURE10);
	glBindTexture(GL_TEXTURE_2D, _diffuseADSMap);
	glActiveTexture(GL_TEXTURE11);
	glBindTexture(GL_TEXTURE_2D, _specularADSMap);
	glActiveTexture(GL_TEXTURE12);
	glBindTexture(GL_TEXTURE_2D, _emissiveADSMap);
	glActiveTexture(GL_TEXTURE13);
	glBindTexture(GL_TEXTURE_2D, _normalADSMap);
	glActiveTexture(GL_TEXTURE14);
	glBindTexture(GL_TEXTURE_2D, _heightADSMap);
	glActiveTexture(GL_TEXTURE15);
	glBindTexture(GL_TEXTURE_2D, _opacityADSMap);

	// PBR light texture maps
	glActiveTexture(GL_TEXTURE10);
	glBindTexture(GL_TEXTURE_2D, _albedoPBRMap);
	glActiveTexture(GL_TEXTURE11);
	glBindTexture(GL_TEXTURE_2D, _metallicPBRMap);	
	glActiveTexture(GL_TEXTURE12);
	glBindTexture(GL_TEXTURE_2D, _emissivePBRMap);
	glActiveTexture(GL_TEXTURE13);
	glBindTexture(GL_TEXTURE_2D, _normalPBRMap);	
	glActiveTexture(GL_TEXTURE14);
	glBindTexture(GL_TEXTURE_2D, _heightPBRMap);
	glActiveTexture(GL_TEXTURE15);
	glBindTexture(GL_TEXTURE_2D, _opacityPBRMap);		
	glActiveTexture(GL_TEXTURE16);
	glBindTexture(GL_TEXTURE_2D, _roughnessPBRMap);
	glActiveTexture(GL_TEXTURE17);
	glBindTexture(GL_TEXTURE_2D, _aoPBRMap);
	
	// Advanced PBR maps
	glActiveTexture(GL_TEXTURE18);
	glBindTexture(GL_TEXTURE_2D, _transmissionPBRMap);
	glActiveTexture(GL_TEXTURE19);
	glBindTexture(GL_TEXTURE_2D, _IORPBRMap);
	glActiveTexture(GL_TEXTURE20);
	glBindTexture(GL_TEXTURE_2D, _sheenColorPBRMap);
	glActiveTexture(GL_TEXTURE21);
	glBindTexture(GL_TEXTURE_2D, _sheenRoughnessPBRMap);
	glActiveTexture(GL_TEXTURE22);
	glBindTexture(GL_TEXTURE_2D, _clearcoatPBRMap);
	glActiveTexture(GL_TEXTURE23);
	glBindTexture(GL_TEXTURE_2D, _clearcoatRoughnessPBRMap);
	glActiveTexture(GL_TEXTURE24);
	glBindTexture(GL_TEXTURE_2D, _clearcoatNormalPBRMap);
}

void TriangleMesh::setupUniforms()
{
	if (!_uniformsDirty) return;
	_prog->bind();
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
	_prog->setUniformValue("texEnabled", _hasTexture);
	_prog->setUniformValue("texUnit", 0);
	_prog->setUniformValue("material.ambient", _material.ambient());
	_prog->setUniformValue("material.diffuse", _material.diffuse());
	_prog->setUniformValue("material.specular", _material.specular());
	_prog->setUniformValue("material.emission", _material.emissive());
	_prog->setUniformValue("material.shininess", _material.shininess());
	_prog->setUniformValue("material.metallic", _material.metallic());
	_prog->setUniformValue("opacity", _material.opacity());
	// ADS light texture maps
	_prog->setUniformValue("hasDiffuseTexture", _hasDiffuseADSMap);
	_prog->setUniformValue("hasSpecularTexture", _hasSpecularADSMap);
	_prog->setUniformValue("hasEmissiveTexture", _hasEmissiveADSMap);
	_prog->setUniformValue("hasNormalTexture", _hasNormalADSMap);
	_prog->setUniformValue("hasHeightTexture", _hasHeightADSMap);
	_prog->setUniformValue("hasOpacityTexture", _hasOpacityADSMap);
	_prog->setUniformValue("opacityTextureInverted", _opacityADSMapInverted);

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

	// Albedo tinting
	_prog->setUniformValue("tintMode", GLint(_material.albedoTint.mode));
	_prog->setUniformValue("tintStrength", _material.albedoTint.strength);
	_prog->setUniformValue("grayEpsilon", _material.albedoTint.grayEps);
	_prog->setUniformValue("useVertexColor", _material.albedoTint.useVertexColor);
	_prog->setUniformValue("tintMaskChannel", _material.albedoTint.maskChannel);

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
	// Advanced PBR Maps
	_prog->setUniformValue("transmissionMap", 18);
	_prog->setUniformValue("iorMap", 19);
	_prog->setUniformValue("sheenColorMap", 20);
	_prog->setUniformValue("sheenRoughnessMap", 21);
	_prog->setUniformValue("clearcoatColorMap", 22);
	_prog->setUniformValue("clearcoatRoughnessMap", 23);
	_prog->setUniformValue("clearcoatNormalMap", 24);

	// KHR_materials_specular
	_prog->setUniformValue("specularFactorMap", 25);
	_prog->setUniformValue("hasSpecularFactorMap", _material.hasSpecularFactorMap());

	_prog->setUniformValue("specularColorMap", 26);
	_prog->setUniformValue("hasSpecularColorMap", _material.hasSpecularColorMap());

	// KHR_materials_pbrSpecularGlossiness
	_prog->setUniformValue("diffuseMap", 10);
	_prog->setUniformValue("specularGlossinessMap", 25);
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
	_prog->setUniformValue("anisotropyMap", 27);
	_prog->setUniformValue("hasAnisotropyMap", _material.hasAnisotropyMap());

	// KHR_materials_iridescence
	_prog->setUniformValue("iridescenceMap", 28);
	_prog->setUniformValue("hasIridescenceMap", _material.hasIridescenceMap());

	_prog->setUniformValue("iridescenceThicknessMap", 29);
	_prog->setUniformValue("hasIridescenceThicknessMap", _material.hasIridescenceThicknessMap());

	// KHR_materials_volume
	_prog->setUniformValue("thicknessMap", 30);
	bool hasThicknessMap = _material.hasThicknessMap();
	_prog->setUniformValue("hasThicknessMap", _material.hasThicknessMap());
	bool hasThicknessAlpa = _material.hasThicknessAlpha();
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
		_material.hasDiffuseTransmissionMap());
	_prog->setUniformValue("diffuseTransmissionMap", 31);
	
	// Diffuse Transmission Color Map
	_prog->setUniformValue("hasDiffuseTransmissionColorMap",
		_material.hasDiffuseTransmissionColorMap());
	_prog->setUniformValue("diffuseTransmissionColorMap", 9);

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
	_prog->setUniformValue("hasTransmissionMap", _material.hasTransmissionMap());
	_prog->setUniformValue("hasIORMap", _material.hasIORMap());
	_prog->setUniformValue("hasSheenColorMap", _material.hasSheenColorMap());
	_prog->setUniformValue("hasSheenRoughnessMap", _material.hasSheenRoughnessMap());
	_prog->setUniformValue("hasClearcoatMap", _material.hasClearcoatColorMap());
	_prog->setUniformValue("hasClearcoatRoughnessMap", _material.hasClearcoatRoughnessMap());
	_prog->setUniformValue("hasClearcoatNormalMap", _material.hasClearcoatNormalMap());

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

void TriangleMesh::enableOpacityADSMap(bool enable)
{
	_hasOpacityADSMap = enable;
}

void TriangleMesh::invertOpacityADSMap(bool invert)
{
	_opacityADSMapInverted = invert;
}

void TriangleMesh::setOpacityADSMap(unsigned int opacityTex)
{
	//glDeleteTextures(1, &_opacityADSMap);
	_opacityADSMap = opacityTex;
	_hasOpacityADSMap = true;
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::enableHeightADSMap(bool enable)
{
	_hasHeightADSMap = enable;
}

void TriangleMesh::setHeightADSMap(unsigned int heightTex)
{
	//glDeleteTextures(1, &_heightADSMap);
	_heightADSMap = heightTex;
	_hasHeightADSMap = true;
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::enableNormalADSMap(bool enable)
{
	_hasNormalADSMap = enable;
}

void TriangleMesh::setNormalADSMap(unsigned int normalTex)
{
	//glDeleteTextures(1, &_normalADSMap);
	_normalADSMap = normalTex;
	_hasNormalADSMap = true;
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::enableSpecularADSMap(bool enable)
{
	_hasSpecularADSMap = enable;
}

void TriangleMesh::setSpecularADSMap(unsigned int specularTex)
{
	//glDeleteTextures(1, &_specularADSMap);
	_specularADSMap = specularTex;
	_hasSpecularADSMap = true;
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::enableEmissiveADSMap(bool enable)
{
	_hasEmissiveADSMap = enable;
}

void TriangleMesh::setEmissiveADSMap(unsigned int emissiveTex)
{
	//glDeleteTextures(1, &_emissiveADSMap);
	_emissiveADSMap = emissiveTex;
	_hasEmissiveADSMap = true;
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::enableDiffuseADSMap(bool enable)
{
	_hasDiffuseADSMap = enable;
}

void TriangleMesh::setDiffuseADSMap(unsigned int diffuseTex)
{
	//glDeleteTextures(1, &_diffuseADSMap);
	_diffuseADSMap = diffuseTex;
	_hasDiffuseADSMap = true;
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearDiffuseADSMap()
{
	glDeleteTextures(1, &_diffuseADSMap);
	_diffuseADSMap = 0;
}

void TriangleMesh::clearSpecularADSMap()
{
	glDeleteTextures(1, &_specularADSMap);
	_specularADSMap = 0;
}

void TriangleMesh::clearEmissiveADSMap()
{
	glDeleteTextures(1, &_emissiveADSMap);
	_emissiveADSMap = 0;
}

void TriangleMesh::clearNormalADSMap()
{
	glDeleteTextures(1, &_normalADSMap);
	_normalADSMap = 0;
}

void TriangleMesh::clearHeightADSMap()
{
	glDeleteTextures(1, &_heightADSMap);
	_heightADSMap = 0;
}

void TriangleMesh::clearOpacityADSMap()
{
	glDeleteTextures(1, &_opacityADSMap);
	_opacityADSMap = 0;
}

void TriangleMesh::clearAllADSMaps()
{
	// Delete all ADS maps
	glDeleteTextures(1, &_diffuseADSMap);
	_diffuseADSMap = 0;
	glDeleteTextures(1, &_specularADSMap);
	_specularADSMap = 0;
	glDeleteTextures(1, &_emissiveADSMap);
	_emissiveADSMap = 0;
	glDeleteTextures(1, &_normalADSMap);
	_normalADSMap = 0;
	glDeleteTextures(1, &_heightADSMap);
	_heightADSMap = 0;

	// Disable all ADS maps
	enableDiffuseADSMap(false);
	enableSpecularADSMap(false);
	enableEmissiveADSMap(false);
	enableNormalADSMap(false);
	enableHeightADSMap(false);
	enableOpacityADSMap(false);

	// Mark dirty to update shaders
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
	markUniformsDirty();
}

void TriangleMesh::setTextureMaps(const GLMaterial& material)
{
	qDebug() << "=== TriangleMesh::setTextureMaps START ===";
	qDebug() << "Received material scalar values:";
	qDebug() << "  Metalness:" << material.metalness();
	qDebug() << "  Roughness:" << material.roughness();
	qDebug() << "  IOR:" << material.ior();
	qDebug() << "  Transmission:" << material.transmission();

	_material = material;

	qDebug() << "After copying to _material:";
	qDebug() << "  Metalness:" << _material.metalness();
	qDebug() << "  Roughness:" << _material.roughness();
	qDebug() << "=== TriangleMesh::setTextureMaps END ===";

	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::render()
{
	if (!_vertexArrayObject.isCreated())
		return;

	setupTextures();

	setupUniforms();

	if(_material.opacity() < 1.0f ||
		_hasOpacityADSMap || _hasOpacityPBRMap || _hasTransmissionPBRMap || 
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
	//std::cout << "TriangleMesh::deleteTextures : _texture = " << _texture << std::endl;

	glDeleteTextures(1, &_texture);
	glDeleteTextures(1, &_diffuseADSMap);
	glDeleteTextures(1, &_specularADSMap);
	glDeleteTextures(1, &_emissiveADSMap);
	glDeleteTextures(1, &_normalADSMap);
	glDeleteTextures(1, &_heightADSMap);
	glDeleteTextures(1, &_opacityADSMap);
	glDeleteTextures(1, &_albedoPBRMap);
	glDeleteTextures(1, &_metallicPBRMap);
	glDeleteTextures(1, &_roughnessPBRMap);
	glDeleteTextures(1, &_normalPBRMap);
	glDeleteTextures(1, &_aoPBRMap);
	glDeleteTextures(1, &_heightPBRMap);
	glDeleteTextures(1, &_opacityPBRMap);
	glDeleteTextures(1, &_transmissionPBRMap);
	glDeleteTextures(1, &_IORPBRMap);
	glDeleteTextures(1, &_sheenColorPBRMap);
	glDeleteTextures(1, &_sheenRoughnessPBRMap);
	glDeleteTextures(1, &_clearcoatPBRMap);
	glDeleteTextures(1, &_clearcoatRoughnessPBRMap);
	glDeleteTextures(1, &_clearcoatNormalPBRMap);
	glDeleteTextures(1, &_specularGlossinessMap);
}

TriangleMesh::~TriangleMesh()
{
	deleteBuffers();
#ifdef Q_OS_WIN
	//deleteTextures(); // causes wrong texture deletion on Linux
#endif
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
	auto xSpan = (xmax - xmin).lengthSquared();
	auto ySpan = (ymax - ymin).lengthSquared();
	auto zSpan = (zmax - zmin).lengthSquared();
	auto dia1 = xmin;
	auto dia2 = xmax;
	auto maxSpan = xSpan;
	if (ySpan > maxSpan)
	{
		maxSpan = ySpan;
		dia1 = ymin;
		dia2 = ymax;
	}
	if (zSpan > maxSpan)
	{
		dia1 = zmin;
		dia2 = zmax;
	}
	auto center = (dia1 + dia2) * 0.5f;
	auto sqRad = (dia2 - center).lengthSquared();
	auto radius = sqrt(sqRad);
	for (auto p : aPoints)
	{
		float d = (p - center).lengthSquared();
		if (d > sqRad)
		{
			auto r = sqrt(d);
			radius = (radius + r) * 0.5f;
			sqRad = radius * radius;
			auto offset = r - radius;
			center = (radius * center + offset * p) / r;
		}
	}

	_boundingSphere.setCenter(center);
	_boundingSphere.setRadius(radius);

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

std::vector<float> TriangleMesh::getTrsfPoints() const
{
	return _trsfPoints;
}

void TriangleMesh::bakeTransformations()
{
	// Transform the points as permanently
	_points   = _trsfPoints ;
	_normals = _trsfNormals;
	_tangents = _trsfTangents;    
	_bitangents = _trsfBitangents;
	resetTransformations();
}

void TriangleMesh::resetTransformations()
{
	_transX = _transY = _transZ = 0.0f;
	_rotateX = _rotateY = _rotateZ = 0.0f;
	_scaleX = _scaleY = _scaleZ = 1.0f;

	_transformation.setToIdentity();

	_trsfPoints.clear();
	_trsfNormals.clear();
	_trsfTangents.clear();  
	_trsfBitangents.clear();

	_trsfPoints = _points;
	_trsfNormals = _normals;
	_trsfTangents = _tangents;     
	_trsfBitangents = _bitangents; 

	_prog->bind();
	_positionBuffer.bind();
	_positionBuffer.allocate(_points.data(), static_cast<int>(_points.size() * sizeof(float)));
	_prog->enableAttributeArray("vertexPosition");
	_prog->setAttributeBuffer("vertexPosition", GL_FLOAT, 0, 3);

	_normalBuffer.bind();
	_normalBuffer.allocate(_normals.data(), static_cast<int>(_normals.size() * sizeof(float)));
	_prog->enableAttributeArray("vertexNormal");
	_prog->setAttributeBuffer("vertexNormal", GL_FLOAT, 0, 3);

	// Reset tangent buffer
	if (!_tangents.empty())
	{
		_tangentBuf.bind();
		_tangentBuf.allocate(_tangents.data(), static_cast<int>(_tangents.size() * sizeof(float)));
		_prog->enableAttributeArray("vertexTangent");
		_prog->setAttributeBuffer("vertexTangent", GL_FLOAT, 0, 3);
	}

	// Reset bitangent buffer
	if (!_bitangents.empty())
	{
		_bitangentBuf.bind();
		_bitangentBuf.allocate(_bitangents.data(), static_cast<int>(_bitangents.size() * sizeof(float)));
		_prog->enableAttributeArray("vertexBitangent");
		_prog->setAttributeBuffer("vertexBitangent", GL_FLOAT, 0, 3);
	}

	buildTriangles();
	computeBounds();
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
	_transX = trans.x() - _transX;
	_transY = trans.y() - _transY;
	_transZ = trans.z() - _transZ;
	_transformation.translate(_transX, _transY, _transZ);
	setupTransformation();
	_transX = trans.x();
	_transY = trans.y();
	_transZ = trans.z();
}

QVector3D TriangleMesh::getRotation() const
{
	return QVector3D(_rotateX, _rotateY, _rotateZ);
}

void TriangleMesh::setRotation(const QVector3D& rota)
{
	_rotateX = rota.x() - _rotateX;
	_rotateY = rota.y() - _rotateY;
	_rotateZ = rota.z() - _rotateZ;
	_transformation.rotate(_rotateX, QVector3D(1.0f, 0.0f, 0.0f));
	_transformation.rotate(_rotateY, QVector3D(0.0f, 1.0f, 0.0f));
	_transformation.rotate(_rotateZ, QVector3D(0.0f, 0.0f, 1.0f));
	setupTransformation();
	_rotateX = rota.x();
	_rotateY = rota.y();
	_rotateZ = rota.z();
}

QVector3D TriangleMesh::getScaling() const
{
	return QVector3D(_scaleX, _scaleY, _scaleZ);
}

void TriangleMesh::setScaling(const QVector3D& scale)
{
	_scaleX = scale.x() / _scaleX;
	_scaleY = scale.y() / _scaleY;
	_scaleZ = scale.z() / _scaleZ;
	_transformation.scale(_scaleX, _scaleY, _scaleZ);
	setupTransformation();
	_scaleX = scale.x();
	_scaleY = scale.y();
	_scaleZ = scale.z();
	
	// Apply scale to material properties that depend on scale
	float appliedScale = (_scaleX + _scaleY + _scaleZ) / 3.0f;	
	_material.setThicknessFactor(_material.thicknessFactor() * appliedScale);
	_material.setAttenuationDistance(_material.attenuationDistance() * appliedScale);
}

QMatrix4x4 TriangleMesh::getTransformation() const
{
	return _transformation;
}

void TriangleMesh::setupTransformation()
{
	_prog->bind();
	_trsfPoints.clear();
	_trsfNormals.clear();
	_trsfTangents.clear();
	_trsfBitangents.clear();

	// ============================================================================
	// TRANSFORM POSITIONS
	// ============================================================================
	for (size_t i = 0; i < _points.size(); i += 3)
	{
		QVector3D p(_points[i + 0], _points[i + 1], _points[i + 2]);
		QVector3D tp = _transformation.map(p);
		_trsfPoints.push_back(tp.x());
		_trsfPoints.push_back(tp.y());
		_trsfPoints.push_back(tp.z());
	}
	_positionBuffer.bind();
	_positionBuffer.allocate(_trsfPoints.data(), static_cast<int>(_trsfPoints.size() * sizeof(float)));
	_prog->enableAttributeArray("vertexPosition");
	_prog->setAttributeBuffer("vertexPosition", GL_FLOAT, 0, 3);

	// ============================================================================
	// TRANSFORM NORMALS
	// ============================================================================
	// Use inverse-transpose for proper normal transformation (handles non-uniform scaling)
	for (size_t i = 0; i < _normals.size(); i += 3)
	{
		QVector3D n(_normals[i + 0], _normals[i + 1], _normals[i + 2]);
		QMatrix4x4 rotMat = _transformation;
		// Extract rotation/scale part only (zero out translation)
		rotMat.setColumn(3, QVector4D(0, 0, 0, 1));
		QVector3D tn = rotMat.map(n);
		_trsfNormals.push_back(tn.x());
		_trsfNormals.push_back(tn.y());
		_trsfNormals.push_back(tn.z());
	}
	_normalBuffer.bind();
	_normalBuffer.allocate(_trsfNormals.data(), static_cast<int>(_trsfNormals.size() * sizeof(float)));
	_prog->enableAttributeArray("vertexNormal");
	_prog->setAttributeBuffer("vertexNormal", GL_FLOAT, 0, 3);

	// ============================================================================
	// TRANSFORM TANGENTS using inverse-transpose
	// This is critical for normal mapping and anisotropy to work correctly
	// ============================================================================
	if (!_tangents.empty())
	{
		QMatrix4x4 rotMat = _transformation;
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

		_tangentBuf.bind();
		_tangentBuf.allocate(_trsfTangents.data(), static_cast<int>(_trsfTangents.size() * sizeof(float)));
		_prog->enableAttributeArray("vertexTangent");
		_prog->setAttributeBuffer("vertexTangent", GL_FLOAT, 0, 3);
	}

	// ============================================================================
	// TRANSFORM BITANGENTS using inverse-transpose
	// ============================================================================
	if (!_bitangents.empty())
	{
		QMatrix4x4 rotMat = _transformation;
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

		_bitangentBuf.bind();
		_bitangentBuf.allocate(_trsfBitangents.data(), static_cast<int>(_trsfBitangents.size() * sizeof(float)));
		_prog->enableAttributeArray("vertexBitangent");
		_prog->setAttributeBuffer("vertexBitangent", GL_FLOAT, 0, 3);
	}

	buildTriangles();
	computeBounds();
}

void TriangleMesh::setTexureImage(const QImage& texImage)
{
	_texImage = texImage;
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, _texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _texImage.width(), _texImage.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, _texImage.bits());
}

bool TriangleMesh::hasTexture() const
{
	return _hasTexture;
}

void TriangleMesh::enableTexture(const bool& bHasTexture)
{
	_hasTexture = bHasTexture;
	markUniformsDirty();
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
	float closestDistance = std::numeric_limits<float>::max();
	bool found = false;
	QVector3D bestIntersection;


	float localMinDist = std::numeric_limits<float>::max();
	QVector3D localIntersection;
	bool localFound = false;

	for (int i = 0; i < _triangles.size(); ++i)
	{
		QVector3D hitPoint;
		if (_triangles[i]->intersectsWithRay(rayPos, rayDir, hitPoint))
		{
			float dist = (hitPoint - rayPos).length();
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
		bestIntersection = localIntersection;
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
	return _hasAlbedoPBRMap;
}

void TriangleMesh::enableAlbedoPBRMap(bool hasAlbedoMap)
{
	_hasAlbedoPBRMap = hasAlbedoMap;
	markUniformsDirty();
}

bool TriangleMesh::hasMetallicPBRMap() const
{
	return _hasMetallicPBRMap;
}

void TriangleMesh::enableMetallicPBRMap(bool hasMetallicMap)
{
	_hasMetallicPBRMap = hasMetallicMap;
	markUniformsDirty();
}

bool TriangleMesh::hasEmissivePBRMap() const
{
	return _hasEmissivePBRMap;
}

void TriangleMesh::enableEmissivePBRMap(bool hasEmissiveMap)
{
	_hasEmissivePBRMap = true;
	markUniformsDirty();
}

bool TriangleMesh::hasRoughnessPBRMap() const
{
	return _hasRoughnessPBRMap;
}

void TriangleMesh::enableRoughnessPBRMap(bool hasRoughnessMap)
{
	_hasRoughnessPBRMap = hasRoughnessMap;
	markUniformsDirty();
}

bool TriangleMesh::hasHeightPBRMap() const
{
	return _hasHeightPBRMap;
}

void TriangleMesh::enableHeightPBRMap(bool hasHeightMap)
{
	_hasHeightPBRMap = hasHeightMap;
	markUniformsDirty();
}

bool TriangleMesh::hasAOPBRMap() const
{
	return _hasAOPBRMap;
}

void TriangleMesh::enableAOPBRMap(bool hasAOMap)
{
	_hasAOPBRMap = hasAOMap;
	markUniformsDirty();
}

bool TriangleMesh::hasNormalPBRMap() const
{
	return _hasNormalPBRMap;
}

void TriangleMesh::enableNormalPBRMap(bool hasNormalMap)
{
	_hasNormalPBRMap = hasNormalMap;
	markUniformsDirty();
}

bool TriangleMesh::hasOpacityPBRMap() const
{
	return _hasOpacityPBRMap;
}

void TriangleMesh::enableOpacityPBRMap(bool hasOpacityMap)
{
	_hasOpacityPBRMap = hasOpacityMap;
	markUniformsDirty();
}

bool TriangleMesh::hasTransmissionPBRMap() const
{
	return _hasTransmissionPBRMap;
}

void TriangleMesh::enableTransmissionPBRMap(bool hasTransmissionMap)
{
	_hasTransmissionPBRMap = hasTransmissionMap;
	markUniformsDirty();
}

void TriangleMesh::setTransmissionPBRMap(unsigned int transmissionMap)
{
	glDeleteTextures(1, &_transmissionPBRMap);
	_transmissionPBRMap = transmissionMap;
	_hasTransmissionPBRMap = true;
	markTexturesDirty();
	markUniformsDirty();
}

bool TriangleMesh::hasIORPBRMap() const
{
	return _hasIORPBRMap;
}

void TriangleMesh::enableIORPBRMap(bool hasIORMap)
{
	_hasIORPBRMap = hasIORMap;
	markUniformsDirty();
}

void TriangleMesh::setIORPBRMap(unsigned int iorMap)
{
	glDeleteTextures(1, &_IORPBRMap);
	_IORPBRMap = iorMap;
	_hasIORPBRMap = true;
	markTexturesDirty();
	markUniformsDirty();
}

bool TriangleMesh::hasSheenColorPBRMap() const
{
	return _hasSheenColorPBRMap;
}

void TriangleMesh::enableSheenColorPBRMap(bool hasSheenColorMap)
{
	_hasSheenColorPBRMap = hasSheenColorMap;
	markUniformsDirty();
}

void TriangleMesh::setSheenColorPBRMap(unsigned int sheenColorMap)
{
	glDeleteTextures(1, &_sheenColorPBRMap);
	_sheenColorPBRMap = sheenColorMap;
	_hasSheenColorPBRMap = true;
	markTexturesDirty();
	markUniformsDirty();
}

bool TriangleMesh::hasSheenRoughnessPBRMap() const
{
	return _hasSheenColorPBRMap;
}

void TriangleMesh::enableSheenRoughnessPBRMap(bool hasSheenRoughnessMap)
{
	_hasSheenRoughnessPBRMap = hasSheenRoughnessMap;
	markUniformsDirty();
}

void TriangleMesh::setSheenRoughnessPBRMap(unsigned int sheenRoughnessMap)
{
	glDeleteTextures(1, &_sheenRoughnessPBRMap);
	_sheenRoughnessPBRMap = sheenRoughnessMap;
	_hasSheenRoughnessPBRMap = true;
	markTexturesDirty();
	markUniformsDirty();
}

bool TriangleMesh::hasClearcoatPBRMap() const
{
	return _hasClearcoatPBRMap;
}

void TriangleMesh::enableClearcoatPBRMap(bool hasClearcoatMap)
{
	_hasClearcoatPBRMap = hasClearcoatMap;
	markUniformsDirty();
}

void TriangleMesh::setClearcoatPBRMap(unsigned int clearcoatColorMap)
{
	glDeleteTextures(1, &_clearcoatPBRMap);
	_clearcoatPBRMap = clearcoatColorMap;
	_hasClearcoatPBRMap = true;
	markTexturesDirty();
	markUniformsDirty();
}

bool TriangleMesh::hasClearcoatRoughnessPBRMap() const
{
	return _hasClearcoatRoughnessPBRMap;
}

void TriangleMesh::enableClearcoatRoughnessPBRMap(bool hasClearcoatRoughnessMap)
{
	_hasClearcoatRoughnessPBRMap = hasClearcoatRoughnessMap;
	markUniformsDirty();
}

void TriangleMesh::setClearcoatRoughnessPBRMap(unsigned int clearcoatRoughnessMap)
{
	glDeleteTextures(1, &_clearcoatRoughnessPBRMap);
	_clearcoatRoughnessPBRMap = clearcoatRoughnessMap;
	_hasClearcoatRoughnessPBRMap = true;
	markTexturesDirty();
	markUniformsDirty();
}

bool TriangleMesh::hasClearcoatNormalPBRMap() const
{
	return _hasClearcoatNormalPBRMap;
}

void TriangleMesh::enableClearcoatNormalPBRMap(bool hasClearcoatNormalMap)
{
	_hasClearcoatNormalPBRMap = hasClearcoatNormalMap;
	markUniformsDirty();
}

void TriangleMesh::setClearcoatNormalPBRMap(unsigned int clearcoatNormalMap)
{
	glDeleteTextures(1, &_clearcoatNormalPBRMap);
	_clearcoatNormalPBRMap = clearcoatNormalMap;
	_hasClearcoatNormalPBRMap = true;
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::setAlbedoPBRMap(unsigned int albedoMap)
{
	glDeleteTextures(1, &_albedoPBRMap);
	_albedoPBRMap = albedoMap;
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::setMetallicPBRMap(unsigned int metallicMap)
{
	glDeleteTextures(1, &_metallicPBRMap);
	_metallicPBRMap = metallicMap;
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::setEmissivePBRMap(unsigned int emissiveMap)
{
	glDeleteTextures(1, &_emissivePBRMap);
	_emissivePBRMap = _emissivePBRMap;
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::setRoughnessPBRMap(unsigned int roughnessMap)
{
	glDeleteTextures(1, &_roughnessPBRMap);
	_roughnessPBRMap = roughnessMap;
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::setNormalPBRMap(unsigned int normalMap)
{
	glDeleteTextures(1, &_normalPBRMap);
	_normalPBRMap = normalMap;
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::setAOPBRMap(unsigned int aoMap)
{
	glDeleteTextures(1, &_aoPBRMap);
	_aoPBRMap = aoMap;
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::setHeightPBRMap(unsigned int heightMap)
{
	glDeleteTextures(1, &_heightPBRMap);
	_heightPBRMap = heightMap;
	markTexturesDirty();
	markUniformsDirty();
}

float TriangleMesh::getHeightPBRMapScale() const
{
	return _heightPBRMapScale;
}

void TriangleMesh::setHeightPBRMapScale(float heightScale)
{
	_heightPBRMapScale = heightScale;
	_material.setHeightScale(heightScale);
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::setOpacityPBRMap(unsigned int opacityMap)
{
	glDeleteTextures(1, &_opacityPBRMap);
	_opacityPBRMap = opacityMap;
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::invertOpacityPBRMap(bool invert)
{
	_opacityPBRMapInverted = invert;
	markTexturesDirty();
	markUniformsDirty();
}

// KHR_materials_pbrSpecularGlossiness
void TriangleMesh::setSpecularGlossinessMap(unsigned int sgMap)
{
	glDeleteTextures(1, &_specularGlossinessMap);
	_specularGlossinessMap = sgMap;
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
		_hasTextureAlpha || _hasOpacityADSMap || _hasOpacityPBRMap ||
		_hasTransmissionPBRMap || _material.transmission() > 0.0f;
}

bool TriangleMesh::needsDepthMaskOff() const
{
	return (_material.opacity() < 0.999f);  // uniform-only transparency
}


void TriangleMesh::clearAlbedoPBRMap()
{
	glDeleteTextures(1, &_albedoPBRMap);
	_albedoPBRMap = 0;
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearMetallicPBRMap()
{
	glDeleteTextures(1, &_metallicPBRMap);
	_metallicPBRMap = 0;
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearRoughnessPBRMap()
{
	glDeleteTextures(1, &_roughnessPBRMap);
	_roughnessPBRMap = 0;
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearNormalPBRMap()
{
	glDeleteTextures(1, &_normalPBRMap);
	_normalPBRMap = 0;
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearAOPBRMap()
{
	glDeleteTextures(1, &_aoPBRMap);
	_aoPBRMap = 0;
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearHeightPBRMap()
{
	glDeleteTextures(1, &_heightPBRMap);
	_heightPBRMap = 0;
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearOpacityPBRMap()
{
	glDeleteTextures(1, &_opacityPBRMap);
	_opacityPBRMap = 0;
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearTransmissionPBRMap()
{
	glDeleteTextures(1, &_transmissionPBRMap);
	_transmissionPBRMap = 0;
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearIORPBRMap()
{
	glDeleteTextures(1, &_IORPBRMap);
	_IORPBRMap = 0;
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearSheenColorPBRMap()
{
	glDeleteTextures(1, &_sheenColorPBRMap);
	_sheenColorPBRMap = 0;
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearSheenRoughnessPBRMap()
{
	glDeleteTextures(1, &_sheenRoughnessPBRMap);
	_sheenRoughnessPBRMap = 0;
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearClearcoatPBRMap()
{
	glDeleteTextures(1, &_clearcoatPBRMap);
	_clearcoatPBRMap = 0;
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearClearcoatRoughnessPBRMap()
{
	glDeleteTextures(1, &_clearcoatRoughnessPBRMap);
	_clearcoatRoughnessPBRMap = 0;
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearClearcoatNormalPBRMap()
{
	glDeleteTextures(1, &_clearcoatNormalPBRMap);
	_clearcoatNormalPBRMap = 0;
	markTexturesDirty();
	markUniformsDirty();
}

void TriangleMesh::clearSpecularGlossinessMap()
{
	glDeleteTextures(1, &_specularGlossinessMap);
	_specularGlossinessMap = 0;
	markTexturesDirty();
	markUniformsDirty();
}


void TriangleMesh::clearAllPBRMaps()
{
	glDeleteTextures(1, &_albedoPBRMap);
	_albedoPBRMap = 0;
	glDeleteTextures(1, &_metallicPBRMap);
	_metallicPBRMap = 0;
	glDeleteTextures(1, &_roughnessPBRMap);
	_roughnessPBRMap = 0;
	glDeleteTextures(1, &_normalPBRMap);
	_normalPBRMap = 0;
	glDeleteTextures(1, &_aoPBRMap);
	_aoPBRMap = 0;
	glDeleteTextures(1, &_heightPBRMap);
	_heightPBRMap = 0;
	glDeleteTextures(1, &_emissivePBRMap);
	_emissivePBRMap = 0;
	glDeleteTextures(1, &_transmissionPBRMap);
	_transmissionPBRMap = 0;
	glDeleteTextures(1, &_IORPBRMap);
	_IORPBRMap = 0;
	glDeleteTextures(1, &_sheenColorPBRMap);
	_sheenColorPBRMap = 0;
	glDeleteTextures(1, &_sheenRoughnessPBRMap);
	_sheenRoughnessPBRMap = 0;
	glDeleteTextures(1, &_clearcoatPBRMap);
	_clearcoatPBRMap = 0;
	glDeleteTextures(1, &_clearcoatRoughnessPBRMap);
	_clearcoatRoughnessPBRMap = 0;
	glDeleteTextures(1, &_clearcoatNormalPBRMap);
	_clearcoatNormalPBRMap = 0;
	glDeleteTextures(1, &_specularGlossinessMap);
	_specularGlossinessMap = 0;

	markTexturesDirty();
	markUniformsDirty();
}

