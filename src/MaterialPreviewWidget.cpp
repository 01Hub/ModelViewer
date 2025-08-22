#include "MaterialPreviewWidget.h"
#include <vector>
#include <cmath>
#include "config.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPainter>

#include "TeapotData.h" // (Kilgard) 

namespace
{

	// ---------- small math helpers ----------
	struct V3 { float x, y, z; };
	inline V3 operator+(V3 a, V3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
	inline V3 operator-(V3 a, V3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
	inline V3 operator*(V3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }
	inline V3 cross(V3 a, V3 b)
	{
		return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
	}
	inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
	inline V3 normalize(V3 v)
	{
		float L = std::sqrt(dot(v, v));
		return (L > 0.f) ? (v * (1.0f / L)) : V3{ 0,0,1 };
	}

	// ---------- Bernstein basis (from your Teapot.cpp) ----------
	static void computeBasis(std::vector<float>& B, std::vector<float>& dB, int grid)
	{ // :contentReference[oaicite:3]{index=3}
		float inc = 1.0f / grid;
		B.resize(4 * (grid + 1));
		dB.resize(4 * (grid + 1));
		for (int i = 0; i <= grid; ++i)
		{
			float t = i * inc;
			float t2 = t * t;
			float omt = 1.0f - t;
			float omt2 = omt * omt;

			B[i * 4 + 0] = omt * omt2;
			B[i * 4 + 1] = 3.0f * omt2 * t;
			B[i * 4 + 2] = 3.0f * omt * t2;
			B[i * 4 + 3] = t * t2;

			dB[i * 4 + 0] = -3.0f * omt2;
			dB[i * 4 + 1] = -6.0f * t * omt + 3.0f * omt2;
			dB[i * 4 + 2] = -3.0f * t2 + 6.0f * t * omt;
			dB[i * 4 + 3] = 3.0f * t2;
		}
	}

	// Evaluate a 4x4 Bezier patch at (u,v) with basis tables (from your Teapot.cpp)  // :contentReference[oaicite:4]{index=4}
	static V3 evalPos(int iu, int iv, const std::vector<float>& B, V3 cp[4][4])
	{
		V3 p{ 0,0,0 };
		for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j)
		{
			float w = B[iu * 4 + i] * B[iv * 4 + j];
			p = p + cp[i][j] * w;
		}
		return p;
	}
	static V3 evalNormal(int iu, int iv, const std::vector<float>& B, const std::vector<float>& dB, V3 cp[4][4])
	{
		V3 du{ 0,0,0 }, dv{ 0,0,0 };
		for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j)
		{
			du = du + cp[i][j] * (dB[iu * 4 + i] * B[iv * 4 + j]);
			dv = dv + cp[i][j] * (B[iu * 4 + i] * dB[iv * 4 + j]);
		}
		return normalize(cross(du, dv));
	}

	// Get one 4x4 patch control grid from Kilgard data (mirrors logic in your Teapot.cpp::getPatch)  // :contentReference[oaicite:5]{index=5} :contentReference[oaicite:6]{index=6}
	// Build a 4x4 control grid for patchNum.
// reverseV toggles the exact indexing trick Kilgard uses.
	static void getPatch(int patchNum, V3 dst[4][4], bool reverseV, float size)
	{
		using namespace TeapotData;
		for (int u = 0; u < 4; ++u)
		{
			for (int v = 0; v < 4; ++v)
			{
				const int vv = reverseV ? (3 - v) : v;
				const int idx = patchdata[patchNum][u * 4 + vv];
				dst[u][v] = { cpdata[idx][0] * size,
							  cpdata[idx][1] * size,
							  cpdata[idx][2] * size - size };
			}
		}
	}


	struct M3 { float m[3][3]; }; // column-major not needed; we’ll multiply explicitly

	static inline V3 mul(const M3& R, V3 a)
	{
		return {
			R.m[0][0] * a.x + R.m[0][1] * a.y + R.m[0][2] * a.z,
			R.m[1][0] * a.x + R.m[1][1] * a.y + R.m[1][2] * a.z,
			R.m[2][0] * a.x + R.m[2][1] * a.y + R.m[2][2] * a.z
		};
	}

	// Identity and simple axis-reflects
	static const M3 R_ID = { {{ 1,0,0 },{ 0,1,0 },{ 0,0,1 }} };
	static const M3 R_X = { {{-1,0,0 },{ 0,1,0 },{ 0,0,1 }} };
	static const M3 R_Y = { {{ 1,0,0 },{ 0,-1,0},{ 0,0,1 }} };
	static const M3 R_XY = { {{-1,0,0 },{ 0,-1,0},{ 0,0,1 }} };

	// Build one triangulated patch with a reflect matrix and optional normal inversion.
	// (No global Y/Z swap here.)
	static void buildOne(V3 patch[4][4],
		const std::vector<float>& B, const std::vector<float>& dB,
		std::vector<float>& verts, std::vector<unsigned int>& idx,
		int grid, const M3& reflect, bool invertNormal)
	{
		const int steps = grid;
		const int stride = steps + 1;
		const unsigned base = (unsigned)(verts.size() / 8);

		for (int iv = 0; iv <= steps; ++iv)
		{
			for (int iu = 0; iu <= steps; ++iu)
			{
				V3 P = evalPos(iu, iv, B, patch);
				V3 dU = { 0,0,0 }, dV = { 0,0,0 };
				// reuse our normal helper
				V3 N = evalNormal(iu, iv, B, dB, patch);

				// apply reflection to both position and normal
				P = mul(reflect, P);
				N = mul(reflect, N);

				if (invertNormal) N = N * -1.0f;
				N = normalize(N);

				float u = float(iu) / steps, v = float(iv) / steps;
				verts.insert(verts.end(), { P.x,P.y,P.z, N.x,N.y,N.z, u,v });
			}
		}

		for (int iv = 0; iv < steps; ++iv)
		{
			for (int iu = 0; iu < steps; ++iu)
			{
				unsigned i0 = base + iv * stride + iu;
				unsigned i1 = base + (iv + 1) * stride + iu;
				unsigned i2 = i0 + 1;
				unsigned i3 = i1 + 1;
				idx.insert(idx.end(), { i0,i1,i2,  i2,i1,i3 });
			}
		}
	}



	// Reflect patch across X and/or Y by multiplying coordinates
	static void reflectPatch(V3 src[4][4], V3 dst[4][4], bool flipX, bool flipY)
	{
		const float sx = flipX ? -1.f : 1.f;
		const float sy = flipY ? -1.f : 1.f;
		for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j)
		{
			V3 p = src[i][j];
			dst[i][j] = { p.x * sx, p.y * sy, p.z };
		}
	}

	// Build a patch with reflections: none / X / Y / X+Y
	static void buildPatchReflect(int patchNum,
		const std::vector<float>& B, const std::vector<float>& dB,
		std::vector<float>& verts, std::vector<unsigned int>& idx,
		int grid, float size,
		bool reflectX, bool reflectY)
	{
		V3 patch[4][4], patchRevV[4][4];
		getPatch(patchNum, patch,    /*reverseV=*/false, size);
		getPatch(patchNum, patchRevV,/*reverseV=*/true, size);

		// 0) Base (no reflection)  --> invert normals (matches Kilgard)
		buildOne(patch, B, dB, verts, idx, grid, R_ID, /*invertNormal=*/true);

		// 1) X reflection uses V-reversed control net, normals NOT inverted
		if (reflectX)
		{
			buildOne(patchRevV, B, dB, verts, idx, grid, R_X, /*invertNormal=*/false);
		}

		// 2) Y reflection uses V-reversed control net, normals NOT inverted
		if (reflectY)
		{
			buildOne(patchRevV, B, dB, verts, idx, grid, R_Y, /*invertNormal=*/false);
		}

		// 3) X+Y reflection uses base control net again, normals inverted
		if (reflectX && reflectY)
		{
			buildOne(patch, B, dB, verts, idx, grid, R_XY, /*invertNormal=*/true);
		}
	}


	// Generate all 10 base patches with proper reflections (as in your Teapot.cpp::generatePatches)  // :contentReference[oaicite:9]{index=9}
	static void generateTeapot(std::vector<float>& verts, std::vector<unsigned int>& idx,
		int grid, float size)
	{
		std::vector<float> B, dB;
		computeBasis(B, dB, grid);

		// Rim
		buildPatchReflect(0, B, dB, verts, idx, grid, size, true, true);
		// Body
		buildPatchReflect(1, B, dB, verts, idx, grid, size, true, true);
		buildPatchReflect(2, B, dB, verts, idx, grid, size, true, true);
		// Lid
		buildPatchReflect(3, B, dB, verts, idx, grid, size, true, true);
		buildPatchReflect(4, B, dB, verts, idx, grid, size, true, true);
		// Bottom
		buildPatchReflect(5, B, dB, verts, idx, grid, size, true, true);
		// Handle
		buildPatchReflect(6, B, dB, verts, idx, grid, size, false, true);
		buildPatchReflect(7, B, dB, verts, idx, grid, size, false, true);
		// Spout
		buildPatchReflect(8, B, dB, verts, idx, grid, size, false, true);
		buildPatchReflect(9, B, dB, verts, idx, grid, size, false, true);
	}


	// Optional: move lid (kept for parity; disabled by default)  // :contentReference[oaicite:10]{index=10}
	static void moveLidCPU(std::vector<float>& /*verts*/, int /*grid*/, const float* /*mat4x4*/)
	{
		// No-op; Kilgard closed-lid dataset doesn’t need it for your preview.
		// If you ever want this: transform vertex positions of lid patch range here.
	}

} // namespace


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
	destroyMesh(_cylinder);
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
	initCylinderMesh();
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
	else if (_currentShape == PreviewShape::Cylinder)
		view.translate(0, 0, -4.5f);
	else if (_currentShape == PreviewShape::Plane)	
		view.translate(0, 0, -4.0f);	
	else if (_currentShape == PreviewShape::Teapot)
		view.translate(0, 0, -7.5f);

	glViewport(0, 0, width(), height());
	glClearColor(0.15f, 0.15f, 0.18f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_TRUE);

	glDisable(GL_CULL_FACE);	

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	_shader->bind();

	
	QMatrix4x4 model;
	model.setToIdentity();

		
	model.scale(_zoom);
	model.rotate(_rotX, 1, 0, 0);
	model.rotate(_rotY, 0, 1, 0);

	if (_currentShape == PreviewShape::Teapot)
	{
		model.rotate(-90, 1, 0, 0);
		model.rotate(135, 0, 0, 1);
	}

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
	_shader->setUniformValue("uOpacityMap", 6);
	_shader->setUniformValue("uEmissiveMap", 7);
	_shader->setUniformValue("uSheenColorMap", 8);
	_shader->setUniformValue("uSheenRoughnessMap", 9);
	_shader->setUniformValue("uClearcoatColorMap", 10);
	_shader->setUniformValue("uClearcoatRoughnessMap", 11);
	_shader->setUniformValue("uClearcoatNormalMap", 12);
	_shader->setUniformValue("uIORMap", 13);
	_shader->setUniformValue("uTransmissionMap", 14);

	// Set up texture units
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, _currentMaterial.albedoTextureId());  
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, _currentMaterial.metallicTextureId());
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, _currentMaterial.roughnessTextureId());
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, _currentMaterial.normalTextureId());
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, _currentMaterial.occlusionTextureId());
	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D, _currentMaterial.heightTextureId());   
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, _currentMaterial.opacityTextureId());  
	glActiveTexture(GL_TEXTURE7);
	glBindTexture(GL_TEXTURE_2D, _currentMaterial.emissiveTextureId());
	glActiveTexture(GL_TEXTURE8);
	glBindTexture(GL_TEXTURE_2D, _currentMaterial.sheenColorTextureId());
	glActiveTexture(GL_TEXTURE9);
	glBindTexture(GL_TEXTURE_2D, _currentMaterial.sheenRoughnessTextureId());
	glActiveTexture(GL_TEXTURE10);
	glBindTexture(GL_TEXTURE_2D, _currentMaterial.clearcoatColorTextureId());
	glActiveTexture(GL_TEXTURE11);
	glBindTexture(GL_TEXTURE_2D, _currentMaterial.clearcoatRoughnessTextureId());
	glActiveTexture(GL_TEXTURE12);
	glBindTexture(GL_TEXTURE_2D, _currentMaterial.clearcoatNormalTextureId());
	glActiveTexture(GL_TEXTURE13);
	glBindTexture(GL_TEXTURE_2D, _currentMaterial.iorTextureId());
	glActiveTexture(GL_TEXTURE14);
	glBindTexture(GL_TEXTURE_2D, _currentMaterial.transmissionTextureId());
		
	// Set up simple lighting	
	_shader->setUniformValue("uLights[0].position", QVector3D(0.5f, 0.7f, 0.5f));
	_shader->setUniformValue("uLights[0].color", QVector3D(1.0f, 1.0f, 1.0f));

	_shader->setUniformValue("uLights[1].position", QVector3D(-0.7f, 0.6f, 0.7f));
	_shader->setUniformValue("uLights[1].color", QVector3D(0.6f, 0.6f, 0.6f));

	_shader->setUniformValue("uLights[2].position", QVector3D(-0.7f, -0.8f, 0.6f));
	_shader->setUniformValue("uLights[2].color", QVector3D(0.4f, 0.4f, 0.4f));

	_shader->setUniformValue("uLights[3].position", QVector3D(0.7f, -0.8f, 0.9f));
	_shader->setUniformValue("uLights[3].color", QVector3D(0.5f, 0.5f, 0.5f));

	_shader->setUniformValue("uNumLights", 4);

	const MeshGL* mesh = nullptr;
	switch (_currentShape)
	{
	case PreviewShape::Sphere: mesh = &_sphere; break;
	case PreviewShape::Cube:   mesh = &_cube;   break;
	case PreviewShape::Cylinder: mesh = &_cylinder; break;
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

void MaterialPreviewWidget::initCylinderMesh()
{
	const int SEGMENTS = 64; // smoothness of roundness

	std::vector<float> vertices;
	std::vector<unsigned int> indices;

	// Side vertices
	for (int i = 0; i <= SEGMENTS; ++i)
	{
		float theta = (float)i / SEGMENTS * 2.0f * M_PI;
		float x = std::cos(theta);
		float z = std::sin(theta);

		// top edge
		vertices.push_back(x); vertices.push_back(1.0f); vertices.push_back(z); // pos
		vertices.push_back(x); vertices.push_back(0.0f); vertices.push_back(z); // normal (radial)
		vertices.push_back((float)i / SEGMENTS); vertices.push_back(1.0f);      // texcoord

		// bottom edge
		vertices.push_back(x); vertices.push_back(-1.0f); vertices.push_back(z);
		vertices.push_back(x); vertices.push_back(0.0f); vertices.push_back(z);
		vertices.push_back((float)i / SEGMENTS); vertices.push_back(0.0f);
	}

	// Side indices (quads → two triangles)
	for (int i = 0; i < SEGMENTS; ++i)
	{
		int top0 = i * 2;
		int bot0 = top0 + 1;
		int top1 = top0 + 2;
		int bot1 = top1 + 1;

		indices.push_back(top0);
		indices.push_back(bot0);
		indices.push_back(top1);

		indices.push_back(top1);
		indices.push_back(bot0);
		indices.push_back(bot1);
	}

	// --- Top disk ---
	int centerTopIndex = vertices.size() / 8;
	vertices.insert(vertices.end(),
		{ 0.0f, 1.0f, 0.0f,   0.0f, 1.0f, 0.0f,   0.5f, 0.5f }); // center vertex

	for (int i = 0; i <= SEGMENTS; ++i)
	{
		float theta = (float)i / SEGMENTS * 2.0f * M_PI;
		float x = std::cos(theta);
		float z = std::sin(theta);

		vertices.push_back(x); vertices.push_back(1.0f); vertices.push_back(z);
		vertices.push_back(0.0f); vertices.push_back(1.0f); vertices.push_back(0.0f);
		vertices.push_back(0.5f + 0.5f * x); vertices.push_back(0.5f - 0.5f * z);
	}

	for (int i = 0; i < SEGMENTS; ++i)
	{
		indices.push_back(centerTopIndex);
		indices.push_back(centerTopIndex + i + 1);
		indices.push_back(centerTopIndex + i + 2);
	}

	// --- Bottom disk ---
	int centerBotIndex = vertices.size() / 8;
	vertices.insert(vertices.end(),
		{ 0.0f, -1.0f, 0.0f,   0.0f, -1.0f, 0.0f,   0.5f, 0.5f }); // center vertex

	for (int i = 0; i <= SEGMENTS; ++i)
	{
		float theta = (float)i / SEGMENTS * 2.0f * M_PI;
		float x = std::cos(theta);
		float z = std::sin(theta);

		vertices.push_back(x); vertices.push_back(-1.0f); vertices.push_back(z);
		vertices.push_back(0.0f); vertices.push_back(-1.0f); vertices.push_back(0.0f);
		vertices.push_back(0.5f + 0.5f * x); vertices.push_back(0.5f + 0.5f * z);
	}

	for (int i = 0; i < SEGMENTS; ++i)
	{
		indices.push_back(centerBotIndex);
		indices.push_back(centerBotIndex + i + 2);
		indices.push_back(centerBotIndex + i + 1);
	}

	// --- Upload to GPU ---
	_cylinder.indexCount = (int)indices.size();

	glGenVertexArrays(1, &_cylinder.vao);
	glGenBuffers(1, &_cylinder.vbo);
	glGenBuffers(1, &_cylinder.ebo);

	glBindVertexArray(_cylinder.vao);
	glBindBuffer(GL_ARRAY_BUFFER, _cylinder.vbo);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _cylinder.ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);

	glBindVertexArray(0);
}



void MaterialPreviewWidget::initPlaneMesh()
{
	// Square in XZ at Y≈0, size 2×2
	const float EPS = 1e-4f; // tiny offset to avoid coplanar z-fighting when both faces render

	// interleaved: pos(3), normal(3), uv(2)
	std::vector<float> v;
	std::vector<unsigned int> idx;
	v.reserve(8 * 8);
	idx.reserve(12);

	// ----- TOP face (normal +Y) -----
	// CCW winding seen from above
	auto push = [&](float x, float y, float z, float nx, float ny, float nz, float u, float vv) {
		v.push_back(x); v.push_back(y); v.push_back(z);
		v.push_back(nx); v.push_back(ny); v.push_back(nz);
		v.push_back(u);  v.push_back(vv);
		};

	// positions
	push(-1, +0.0f + EPS, -1, 0, 1, 0, 0, 0);
	push(+1, +0.0f + EPS, -1, 0, 1, 0, 1, 0);
	push(+1, +0.0f + EPS, +1, 0, 1, 0, 1, 1);
	push(-1, +0.0f + EPS, +1, 0, 1, 0, 0, 1);

	idx.insert(idx.end(), { 0,1,2,  0,2,3 });

	// ----- BOTTOM face (normal -Y) -----
	// Also CCW when viewed from below. Slightly offset to avoid z-fighting.
	const unsigned base = 4;

	push(-1, -0.0f - EPS, -1, 0, -1, 0, 0, 0);
	push(-1, -0.0f - EPS, +1, 0, -1, 0, 0, 1);
	push(+1, -0.0f - EPS, +1, 0, -1, 0, 1, 1);
	push(+1, -0.0f - EPS, -1, 0, -1, 0, 1, 0);

	// triangles CCW as seen from below
	idx.insert(idx.end(), { base + 0, base + 1, base + 2,  base + 0, base + 2, base + 3 });

	// ---- upload ----
	_plane.indexCount = static_cast<int>(idx.size());

	glGenVertexArrays(1, &_plane.vao);
	glGenBuffers(1, &_plane.vbo);
	glGenBuffers(1, &_plane.ebo);

	glBindVertexArray(_plane.vao);

	glBindBuffer(GL_ARRAY_BUFFER, _plane.vbo);
	glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _plane.ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int), idx.data(), GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);

	glBindVertexArray(0);
}



void MaterialPreviewWidget::initTeapotMesh()
{
	const int   grid = 12;
	const float size = 1.0f;

	std::vector<float> vertices; vertices.reserve(32 * (grid + 1) * (grid + 1) * 8);
	std::vector<unsigned int> indices; indices.reserve(32 * grid * grid * 6);

	generateTeapot(vertices, indices, grid, size);  // <<— uses logic above

	_teapot.indexCount = (int)indices.size();
	glGenVertexArrays(1, &_teapot.vao);
	glGenBuffers(1, &_teapot.vbo);
	glGenBuffers(1, &_teapot.ebo);

	glBindVertexArray(_teapot.vao);
	glBindBuffer(GL_ARRAY_BUFFER, _teapot.vbo);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _teapot.ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);                glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float))); glEnableVertexAttribArray(2);

	glBindVertexArray(0);
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
		_rotY = 20.0f;
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
