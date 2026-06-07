#include "AssImpMesh.h"
#include "TextureLocationManager.h"

#include <QFileInfo>
#include <QImage>
#include <QVariantMap>
#include <QDebug>

#include <algorithm>
#include <cmath>
#include <meshoptimizer.h>
#include <utility>

using namespace std;

bool AssImpMesh::_currentBlendEnabled;
GLenum AssImpMesh::_currentFrontFace;

/*  Functions  */
// Constructor
AssImpMesh::AssImpMesh(QOpenGLShaderProgram* shader, QString name, vector<Vertex> vertices, vector<unsigned int> indices, vector<GLMaterial::Texture> textures, GLMaterial material, bool skipOptimization) : TriangleMesh(shader, "AssImpMesh")
{
	_currentBoundShader = nullptr;
	_currentBlendEnabled = false;
	_currentFrontFace = GL_CCW;
	_skipOptimization = skipOptimization;
	//setAutoIncrName(name);
	_name = name;
	_vertices = vertices;
	_baseVertices = vertices;
	_indices = indices;
	_textures = textures;
	_material = material;
	cacheBaseVolumeProperties();

	// Optimize the mesh (reorder indices and vertices for better vertex cache locality, overdraw, and vertex fetch)
	optimizeMesh();

	// Now that we have all the required data, set the vertex buffers and its attribute pointers.
	setupMesh();
}

AssImpMesh::~AssImpMesh()
{
	/*if (_textures.size())
	{
		for (const GLMaterial::Texture &t : _textures)
		{
			glDeleteTextures(1, &t.id);
		}
	}*/
	releaseCurrentShader();
}

TriangleMesh* AssImpMesh::clone()
{
	AssImpMesh* mesh = new AssImpMesh(_prog, _name, _baseVertices, _indices, _textures, _material, _skipOptimization);
	mesh->setMorphTargets(_morphTargets, _defaultMorphWeights);
	if (!_currentMorphWeights.isEmpty())
		mesh->applyMorphWeights(_currentMorphWeights);
	return mesh;
}


void AssImpMesh::render()
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

	// Always upload the per-mesh transform state. Skipping identity meshes lets
	// them inherit the previous draw's model matrix from shader state, which
	// causes unrelated meshes later in render order to appear transformed.
	if (_prog->uniformLocation("modelMatrix") >= 0)
		_prog->setUniformValue("modelMatrix", modelMatrix);
	if (_prog->uniformLocation("modelViewMatrix") >= 0)
		_prog->setUniformValue("modelViewMatrix", modelViewMatrix);
	if (_prog->uniformLocation("normalMatrix") >= 0)
		_prog->setUniformValue("normalMatrix", modelViewMatrix.normalMatrix());
	if (_prog->uniformLocation("hasSkinning") >= 0)
		_prog->setUniformValue("hasSkinning", hasSkinning());
	if (_prog->uniformLocation("jointCount") >= 0)
		_prog->setUniformValue("jointCount", static_cast<int>(jointPalette().size()));
	if (hasSkinning() && !jointPalette().isEmpty())
	{
		const int maxJoints = std::min(static_cast<int>(jointPalette().size()), 128);
		for (int i = 0; i < maxJoints; ++i)
		{
			const QString uniformName = QStringLiteral("jointMatrices[%1]").arg(i);
			if (_prog->uniformLocation(uniformName.toUtf8().constData()) >= 0)
				_prog->setUniformValue(uniformName.toUtf8().constData(), jointPalette()[i]);
		}
	}

	if (/*shaderChanged ||*/ _uniformsDirty)
	{
		setupUniformsOptimized();
		_uniformsDirty = false;
	}

	// Apply debug uniform overrides (TextureDebugPanel extension toggles).
	// Called unconditionally — NOT inside the _uniformsDirty gate — so the
	// shader reflects the user's toggle state every frame even when the
	// uniform cache is clean.
	applyDebugUniformOverrides();

	// Bind textures efficiently
	bindTexturesOptimized();
	applyDebugTextureOverrides();  // TextureDebugPanel per-unit overrides

	// Set render state efficiently
	setRenderStateOptimized();
	
	// Transparent draws must NOT write depth, but should still depth-test.	
	GLboolean prevDepthMask = GL_TRUE;
	glGetBooleanv(GL_DEPTH_WRITEMASK, &prevDepthMask);
	if (isTransparent() && needsDepthMaskOff()) glDepthMask(GL_FALSE);

	_vertexArrayObject.bind();

	// Adjust vertex count based on primitive mode
	GLsizei drawCount = _nVerts;

	// For point rendering, use point size
	if (_primitiveMode == GL_POINTS)
	{
		glEnable(GL_PROGRAM_POINT_SIZE);
		glPointSize(3.0f);
	}

	// For line rendering, use line width
	if (_primitiveMode == GL_LINES || _primitiveMode == GL_LINE_STRIP || _primitiveMode == GL_LINE_LOOP)
	{
		glLineWidth(1.5f);
	}

	// Draw indexed primitives when an element buffer exists, otherwise fall
	// back to array drawing for glTF point/line primitives that omit indices.
	if (_indices.empty())
		glDrawArrays(_primitiveMode, 0, drawCount);
	else
		glDrawElements(_primitiveMode, drawCount, GL_UNSIGNED_INT, nullptr);
	
	// Reset point size
	if (_primitiveMode == GL_POINTS)
	{
		glDisable(GL_PROGRAM_POINT_SIZE);
	}

	_vertexArrayObject.release();

	if (isTransparent()) glDepthMask(prevDepthMask); // restore immediately

	_prog->release();
	
}

void AssImpMesh::optimizeMesh()
{
	// ============================================
	// MESH OPTIMIZATION (before splitting arrays)
	// ============================================
	if (_skipOptimization)
		return;

	// Check if this is a valid triangle mesh
	if (_indices.empty() || (_indices.size() % 3 != 0))
	{
		// Not a triangle mesh - skip meshoptimizer
		return;
	}
	if (_indices.size() > 300 && _vertices.size() > 100)
	{
		size_t vertexCount = _vertices.size();

		// Extract positions temporarily for overdraw optimization
		std::vector<float> tempPositions(vertexCount * 3);
		for (size_t i = 0; i < vertexCount; i++)
		{
			tempPositions[i * 3 + 0] = _vertices[i].Position.x;
			tempPositions[i * 3 + 1] = _vertices[i].Position.y;
			tempPositions[i * 3 + 2] = _vertices[i].Position.z;
		}

		// Step 1: Vertex Cache Optimization
		meshopt_optimizeVertexCache(
			_indices.data(),
			_indices.data(),
			_indices.size(),
			vertexCount
		);

		// Step 2: Overdraw Optimization
		meshopt_optimizeOverdraw(
			_indices.data(),
			_indices.data(),
			_indices.size(),
			tempPositions.data(),
			vertexCount,
			sizeof(float) * 3,
			1.05f
		);

		// Step 3: Vertex Fetch Optimization
		meshopt_optimizeVertexFetch(
			_vertices.data(),
			_indices.data(),
			_indices.size(),
			_vertices.data(),
			vertexCount,
			sizeof(Vertex)
		);
	}
}

/*  Functions    */
// Initializes all the buffer objects/arrays
void AssImpMesh::setupMesh()
{
	// ============================================
	// Extract to separate arrays
	// ============================================
	std::vector<float> points;
	std::vector<float> normals;
	std::vector<float> colors;
	std::vector<float> texCoords;
	std::vector<float> tangents;
	std::vector<float> bitangents;
	std::vector<float> jointIndices;
	std::vector<float> jointWeights;

	for (const Vertex& v : _vertices)
	{
		points.push_back(v.Position.x);
		points.push_back(v.Position.y);
		points.push_back(v.Position.z);

		normals.push_back(v.Normal.x);
		normals.push_back(v.Normal.y);
		normals.push_back(v.Normal.z);
				
		colors.reserve(_vertices.size() * 4);
		colors.push_back(v.Color.r);
		colors.push_back(v.Color.g);
		colors.push_back(v.Color.b);
		colors.push_back(v.Color.a);
		
		// Extract all 4 texCoord sets
		for (int i = 0; i < 4; i++)
		{
			texCoords.push_back(v.TexCoords[i].x);
			texCoords.push_back(v.TexCoords[i].y);
		}

		tangents.push_back(v.Tangent.x);
		tangents.push_back(v.Tangent.y);
		tangents.push_back(v.Tangent.z);

		bitangents.push_back(v.Bitangent.x);
		bitangents.push_back(v.Bitangent.y);
		bitangents.push_back(v.Bitangent.z);

		jointIndices.push_back(v.JointIndices.x);
		jointIndices.push_back(v.JointIndices.y);
		jointIndices.push_back(v.JointIndices.z);
		jointIndices.push_back(v.JointIndices.w);

		jointWeights.push_back(v.JointWeights.x);
		jointWeights.push_back(v.JointWeights.y);
		jointWeights.push_back(v.JointWeights.z);
		jointWeights.push_back(v.JointWeights.w);
	}

	initBuffers(&_indices, &points, &normals, &colors, &texCoords, &tangents, &bitangents, &jointIndices, &jointWeights);
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
	// New glTF extension counters
	int specularFactorNr = 1, specularColorNr = 1;
	int anisotropyNr = 1;
	int iridescenceNr = 1, iridescenceThicknessNr = 1;
	int thicknessNr = 1;
	int diffuseSpecGlossNr = 1, specularGlossinessNr = 1;

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
			_hasTextureAlpha = texture.hasAlpha;
		}
		else if (texture.type == "albedoMap")
		{
			addBinding("texture_diffuse" /*+ std::to_string(diffuseNr)*/, GL_TEXTURE10);
			addBinding("albedoMap" /*+ std::to_string(albedoNr)*/, GL_TEXTURE10);
			albedoNr++;
			_hasTextureAlpha = texture.hasAlpha;
		}
		else if (texture.type == "texture_specular")
		{
			addBinding("texture_specular" /*+ std::to_string(specularNr)*/, GL_TEXTURE11);
			addBinding("metallicMap" /*+ std::to_string(specularNr)*/, GL_TEXTURE11);
			specularNr++;
		}
		else if (texture.type == "metallicMap")
		{
			addBinding("texture_specular" /*+ std::to_string(specularNr)*/, GL_TEXTURE11);
			addBinding("metallicMap" /*+ std::to_string(metallicNr)*/, GL_TEXTURE11);
			metallicNr++;
		}
		else if (texture.type == "texture_emissive")
		{
			addBinding("texture_emissive" /*+ std::to_string(emissiveNr)*/, GL_TEXTURE12);
			addBinding("emissiveMap" /*+ std::to_string(emissiveNr)*/, GL_TEXTURE12);
			emissiveNr++;
		}
		else if (texture.type == "emissiveMap")
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
		else if (texture.type == "normalMap")
		{
			addBinding("normalMap" /*+ std::to_string(normalNr)*/, GL_TEXTURE13);
			normalNr++;
		}
		else if (texture.type == "texture_height")
		{
			addBinding("texture_height" /*+ std::to_string(heightNr)*/, GL_TEXTURE14);
			addBinding("heightMap" /*+ std::to_string(heightNr)*/, GL_TEXTURE14);
			heightNr++;
		}
		else if (texture.type == "heightMap")
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
		else if (texture.type == "opacityMap")
		{
			addBinding("texture_opacity" /*+ std::to_string(opacityNr)*/, GL_TEXTURE15);
			addBinding("opacityMap" /*+ std::to_string(opacityNr)*/, GL_TEXTURE15);
			opacityNr++;
		}
		else if (texture.type == "roughnessMap")
		{
			addBinding("roughnessMap" /*+ std::to_string(roughnessNr)*/, GL_TEXTURE16);
			roughnessNr++;
		}
		else if (texture.type == "aoMap" || texture.type == "occlusionMap")
		{
			addBinding("aoMap" /*+ std::to_string(aoNr)*/, GL_TEXTURE17);
			aoNr++;
		}
		else if (texture.type == "transmissionMap")
		{
			addBinding("transmissionMap" /*+ std::to_string(transmissionNr)*/, GL_TEXTURE28);
			transmissionNr++;
		}
		else if (texture.type == "iorMap")
		{
			addBinding("iorMap" /*+ std::to_string(iorNr)*/, GL_TEXTURE29);
			iorNr++;
		}
		else if (texture.type == "sheenColorMap")
		{
			addBinding("sheenColorMap" /*+ std::to_string(sheenColorNr)*/, GL_TEXTURE26);
			sheenColorNr++;
		}
		else if (texture.type == "sheenRoughnessMap")
		{
			addBinding("sheenRoughnessMap" /*+ std::to_string(sheenRoughnessNr)*/, GL_TEXTURE27);
			sheenRoughnessNr++;
		}
		else if (texture.type == "clearcoatColorMap")
		{
			addBinding("clearcoatColorMap" /*+ std::to_string(clearcoatNr)*/, GL_TEXTURE18);
			clearcoatNr++;
		}
		else if (texture.type == "clearcoatRoughnessMap")
		{
			addBinding("clearcoatRoughnessMap" /*+ std::to_string(clearcoatRoughnessNr)*/, GL_TEXTURE19);
			clearcoatRoughnessNr++;
		}
		else if (texture.type == "clearcoatNormalMap")
		{
			addBinding("clearcoatNormalMap" /*+ std::to_string(clearcoatNormalNr)*/, GL_TEXTURE20);
			clearcoatNormalNr++;
		}
		// === NEW GLTF EXTENSION TEXTURES ===
		else if (texture.type == "specularFactorMap")
		{
			addBinding("specularFactorMap" /*+ std::to_string(specularFactorNr)*/, GL_TEXTURE21);
			specularFactorNr++;
		}
		else if (texture.type == "specularColorMap")
		{
			addBinding("specularColorMap" /*+ std::to_string(specularColorNr)*/, GL_TEXTURE22);
			specularColorNr++;
		}		
		else if (texture.type == "diffuseMap") // === KHR_materials_pbrSpecularGlossiness ===
		{
			addBinding("diffuseMap" /*+ std::to_string(diffuseSpecGlossNr)*/, GL_TEXTURE10);
			diffuseSpecGlossNr++;
			_hasTextureAlpha = texture.hasAlpha;
		}
		else if (texture.type == "specularGlossinessMap")
		{
			// GL_TEXTURE21 is reused (mutually exclusive with specularFactorMap)
			// The shader checks hasSpecularGlossinessMap to determine which to use
			addBinding("specularGlossinessMap" /*+ std::to_string(specularGlossinessNr)*/, GL_TEXTURE21);
			specularGlossinessNr++;
		}
		else if (texture.type == "anisotropyMap")
		{
			addBinding("anisotropyMap" /*+ std::to_string(anisotropyNr)*/, GL_TEXTURE23);
			anisotropyNr++;
		}
		else if (texture.type == "iridescenceMap")
		{
			addBinding("iridescenceMap" /*+ std::to_string(iridescenceNr)*/, GL_TEXTURE24);
			iridescenceNr++;
		}
		else if (texture.type == "iridescenceThicknessMap")
		{
			addBinding("iridescenceThicknessMap" /*+ std::to_string(iridescenceThicknessNr)*/, GL_TEXTURE25);
			iridescenceThicknessNr++;
		}
		else if (texture.type == "thicknessMap")
		{
			addBinding("thicknessMap" /*+ std::to_string(thicknessNr)*/, GL_TEXTURE30);
			thicknessNr++;
		}
		else if (texture.type == "diffuseTransmissionMap")
		{
			addBinding("diffuseTransmissionMap", GL_TEXTURE0 + 34);
		}
		else if (texture.type == "diffuseTransmissionColorMap")
		{
			addBinding("diffuseTransmissionColorMap", GL_TEXTURE0 + 35);
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
	const bool shouldBlend =
		_material.opacity() < 1.0f ||
		_material.hasOpacityMap() ||
		_material.hasTransmissionMap() ||
		_material.transmission() > 0.0f ||
		_material.blendMode() == GLMaterial::BlendMode::Alpha ||
		_material.alphaThreshold() > 0.0f ||
		_hasTextureAlpha;

	if (shouldBlend != _currentBlendEnabled)
	{
		if (shouldBlend)
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
		_currentBlendEnabled = shouldBlend;
	}

	// Front face correction
	GLenum frontFace = GL_CCW;
	const int neg = (_scaleX < 0) + (_scaleY < 0) + (_scaleZ < 0);
	if (neg == 1 || neg == 3) frontFace = GL_CW;
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


// Convert a QString to aiString helper
static aiString qstrToAiString(const QString& s)
{
	// aiString stores a copy internally
	return aiString(s.toUtf8().constData());
}

void AssImpMesh::syncTexturesFromMaterialIfNeeded()
{
	// If mesh already has explicit paths for textures, do nothing
	bool hasAnyPath = false;
	for (const GLMaterial::Texture& t : _textures)
	{
		if (!QString::fromUtf8(t.path).isEmpty()) { hasAnyPath = true; break; }
	}
	if (hasAnyPath) return;

	// Use material->toVariantMap() so we don't need private-member access.
	QVariantMap vm = _material.toVariantMap();

	// Helper lambda to add a texture entry if path exists and file seems plausible.
	auto pushIfPresent = [&](const QString& matKey, const std::string& outType) {
		QVariant v = vm.value(matKey);
		if (!v.isValid()) return;
		QString path = v.toString().trimmed();
		if (path.isEmpty()) return;

		// make path absolute attempt is caller's responsibility; we just store what material had.
		GLMaterial::Texture t;
		t.id = 0;
		t.type = outType;
		t.path = path.toStdString();

		// CRITICAL: Copy sampler values from material's texture array
		// Find the matching texture type in material and copy its sampler settings
		auto matTexType = GLMaterial::stringToTextureType(QString::fromStdString(outType));
		if (matTexType != GLMaterial::TextureType::Count)
		{
			const auto& matTex = _material.texture(matTexType);
			t.wrapS = matTex.wrapS;
			t.wrapT = matTex.wrapT;
			t.magFilter = matTex.magFilter;
			t.minFilter = matTex.minFilter;
			t.texCoordIndex = matTex.texCoordIndex;
			t.scale = matTex.scale;
			t.offset = matTex.offset;
			t.rotation = matTex.rotation;
		}

		// Optionally detect alpha channel (light-weight check)
		bool hasAlpha = false;
		GLuint id = createGLTextureFromFile(path, hasAlpha);
		t.id = id;
		t.hasAlpha = hasAlpha;

		_textures.push_back(t);
		};

	// Map GLMaterial variant keys -> mesh texture type strings used throughout AssImpMesh
	// (these type strings match the checks in setupMesh()/cacheTextureBindings)
	pushIfPresent("albedoMapPath", "albedoMap");        // PBR albedo
	pushIfPresent("normalMapPath", "normalMap");        // normal
	pushIfPresent("metallicMapPath", "metallicMap");      // metallic
	pushIfPresent("roughnessMapPath", "roughnessMap");     // roughness
	pushIfPresent("aoMapPath", "aoMap");            // ao / lightmap
	pushIfPresent("occlusionMapPath", "aoMap");            // ao / lightmap
	pushIfPresent("emissiveMapPath", "emissiveMap");      // emissive
	pushIfPresent("opacityMapPath", "opacityMap");       // opacity/alpha
	pushIfPresent("heightMapPath", "heightMap");        // height
	pushIfPresent("transmissionMapPath", "transmissionMap");  // transmission
	pushIfPresent("iorMapPath", "iorMap");
	pushIfPresent("sheenColorMapPath", "sheenColorMap");
	pushIfPresent("sheenRoughnessMapPath", "sheenRoughnessMap");
	pushIfPresent("clearcoatColorMapPath", "clearcoatColorMap");
	pushIfPresent("clearcoatRoughnessMapPath", "clearcoatRoughnessMap");
	pushIfPresent("clearcoatNormalMapPath", "clearcoatNormalMap");
	// New glTF extension textures
	pushIfPresent("specularFactorMapPath", "specularFactorMap");
	pushIfPresent("specularColorMapPath", "specularColorMap");
	pushIfPresent("anisotropyMapPath", "anisotropyMap");
	pushIfPresent("iridescenceMapPath", "iridescenceMap");
	pushIfPresent("iridescenceThicknessMapPath", "iridescenceThicknessMap");
	pushIfPresent("thicknessMapPath", "thicknessMap");
	pushIfPresent("diffuseTransmissionMapPath", "diffuseTransmissionMap");
	pushIfPresent("diffuseTransmissionColorMapPath", "diffuseTransmissionColorMap");


	// Also add common legacy ADS keys (in case materials were saved using legacy names)
	pushIfPresent("albedoMap", "albedoMap");
	pushIfPresent("diffuse", "texture_diffuse");
	pushIfPresent("specular", "texture_specular");
	pushIfPresent("emissive", "texture_emissive");
	pushIfPresent("normal", "texture_normal");
	pushIfPresent("opacity", "texture_opacity");

	// If we added anything, rebuild mesh flags and buffers
	if (!_textures.empty())
	{
		// Recompute _hasXXX flags and buffers
		setupMesh();

		// Ensure the next render refresh uses new textures/uniforms
		markTexturesDirty();
		markUniformsDirty();
	}
}


GLuint AssImpMesh::createGLTextureFromFile(const QString& fullPath, bool& outHasAlpha)
{
	outHasAlpha = false;
	if (fullPath.isEmpty()) return 0;
	if (!QFileInfo::exists(fullPath))
	{
			qWarning() << "createGLTextureFromFile: file not found:" << fullPath;
			return 0;
		}

	QImage img;
	if (!img.load(fullPath))
	{
		qWarning() << "createGLTextureFromFile: QImage failed to load:" << fullPath;
		return 0;
	}

	// Detect alpha before any conversion
	outHasAlpha = img.hasAlphaChannel();

	// Convert to a known format and flip vertically to match GL origin (bottom-left)
	QImage glimg = img.convertToFormat(QImage::Format_ARGB32);
	glimg = glimg.mirrored(false, true); // horizontal=false, vertical=true

	// Ensure GL context is present (caller must be on GL thread)
	if (!QOpenGLContext::currentContext())
	{
		qWarning() << "createGLTextureFromFile: no GL context; cannot create texture now for" << fullPath;
		return 0;
	}

	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);

	// Defensive: ensure unpack alignment won't cause row padding problems
	GLint prevAlign = 0;
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &prevAlign);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	// Upload: QImage::Format_ARGB32 corresponds to BGRA ordering on many platforms.
	glTexImage2D(GL_TEXTURE_2D,
		0,
		GL_RGBA,
		glimg.width(),
		glimg.height(),
		0,
		GL_BGRA,
		GL_UNSIGNED_BYTE,
		glimg.bits());

	// Restore previous alignment
	glPixelStorei(GL_UNPACK_ALIGNMENT, prevAlign);

	glGenerateMipmap(GL_TEXTURE_2D);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Optional: set wrap modes if needed (repeat/clamp)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBindTexture(GL_TEXTURE_2D, 0);
	return tex;
}


vector<Vertex> AssImpMesh::vertices() const
{
    return _vertices;
}

vector<unsigned int> AssImpMesh::indices() const
{
    return _indices;
}

vector<GLMaterial::Texture> AssImpMesh::textures() const
{
    return _textures;
}

void AssImpMesh::getMeshData(std::vector<Vertex>& vertices,
	std::vector<unsigned int>& indices) const
{
	vertices = _vertices;
	indices = _indices;
}

// Set new mesh data and upload to GPU (no optimization)
void AssImpMesh::setMeshData(const std::vector<Vertex>& vertices,
	const std::vector<unsigned int>& indices,
	const std::vector<unsigned int>* sourceVertexMap)
{
	QVector<MorphTargetData> remappedMorphTargets;
	if (!_morphTargets.isEmpty() &&
		sourceVertexMap &&
		sourceVertexMap->size() == vertices.size())
	{
		remappedMorphTargets = _morphTargets;
		for (MorphTargetData& morphTarget : remappedMorphTargets)
		{
			auto remapDeltas = [&](std::vector<glm::vec3>& deltas)
			{
				if (deltas.empty())
					return;

				std::vector<glm::vec3> remapped(vertices.size(), glm::vec3(0.0f));
				for (size_t i = 0; i < sourceVertexMap->size(); ++i)
				{
					const unsigned int sourceIndex = (*sourceVertexMap)[i];
					if (sourceIndex >= deltas.size())
					{
						deltas.clear();
						return;
					}

					remapped[i] = deltas[sourceIndex];
				}

				deltas = std::move(remapped);
			};

			remapDeltas(morphTarget.positionDeltas);
			remapDeltas(morphTarget.normalDeltas);
			remapDeltas(morphTarget.tangentDeltas);
		}
	}

	_vertices = vertices;
	_baseVertices = vertices;
	_indices = indices;
	if (!remappedMorphTargets.isEmpty())
		_morphTargets = std::move(remappedMorphTargets);

	// Re-upload to GPU (no optimization)
	setupMesh();

	// Setup transformation again (in case bounds changed)
	setupTransformation();

	if (!_morphTargets.isEmpty())
	{
		const QVector<float> weightsToApply = !_currentMorphWeights.isEmpty()
			? _currentMorphWeights
			: _defaultMorphWeights;
		_currentMorphWeights.clear();
		if (!weightsToApply.isEmpty())
			applyMorphWeights(weightsToApply);
	}
}

void AssImpMesh::setMorphTargets(const QVector<MorphTargetData>& targets,
	const QVector<float>& defaultWeights)
{
	_morphTargets = targets;
	_defaultMorphWeights = defaultWeights;
	_currentMorphWeights.clear();
	if (_baseVertices.empty())
		_baseVertices = _vertices;

	bool hasNonZeroDefault = false;
	for (float weight : std::as_const(_defaultMorphWeights))
	{
		if (std::abs(weight) > 0.0001f)
		{
			hasNonZeroDefault = true;
			break;
		}
	}

	if (hasNonZeroDefault)
		applyMorphWeights(_defaultMorphWeights);
	else
		_currentMorphWeights = _defaultMorphWeights;
}

void AssImpMesh::applyMorphWeights(const QVector<float>& weights)
{
	if (_morphTargets.isEmpty() || _baseVertices.empty())
		return;

	QVector<float> clampedWeights = weights;
	if (clampedWeights.size() < _morphTargets.size())
		clampedWeights.resize(_morphTargets.size());

	if (_currentMorphWeights == clampedWeights)
		return;

	_vertices = _baseVertices;
	for (size_t vertexIndex = 0; vertexIndex < _vertices.size(); ++vertexIndex)
	{
		glm::vec3 position = _baseVertices[vertexIndex].Position;
		glm::vec3 normal = _baseVertices[vertexIndex].Normal;
		glm::vec3 tangent = _baseVertices[vertexIndex].Tangent;
		bool normalChanged = false;
		bool tangentChanged = false;

		for (int targetIndex = 0; targetIndex < _morphTargets.size(); ++targetIndex)
		{
			const float weight = clampedWeights.value(targetIndex, 0.0f);
			if (std::abs(weight) <= 0.0001f)
				continue;

			const MorphTargetData& target = _morphTargets[targetIndex];
			if (target.positionDeltas.size() == _vertices.size())
				position += target.positionDeltas[vertexIndex] * weight;
			if (target.normalDeltas.size() == _vertices.size())
			{
				normal += target.normalDeltas[vertexIndex] * weight;
				normalChanged = true;
			}
			if (target.tangentDeltas.size() == _vertices.size())
			{
				tangent += target.tangentDeltas[vertexIndex] * weight;
				tangentChanged = true;
			}
		}

		_vertices[vertexIndex].Position = position;

		if (normalChanged && glm::length(normal) > 0.0001f)
			normal = glm::normalize(normal);
		if (tangentChanged && glm::length(tangent) > 0.0001f)
			tangent = glm::normalize(tangent);

		_vertices[vertexIndex].Normal = normal;
		_vertices[vertexIndex].Tangent = tangent;

		if ((normalChanged || tangentChanged) &&
			glm::length(normal) > 0.0001f &&
			glm::length(tangent) > 0.0001f)
		{
			glm::vec3 bitangent = glm::cross(normal, tangent);
			if (glm::length(bitangent) > 0.0001f)
				_vertices[vertexIndex].Bitangent = glm::normalize(bitangent);
		}
	}

	_currentMorphWeights = clampedWeights;
	setupMesh();
	setupTransformation();
}

void AssImpMesh::resetMorphTargets()
{
	if (_morphTargets.isEmpty())
		return;

	applyMorphWeights(_defaultMorphWeights);
}

void AssImpMesh::syncVertexDataAfterBake()
{
	// Sync baked geometry back into _vertices to ensure export reads correct data.
	// This is called during bakeTransformations() to keep _vertices in sync with
	// _points (_normals, _tangents, _bitangents) which contain the baked geometry.
	// UVs (TexCoords) are preserved unchanged.

	// Validate data consistency
	const size_t pointCount = _points.size() / 3;  // Each point is 3 floats
	const size_t normalCount = _normals.size() / 3;
	const size_t tangentCount = _tangents.size() / 3;
	const size_t bitangentCount = _bitangents.size() / 3;

	if (pointCount != _vertices.size())
	{
		qWarning() << "AssImpMesh::syncVertexDataAfterBake: Point count mismatch."
			<< "Expected" << _vertices.size() << "got" << pointCount;
		return;
	}

	if (normalCount != _vertices.size() || tangentCount != _vertices.size() || bitangentCount != _vertices.size())
	{
		qWarning() << "AssImpMesh::syncVertexDataAfterBake: Tangent space data count mismatch";
		return;
	}

	// Rebuild each vertex with baked data
	for (size_t i = 0; i < _vertices.size(); ++i)
	{
		// Baked position
		_vertices[i].Position.x = _points[i * 3];
		_vertices[i].Position.y = _points[i * 3 + 1];
		_vertices[i].Position.z = _points[i * 3 + 2];

		// Baked normal
		_vertices[i].Normal.x = _normals[i * 3];
		_vertices[i].Normal.y = _normals[i * 3 + 1];
		_vertices[i].Normal.z = _normals[i * 3 + 2];

		// Baked tangent
		if (!_tangents.empty())
		{
			_vertices[i].Tangent.x = _tangents[i * 3];
			_vertices[i].Tangent.y = _tangents[i * 3 + 1];
			_vertices[i].Tangent.z = _tangents[i * 3 + 2];
		}

		// Baked bitangent
		if (!_bitangents.empty())
		{
			_vertices[i].Bitangent.x = _bitangents[i * 3];
			_vertices[i].Bitangent.y = _bitangents[i * 3 + 1];
			_vertices[i].Bitangent.z = _bitangents[i * 3 + 2];
		}

		// TexCoords unchanged - preserved from before baking
		// _vertices[i].TexCoords[0..3] remain as-is
	}

	_baseVertices = _vertices;
	_currentMorphWeights.clear();

	// Re-sync GPU buffers without re-optimizing the mesh
	// (setupMesh would re-optimize, which we don't want)
	setupMesh();

}

void AssImpMesh::setAlbedoPBRMap(unsigned int albedoMap)
{
	_material.setAlbedoTextureId(albedoMap);
	replaceOrAppendTexture("albedoMap", albedoMap, false);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setMetallicPBRMap(unsigned int metallicMap)
{
	_material.setMetallicTextureId(metallicMap);
	replaceOrAppendTexture("metallicMap", metallicMap, false);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setEmissivePBRMap(unsigned int emissiveMap)
{
	_material.setEmissiveTextureId(emissiveMap);
	replaceOrAppendTexture("emissiveMap", emissiveMap, false);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setRoughnessPBRMap(unsigned int roughnessMap)
{
	_material.setRoughnessTextureId(roughnessMap);
	replaceOrAppendTexture("roughnessMap", roughnessMap, false);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setNormalPBRMap(unsigned int normalMap)
{
	_material.setNormalTextureId(normalMap);
	replaceOrAppendTexture("normalMap", normalMap, false);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setAOPBRMap(unsigned int aoMap)
{
	_material.setOcclusionTextureId(aoMap);
	replaceOrAppendTexture("aoMap", aoMap, false);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setHeightPBRMap(unsigned int heightMap)
{
	_material.setHeightTextureId(heightMap);
	replaceOrAppendTexture("heightMap", heightMap, false);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setOpacityPBRMap(unsigned int opacityMap)
{
	_material.setOpacityTextureId(opacityMap);
	_material.setBlendMode(GLMaterial::BlendMode::Alpha);
	replaceOrAppendTexture("opacityMap", opacityMap, true);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setIORPBRMap(unsigned int iorMap)
{
	_material.setIORTextureId(iorMap);
	replaceOrAppendTexture("iorMap", iorMap, false);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setClearcoatPBRMap(unsigned int clearcoatColorMap)
{
	_material.setClearcoatColorTextureId(clearcoatColorMap);
	replaceOrAppendTexture("clearcoatColorMap", clearcoatColorMap, false);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setClearcoatRoughnessPBRMap(unsigned int clearcoatRoughnessMap)
{
	_material.setClearcoatRoughnessTextureId(clearcoatRoughnessMap);
	replaceOrAppendTexture("clearcoatRoughnessMap", clearcoatRoughnessMap, false);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setClearcoatNormalPBRMap(unsigned int clearcoatNormalMap)
{
	_material.setClearcoatNormalTextureId(clearcoatNormalMap);
	replaceOrAppendTexture("clearcoatNormalMap", clearcoatNormalMap, false);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setSheenColorPBRMap(unsigned int sheenMap)
{
	_material.setSheenColorTextureId(sheenMap);
	replaceOrAppendTexture("sheenColorMap", sheenMap, false);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setSheenRoughnessPBRMap(unsigned int sheenRoughnessMap)
{
	_material.setSheenRoughnessTextureId(sheenRoughnessMap);
	replaceOrAppendTexture("sheenRoughnessMap", sheenRoughnessMap, false);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setTransmissionPBRMap(unsigned int transmissionMap)
{
	_material.setTransmissionTextureId(transmissionMap);
	replaceOrAppendTexture("transmissionMap", transmissionMap, false);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::clearAlbedoPBRMap()
{
	_material.setAlbedoTextureId(0);
	removeTexturesByType({ "albedoMap" });
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::clearMetallicPBRMap()
{
	_material.setMetallicTextureId(0);
	removeTexturesByType({ "metallicMap" });
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::clearRoughnessPBRMap()
{
	_material.setRoughnessTextureId(0);
	removeTexturesByType({ "roughnessMap" });
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::clearNormalPBRMap()
{
	_material.setNormalTextureId(0);
	removeTexturesByType({ "normalMap" });
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::clearAOPBRMap()
{
	_material.setOcclusionTextureId(0);
	removeTexturesByType({ "aoMap", "occlusionMap" });
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::clearHeightPBRMap()
{
	_material.setHeightTextureId(0);
	removeTexturesByType({ "heightMap" });
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::clearOpacityPBRMap()
{
	_material.setOpacityTextureId(0);
	removeTexturesByType({ "opacityMap" });
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::clearTransmissionPBRMap()
{
	_material.setTransmissionTextureId(0);
	removeTexturesByType({ "transmissionMap" });
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::clearIORPBRMap()
{
	_material.setIORTextureId(0);
	removeTexturesByType({ "iorMap" });
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::clearSheenColorPBRMap()
{
	_material.setSheenColorTextureId(0);
	removeTexturesByType({ "sheenColorMap" });
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::clearSheenRoughnessPBRMap()
{
	_material.setSheenRoughnessTextureId(0);
	removeTexturesByType({ "sheenRoughnessMap" });
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::clearClearcoatPBRMap()
{
	_material.setClearcoatColorTextureId(0);
	removeTexturesByType({ "clearcoatColorMap" });
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::clearClearcoatRoughnessPBRMap()
{
	_material.setClearcoatRoughnessTextureId(0);
	removeTexturesByType({ "clearcoatRoughnessMap" });
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::clearClearcoatNormalPBRMap()
{
	_material.setClearcoatNormalTextureId(0);
	removeTexturesByType({ "clearcoatNormalMap" });
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::clearAllPBRMaps()
{
	_material.setAlbedoTextureId(0);
	_material.setMetallicTextureId(0);
	_material.setEmissiveTextureId(0);
	_material.setRoughnessTextureId(0);
	_material.setNormalTextureId(0);
	_material.setOcclusionTextureId(0);
	_material.setHeightTextureId(0);
	_material.setOpacityTextureId(0);
	_material.setTransmissionTextureId(0);
	_material.setIORTextureId(0);
	_material.setSheenColorTextureId(0);
	_material.setSheenRoughnessTextureId(0);
	_material.setClearcoatColorTextureId(0);
	_material.setClearcoatRoughnessTextureId(0);
	_material.setClearcoatNormalTextureId(0);
	_material.setSpecularGlossinessTextureId(0);

	removeTexturesByType({
		"albedoMap",
		"metallicMap",
		"emissiveMap",
		"roughnessMap",
		"normalMap",
		"aoMap",
		"occlusionMap",
		"heightMap",
		"opacityMap",
		"transmissionMap",
		"iorMap",
		"sheenColorMap",
		"sheenRoughnessMap",
		"clearcoatColorMap",
		"clearcoatRoughnessMap",
		"clearcoatNormalMap",
		"specularGlossinessMap"
	});
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setDiffuseADSMap(unsigned int diffuseTex)
{
	_diffuseADSMap = diffuseTex;
	replaceOrAppendTexture("texture_diffuse", diffuseTex, false);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setSpecularADSMap(unsigned int specularTex)
{
	_specularADSMap = specularTex;
	replaceOrAppendTexture("texture_specular", specularTex, false);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setEmissiveADSMap(unsigned int emissiveTex)
{
	_emissiveADSMap = emissiveTex;
	replaceOrAppendTexture("texture_emissive", emissiveTex, false);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setNormalADSMap(unsigned int normalTex)
{
	_normalADSMap = normalTex;
	replaceOrAppendTexture("texture_normal", normalTex, false);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setHeightADSMap(unsigned int heightTex)
{
	_heightADSMap = heightTex;
	replaceOrAppendTexture("texture_height", heightTex, false);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::setOpacityADSMap(unsigned int opacityTex)
{
	_opacityADSMap = opacityTex;
	replaceOrAppendTexture("texture_opacity", opacityTex, true);
	_material.setBlendMode(GLMaterial::BlendMode::Alpha);
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::clearDiffuseADSMap()
{
	_diffuseADSMap = 0;
	removeTexturesByType({ "texture_diffuse" });
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::clearSpecularADSMap()
{
	_specularADSMap = 0;
	removeTexturesByType({ "texture_specular" });
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::clearEmissiveADSMap()
{
	_emissiveADSMap = 0;
	removeTexturesByType({ "texture_emissive" });
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::clearNormalADSMap()
{
	_normalADSMap = 0;
	removeTexturesByType({ "texture_normal" });
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::clearHeightADSMap()
{
	_heightADSMap = 0;
	removeTexturesByType({ "texture_height" });
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::clearOpacityADSMap()
{
	_opacityADSMap = 0;
	removeTexturesByType({ "texture_opacity" });
	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::clearAllADSMaps()
{
	clearDiffuseADSMap();
	clearSpecularADSMap();
	clearEmissiveADSMap();
	clearNormalADSMap();
	clearHeightADSMap();
	clearOpacityADSMap();
}

void AssImpMesh::setTextureMaps(const GLMaterial& material)
{
	// Runtime-resolved GLMaterial instances can point at shared textures from
	// GLWidget's cache. Sync to those ids without deleting or recreating them.
	_material = material;
	cacheBaseVolumeProperties();
	applyScaledVolumeProperties();

	_textures.clear();

	auto syncTexture = [this](bool present, const char* type, GLuint id, bool hasAlpha = false)
	{
		if (present)
			replaceOrAppendTexture(type, id, hasAlpha);
	};

	syncTexture(material.hasAlbedoMap(), "albedoMap", material.albedoTextureId());
	syncTexture(material.hasMetallicMap(), "metallicMap", material.metallicTextureId());
	syncTexture(material.hasEmissiveMap(), "emissiveMap", material.emissiveTextureId());
	syncTexture(material.hasRoughnessMap(), "roughnessMap", material.roughnessTextureId());
	syncTexture(material.hasNormalMap(), "normalMap", material.normalTextureId());
	syncTexture(material.hasAOMap(), "aoMap", material.occlusionTextureId());
	syncTexture(material.hasHeightMap(), "heightMap", material.heightTextureId());
	syncTexture(material.hasOpacityMap(), "opacityMap", material.opacityTextureId(), true);
	syncTexture(material.hasTransmissionMap(), "transmissionMap", material.transmissionTextureId());
	syncTexture(material.hasIORMap(), "iorMap", material.iorTextureId());
	syncTexture(material.hasSheenColorMap(), "sheenColorMap", material.sheenColorTextureId());
	syncTexture(material.hasSheenRoughnessMap(), "sheenRoughnessMap", material.sheenRoughnessTextureId());
	syncTexture(material.hasClearcoatColorMap(), "clearcoatColorMap", material.clearcoatColorTextureId());
	syncTexture(material.hasClearcoatRoughnessMap(), "clearcoatRoughnessMap", material.clearcoatRoughnessTextureId());
	syncTexture(material.hasClearcoatNormalMap(), "clearcoatNormalMap", material.clearcoatNormalTextureId());
	syncTexture(material.hasIridescenceMap(), "iridescenceMap", material.iridescenceTextureId());
	syncTexture(material.hasIridescenceThicknessMap(), "iridescenceThicknessMap", material.iridescenceThicknessTextureId());
	syncTexture(material.hasSpecularFactorMap(), "specularFactorMap", material.specularFactorTextureId());
	syncTexture(material.hasSpecularColorMap(), "specularColorMap", material.specularColorTextureId());
	syncTexture(material.hasAnisotropyMap(), "anisotropyMap", material.anisotropyTextureId());
	syncTexture(material.hasThicknessMap(), "thicknessMap", material.thicknessTextureId());
	syncTexture(material.hasDiffuseMap(), "diffuseMap", material.diffuseTextureId());
	syncTexture(material.hasDiffuseTransmissionMap(), "diffuseTransmissionMap", material.diffuseTransmissionTextureId());
	syncTexture(material.hasDiffuseTransmissionColorMap(), "diffuseTransmissionColorMap", material.diffuseTransmissionColorTextureId());
	syncTexture(material.hasSpecularGlossinessMap(), "specularGlossinessMap", material.specularGlossinessTextureId());

	_diffuseADSMap = material.hasAlbedoMap()
		? material.albedoTextureId()
		: (material.hasDiffuseMap() ? material.diffuseTextureId() : 0U);
	_specularADSMap = material.hasMetallicMap() ? material.metallicTextureId() : 0U;
	_emissiveADSMap = material.hasEmissiveMap() ? material.emissiveTextureId() : 0U;
	_normalADSMap = material.hasNormalMap() ? material.normalTextureId() : 0U;
	_heightADSMap = material.hasHeightMap() ? material.heightTextureId() : 0U;
	_opacityADSMap = material.hasOpacityMap() ? material.opacityTextureId() : 0U;

	syncTexture(_diffuseADSMap != 0, "texture_diffuse", _diffuseADSMap);
	syncTexture(_specularADSMap != 0, "texture_specular", _specularADSMap);
	syncTexture(_emissiveADSMap != 0, "texture_emissive", _emissiveADSMap);
	syncTexture(_normalADSMap != 0, "texture_normal", _normalADSMap);
	syncTexture(_heightADSMap != 0, "texture_height", _heightADSMap);
	syncTexture(_opacityADSMap != 0, "texture_opacity", _opacityADSMap, true);

	markTexturesDirty();
	markUniformsDirty();
}

void AssImpMesh::replaceOrAppendTexture(const std::string& type, GLuint id, bool hasAlpha)
{
	for (auto& t : _textures)
	{
		if (t.type == type)
		{
			t.id = id;
			t.hasAlpha = hasAlpha;
			return;
		}
	}
	GLMaterial::Texture t; t.id = id; t.type = type; t.hasAlpha = hasAlpha;
	_textures.push_back(t);
}

void AssImpMesh::removeTexturesByType(std::initializer_list<std::string> types)
{
	_textures.erase(
		std::remove_if(_textures.begin(), _textures.end(),
			[&](const GLMaterial::Texture& texture)
			{
				return std::find(types.begin(), types.end(), texture.type) != types.end();
			}),
		_textures.end());
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

void AssImpMesh::deleteTextures()
{
	glDeleteTextures(1, &_diffuseADSMap);
	glDeleteTextures(1, &_specularADSMap);
	glDeleteTextures(1, &_emissiveADSMap);
	glDeleteTextures(1, &_normalADSMap);
	glDeleteTextures(1, &_heightADSMap);
	glDeleteTextures(1, &_opacityADSMap);
	TriangleMesh::deleteTextures();
}
