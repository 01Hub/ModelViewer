#pragma once
#include <QOpenGLWidget>
#include <QOpenGLFunctions_4_5_Core>
#include <QOpenGLShaderProgram>
#include <QMatrix4x4>
#include "GLMaterial.h"

class MaterialPreviewWidget : public QOpenGLWidget, protected QOpenGLFunctions_4_5_Core
{
    Q_OBJECT
public:
    explicit MaterialPreviewWidget(QWidget *parent = nullptr);
    ~MaterialPreviewWidget();

    void setMaterial(const GLMaterial &mat);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    void initSphereMesh();
    void updateShaderFromMaterial(const GLMaterial &mat);

private:
    QOpenGLShaderProgram shader;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;

    int indexCount = 0;

    QMatrix4x4 proj;
    QMatrix4x4 view;

    GLMaterial currentMaterial;
};
