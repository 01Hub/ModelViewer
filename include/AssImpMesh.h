#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "TriangleMesh.h"
#include "GLMaterial.h"


struct Vertex
{
	// Position
	glm::vec3 Position;
	// Normal
	glm::vec3 Normal;
	// TexCoords
	glm::vec2 TexCoords;
	// tangent
	glm::vec3 Tangent;
	// bitangent
	glm::vec3 Bitangent;
	// Vertex Color 
	glm::vec4 Color;     
};

struct Texture
{
	unsigned int id;
	std::string type;
	aiString path;
	bool hasAlpha = false; // Indicates if the texture has an alpha channel

	// print for debugging
	friend std::ostream& operator<<(std::ostream& os, const Texture& texture)
	{
		os << "Id: " << texture.id << " type: " << texture.type << " path: " << texture.path.C_Str();
		return os;
	}

};

class AssImpMesh : public TriangleMesh
{
public:

	/*  Functions  */
	// Constructor
	AssImpMesh(QOpenGLShaderProgram* shader, QString name, std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<Texture> textures, GLMaterial material);
	~AssImpMesh();
	virtual TriangleMesh* clone();
	void render();

    std::vector<Vertex> vertices() const;

    std::vector<unsigned int> indices() const;

    std::vector<Texture> textures() const;

	void serialize(QDataStream& out) const;
	void deserialize(QDataStream& in);

	void setAlbedoPBRMap(unsigned int albedoMap) override;
	void setMetallicPBRMap(unsigned int metallicMap) override;
	void setEmissivePBRMap(unsigned int emissiveMap) override;
	void setRoughnessPBRMap(unsigned int roughnessMap) override;
	void setNormalPBRMap(unsigned int normalMap) override;
	void setAOPBRMap(unsigned int aoMap) override;
	void setHeightPBRMap(unsigned int heightMap) override;
	void setOpacityPBRMap(unsigned int opacityMap) override;
	void setIORPBRMap(unsigned int iorMap) override;
	void setClearcoatPBRMap(unsigned int clearcoatMap) override;
	void setClearcoatRoughnessPBRMap(unsigned int clearcoatRoughnessMap) override;
	void setClearcoatNormalPBRMap(unsigned int clearcoatNormalMap) override;
	void setSheenColorPBRMap(unsigned int sheenMap) override;
	void setSheenRoughnessPBRMap(unsigned int sheenRoughnessMap) override;
	void setTransmissionPBRMap(unsigned int transmissionMap) override;	

	// implementations for enabling/disabling textures
	void setDiffuseADSMap(unsigned int diffuseTex) override;
	void setSpecularADSMap(unsigned int specularTex) override;
	void setEmissiveADSMap(unsigned int emissiveTex) override;
	void setNormalADSMap(unsigned int normalTex) override;
	void setHeightADSMap(unsigned int heightTex) override;
	void setOpacityADSMap(unsigned int opacityTex) override;

	virtual void setTextureMaps(const GLMaterial& material) override;
	void replaceOrAppendTexture(const std::string& type, GLuint id, bool hasAlpha);


	void releaseCurrentShader();

	
private:
	/*  Functions    */
	// Initializes all the buffer objects/arrays
	void setupMesh();

	void cacheTextureBindings();
	void bindTexturesOptimized();
	void setRenderStateOptimized();
	void setupUniformsOptimized();

	// sync texture path entries into _textures from the material's stored map (if _textures empty)
	void syncTexturesFromMaterialIfNeeded();
	GLuint createGLTextureFromFile(const QString& path, bool& outHasAlpha);

private:
	/*  Mesh Data  */
	std::vector<Vertex> _vertices;
	std::vector<unsigned int> _indices;
	std::vector<Texture> _textures;

	struct PrecomputedTexture
	{
		GLuint textureId;
		GLuint textureUnit;
		int uniformLocation; // Cache uniform location
		bool isValid;
	};
	std::vector<PrecomputedTexture> _textureBindings;
	// State caching
	QOpenGLShaderProgram* _currentBoundShader;
	static bool _currentBlendEnabled;
	static GLenum _currentFrontFace;	
};
