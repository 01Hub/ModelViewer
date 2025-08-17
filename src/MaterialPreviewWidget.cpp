#include "MaterialPreviewWidget.h"
#include <vector>
#include <cmath>

MaterialPreviewWidget::MaterialPreviewWidget(QWidget *parent)
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

void MaterialPreviewWidget::setMaterial(const GLMaterial &mat)
{
    currentMaterial = mat;
    update();
}

void MaterialPreviewWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);

    shader.addShaderFromSourceCode(QOpenGLShader::Vertex, R"(
        #version 450 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec3 aNormal;

        uniform mat4 uMVP;
        uniform mat4 uModel;
        uniform mat3 uNormalMatrix;

        out vec3 vNormal;
        out vec3 vPos;

        void main() {
            vPos = vec3(uModel * vec4(aPos,1.0));
            vNormal = normalize(uNormalMatrix * aNormal);
            gl_Position = uMVP * vec4(aPos,1.0);
        }
    )");

    shader.addShaderFromSourceCode(QOpenGLShader::Fragment, R"(
        #version 450 core
        in vec3 vNormal;
        in vec3 vPos;
        out vec4 FragColor;

        uniform vec3 uCamPos;
        uniform vec3 uAlbedo;
        uniform float uMetalness;
        uniform float uRoughness;
        uniform float uOpacity;

        void main() {
            vec3 N = normalize(vNormal);
            vec3 L = normalize(vec3(0.3,0.6,0.8));
            vec3 V = normalize(uCamPos - vPos);

            float NdotL = max(dot(N,L),0.0);
            vec3 diffuse = uAlbedo * NdotL;

            vec3 H = normalize(L+V);
            float NdotH = max(dot(N,H),0.0);
            float shininess = mix(8.0, 128.0, 1.0 - uRoughness);
            float spec = pow(NdotH, shininess);
            vec3 specular = mix(vec3(0.04), uAlbedo, uMetalness) * spec;

            vec3 color = diffuse + specular;
            FragColor = vec4(color, uOpacity);
        }
    )");

    shader.link();
    initSphereMesh();
}

void MaterialPreviewWidget::resizeGL(int w, int h)
{
    proj.setToIdentity();
    proj.perspective(45.0f, float(w)/float(h), 0.1f, 10.0f);

    view.setToIdentity();
    view.translate(0,0,-3.0f);
}

void MaterialPreviewWidget::paintGL()
{
    glClearColor(0.15f,0.15f,0.18f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shader.bind();

    QMatrix4x4 model;
    model.rotate(25, 1,0,0);
    model.rotate(30, 0,1,0);

    QMatrix4x4 mvp = proj * view * model;
    QMatrix3x3 normalMat = model.normalMatrix();

    shader.setUniformValue("uMVP", mvp);
    shader.setUniformValue("uModel", model);
    shader.setUniformValue("uNormalMatrix", normalMat);
    shader.setUniformValue("uCamPos", QVector3D(0,0,3));

    shader.setUniformValue("uAlbedo", currentMaterial.albedoColor());
    shader.setUniformValue("uMetalness", currentMaterial.metalness());
    shader.setUniformValue("uRoughness", currentMaterial.roughness());
    shader.setUniformValue("uOpacity", currentMaterial.opacity());

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    shader.release();
}

void MaterialPreviewWidget::initSphereMesh()
{
    const int X_SEGMENTS = 32;
    const int Y_SEGMENTS = 16;

    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    for (int y = 0; y <= Y_SEGMENTS; ++y) {
        for (int x = 0; x <= X_SEGMENTS; ++x) {
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
        }
    }

    for (int y = 0; y < Y_SEGMENTS; ++y) {
        for (int x = 0; x < X_SEGMENTS; ++x) {
            int i0 = y * (X_SEGMENTS+1) + x;
            int i1 = i0 + X_SEGMENTS + 1;
            indices.push_back(i0);
            indices.push_back(i1);
            indices.push_back(i0+1);

            indices.push_back(i0+1);
            indices.push_back(i1);
            indices.push_back(i1+1);
        }
    }

    indexCount = (int)indices.size();

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size()*sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size()*sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}
