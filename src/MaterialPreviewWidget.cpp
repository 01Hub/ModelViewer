#include "MaterialPreviewWidget.h"
#include <vector>
#include <cmath>
#include "config.h"

MaterialPreviewWidget::MaterialPreviewWidget(QWidget* parent)
	: QOpenGLWidget(parent)
{
	setMinimumSize(200, 200);
}

MaterialPreviewWidget::~MaterialPreviewWidget()
{
	makeCurrent();
	if (vao) glDeleteVertexArrays(1, &vao);
	if (vbo) glDeleteBuffers(1, &vbo);
	if (ebo) glDeleteBuffers(1, &ebo);
	doneCurrent();
}

void MaterialPreviewWidget::setMaterial(const GLMaterial& mat)
{
	_currentMaterial = mat;
	update();
}

void MaterialPreviewWidget::initializeGL()
{
	initializeOpenGLFunctions();
	glEnable(GL_DEPTH_TEST);

	const QString path = QString(MODELVIEWER_DATA_DIR) + "/";

	_shader = std::make_unique<ShaderProgram>(); _shader->setObjectName("_shader");
	_shader->loadCompileAndLinkShaderFromFile(path + "shaders/preview_swatch.vert",
		path + "shaders/preview_swatch.frag");

	initSphereMesh();
}

void MaterialPreviewWidget::resizeGL(int w, int h)
{
	proj.setToIdentity();
	proj.perspective(60.0f, float(w) / float(h), 0.1f, 10.0f);

	view.setToIdentity();
	view.translate(0, 0, -2.25f);
}

void MaterialPreviewWidget::paintGL()
{
	glClearColor(0.15f, 0.15f, 0.18f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	_shader->bind();

	QMatrix4x4 model;
	model.rotate(25, 1, 0, 0);
	model.rotate(30, 0, 1, 0);

	QMatrix4x4 mvp = proj * view * model;
	QMatrix3x3 normalMat = model.normalMatrix();

	_shader->setUniformValue("uMVP", mvp);
	_shader->setUniformValue("uModel", model);
	_shader->setUniformValue("uNormalMatrix", normalMat);
	_shader->setUniformValue("uCamPos", QVector3D(0, 0, 3));

	_shader->setUniformValue("uAlbedo", _currentMaterial.albedoColor());
	_shader->setUniformValue("uMetalness", _currentMaterial.metalness());
	_shader->setUniformValue("uRoughness", _currentMaterial.roughness());
	_shader->setUniformValue("uOpacity", _currentMaterial.opacity());
	_shader->setUniformValue("uClearcoat", _currentMaterial.clearcoat());
	_shader->setUniformValue("uClearcoatRoughness", _currentMaterial.clearcoatRoughness());
	_shader->setUniformValue("uSheenColor", _currentMaterial.sheenColor());
	_shader->setUniformValue("uSheenRoughness", _currentMaterial.sheenRoughness());
	_shader->setUniformValue("uTransmission", _currentMaterial.transmission());
	_shader->setUniformValue("uIOR", _currentMaterial.ior());
	_shader->setUniformValue("uSpecular", _currentMaterial.specular());

	// Texture support
	_shader->setUniformValue("uUseAlbedoMap", false);
	_shader->setUniformValue("uUseMetalnessMap", false);
	_shader->setUniformValue("uUseRoughnessMap", false);
	_shader->setUniformValue("uUseNormalMap", false);
	_shader->setUniformValue("uUseAOMap", false);
	_shader->setUniformValue("uUseHeightMap", false);
	_shader->setUniformValue("uUseEmissiveMap", false);
	_shader->setUniformValue("uNormalIntensity", 1.0f);
	_shader->setUniformValue("uAOIntensity", 1.0f);
	_shader->setUniformValue("uHeightIntensity", 1.0f);
	_shader->setUniformValue("uEmissiveColor", QVector3D(1.0f, 1.0f, 1.0f));
	_shader->setUniformValue("uEmissiveIntensity", 1.0f);
	_shader->setUniformValue("uUVScale", QVector2D(1.0f, 1.0f));
	// Set up texture samplers
	_shader->setUniformValue("uAlbedoMap", 0);
	_shader->setUniformValue("uMetalnessMap", 1);
	_shader->setUniformValue("uRoughnessMap", 2);
	_shader->setUniformValue("uNormalMap", 3);
	_shader->setUniformValue("uAOMap", 4);
	_shader->setUniformValue("uHeightMap", 5);
	_shader->setUniformValue("uEmissiveMap", 6);
	// Set up texture units
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0); // No albedo map
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0); // No metalness map
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, 0); // No roughness map
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, 0); // No normal map
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, 0); // No AO map
	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D, 0); // No height map
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, 0); // No emissive map
	
	// Set up simple lighting
	_shader->setUniformValue("uLights[0].position", QVector3D(3.0f, 3.0f, 3.0f));
	_shader->setUniformValue("uLights[0].color", QVector3D(1.0f, 1.0f, 1.0f));

	_shader->setUniformValue("uLights[1].position", QVector3D(-3.0f, 3.0f, 1.0f));
	_shader->setUniformValue("uLights[1].color", QVector3D(0.8f, 0.8f, 0.8f));

	_shader->setUniformValue("uLights[2].position", QVector3D(0.0f, -3.0f, 2.0f));
	_shader->setUniformValue("uLights[2].color", QVector3D(0.6f, 0.6f, 0.6f));


	glBindVertexArray(vao);
	glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);

	_shader->release();
}

void MaterialPreviewWidget::initSphereMesh()
{
	const int X_SEGMENTS = 64;
	const int Y_SEGMENTS = 64;

	std::vector<float> vertices;
	std::vector<unsigned int> indices;

	for (int y = 0; y <= Y_SEGMENTS; ++y)
	{
		for (int x = 0; x <= X_SEGMENTS; ++x)
		{
			float xSeg = (float)x / (float)X_SEGMENTS;
			float ySeg = (float)y / (float)Y_SEGMENTS;
			float xPos = std::cos(xSeg * 2.0f * M_PI) * std::sin(ySeg * M_PI);
			float yPos = std::cos(ySeg * M_PI);
			float zPos = std::sin(xSeg * 2.0f * M_PI) * std::sin(ySeg * M_PI);

			vertices.push_back(xPos);
			vertices.push_back(yPos);
			vertices.push_back(zPos);
			vertices.push_back(xPos);
			vertices.push_back(yPos);
			vertices.push_back(zPos);

            float u = xSeg;
            float v = ySeg;
            vertices.push_back(u);  // Texture U
            vertices.push_back(v);  // Texture V
		}
	}

	for (int y = 0; y < Y_SEGMENTS; ++y)
	{
		for (int x = 0; x < X_SEGMENTS; ++x)
		{
			int i0 = y * (X_SEGMENTS + 1) + x;
			int i1 = i0 + X_SEGMENTS + 1;
			indices.push_back(i0);
			indices.push_back(i1);
			indices.push_back(i0 + 1);

			indices.push_back(i0 + 1);
			indices.push_back(i1);
			indices.push_back(i1 + 1);
		}
	}

	indexCount = (int)indices.size();

	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glGenBuffers(1, &ebo);

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0); // Position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float))); // Normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float))); // TexCoord
    glEnableVertexAttribArray(2);

	glBindVertexArray(0);
}
