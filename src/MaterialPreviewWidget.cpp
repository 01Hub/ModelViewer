#include "MaterialPreviewWidget.h"
#include <vector>
#include <cmath>
#include "config.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPainter>

MaterialPreviewWidget::MaterialPreviewWidget(QWidget* parent)
	: QOpenGLWidget(parent)
{
	setMinimumSize(200, 200);	
	setContextMenuPolicy(Qt::NoContextMenu);

	// Timer to trigger fade-out repaint
	connect(&_overlayUpdateTimer, &QTimer::timeout, this, [this]() { update(); });
	_overlayUpdateTimer.start(50); // ~20 FPS fade

	// ~60 FPS spin updates
	_spinTimer.setInterval(16);
	connect(&_spinTimer, &QTimer::timeout, this, [this]() {
		// compute dt in seconds
		double dt = _spinClock.elapsed() * 0.001;
		_spinClock.restart();

		// exponential damping: v *= e^(-lambda*dt)
		float k = std::exp(-_spinDamping * float(dt));
		_spinVelX *= k;
		_spinVelY *= k;

		// integrate angles
		_rotX += _spinVelX * float(dt);
		_rotY += _spinVelY * float(dt);

		// clamp pitch
		if (_rotX > 89.0f) _rotX = 89.0f;
		if (_rotX < -89.0f) _rotX = -89.0f;

		// stop if slow enough
		if (std::abs(_spinVelX) < _spinMinSpeed && std::abs(_spinVelY) < _spinMinSpeed)
		{
			stopSpin();
			return;
		}

		update();
		});

}

MaterialPreviewWidget::~MaterialPreviewWidget()
{
	makeCurrent();
	destroyMesh(_sphere);
	destroyMesh(_cube);
	destroyMesh(_plane);
	destroyMesh(_teapot);
	doneCurrent();
}

void MaterialPreviewWidget::setMaterial(const GLMaterial& mat)
{
	_currentMaterial = mat;
	update();
}

void MaterialPreviewWidget::setPreviewShape(PreviewShape s)
{
	_currentShape = s;
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
	initCubeMesh();  
	initPlaneMesh(); 
	initTeapotMesh();
}


void MaterialPreviewWidget::resizeGL(int w, int h)
{
	proj.setToIdentity();
	proj.perspective(45.0f, float(w) / float(h), 0.1f, 10.0f);
}

void MaterialPreviewWidget::paintGL()
{
	view.setToIdentity();
	if (_currentShape == PreviewShape::Sphere)
		view.translate(0, 0, -3.0f);
	else if (_currentShape == PreviewShape::Cube)
		view.translate(0, 0, -4.5f);
	else if (_currentShape == PreviewShape::Plane)
		view.translate(0, 0, -4.0f);
	else if (_currentShape == PreviewShape::Teapot)
		view.translate(0, 0, -4.5f);

	glViewport(0, 0, width(), height());
	glClearColor(0.15f, 0.15f, 0.18f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_TRUE);

	glDisable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	_shader->bind();

	QMatrix4x4 model;
	model.scale(_zoom);
	model.rotate(_rotX, 1, 0, 0);
	model.rotate(_rotY, 0, 1, 0);

	QMatrix4x4 mvp = proj * view * model;
	QMatrix3x3 normalMat = model.normalMatrix();

	_shader->setUniformValue("uMVP", mvp);
	_shader->setUniformValue("uModel", model);
	_shader->setUniformValue("uNormalMatrix", normalMat);

	QMatrix4x4 invView = view.inverted();
	QVector3D camWorld = (invView * QVector4D(0, 0, 0, 1)).toVector3D();
	_shader->setUniformValue("uCamPos", camWorld);

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


	const MeshGL* mesh = nullptr;
	switch (_currentShape)
	{
	case PreviewShape::Sphere: mesh = &_sphere; break;
	case PreviewShape::Cube:   mesh = &_cube;   break;
	case PreviewShape::Plane:  mesh = &_plane;  break;
	case PreviewShape::Teapot: mesh = &_teapot; break;
	}
	if (mesh && mesh->vao && mesh->indexCount > 0)
	{
		glBindVertexArray(mesh->vao);
		glDrawElements(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}


	_shader->release();


	if (_overlayActive)
	{
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);

		const qint64 elapsed = _overlayTimer.elapsed();
		float alpha = 1.0f;
		if (elapsed > _overlayShowMs)
		{
			alpha = 1.0f - float(elapsed - _overlayShowMs) / float(_overlayFadeMs);
			if (alpha <= 0.0f)
			{
				_overlayActive = false;
				_overlayUpdateTimer.stop();
				return;
			}
		}

		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		p.setRenderHint(QPainter::TextAntialiasing);

		QColor textColor(255, 255, 255, int(alpha * 200));
		QColor bgColor(0, 0, 0, int(alpha * 150));

		QString hint =
			"L-drag: rotate\n"
			"R-drag: zoom\n"
			"Double-click L: reset all\n"
			"Double-click R: reset zoom";

		QFont font = p.font();
		font.setPointSize(9);
		p.setFont(font);

		QRect rect = p.boundingRect(this->rect().adjusted(10, 10, -10, -10),
			Qt::AlignLeft | Qt::AlignTop, hint);

		p.setBrush(bgColor);
		p.setPen(Qt::NoPen);
		p.drawRoundedRect(rect.adjusted(-6, -4, +6, +4), 6, 6);

		p.setPen(textColor);
		p.drawText(rect, Qt::AlignLeft | Qt::AlignTop, hint);
	}
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

			// pos
			vertices.push_back(xPos); vertices.push_back(yPos); vertices.push_back(zPos);
			// normal
			vertices.push_back(xPos); vertices.push_back(yPos); vertices.push_back(zPos);
			// uv
			vertices.push_back(xSeg); vertices.push_back(ySeg);
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

	_sphere.indexCount = (int)indices.size();

	glGenVertexArrays(1, &_sphere.vao);
	glGenBuffers(1, &_sphere.vbo);
	glGenBuffers(1, &_sphere.ebo);

	glBindVertexArray(_sphere.vao);
	glBindBuffer(GL_ARRAY_BUFFER, _sphere.vbo);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _sphere.ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);               glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float))); glEnableVertexAttribArray(2);

	glBindVertexArray(0);
}


void MaterialPreviewWidget::initCubeMesh()
{
	// 8 floats per vertex: pos(3) + normal(3) + uv(2)
	const float v[] = {
		// +X face
		1, -1, -1,   1, 0, 0,   0, 0,
		1,  1, -1,   1, 0, 0,   0, 1,
		1,  1,  1,   1, 0, 0,   1, 1,
		1, -1,  1,   1, 0, 0,   1, 0,
		// -X face
	   -1, -1,  1,  -1, 0, 0,   0, 0,
	   -1,  1,  1,  -1, 0, 0,   0, 1,
	   -1,  1, -1,  -1, 0, 0,   1, 1,
	   -1, -1, -1,  -1, 0, 0,   1, 0,
	   // +Y face
	  -1,  1, -1,   0, 1, 0,   0, 0,
	  -1,  1,  1,   0, 1, 0,   0, 1,
	   1,  1,  1,   0, 1, 0,   1, 1,
	   1,  1, -1,   0, 1, 0,   1, 0,
	   // -Y face
	  -1, -1,  1,   0,-1, 0,   0, 0,
	  -1, -1, -1,   0,-1, 0,   0, 1,
	   1, -1, -1,   0,-1, 0,   1, 1,
	   1, -1,  1,   0,-1, 0,   1, 0,
	   // +Z face
	  -1, -1,  1,   0, 0, 1,   0, 0,
	   1, -1,  1,   0, 0, 1,   0, 1,
	   1,  1,  1,   0, 0, 1,   1, 1,
	  -1,  1,  1,   0, 0, 1,   1, 0,
	  // -Z face
	  1, -1, -1,   0, 0,-1,   0, 0,
	 -1, -1, -1,   0, 0,-1,   0, 1,
	 -1,  1, -1,   0, 0,-1,   1, 1,
	  1,  1, -1,   0, 0,-1,   1, 0,
	};
	const unsigned int idx[] = {
		0,1,2, 0,2,3,       4,5,6, 4,6,7,
		8,9,10, 8,10,11,   12,13,14, 12,14,15,
		16,17,18, 16,18,19, 20,21,22, 20,22,23
	};

	_cube.indexCount = (int)(sizeof(idx) / sizeof(idx[0]));

	glGenVertexArrays(1, &_cube.vao);
	glGenBuffers(1, &_cube.vbo);
	glGenBuffers(1, &_cube.ebo);

	glBindVertexArray(_cube.vao);
	glBindBuffer(GL_ARRAY_BUFFER, _cube.vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _cube.ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);                 glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float))); glEnableVertexAttribArray(2);

	glBindVertexArray(0);
}


void MaterialPreviewWidget::initPlaneMesh()
{
	// 2x2 quad centered, size 2; in XZ plane with Y=0; uv 0..1
	const float v[] = {
		-1, 0, -1,   0,1,0,   0,0,
		 1, 0, -1,   0,1,0,   1,0,
		 1, 0,  1,   0,1,0,   1,1,
		-1, 0,  1,   0,1,0,   0,1
	};
	const unsigned int idx[] = { 0,1,2, 0,2,3 };

	_plane.indexCount = 6;

	glGenVertexArrays(1, &_plane.vao);
	glGenBuffers(1, &_plane.vbo);
	glGenBuffers(1, &_plane.ebo);

	glBindVertexArray(_plane.vao);
	glBindBuffer(GL_ARRAY_BUFFER, _plane.vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _plane.ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);                 glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float))); glEnableVertexAttribArray(2);

	glBindVertexArray(0);
}


void MaterialPreviewWidget::initTeapotMesh()
{
	// Intentionally left empty to keep the preview widget lightweight.
	// If you later want the embedded Utah teapot (closed lid), I’ll provide:
	//  - static control points and patch index tables
	//  - Bezier surface evaluator (pos + dp/du, dp/dv -> normal)
	//  - UVs from (u,v)
	//  - Triangulation at a selectable tessLevel
	_teapot = {};
}


void MaterialPreviewWidget::destroyMesh(MeshGL& m)
{
	if (m.vao) glDeleteVertexArrays(1, &m.vao);
	if (m.vbo) glDeleteBuffers(1, &m.vbo);
	if (m.ebo) glDeleteBuffers(1, &m.ebo);
	m = {}; // reset
}

void MaterialPreviewWidget::startOverlayHint(int showMs, int fadeMs)
{
	_overlayShowMs = showMs;
	_overlayFadeMs = fadeMs;
	_overlayActive = true;
	_overlayTimer.restart();
	if (!_overlayUpdateTimer.isActive())
		_overlayUpdateTimer.start();
	update();
}

void MaterialPreviewWidget::startSpin(float velPitchDegPerSec, float velYawDegPerSec)
{
	_spinVelX = velPitchDegPerSec;
	_spinVelY = velYawDegPerSec;
	_spinClock.restart();
	if (!_spinTimer.isActive()) _spinTimer.start();
}

void MaterialPreviewWidget::stopSpin()
{
	_spinTimer.stop();
	_spinVelX = _spinVelY = 0.0f;
}


void MaterialPreviewWidget::mousePressEvent(QMouseEvent* e)
{
	if (e->button() == Qt::LeftButton)
	{
		_dragging = true;
		_dragDelta = QPoint();
		_lastPos = e->pos();
		setCursor(Qt::ClosedHandCursor);
		stopSpin(); // interacting cancels spin
	}
	else if (e->button() == Qt::RightButton)
	{
		_rightDragging = true;
		_lastPos = e->pos();
		setCursor(Qt::SizeVerCursor);
		// no spin for zoom
	}
	QOpenGLWidget::mousePressEvent(e);
}

void MaterialPreviewWidget::mouseMoveEvent(QMouseEvent* e)
{
	QPoint delta = e->pos() - _lastPos;
	_lastPos = e->pos();

	if (_dragging)
	{
		_dragDelta = delta; // remember for release velocity
		_rotY += delta.x() * _rotSpeed;
		_rotX += delta.y() * _rotSpeed;
		if (_rotX > 89.0f) _rotX = 89.0f;
		if (_rotX < -89.0f) _rotX = -89.0f;
		update();
	}
	else if (_rightDragging)
	{
		_zoom *= (1.0f - delta.y() * _zoomSpeed);
		_zoom = std::clamp(_zoom, _minZoom, _maxZoom);
		update();
	}

	QOpenGLWidget::mouseMoveEvent(e);
}

void MaterialPreviewWidget::mouseReleaseEvent(QMouseEvent* e)
{
	if (e->button() == Qt::LeftButton && _dragging)
	{
		_dragging = false;
		unsetCursor();

		// convert last pixel delta into deg/sec
		// pixels -> degrees: delta * _rotSpeed (deg/pixel)
		// deg/frame -> deg/sec : assume ~1/0.016 ≈ 62.5 fps or measure:
		// We’ll use the spin timer’s interval as a baseline (~16ms).
		const float frameSeconds = 0.016f; // approximate is fine for inertia
		float velYaw = (_dragDelta.x() * _rotSpeed) / frameSeconds; // deg/sec
		float velPitch = (_dragDelta.y() * _rotSpeed) / frameSeconds; // deg/sec

		// start only if there was a meaningful flick
		float mag = std::sqrt(velYaw * velYaw + velPitch * velPitch);
		if (mag > 40.0f)
		{ // small threshold to avoid tiny spins
			startSpin(velPitch, velYaw);
		}
		else
		{
			stopSpin();
		}
	}
	else if (e->button() == Qt::RightButton && _rightDragging)
	{
		_rightDragging = false;
		unsetCursor();
		// no spin on zoom
	}
	QOpenGLWidget::mouseReleaseEvent(e);
}


void MaterialPreviewWidget::mouseDoubleClickEvent(QMouseEvent* e)
{
	if (e->button() == Qt::LeftButton)
	{
		// full reset
		_rotX = 25.0f;
		_rotY = 30.0f;
		_zoom = 1.0f;
		update();
		e->accept();
		return;
	}
	if (e->button() == Qt::RightButton)
	{
		// zoom-only reset
		_zoom = 1.0f;
		update();
		e->accept();
		return;
	}
	QOpenGLWidget::mouseDoubleClickEvent(e);
}



void MaterialPreviewWidget::wheelEvent(QWheelEvent* e)
{
	QPoint numDegrees = e->angleDelta() / 8; // Qt reports in 1/8 degree
	if (!numDegrees.isNull())
	{
		int numSteps = numDegrees.y() / 15;  // 15 deg per step
		_zoom *= (1.0f + numSteps * 0.05f);  // each step = ±5%
		_zoom = std::clamp(_zoom, _minZoom, _maxZoom);
		update();
	}
	e->accept();
}

void MaterialPreviewWidget::showEvent(QShowEvent* e)
{
	QOpenGLWidget::showEvent(e);
	startOverlayHint(); // restart each time the widget becomes visible
}



void MaterialPreviewWidget::setPreviewRotation(float pitchDeg, float yawDeg)
{
	_rotX = pitchDeg;
	_rotY = yawDeg;
	update();
}
