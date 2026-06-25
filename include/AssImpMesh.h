#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <initializer_list>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "TriangleMesh.h"
#include "GLMaterial.h"


struct Vertex
{
	// Vertex Color 
	glm::vec4 Color;
	// Position
	glm::vec3 Position;
	// Normal
	glm::vec3 Normal;	
	// tangent
	glm::vec3 Tangent;
	// bitangent
	glm::vec3 Bitangent;
	// TexCoords
	glm::vec2 TexCoords[4];
	// Skinning
	glm::vec4 JointIndices = glm::vec4(0.0f);
	glm::vec4 JointWeights = glm::vec4(0.0f);
};

inline glm::vec2 getTexCoord(const Vertex& v, int index = 0)
{
	return (index >= 0 && index < 4) ? v.TexCoords[index] : glm::vec2(0.0f);
}

static_assert(sizeof(Vertex) == sizeof(float) * (4 + 3 + 3 + 3 + 3 + 8 + 4 + 4),
	"Vertex struct has unexpected padding - meshopt stride will be incorrect");

struct MorphTargetData
{
	std::vector<glm::vec3> positionDeltas;
	std::vector<glm::vec3> normalDeltas;
	std::vector<glm::vec3> tangentDeltas;
};

class AssImpMesh : public TriangleMesh
{
public:

	/*  Functions  */
	// Constructor
	AssImpMesh(QOpenGLShaderProgram* shader, QString name, std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<GLMaterial::Texture> textures, GLMaterial material, bool skipOptimization = false);
	~AssImpMesh();
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
	// edgeVerts is a flat {x0,y0,z0, x1,y1,z1, ...} list in model space.
	// When set, renderFeatureEdgesFast() uses this buffer instead of the
	// heuristic feature-edge classifier.
	void setPrecomputedOccEdges(const std::vector<float>& edgeVerts);
	const std::vector<float>& getOccEdgeSegments() const { return _occEdgeSegments; }
	bool hasMorphTargets() const override { return !_morphTargets.isEmpty(); }
	const QVector<MorphTargetData>& getMorphTargets() const { return _morphTargets; }
	QVector<float> defaultMorphWeights() const override { return _defaultMorphWeights; }
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
	std::vector<Vertex> _vertices;
	std::vector<Vertex> _baseVertices;
	std::vector<unsigned int> _indices;
	std::vector<GLMaterial::Texture> _textures;

	// Feature edge buffers — built by buildAndUploadFeatureEdges(), rendered by renderFeatureEdgesFast()
	QOpenGLBuffer               _featureEdgeIndexBuffer { QOpenGLBuffer::IndexBuffer };
	QOpenGLVertexArrayObject    _featureEdgeVAO;
	GLsizei                     _featureEdgeCount = 0;

	// OCC B-Rep edge buffers — set by setPrecomputedOccEdges(), take priority over
	// the heuristic feature edges in renderFeatureEdgesFast().
	QOpenGLBuffer               _occEdgeVertexBuffer { QOpenGLBuffer::VertexBuffer };
	QOpenGLVertexArrayObject    _occEdgeVAO;
	GLsizei                     _occEdgeCount = 0;
	// CPU copy of the edge segment data retained for clone() and MVF serialization.
	std::vector<float>          _occEdgeSegments;
	QVector<MorphTargetData> _morphTargets;
	QVector<float> _defaultMorphWeights;
	QVector<float> _currentMorphWeights;

	struct PrecomputedTexture
	{
		GLuint textureId;
		GLuint textureUnit;
		int uniformLocation; // Cache uniform location
		bool isValid;
	};
	std::vector<PrecomputedTexture> _textureBindings;
	// State caching
	static QOpenGLShaderProgram* _currentUniformStateShader;
	static quint64 _currentUniformStateSignature;
	static bool _currentUniformStateHadDebugOverrides;
	static bool _currentBlendEnabled;
	static GLenum _currentFrontFace;
	mutable bool _uniformStateSignatureDirty = true;
	mutable quint64 _cachedUniformStateSignature = 0;

	unsigned int _diffuseADSMap = 0;
	unsigned int _specularADSMap = 0;
	unsigned int _emissiveADSMap = 0;
	unsigned int _normalADSMap = 0;
	unsigned int _heightADSMap = 0;
	unsigned int _opacityADSMap = 0;
	bool _skipOptimization = false;
};
