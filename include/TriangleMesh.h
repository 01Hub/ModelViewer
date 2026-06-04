#pragma once

#include <vector>
#include "Drawable.h"
#include "BoundingSphere.h"
#include "BoundingBox.h"
#include "GLMaterial.h"
#include "GltfAnimationData.h"
#include "GltfVariantData.h"

#include <QMap>
#include <QVariant>
#include <QVector3D>

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
	QVector3D getStableTransformCenter() const;
	float getStableTransformRadius() const;

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
	QMatrix4x4 getSceneRenderTransform() const;
	QMatrix4x4 combinedRenderTransform() const;

	std::vector<unsigned int> getIndices() const;
	std::vector<float> getPoints() const;
	std::vector<float> getNormals() const;
	std::vector<float> getTexCoords() const;
	const std::vector<float>& getTrsfPoints() const;

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

	// -----------------------------------------------------------------------
	// Source file tracking
	// -----------------------------------------------------------------------
	void    setSourceFile(const QString& path) { _sourceFile = path; }
	QString getSourceFile() const              { return _sourceFile; }
	void    setSourceNodeName(const QString& name) { _sourceNodeName = name; }
	QString getSourceNodeName() const              { return _sourceNodeName; }
	void    setSceneRenderTransform(const QMatrix4x4& trsf);
	void    setSceneRenderTransformFast(const QMatrix4x4& trsf);

	// -----------------------------------------------------------------------
	// KHR_materials_variants support
	// -----------------------------------------------------------------------

	// Per-primitive variant->material mappings (empty for non-variant meshes).
	void setVariantMappings(const QVector<GltfVariantMapping>& m) { _variantMappings = m; }
	const QVector<GltfVariantMapping>& variantMappings() const    { return _variantMappings; }

	// Pre-built GLMaterial for every material index referenced by variants.
	// Includes the default material keyed by originalMaterialIndex.
	void setAllVariantMaterials(const QMap<int, GLMaterial>& mats) { _allVariantMaterials = mats; }
	const QMap<int, GLMaterial>& allVariantMaterials() const       { return _allVariantMaterials; }

	bool hasVariants() const { return !_variantMappings.isEmpty(); }

	// Return a pointer to the prebuilt GLMaterial for the given variant index,
	// or nullptr when this mesh has no mapping for that variant (keep default).
	// Pass variantIndex = -1 to retrieve the original/default material.
	const GLMaterial* materialForVariant(int variantIndex) const;

	void setActiveVariantIndex(int idx) { _activeVariantIndex = idx; }
	int  activeVariantIndex() const     { return _activeVariantIndex; }

	void setSkinJoints(const QVector<GltfSkinJoint>& joints) { _skinJoints = joints; }
	const QVector<GltfSkinJoint>& skinJoints() const { return _skinJoints; }
	bool hasSkinning() const { return !_skinJoints.isEmpty(); }
	void setJointPalette(const QVector<QMatrix4x4>& palette) { _jointPalette = palette; }
	const QVector<QMatrix4x4>& jointPalette() const { return _jointPalette; }
	virtual bool hasMorphTargets() const { return false; }
	virtual QVector<float> defaultMorphWeights() const { return {}; }
	virtual void applyMorphWeights(const QVector<float>&) { }
	virtual void resetMorphTargets() { }

	// Returns a sort key based on primary texture IDs to minimise GPU texture
	// state changes when opaque meshes are sorted before drawing.
	uint64_t getTextureSortKey() const
	{
		// Combine albedo + normal as the two most commonly varying maps.
		// XOR-shift spreads bits to reduce collisions on small IDs.
		uint64_t a = _material.hasAlbedoMap() ? static_cast<uint64_t>(_material.albedoTextureId())
			: (_material.hasDiffuseMap() ? static_cast<uint64_t>(_material.diffuseTextureId()) : 0ULL);
		uint64_t n = _material.hasNormalMap() ? static_cast<uint64_t>(_material.normalTextureId()) : 0ULL;
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

	virtual unsigned int getAlbedoPBRMap() const { return static_cast<unsigned int>(_material.albedoTextureId()); }
	virtual unsigned int getNormalPBRMap() const { return static_cast<unsigned int>(_material.normalTextureId()); }
	virtual unsigned int getMetallicPBRMap() const { return static_cast<unsigned int>(_material.metallicTextureId()); }
	virtual unsigned int getEmissivePBRMap() const { return static_cast<unsigned int>(_material.emissiveTextureId()); }
	virtual unsigned int getRoughnessPBRMap() const { return static_cast<unsigned int>(_material.roughnessTextureId()); }
	virtual unsigned int getAOPBRMap() const { return static_cast<unsigned int>(_material.occlusionTextureId()); }
	virtual unsigned int getHeightPBRMap() const { return static_cast<unsigned int>(_material.heightTextureId()); }
	virtual unsigned int getOpacityPBRMap() const { return static_cast<unsigned int>(_material.opacityTextureId()); }
	virtual unsigned int getTransmissionPBRMap() const { return static_cast<unsigned int>(_material.transmissionTextureId()); }
	virtual unsigned int getIORPBRMap() const { return static_cast<unsigned int>(_material.iorTextureId()); }
	virtual unsigned int getSheenColorPBRMap() const { return static_cast<unsigned int>(_material.sheenColorTextureId()); }
	virtual unsigned int getSheenRoughnessPBRMap() const { return static_cast<unsigned int>(_material.sheenRoughnessTextureId()); }
	virtual unsigned int getClearcoatPBRMap() const { return static_cast<unsigned int>(_material.clearcoatColorTextureId()); }
	virtual unsigned int getClearcoatRoughnessPBRMap() const { return static_cast<unsigned int>(_material.clearcoatRoughnessTextureId()); }
	virtual unsigned int getClearcoatNormalPBRMap() const { return static_cast<unsigned int>(_material.clearcoatNormalTextureId()); }


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

	void cacheBaseVolumeProperties();
	void applyScaledVolumeProperties();
	
	virtual void setDiffuseADSMap(unsigned int diffuseTex);
	virtual void setSpecularADSMap(unsigned int specularTex);
	virtual void setEmissiveADSMap(unsigned int emissiveTex);
	virtual void setNormalADSMap(unsigned int normalTex);
	virtual void setHeightADSMap(unsigned int heightTex);
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
	virtual void updateRuntimeBounds();

	virtual void deleteTextures();

	// ---- Debug texture overrides (TextureDebugPanel) -------------------------
	// Replace a texture unit with an alternative texture for the next draw call.
	// Pass replaceTex = 0 to bind "no texture" (black), or pass the ID of a
	// neutral 1×1 placeholder created by GLWidget.
	void setDebugTextureOverride(int unit, GLuint replaceTex);
	void clearDebugTextureOverride(int unit);
	void clearAllDebugTextureOverrides();

	// ---- Debug uniform overrides (TextureDebugPanel extension toggles) --------
	// Override a named scalar/vec3 uniform after setupUniforms() for one frame.
	// Supports float and QVector3D values.  Cleared automatically when the panel
	// re-enables the extension (which also sets markUniformsDirty so the shader
	// restores the original value on the very next frame).
	void setDebugUniformOverride(const QString& name, const QVariant& value);
	void clearDebugUniformOverride(const QString& name);
	void clearAllDebugUniformOverrides();

protected: // methods
	virtual void initBuffers(
		std::vector<unsigned int>* indices,
		std::vector<float>* points,
		std::vector<float>* normals,
		std::vector<float>* colors = nullptr,
		std::vector<float>* texCoords = nullptr,
		std::vector<float>* tangents = nullptr,
		std::vector<float>* bitangents = nullptr,
		std::vector<float>* jointIndices = nullptr,
		std::vector<float>* jointWeights = nullptr
	);

	void buildTriangles();
    void computeBounds();
    void deleteBuffers();
	void rebuildAbsoluteTransformation();

    virtual void setupTransformation();
	virtual void setupTextures();
	virtual void setupUniforms();

	// Rebinds debug-override textures after the normal texture setup.
	// Call at the end of any render() path that binds textures.
	void applyDebugTextureOverrides();

	// Re-sets debug-override uniforms after setupUniforms().
	// Must be called unconditionally every frame (not gated by _uniformsDirty).
	void applyDebugUniformOverrides();

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
	QOpenGLBuffer _jointIndexBuffer;
	QOpenGLBuffer _jointWeightBuffer;

	QOpenGLBuffer _coordBuf;

	unsigned int _nVerts;     // Number of vertices
	QOpenGLVertexArrayObject _vertexArrayObject;        // The Vertex Array Object

	// Vertex buffers
	std::vector<QOpenGLBuffer> _buffers;

	BoundingSphere _boundingSphere;
	BoundingBox    _boundingBox;
	// Local-space bounding box of _points (no transform applied).
	// Computed once in updateRuntimeBounds() and used by setSceneRenderTransformFast()
	// to cheaply recompute _boundingBox each animation frame without iterating
	// every vertex.  This keeps frustum culling correct for animated meshes.
	BoundingBox    _localBoundingBox;

	std::vector<Triangle*> _triangles;

	GLMaterial _material;
	float _baseThicknessFactor;
	float _baseAttenuationDistance;

	// Internal always-valid fallback texture bound on unit 0. This is no longer
	// a user-facing mesh texture path, but some render paths still rely on the
	// presence of a complete 2D texture object there.
	QImage _fallbackTextureImage, _fallbackTextureBuffer;
	unsigned int _fallbackTexture;
	bool _hasTextureAlpha;
	
	unsigned int _sMax;
	unsigned int _tMax;

	bool _textureBindingsDirty = true;

	bool _uniformsDirty = true;

	// Debug texture overrides set by TextureDebugPanel.
	// Maps GL texture unit index → replacement texture ID.
	// Applied after setupTextures() / bindTexturesOptimized() so they override
	// whatever was just bound, without modifying the actual material state.
	QMap<int, GLuint> _debugTextureOverrides;

	// Debug uniform overrides set by TextureDebugPanel extension toggles.
	// Maps uniform name → replacement value (float or QVector3D).
	// Applied unconditionally every frame after setupUniforms() so the override
	// persists even when _uniformsDirty is false.
	QMap<QString, QVariant> _debugUniformOverrides;

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
	std::vector<float> _jointIndices;
	std::vector<float> _jointWeights;

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

	// Absolute path of the file this mesh was loaded from (empty for parametric shapes).
	QString _sourceFile;
	QString _sourceNodeName;
	QMatrix4x4 _sceneRenderTransform;

	// KHR_materials_variants: per-primitive variant->material mappings.
	QVector<GltfVariantMapping> _variantMappings;

	// Pre-built GLMaterial for each material index referenced by variants.
	QMap<int, GLMaterial> _allVariantMaterials;

	// Currently active variant (-1 = file default).
	int _activeVariantIndex = -1;
	QVector<GltfSkinJoint> _skinJoints;
	QVector<QMatrix4x4> _jointPalette;

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
