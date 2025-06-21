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
	void setRoughnessPBRMap(unsigned int roughnessMap) override;
	void setNormalPBRMap(unsigned int normalMap) override;
	void setAOPBRMap(unsigned int aoMap) override;
	void setHeightPBRMap(unsigned int heightMap) override;
	void setOpacityPBRMap(unsigned int opacityMap) override;
	
private:
	/*  Functions    */
	// Initializes all the buffer objects/arrays
	void setupMesh();

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
	
	void precomputeTextureBindings();
	void bindTexturesOptimized();
};
