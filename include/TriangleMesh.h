#pragma once

#include <vector>
#include "Drawable.h"
#include "BoundingSphere.h"
#include "BoundingBox.h"
#include "GLMaterial.h"

class Triangle;

class TriangleMesh : public Drawable
{
	Q_OBJECT
public:
	TriangleMesh(QOpenGLShaderProgram* prog, const QString name);

	virtual ~TriangleMesh();

	virtual void setProg(QOpenGLShaderProgram* prog);

	virtual TriangleMesh* clone() = 0;

	// Setter for primitive mode (from glTF)
	void setPrimitiveMode(GLenum mode)
	{
		_primitiveMode = mode;
	}

	GLenum getPrimitiveMode() const
	{
		return _primitiveMode;
	}

	virtual void render();
	virtual void renderShadow(); // Lightweight render for shadow mapping

	virtual void select()
	{
		_selected = true;
		markUniformsDirty();
	}
	virtual void deselect()
	{
		_selected = false;
		markUniformsDirty();
	}

	virtual BoundingSphere getBoundingSphere() const { return _boundingSphere; }
	virtual BoundingBox getBoundingBox() const { return _boundingBox; }

	virtual QOpenGLVertexArrayObject& getVAO();
	virtual QString getName() const
	{
		return _name;
	}

	virtual unsigned long long memorySize() const;

	QVector3D ambientMaterial() const;
	void setAmbientMaterial(const QVector3D& ambient);

	QVector3D diffuseMaterial() const;
	void setDiffuseMaterial(const QVector3D& diffuse);

	QVector3D specularMaterial() const;
	void setSpecularMaterial(const QVector3D& specular);

	QVector3D emmissiveMaterial() const;
	void setEmmissiveMaterial(const QVector3D& emissive);

	float opacity() const;
	void setOpacity(const float& opacity);

	float shininess() const;
	void setShininess(const float& shininess);

	bool isMetallic() const;
	void setMetallic(bool metallic);

	bool hasTexture() const;
	void enableTexture(const bool& bHasTexture);

	void setTexureImage(const QImage& texImage);

	void setPBRAlbedoColor(const float& r, const float& g, const float& b);
	void setPBRMetallic(const float& val);
	void setPBRRoughness(const float& val);

	float getHighestXValue() const;
	float getLowestXValue() const;
	float getHighestYValue() const;
	float getLowestYValue() const;
	float getHighestZValue() const;
	float getLowestZValue() const;
	QRect projectedRect(const QMatrix4x4& modelView, const QMatrix4x4& projection, const QRect& viewport, const QRect& window) const;

	QVector3D getTranslation() const;
	void setTranslation(const QVector3D& trans);

	QVector3D getRotation() const;
	void setRotation(const QVector3D& rota);

	QVector3D getScaling() const;
	void setScaling(const QVector3D& scale);

	QMatrix4x4 getTransformation() const;

	std::vector<unsigned int> getIndices() const;
	std::vector<float> getPoints() const;
	std::vector<float> getNormals() const;
	std::vector<float> getTexCoords() const;
	std::vector<float> getTrsfPoints() const;

	void bakeTransformations();
	void resetTransformations();

	// Sync vertex data after baking transformations.
	// Called by bakeTransformations() to ensure vertex positions/normals match baked geometry.
	// Subclasses override to update their specific vertex storage.
	virtual void syncVertexDataAfterBake() { }

	void setHasNegativeScale(bool hasNegativeScale) { _hasNegativeScale = hasNegativeScale; };
	bool hasNegativeScale() const { return _hasNegativeScale; }

	void setSceneIndex(int index) { _sceneIndex = index; }
	int getSceneIndex() const { return _sceneIndex; }

	// Store the original material index from aiMesh::mMaterialIndex at import time.
	// Used during export to ensure correct material assignment without relying on name matching.
	void setOriginalMaterialIndex(int index) { _originalMaterialIndex = index; }
	int getOriginalMaterialIndex() const { return _originalMaterialIndex; }

	// Returns a sort key based on primary texture IDs to minimise GPU texture
	// state changes when opaque meshes are sorted before drawing.
	uint64_t getTextureSortKey() const
	{
		// Combine albedo + normal as the two most commonly varying maps.
		// XOR-shift spreads bits to reduce collisions on small IDs.
		uint64_t a = _albedoPBRMap ? _albedoPBRMap : _diffuseADSMap;
		uint64_t n = _normalPBRMap ? _normalPBRMap : _normalADSMap;
		return (a << 32) ^ (n * 2654435761ULL);
	}

	virtual bool intersectsWithRay(const QVector3D& rayPos, const QVector3D& rayDir, QVector3D& outIntersectionPoint);

	virtual void setAlbedoPBRMap(unsigned int albedoMap);
	virtual void setNormalPBRMap(unsigned int normalMap);
	virtual void setMetallicPBRMap(unsigned int metallicMap);
	virtual void setEmissivePBRMap(unsigned int emissiveMap);
	virtual void setRoughnessPBRMap(unsigned int roughnessMap);
	virtual void setAOPBRMap(unsigned int aoMap);
	virtual void setHeightPBRMap(unsigned int heightMap);
	virtual void setOpacityPBRMap(unsigned int opacityMap);
	virtual void invertOpacityPBRMap(bool invert);
	virtual void setSpecularGlossinessMap(unsigned int sgMap);

	virtual unsigned int getAlbedoPBRMap() const { return _albedoPBRMap; }
	virtual unsigned int getNormalPBRMap() const { return _normalPBRMap; }
	virtual unsigned int getMetallicPBRMap() const { return _metallicPBRMap; }
	virtual unsigned int getEmissivePBRMap() const { return _emissivePBRMap; }
	virtual unsigned int getRoughnessPBRMap() const { return _roughnessPBRMap; }
	virtual unsigned int getAOPBRMap() const { return _aoPBRMap; }
	virtual unsigned int getHeightPBRMap() const { return _heightPBRMap; }
	virtual unsigned int getOpacityPBRMap() const { return _opacityPBRMap; }
	virtual unsigned int getTransmissionPBRMap() const { return _transmissionPBRMap; }
	virtual unsigned int getIORPBRMap() const { return _IORPBRMap; }
	virtual unsigned int getSheenColorPBRMap() const { return _sheenColorPBRMap; }
	virtual unsigned int getSheenRoughnessPBRMap() const { return _sheenRoughnessPBRMap; }
	virtual unsigned int getClearcoatPBRMap() const { return _clearcoatPBRMap; }
	virtual unsigned int getClearcoatRoughnessPBRMap() const { return _clearcoatRoughnessPBRMap; }
	virtual unsigned int getClearcoatNormalPBRMap() const { return _clearcoatNormalPBRMap; }


	virtual bool hasAlbedoPBRMap() const;
	virtual void enableAlbedoPBRMap(bool hasAlbedoMap);

	virtual bool hasMetallicPBRMap() const;
	virtual void enableMetallicPBRMap(bool hasMetallicMap);

	virtual bool hasEmissivePBRMap() const;
	virtual void enableEmissivePBRMap(bool hasEmissiveMap);
	 
	virtual bool hasRoughnessPBRMap() const;
	virtual void enableRoughnessPBRMap(bool hasRoughnessMap);
	
	virtual bool hasNormalPBRMap() const;
	virtual void enableNormalPBRMap(bool hasNormalMap);
	
	virtual bool hasAOPBRMap() const;
	virtual void enableAOPBRMap(bool hasAOMap);
	
	virtual bool hasHeightPBRMap() const;
	virtual void enableHeightPBRMap(bool hasHeightMap);
	
	virtual float getHeightPBRMapScale() const;
	virtual void setHeightPBRMapScale(float heightScale);
	
	virtual bool hasOpacityPBRMap() const;
	virtual void enableOpacityPBRMap(bool hasHeightMap);

	virtual bool hasTransmissionPBRMap() const;
	virtual void enableTransmissionPBRMap(bool hasTransmissionMap);
	virtual void setTransmissionPBRMap(unsigned int transmissionMap);

	virtual bool hasIORPBRMap() const;
	virtual void enableIORPBRMap(bool hasIORMap);
	virtual void setIORPBRMap(unsigned int iorMap);

	virtual bool hasSheenColorPBRMap() const;
	virtual void enableSheenColorPBRMap(bool hasSheenColorMap);
	virtual void setSheenColorPBRMap(unsigned int sheenColorMap);

	virtual bool hasSheenRoughnessPBRMap() const;
	virtual void enableSheenRoughnessPBRMap(bool hasSheenRoughnessMap);
	virtual void setSheenRoughnessPBRMap(unsigned int sheenRoughnessMap);

	virtual bool hasClearcoatPBRMap() const;
	virtual void enableClearcoatPBRMap(bool hasClearcoatMap);
	virtual void setClearcoatPBRMap(unsigned int clearcoatColorMap);

	virtual bool hasClearcoatRoughnessPBRMap() const;
	virtual void enableClearcoatRoughnessPBRMap(bool hasClearcoatRoughnessMap);
	virtual void setClearcoatRoughnessPBRMap(unsigned int clearcoatRoughnessMap);

	virtual bool hasClearcoatNormalPBRMap() const;
	virtual void enableClearcoatNormalPBRMap(bool hasClearcoatNormalMap);
	virtual void setClearcoatNormalPBRMap(unsigned int clearcoatNormalMap);

	virtual bool isTransparent() const;
	virtual bool needsDepthMaskOff() const ;
	
	virtual void clearAlbedoPBRMap();
	virtual void clearMetallicPBRMap();
	virtual void clearRoughnessPBRMap();
	virtual void clearNormalPBRMap();
	virtual void clearAOPBRMap();
	virtual void clearHeightPBRMap();
	virtual void clearOpacityPBRMap();
	virtual void clearTransmissionPBRMap();
	virtual void clearIORPBRMap();
	virtual void clearSheenColorPBRMap();
	virtual void clearSheenRoughnessPBRMap();
	virtual void clearClearcoatPBRMap();
	virtual void clearClearcoatRoughnessPBRMap();
	virtual void clearClearcoatNormalPBRMap();
	virtual void clearSpecularGlossinessMap();

	virtual void clearAllPBRMaps();
	
	virtual GLMaterial getMaterial() const;
	virtual void setMaterial(const GLMaterial& material);

	virtual void setTextureMaps(const GLMaterial& material);
	
	virtual void enableDiffuseADSMap(bool enable);
	virtual void setDiffuseADSMap(unsigned int diffuseTex);
	virtual void enableSpecularADSMap(bool enable);
	virtual void setSpecularADSMap(unsigned int specularTex);
	virtual void enableEmissiveADSMap(bool enable);
	virtual void setEmissiveADSMap(unsigned int emissiveTex);
	virtual void enableNormalADSMap(bool enable);
	virtual void setNormalADSMap(unsigned int normalTex);
	virtual void enableHeightADSMap(bool enable);
	virtual void setHeightADSMap(unsigned int heightTex);
	virtual void enableOpacityADSMap(bool enable);
	virtual void invertOpacityADSMap(bool invert);
	virtual void setOpacityADSMap(unsigned int opacityTex);
	
	virtual void clearDiffuseADSMap();
	virtual void clearSpecularADSMap();
	virtual void clearEmissiveADSMap();
	virtual void clearNormalADSMap();
	virtual void clearHeightADSMap();
	virtual void clearOpacityADSMap();
	virtual void clearAllADSMaps();
	

	virtual void markTexturesDirty() { _textureBindingsDirty = true; }
	virtual void markUniformsDirty() { _uniformsDirty = true; }

	virtual void deleteTextures();

protected: // methods
	virtual void initBuffers(
		std::vector<unsigned int>* indices,
		std::vector<float>* points,
		std::vector<float>* normals,
		std::vector<float>* colors = nullptr,
		std::vector<float>* texCoords = nullptr,
		std::vector<float>* tangents = nullptr,
		std::vector<float>* bitangents = nullptr
	);

	void buildTriangles();
    void computeBounds();
    void deleteBuffers();

    virtual void setupTransformation();
	virtual void setupTextures();
	virtual void setupUniforms();

protected:

	QOpenGLBuffer _indexBuffer;
	QOpenGLBuffer _positionBuffer;
	QOpenGLBuffer _normalBuffer;
	QOpenGLBuffer _colorBuffer;
	QOpenGLBuffer _texCoord0Buffer;
	QOpenGLBuffer _texCoord1Buffer;
	QOpenGLBuffer _texCoord2Buffer;
	QOpenGLBuffer _texCoord3Buffer;
	QOpenGLBuffer _tangentBuf;
	QOpenGLBuffer _bitangentBuf;

	QOpenGLBuffer _coordBuf;

	unsigned int _nVerts;     // Number of vertices
	QOpenGLVertexArrayObject _vertexArrayObject;        // The Vertex Array Object

	// Vertex buffers
	std::vector<QOpenGLBuffer> _buffers;

	BoundingSphere _boundingSphere;
	BoundingBox    _boundingBox;

	std::vector<Triangle*> _triangles;

	GLMaterial _material;

	QImage _texImage, _texBuffer;
	// ADS texture light maps
	unsigned int _texture;
	unsigned int _diffuseADSMap;
	unsigned int _specularADSMap;
	unsigned int _emissiveADSMap;
	unsigned int _normalADSMap;
	unsigned int _heightADSMap;
	unsigned int _opacityADSMap;
	bool _hasTexture;
	bool _hasTextureAlpha;
	bool _hasDiffuseADSMap;
	bool _hasSpecularADSMap;
	bool _hasEmissiveADSMap;
	bool _hasNormalADSMap;
	bool _hasHeightADSMap;
	bool _hasOpacityADSMap;
	bool _opacityADSMapInverted;
	
	unsigned int _sMax;
	unsigned int _tMax;

	// PBR texture maps
	unsigned int _albedoPBRMap;
	unsigned int _metallicPBRMap;
	unsigned int _emissivePBRMap;
	unsigned int _roughnessPBRMap;
	unsigned int _normalPBRMap;
	unsigned int _aoPBRMap;
	unsigned int _heightPBRMap;
	unsigned int _opacityPBRMap;
	unsigned int _transmissionPBRMap;
	unsigned int _IORPBRMap;
	unsigned int _sheenColorPBRMap;
	unsigned int _sheenRoughnessPBRMap;
	unsigned int _clearcoatPBRMap;
	unsigned int _clearcoatRoughnessPBRMap;
	unsigned int _clearcoatNormalPBRMap;
	// KHR_materials_pbrSpecularGlossiness
	unsigned int _specularGlossinessMap = 0;
	bool _hasAlbedoPBRMap;
	bool _hasMetallicPBRMap;
	bool _hasEmissivePBRMap;
	bool _hasRoughnessPBRMap;
	bool _hasNormalPBRMap;
	bool _hasAOPBRMap;
	bool _hasHeightPBRMap;
	float _heightPBRMapScale;
	bool _hasOpacityPBRMap;
	bool _opacityPBRMapInverted;
	bool _hasTransmissionPBRMap;
	bool _hasIORPBRMap;
	bool _hasSheenColorPBRMap;
	bool _hasSheenRoughnessPBRMap;
	bool _hasClearcoatPBRMap;
	bool _hasClearcoatRoughnessPBRMap;
	bool _hasClearcoatNormalPBRMap;

	bool _textureBindingsDirty = true;

	bool _uniformsDirty = true;

	std::vector<unsigned int> _indices;
	std::vector<float> _points;
	std::vector<float> _normals;
	std::vector<float> _colors;
	std::vector<float> _tangents;
	std::vector<float> _bitangents;
	std::vector<float> _texCoords;
	std::vector<float> _trsfPoints;
	std::vector<float> _trsfNormals;
	std::vector<float> _trsfTangents;
	std::vector<float> _trsfBitangents;

	bool _hasVertexColors;

	bool _hasNegativeScale = false;

	// Primitive mode from glTF (GL_POINTS=0, GL_LINES=1, GL_LINE_STRIP=3, GL_TRIANGLE_STRIP=5, GL_TRIANGLES=4)
	GLenum _primitiveMode = GL_TRIANGLES;  // Default to triangles for backward compatibility

	// Original index into aiScene::mMeshes[] at load time.
	// -1 for meshes not originating from an Assimp scene (parametric shapes, etc.).
	// Used by the exporter to match surviving TriangleMesh objects back to aiMesh
	// entries in the deep-copied scene without relying on name strings.
	int _sceneIndex = -1;

	// Material index from aiMesh::mMaterialIndex at import time.
	// Stores which material in the scene's material array applies to this mesh.
	// Used during export to ensure correct material assignment via index matching,
	// avoiding fragile name-based matching. -1 if not from an Assimp scene.
	int _originalMaterialIndex = -1;

	// Individual transformation components
	float _transX;
	float _transY;
	float _transZ;

	float _rotateX;
	float _rotateY;
	float _rotateZ;

	float _scaleX;
	float _scaleY;
	float _scaleZ;

	QMatrix4x4 _transformation;

	unsigned long long _memorySize;
};
