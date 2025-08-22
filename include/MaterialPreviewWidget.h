#pragma once
#include <QOpenGLWidget>
#include <QOpenGLFunctions_4_5_Core>
#include <QMatrix4x4>
#include <QElapsedTimer>
#include <QTimer>
#include "GLMaterial.h"
#include "ShaderProgram.h"

#include <QDateTime>

class QMouseEvent;
class QWheelEvent;

enum class PreviewShape { Sphere, Cube, Cylinder, Plane, Teapot };

struct GpuTexCache
{
    GLuint id = 0;
    QString lastPath;
    QDateTime lastModified;   // local time or UTC; stay consistent
    qint64 lastSize = -1;
};

class MaterialPreviewWidget : public QOpenGLWidget, protected QOpenGLFunctions_4_5_Core
{
    Q_OBJECT

    struct MeshGL
    {
        GLuint vao = 0, vbo = 0, ebo = 0;
        int indexCount = 0;
    };

public:
    explicit MaterialPreviewWidget(QWidget *parent = nullptr);
    ~MaterialPreviewWidget();

    void setMaterial(const GLMaterial &mat);
    
    void setPreviewShape(PreviewShape s);

	PreviewShape currentShape() const { return _currentShape; }

    void setPreviewRotation(float pitchDeg, float yawDeg);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e);
    void wheelEvent(QWheelEvent* e);

    void showEvent(QShowEvent* e) override;

private:
    void initSphereMesh();    
    void initCubeMesh();
    void initCylinderMesh();
    void initPlaneMesh();     
    void initTeapotMesh();    

    void destroyMesh(MeshGL& m);

    void startOverlayHint(int showMs = 3000, int fadeMs = 1500);

    void startSpin(float velPitchDegPerSec, float velYawDegPerSec);
    void stopSpin();

    bool shouldReload(const QString& path, const GpuTexCache& cache);

    void syncTextureFromMaterial(
        GpuTexCache& cache,
        const QString& path,
        int texUnit,
        const char* uniformSamplerName,
        const char* uniformUseName,
        bool srgb);
	void syncAllTexturesFromMaterial();

private:
    std::unique_ptr<ShaderProgram> _shader;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;

    int indexCount = 0;

    QMatrix4x4 proj;
    QMatrix4x4 view;

    GLMaterial _currentMaterial = GLMaterial::METAL_ALUMINUM();
    
    GpuTexCache _albedoTex;
	GpuTexCache _metallicTex;
	GpuTexCache _roughnessTex;
	GpuTexCache _normalTex;
	GpuTexCache _aoTex;
	GpuTexCache _opacityTex;
	GpuTexCache _emissiveTex;
	GpuTexCache _heightTex;
	GpuTexCache _sheenColorTex;
	GpuTexCache _sheenRoughnessTex;
	GpuTexCache _clearcoatColorTex;
	GpuTexCache _clearcoatRoughnessTex;
	GpuTexCache _clearcoatNormalTex;
	GpuTexCache _transmissionTex;
	GpuTexCache _iorTex;

    MeshGL _sphere{};
    MeshGL _cube{};
    MeshGL _cylinder{};
    MeshGL _plane{};
    MeshGL _teapot{};

    PreviewShape _currentShape = PreviewShape::Sphere;
        
    float _rotX = 25.0f;   // pitch  (about X)
    float _rotY = 20.0f;   // yaw    (about Y)
    float _rotSpeed = 0.3f; // degrees per pixel

    bool _dragging = false;
    bool _rightDragging = false;
    QPoint _dragDelta;

    QPoint _lastPos;

    float _zoom = 1.0f;          // scale factor
    float _minZoom = 0.5f;       // -50%
    float _maxZoom = 1.5f;       // +50%
    float _zoomSpeed = 0.002f;    // zoom per pixel dragged vertically

    bool _overlayActive = false;
    QElapsedTimer _overlayTimer;
    QTimer _overlayUpdateTimer;
    int _overlayShowMs = 3000;     // full opacity duration
    int _overlayFadeMs = 1500;     // fade duration

    // Spin (inertia) state
    QTimer _spinTimer;
    QElapsedTimer _spinClock;   // for dt
    float _spinVelX = 0.0f;     // deg/sec (pitch velocity)
    float _spinVelY = 0.0f;     // deg/sec (yaw velocity)
    float _spinDamping = 2.0f;  // per-second damping (>0). Bigger = stops quicker
    float _spinMinSpeed = 5.0f; // deg/sec threshold to stop
};
