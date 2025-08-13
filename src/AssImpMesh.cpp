#include "AssImpMesh.h"

using namespace std;

bool AssImpMesh::_currentBlendEnabled;
GLenum AssImpMesh::_currentFrontFace;

/*  Functions  */
// Constructor
AssImpMesh::AssImpMesh(QOpenGLShaderProgram* shader, QString name, vector<Vertex> vertices, vector<unsigned int> indices, vector<Texture> textures, GLMaterial material) : TriangleMesh(shader, "AssImpMesh")
{
	_currentBoundShader = nullptr;
	_currentBlendEnabled = false;
	_currentFrontFace = GL_CCW;
	//setAutoIncrName(name);
	_name = name;
	_vertices = vertices;
	_indices = indices;
	_textures = textures;
	_material = material;

	// Now that we have all the required data, set the vertex buffers and its attribute pointers.
	setupMesh();

}

AssImpMesh::~AssImpMesh()
{
	if (_textures.size())
	{
		for (const Texture &t : _textures)
		{
			glDeleteTextures(1, &t.id);
		}
	}
	releaseCurrentShader();
}

TriangleMesh* AssImpMesh::clone()
{
	return new AssImpMesh(_prog, _name, _vertices, _indices, _textures, _material);
}


void AssImpMesh::render()
{
	if (!_vertexArrayObject.isCreated())
		return;

	// Smart shader binding - only bind if different
	bool shaderChanged = false;
	if (_currentBoundShader != _prog)
	{
		if (_currentBoundShader)
		{
			_currentBoundShader->release();
		}
		_prog->bind();
		_currentBoundShader = _prog;
		shaderChanged = true;

		// When shader changes, we need to recache texture bindings
		_textureBindingsDirty = true;
		_uniformsDirty = true;
	}

	cacheTextureBindings();

	if (/*shaderChanged ||*/ _uniformsDirty)
	{
		setupUniformsOptimized();
	}

	// Bind textures efficiently
	bindTexturesOptimized();

	// Set render state efficiently
	setRenderStateOptimized();
	
	_vertexArrayObject.bind();
	glDrawElements(GL_TRIANGLES, _nVerts, GL_UNSIGNED_INT, 0);
	_vertexArrayObject.release();
	_prog->release();
	glDisable(GL_BLEND);
	
}

/*  Functions    */
// Initializes all the buffer objects/arrays
void AssImpMesh::setupMesh()
{
	std::vector<float> points;
	std::vector<float> normals;
	std::vector<float> texCoords;
	std::vector<float> tangents;
	std::vector<float> bitangents;

	for (const Vertex& v : _vertices)
	{
		points.push_back(v.Position.x);
		points.push_back(v.Position.y);
		points.push_back(v.Position.z);

		normals.push_back(v.Normal.x);
		normals.push_back(v.Normal.y);
		normals.push_back(v.Normal.z);

		texCoords.push_back(v.TexCoords.x);
		texCoords.push_back(v.TexCoords.y);

		tangents.push_back(v.Tangent.x);
		tangents.push_back(v.Tangent.y);
		tangents.push_back(v.Tangent.z);
		bitangents.push_back(v.Bitangent.x);
		bitangents.push_back(v.Bitangent.y);
		bitangents.push_back(v.Bitangent.z);
	}

	_hasTexture = false;

	for (unsigned int i = 0; i < _textures.size(); i++)
	{
		string name = _textures[i].type;

		if (name == "texture_diffuse")
		{
			_hasDiffuseADSMap = true;
			_hasAlbedoPBRMap = true;
		}
		if (name == "texture_specular")
		{
			_hasSpecularADSMap = true;
			_hasMetallicPBRMap = true;
		}
		if (name == "texture_emissive")
		{
			_hasEmissiveADSMap = true;
			_hasEmissivePBRMap = true;
		}
		if (name == "texture_normal")
		{
			_hasNormalADSMap = true;
			_hasNormalPBRMap = true;
		}
		if (name == "texture_height")
		{
			_hasHeightADSMap = true;
			_hasHeightPBRMap = true;
		}
		if (name == "texture_opacity")
		{
			_hasOpacityADSMap = true;
			_hasOpacityPBRMap = true;
		}	

		// PBR from model
		if (name == "albedoMap")
		{			
			_hasAlbedoPBRMap = true;
		}
		if (name == "metallicMap")
		{
			_hasSpecularADSMap = true;
			_hasMetallicPBRMap = true;
		}
		if (name == "emissiveMap")
		{
			_hasEmissiveADSMap = true;
			_hasEmissivePBRMap = true;
		}
		if (name == "roughnessMap")
		{
			_hasRoughnessPBRMap = true;
		}
		if (name == "normalMap")
		{
			_hasNormalPBRMap = true;
		}
		if (name == "aoMap")
		{
			_hasAOPBRMap = true;
		}
		if( name == "heightMap")
		{
			_hasHeightPBRMap = true;
		}
		if (name == "opacityMap")
		{
			_hasOpacityPBRMap = true;
			//_opacityPBRMapInverted = _textures[i].invertOpacity;
		}
		if (name == "transmissionMap")
		{
			_hasTransmissionPBRMap = true;
		}
		if (name == "iorMap")
		{
			_hasIORPBRMap = true;
		}
		if (name == "sheenColorMap")
		{
			_hasSheenColorPBRMap = true;
		}
		if (name == "sheenRoughnessMap")
		{
			_hasSheenRoughnessPBRMap = true;
		}
		if (name == "clearcoatMap")
		{
			_hasClearcoatPBRMap = true;
		}
		if (name == "clearcoatRoughnessMap")
		{
			_hasClearcoatRoughnessPBRMap = true;
		}
		if (name == "clearcoatNormalMap")
		{
			_hasClearcoatNormalPBRMap = true;		
		}
	}

	initBuffers(&_indices, &points, &normals, &texCoords, &tangents, &bitangents);
	computeBounds();
}

void AssImpMesh::cacheTextureBindings()
{
	if (!_textureBindingsDirty) return;

	_textureBindings.clear();
	_textureBindings.reserve(_textures.size() * 2); // Account for duplicates

	// Counters for numbering
	int diffuseNr = 1, specularNr = 1, emissiveNr = 1, normalNr = 1;
	int heightNr = 1, opacityNr = 1, albedoNr = 1, metallicNr = 1;
	int roughnessNr = 1, normalPBRNr = 1, aoNr = 1;
	int transmissionNr = 1, iorNr = 1;
	int sheenColorNr = 1, sheenRoughnessNr = 1;
	int clearcoatNr = 1, clearcoatRoughnessNr = 1, clearcoatNormalNr = 1;

	for (size_t i = 0; i < _textures.size(); ++i)
	{
		const auto& texture = _textures[i];

		// Helper lambda to add binding
		auto addBinding = [&](const std::string& uniformName, GLuint unit) {
			PrecomputedTexture binding;
			binding.textureId = texture.id;
			binding.textureUnit = unit;
			binding.uniformLocation = _prog->uniformLocation(uniformName.c_str());
			binding.isValid = (binding.uniformLocation != -1);
			if (binding.isValid)
			{
				_textureBindings.push_back(binding);
			}
			};

		// Handle different texture types
		if (texture.type == "texture_diffuse")
		{
			addBinding("texture_diffuse" /*+ std::to_string(diffuseNr)*/, GL_TEXTURE10);
			addBinding("albedoMap" /*+ std::to_string(diffuseNr)*/, GL_TEXTURE10); // PBR duplicate
			diffuseNr++;
		}
		else if (texture.type == "texture_specular")
		{
			addBinding("texture_specular" /*+ std::to_string(specularNr)*/, GL_TEXTURE11);
			addBinding("metallicMap" /*+ std::to_string(specularNr)*/, GL_TEXTURE11);
			specularNr++;
		}
		else if (texture.type == "texture_emissive")
		{
			addBinding("texture_emissive" /*+ std::to_string(emissiveNr)*/, GL_TEXTURE12);
			addBinding("emissiveMap" /*+ std::to_string(emissiveNr)*/, GL_TEXTURE12);
			emissiveNr++;
		}
		else if (texture.type == "texture_normal")
		{
			addBinding("texture_normal" /*+ std::to_string(normalNr)*/, GL_TEXTURE13);
			addBinding("normalMap" /*+ std::to_string(normalNr)*/, GL_TEXTURE13);
			normalNr++;
		}
		else if (texture.type == "texture_height")
		{
			addBinding("texture_height" /*+ std::to_string(heightNr)*/, GL_TEXTURE14);
			addBinding("heightMap" /*+ std::to_string(heightNr)*/, GL_TEXTURE14);
			heightNr++;
		}
		else if (texture.type == "texture_opacity")
		{
			addBinding("texture_opacity" /*+ std::to_string(opacityNr)*/, GL_TEXTURE15);
			addBinding("opacityMap" /*+ std::to_string(opacityNr)*/, GL_TEXTURE15);
			opacityNr++;
		}
		
		else if (texture.type == "albedoMap")
		{
			addBinding("texture_diffuse" /*+ std::to_string(diffuseNr)*/, GL_TEXTURE10);
			addBinding("albedoMap" /*+ std::to_string(albedoNr)*/, GL_TEXTURE10);
			albedoNr++;
		}
		else if (texture.type == "metallicMap")
		{
			addBinding("texture_specular" /*+ std::to_string(specularNr)*/, GL_TEXTURE11);
			addBinding("metallicMap" /*+ std::to_string(metallicNr)*/, GL_TEXTURE11);
			metallicNr++;
		}		
		else if (texture.type == "emissiveMap")
		{
			addBinding("texture_emissive" /*+ std::to_string(emissiveNr)*/, GL_TEXTURE12);
			addBinding("emissiveMap" /*+ std::to_string(emissiveNr)*/, GL_TEXTURE12);
			emissiveNr++;
		}
		else if (texture.type == "normalMap")
		{
			addBinding("normalMap" /*+ std::to_string(normalNr)*/, GL_TEXTURE13);
			normalNr++;
		}
		else if (texture.type == "roughnessMap")
		{
			addBinding("roughnessMap" /*+ std::to_string(roughnessNr)*/, GL_TEXTURE16);
			roughnessNr++;
		}
		else if (texture.type == "aoMap")
		{
			addBinding("aoMap" /*+ std::to_string(aoNr)*/, GL_TEXTURE17);
			aoNr++;
		}
		else if (texture.type == "transmissionMap")
		{
			addBinding("transmissionMap" /*+ std::to_string(i)*/, GL_TEXTURE18);
			transmissionNr++;
		}
		else if (texture.type == "iorMap")
		{
			addBinding("iorMap" /*+ std::to_string(i)*/, GL_TEXTURE19);
			iorNr++;
		}
		else if (texture.type == "sheenColorMap")
		{
			addBinding("sheenColorMap" /*+ std::to_string(i)*/, GL_TEXTURE20);
			sheenColorNr++;
		}
		else if (texture.type == "sheenRoughnessMap")
		{
			addBinding("sheenRoughnessMap" /*+ std::to_string(i)*/, GL_TEXTURE21);
			sheenRoughnessNr++;
		}
		else if (texture.type == "clearcoatMap")
		{
			addBinding("clearcoatMap" /*+ std::to_string(i)*/, GL_TEXTURE22);
			clearcoatNr++;
			std::cout << "Clearcoat map binding: " << i << std::endl;
		}
		else if (texture.type == "clearcoatRoughnessMap")
		{
			addBinding("clearcoatRoughnessMap" /*+ std::to_string(i)*/, GL_TEXTURE23);
			clearcoatRoughnessNr++;
			std::cout << "Clearcoat roughness map binding: " << i << std::endl;
		}
		else if (texture.type == "clearcoatNormalMap")
		{
			addBinding("clearcoatNormalMap" /*+ std::to_string(i)*/, GL_TEXTURE24);
			clearcoatNormalNr++;
			std::cout << "Clearcoat normal map binding: " << i << std::endl;
		}
	}

	_textureBindingsDirty = false;
}

void AssImpMesh::bindTexturesOptimized()
{
	for (const auto& binding : _textureBindings)
	{
		if (binding.isValid)
		{
			glActiveTexture(binding.textureUnit);
			glBindTexture(GL_TEXTURE_2D, binding.textureId);
			glUniform1i(binding.uniformLocation, binding.textureUnit - GL_TEXTURE0);
		}
	}
}

void AssImpMesh::setRenderStateOptimized()
{
	// Handle blending efficiently
	// Account for opacity if transmission is enabled
	bool needsBlending = (
		_material.opacity() < 1.0f ||
		_hasOpacityADSMap || _hasOpacityPBRMap
		);
	if (needsBlending != _currentBlendEnabled)
	{
		if (needsBlending)
		{
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glEnable(GL_LINE_SMOOTH);
			glEnable(GL_POLYGON_SMOOTH);
		}
		else
		{
			glDisable(GL_BLEND);
			glDisable(GL_LINE_SMOOTH);
			glDisable(GL_POLYGON_SMOOTH);
		}
		_currentBlendEnabled = needsBlending;
	}

	// Handle face culling efficiently
	GLenum frontFace = GL_CCW; // Default

	// Calculate front face based on negative scaling
	int negativeScales = (_scaleX < 0 ? 1 : 0) + (_scaleY < 0 ? 1 : 0) + (_scaleZ < 0 ? 1 : 0);
	if (negativeScales == 1 || negativeScales == 3)
	{
		frontFace = GL_CW;
	}

	if (frontFace != _currentFrontFace)
	{
		glFrontFace(frontFace);
		_currentFrontFace = frontFace;
	}
}

void AssImpMesh::setupUniformsOptimized()
{
	if (!_uniformsDirty) return;

	// Only call the expensive setupUniforms when actually needed
	setupUniforms();
	_uniformsDirty = false;
}


vector<Vertex> AssImpMesh::vertices() const
{
    return _vertices;
}

vector<unsigned int> AssImpMesh::indices() const
{
    return _indices;
}

vector<Texture> AssImpMesh::textures() const
{
    return _textures;
}

#include <QDataStream>

// --- Serialization ---
void AssImpMesh::serialize(QDataStream& out) const
{
	// Write mesh name
	out << _name;

	// Write vertices
	out << static_cast<quint32>(_vertices.size());
	for (const Vertex& v : _vertices) {
		out << v.Position.x << v.Position.y << v.Position.z;
		out << v.Normal.x << v.Normal.y << v.Normal.z;
		out << v.TexCoords.x << v.TexCoords.y;
		out << v.Tangent.x << v.Tangent.y << v.Tangent.z;
		out << v.Bitangent.x << v.Bitangent.y << v.Bitangent.z;
	}

	// Write indices
	out << static_cast<quint32>(_indices.size());
	for (unsigned int idx : _indices) {
		out << static_cast<quint32>(idx);
	}

	// Write textures
	out << static_cast<quint32>(_textures.size());
	for (const Texture& t : _textures) {
		out << t.type.c_str();
		out << t.path.C_Str();
		// Note: t.id is an OpenGL handle, not portable, so don't serialize it
	}

	// Write material (assuming GLMaterial is serializable, otherwise write its fields)
	_material.serialize(out);

	// Write the transform matrix
	out << _transformation;
}

// --- Deserialization ---
void AssImpMesh::deserialize(QDataStream& in)
{
	// Read mesh name
	in >> _name;

	// Read vertices
	quint32 vCount;
	in >> vCount;
	_vertices.clear();
	_vertices.reserve(vCount);
	for (quint32 i = 0; i < vCount; ++i) {
		Vertex v;
		in >> v.Position.x >> v.Position.y >> v.Position.z;
		in >> v.Normal.x >> v.Normal.y >> v.Normal.z;
		in >> v.TexCoords.x >> v.TexCoords.y;
		in >> v.Tangent.x >> v.Tangent.y >> v.Tangent.z;
		in >> v.Bitangent.x >> v.Bitangent.y >> v.Bitangent.z;
		_vertices.push_back(v);
	}

	// Read indices
	quint32 iCount;
	in >> iCount;
	_indices.clear();
	_indices.reserve(iCount);
	for (quint32 i = 0; i < iCount; ++i) {
		quint32 idx;
		in >> idx;
		_indices.push_back(idx);
	}

	// Read textures
	quint32 tCount;
	in >> tCount;
	_textures.clear();
	_textures.reserve(tCount);
	for (quint32 i = 0; i < tCount; ++i) {
		Texture t;
		QString type, path;
		in >> type >> path;
		t.type = type.toStdString();
		t.path = path.toStdString();
		t.id = 0; // Will be reloaded as needed
		_textures.push_back(t);
	}

	// Read material
	_material.deserialize(in);

	// Read the transformation matrix
	in >> _transformation;

	// Re-setup OpenGL buffers
	setupMesh();

	// Set the transformation matrix
	setupTransformation();
}

void AssImpMesh::setAlbedoPBRMap(unsigned int albedoMap)
{
	glDeleteTextures(1, &_albedoPBRMap);
	_albedoPBRMap = albedoMap;
	Texture t;
	t.id = albedoMap;
	t.type = "albedoMap";
	t.path = ""; // No path for OpenGL texture handles
	_textures.push_back(t);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setMetallicPBRMap(unsigned int metallicMap)
{
	glDeleteTextures(1, &_metallicPBRMap);
	_metallicPBRMap = metallicMap;
	Texture t;
	t.id = metallicMap;
	t.type = "metallicMap";
	t.path = ""; // No path for OpenGL texture handles
	_textures.push_back(t);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setEmissivePBRMap(unsigned int emissiveMap)
{
	glDeleteTextures(1, &_emissivePBRMap);
	_emissivePBRMap = emissiveMap;
	Texture t;
	t.id = emissiveMap;
	t.type = "emissiveMap";
	t.path = ""; // No path for OpenGL texture handles
	_textures.push_back(t);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setRoughnessPBRMap(unsigned int roughnessMap)
{
	glDeleteTextures(1, &_roughnessPBRMap);
	_roughnessPBRMap = roughnessMap;
	Texture t;
	t.id = roughnessMap;
	t.type = "roughnessMap";
	t.path = ""; // No path for OpenGL texture handles
	_textures.push_back(t);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setNormalPBRMap(unsigned int normalMap)
{
	glDeleteTextures(1, &_normalPBRMap);
	_normalPBRMap = normalMap;
	Texture t;
	t.id = normalMap;
	t.type = "normalMap";
	t.path = ""; // No path for OpenGL texture handles
	_textures.push_back(t);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setAOPBRMap(unsigned int aoMap)
{
	glDeleteTextures(1, &_aoPBRMap);
	_aoPBRMap = aoMap;
	Texture t;
	t.id = aoMap;
	t.type = "aoMap";
	t.path = ""; // No path for OpenGL texture handles
	_textures.push_back(t);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setHeightPBRMap(unsigned int heightMap)
{
	glDeleteTextures(1, &_heightPBRMap);
	_heightPBRMap = heightMap;
	Texture t;
	t.id = heightMap;
	t.type = "heightMap";
	t.path = ""; // No path for OpenGL texture handles
	_textures.push_back(t);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setOpacityPBRMap(unsigned int opacityMap)
{
	glDeleteTextures(1, &_opacityPBRMap);
	_opacityPBRMap = opacityMap;
	Texture t;
	t.id = opacityMap;
	t.type = "opacityMap";
	t.path = ""; // No path for OpenGL texture handles
	_textures.push_back(t);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setIORPBRMap(unsigned int iorMap)
{
	glDeleteTextures(1, &_IORPBRMap);
	_IORPBRMap = iorMap;
	Texture t;
	t.id = iorMap;
	t.type = "iorMap";
	t.path = ""; // No path for OpenGL texture handles
	_textures.push_back(t);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setClearcoatPBRMap(unsigned int clearcoatMap)
{
	glDeleteTextures(1, &_clearcoatPBRMap);
	_clearcoatPBRMap = clearcoatMap;
	Texture t;
	t.id = clearcoatMap;
	t.type = "clearcoatMap";
	t.path = ""; // No path for OpenGL texture handles
	_textures.push_back(t);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setClearcoatRoughnessPBRMap(unsigned int clearcoatRoughnessMap)
{
	glDeleteTextures(1, &_clearcoatRoughnessPBRMap);
	_clearcoatRoughnessPBRMap = clearcoatRoughnessMap;
	Texture t;
	t.id = clearcoatRoughnessMap;
	t.type = "clearcoatRoughnessMap";
	t.path = ""; // No path for OpenGL texture handles
	_textures.push_back(t);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setClearcoatNormalPBRMap(unsigned int clearcoatNormalMap)
{
	glDeleteTextures(1, &_clearcoatNormalPBRMap);
	_clearcoatNormalPBRMap = clearcoatNormalMap;
	Texture t;
	t.id = clearcoatNormalMap;
	t.type = "clearcoatNormalMap";
	t.path = ""; // No path for OpenGL texture handles
	_textures.push_back(t);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setSheenColorPBRMap(unsigned int sheenMap)
{
	glDeleteTextures(1, &_sheenColorPBRMap);
	_sheenColorPBRMap = sheenMap;
	Texture t;
	t.id = sheenMap;
	t.type = "sheenColorMap";
	t.path = ""; // No path for OpenGL texture handles
	_textures.push_back(t);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setSheenRoughnessPBRMap(unsigned int sheenRoughnessMap)
{
	glDeleteTextures(1, &_sheenRoughnessPBRMap);
	_sheenRoughnessPBRMap = sheenRoughnessMap;
	Texture t;
	t.id = sheenRoughnessMap;
	t.type = "sheenRoughnessMap";
	t.path = ""; // No path for OpenGL texture handles
	_textures.push_back(t);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setTransmissionPBRMap(unsigned int transmissionMap)
{
	glDeleteTextures(1, &_transmissionPBRMap);
	_transmissionPBRMap = transmissionMap;
	Texture t;
	t.id = transmissionMap;
	t.type = "transmissionMap";
	t.path = ""; // No path for OpenGL texture handles
	_textures.push_back(t);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setDiffuseADSMap(unsigned int diffuseTex)
{
	glDeleteTextures(1, &_diffuseADSMap);
	_diffuseADSMap = diffuseTex;
	Texture t;
	t.id = diffuseTex;
	t.type = "texture_diffuse";
	t.path = ""; // No path for OpenGL texture handles
	_textures.push_back(t);
	_hasDiffuseADSMap = true;
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setSpecularADSMap(unsigned int specularTex)
{
	glDeleteTextures(1, &_specularADSMap);
	_specularADSMap = specularTex;
	Texture t;
	t.id = specularTex;
	t.type = "texture_specular";
	t.path = ""; // No path for OpenGL texture handles
	_textures.push_back(t);
	_hasSpecularADSMap = true;
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setEmissiveADSMap(unsigned int emissiveTex)
{
	glDeleteTextures(1, &_emissiveADSMap);
	_emissiveADSMap = emissiveTex;
	Texture t;
	t.id = emissiveTex;
	t.type = "texture_emissive";
	t.path = ""; // No path for OpenGL texture handles
	_textures.push_back(t);
	_hasEmissiveADSMap = true;
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setNormalADSMap(unsigned int normalTex)
{
	glDeleteTextures(1, &_normalADSMap);
	_normalADSMap = normalTex;
	Texture t;
	t.id = normalTex;
	t.type = "texture_normal";
	t.path = ""; // No path for OpenGL texture handles
	_textures.push_back(t);
	_hasNormalADSMap = true;
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setHeightADSMap(unsigned int heightTex)
{
	glDeleteTextures(1, &_heightADSMap);
	_heightADSMap = heightTex;
	Texture t;
	t.id = heightTex;
	t.type = "texture_height";
	t.path = ""; // No path for OpenGL texture handles
	_textures.push_back(t);
	_hasHeightADSMap = true;
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setOpacityADSMap(unsigned int opacityTex)
{
	glDeleteTextures(1, &_opacityADSMap);
	_opacityADSMap = opacityTex;
	Texture t;
	t.id = opacityTex;
	t.type = "texture_opacity";
	t.path = ""; // No path for OpenGL texture handles
	_textures.push_back(t);
	_hasOpacityADSMap = true;
	markTexturesDirty();
	markUniformsDirty();
}

// Cleanup method
void AssImpMesh::releaseCurrentShader()
{
	if (_currentBoundShader)
	{
		QOpenGLContext* ctx = QOpenGLContext::currentContext();
		if (ctx && ctx->isValid() && ctx->thread() == QThread::currentThread())
		{
			_currentBoundShader->release();			
		}
	}
	_currentBoundShader = nullptr;
}

