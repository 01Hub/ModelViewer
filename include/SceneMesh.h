#pragma once

#include <string>
#include <vector>
#include <initializer_list>

#include "DeformableMesh.h"
// GLMaterial, Vertex, MorphTargetData — transitive via DeformableMesh.h → TriangleMesh.h

class SceneMesh : public DeformableMesh
{
public:

	/*  Functions  */
	// Constructor
	SceneMesh(QOpenGLShaderProgram* shader, QString name, std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<GLMaterial::Texture> textures, GLMaterial material, bool skipOptimization = false);
	~SceneMesh();
	virtual TriangleMesh* clone();
	void setProg(QOpenGLShaderProgram* prog) override;
	void render();
	void renderWireframeFast(QOpenGLShaderProgram* wireProg) override;
	void renderFeatureEdgesFast(QOpenGLShaderProgram* wireProg) override;
	quint64 getRenderMaterialSortKey() const override;
	void markUniformsDirty() override;
	static void resetSharedUniformStateCache();

    std::vector<Vertex> vertices() const;

    std::vector<unsigned int> indices() const;

    std::vector<GLMaterial::Texture> textures() const;

	void getMeshData(std::vector<Vertex>& vertices,
		std::vector<unsigned int>& indices) const;

	// Set new mesh data and upload to GPU (no optimization)
	void setMeshData(const std::vector<Vertex>& vertices,
		const std::vector<unsigned int>& indices,
		const std::vector<unsigned int>* sourceVertexMap = nullptr);
	void setMorphTargets(const QVector<MorphTargetData>& targets,
		const QVector<float>& defaultWeights);

	// Upload precomputed B-Rep edge segments from OCC (STEP/IGES/BREP).
	// edgeVerts is flat {x0,y0,z0, x1,y1,z1, ...}; bounds[i] = first vec3-index of
	// topological edge i (bounds.back() = total count, sentinel).
	// When set, renderFeatureEdgesFast() uses this buffer instead of the heuristic classifier.
	void setPrecomputedOccEdges(const std::vector<float>& edgeVerts,
	                            const std::vector<int>& bounds = {});
	const std::vector<float>& getOccEdgeSegments()    const { return _importState.occEdgeSegments(); }
	const std::vector<int>&   getOccEdgeBoundaries()  const { return _importState.occEdgeBoundaries(); }

	// hasMorphTargets(), getMorphTargets(), defaultMorphWeights() — now on TriangleMesh base
	void applyMorphWeights(const QVector<float>& weights) override;
	void resetMorphTargets() override;

	void setAlbedoPBRMap(unsigned int albedoMap) override;
	void setMetallicPBRMap(unsigned int metallicMap) override;
	void setEmissivePBRMap(unsigned int emissiveMap) override;
	void setRoughnessPBRMap(unsigned int roughnessMap) override;
	void setNormalPBRMap(unsigned int normalMap) override;
	void setAOPBRMap(unsigned int aoMap) override;
	void setHeightPBRMap(unsigned int heightMap) override;
	void setOpacityPBRMap(unsigned int opacityMap) override;
	void setIORPBRMap(unsigned int iorMap) override;
	void setClearcoatPBRMap(unsigned int clearcoatColorMap) override;
	void setClearcoatRoughnessPBRMap(unsigned int clearcoatRoughnessMap) override;
	void setClearcoatNormalPBRMap(unsigned int clearcoatNormalMap) override;
	void setSheenColorPBRMap(unsigned int sheenMap) override;
	void setSheenRoughnessPBRMap(unsigned int sheenRoughnessMap) override;
	void setTransmissionPBRMap(unsigned int transmissionMap) override;
	void clearAlbedoPBRMap() override;
	void clearMetallicPBRMap() override;
	void clearRoughnessPBRMap() override;
	void clearNormalPBRMap() override;
	void clearAOPBRMap() override;
	void clearHeightPBRMap() override;
	void clearOpacityPBRMap() override;
	void clearTransmissionPBRMap() override;
	void clearIORPBRMap() override;
	void clearSheenColorPBRMap() override;
	void clearSheenRoughnessPBRMap() override;
	void clearClearcoatPBRMap() override;
	void clearClearcoatRoughnessPBRMap() override;
	void clearClearcoatNormalPBRMap() override;
	void clearAllPBRMaps() override;

	// implementations for enabling/disabling textures
	void setDiffuseADSMap(unsigned int diffuseTex) override;
	void setSpecularADSMap(unsigned int specularTex) override;
	void setEmissiveADSMap(unsigned int emissiveTex) override;
	void setNormalADSMap(unsigned int normalTex) override;
	void setHeightADSMap(unsigned int heightTex) override;
	void setOpacityADSMap(unsigned int opacityTex) override;
	void clearDiffuseADSMap() override;
	void clearSpecularADSMap() override;
	void clearEmissiveADSMap() override;
	void clearNormalADSMap() override;
	void clearHeightADSMap() override;
	void clearOpacityADSMap() override;
	void clearAllADSMaps() override;

	virtual void setTextureMaps(const GLMaterial& material) override;
	void deleteTextures() override;
	void replaceOrAppendTexture(const std::string& type, GLuint id, bool hasAlpha);

	
private:
	void optimizeMesh();
	/*  Functions    */
	// Initializes all the buffer objects/arrays
	void setupMesh();
	void buildAndUploadFeatureEdges(float thresholdDegrees = 15.0f);

	void cacheTextureBindings();
	void bindTexturesOptimized();
	void setRenderStateOptimized();
	void setupUniformsOptimized();
	quint64 uniformStateSignature() const;
	void removeTexturesByType(std::initializer_list<std::string> types);

	// sync texture path entries into _textures from the material's stored map (if _textures empty)
	void syncTexturesFromMaterialIfNeeded();
	GLuint createGLTextureFromFile(const QString& path, bool& outHasAlpha);

private:
	/*  Mesh Data  */
	// _vertices, _baseVertices → DeformableMesh (protected, Phase 12a)
	// _morphTargets, _defaultMorphWeights → DeformableMesh (protected, Phase 12a)
	// _indices → TriangleMesh (protected, Phase 4b) — AssImpMesh uses the inherited field directly
	// Reference alias into _materialState.textures() — same zero-churn pattern
	// as GLMaterial& _material in TriangleMesh. Initialised in the constructor
	// init-list; all existing _textures.xxx call sites remain unchanged.
	std::vector<GLMaterial::Texture>& _textures;

	// Reference alias into _animState.currentMorphWeights() (Phase 6).
	// All existing _currentMorphWeights call sites in AssImpMesh.cpp compile unchanged.
	QVector<float>& _currentMorphWeights;

	// State caching
	static QOpenGLShaderProgram* _currentUniformStateShader;
	static quint64 _currentUniformStateSignature;
	static bool _currentUniformStateHadDebugOverrides;
	static bool _currentBlendEnabled;
	static GLenum _currentFrontFace;
};
