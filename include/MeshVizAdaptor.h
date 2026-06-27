#pragma once

#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QHash>
#include <QByteArray>
#include <QImage>
#include <QMap>
#include <QVariant>

#include <vector>

// Forward declaration — only a pointer is stored, no full definition needed here.
class QOpenGLShaderProgram;

// GL resource container for a TriangleMesh.
//
// Owns all per-mesh OpenGL state: vertex/index buffers, VAO, fallback texture,
// per-frame dirty flags, the uniform-location cache, and debug override maps.
//
// TriangleMesh embeds one of these as _vizState and exposes each field through
// a reference alias (same zero-churn pattern used for GLMaterial& _material).
// All existing call sites in TriangleMesh.cpp, AssImpMesh.cpp, FloorPlane.cpp,
// and ViewCubeMesh.cpp continue to compile unchanged.
//
// Intentionally free of geometry, material, transform, and import provenance.
// The draw methods and shader-program knowledge remain in TriangleMesh/AssImpMesh
// until Phase 4, when they migrate here.
class MeshVizAdaptor
{
public:
    MeshVizAdaptor() = default;

    // MeshVizAdaptor is non-copyable and non-movable because it contains
    // QObject-derived members (QOpenGLVertexArrayObject) and QOpenGLBuffer
    // objects whose ownership must not be transferred implicitly.
    MeshVizAdaptor(const MeshVizAdaptor&)            = delete;
    MeshVizAdaptor& operator=(const MeshVizAdaptor&) = delete;
    MeshVizAdaptor(MeshVizAdaptor&&)                 = delete;
    MeshVizAdaptor& operator=(MeshVizAdaptor&&)      = delete;

    // ---- Vertex / index buffers -----------------------------------------
    QOpenGLBuffer& indexBuffer()       { return _indexBuffer; }
    QOpenGLBuffer& positionBuffer()    { return _positionBuffer; }
    QOpenGLBuffer& normalBuffer()      { return _normalBuffer; }
    QOpenGLBuffer& colorBuffer()       { return _colorBuffer; }
    QOpenGLBuffer& texCoord0Buffer()   { return _texCoord0Buffer; }
    QOpenGLBuffer& texCoord1Buffer()   { return _texCoord1Buffer; }
    QOpenGLBuffer& texCoord2Buffer()   { return _texCoord2Buffer; }
    QOpenGLBuffer& texCoord3Buffer()   { return _texCoord3Buffer; }
    QOpenGLBuffer& tangentBuffer()     { return _tangentBuf; }
    QOpenGLBuffer& bitangentBuffer()   { return _bitangentBuf; }
    QOpenGLBuffer& jointIndexBuffer()  { return _jointIndexBuffer; }
    QOpenGLBuffer& jointWeightBuffer() { return _jointWeightBuffer; }
    QOpenGLBuffer& coordBuffer()       { return _coordBuf; }

    // ---- VAO & buffer ownership list ------------------------------------
    QOpenGLVertexArrayObject& vao()          { return _vertexArrayObject; }
    std::vector<QOpenGLBuffer>& buffers()    { return _buffers; }

    // ---- Vertex count ---------------------------------------------------
    unsigned int& nVerts() { return _nVerts; }

    // ---- Fallback texture -----------------------------------------------
    // A 1×1 placeholder always bound on unit 0 so the shader never samples
    // an unbound unit.  Some render paths upload image data when dirty.
    unsigned int& fallbackTexture()       { return _fallbackTexture; }
    QImage& fallbackTextureImage()        { return _fallbackTextureImage; }
    QImage& fallbackTextureBuffer()       { return _fallbackTextureBuffer; }

    // ---- Per-frame dirty flags ------------------------------------------
    bool& textureBindingsDirty() { return _textureBindingsDirty; }
    bool& uniformsDirty()        { return _uniformsDirty; }

    // ---- Uniform location cache -----------------------------------------
    // Accessed from const TriangleMesh methods via the reference alias;
    // references are transparent to const so no mutable annotation is needed.
    QHash<QByteArray, int>& uniformLocationCache()  { return _uniformLocationCache; }
    QOpenGLShaderProgram*&  vaoConfiguredProgram()  { return _vaoConfiguredProgram; }

    // ---- Debug override maps --------------------------------------------
    QMap<int, unsigned int>&    debugTextureOverrides()  { return _debugTextureOverrides; }
    QMap<QString, QVariant>&    debugUniformOverrides()  { return _debugUniformOverrides; }

private:
    QOpenGLBuffer _indexBuffer;
    QOpenGLBuffer _positionBuffer;
    QOpenGLBuffer _normalBuffer;
    QOpenGLBuffer _colorBuffer;
    QOpenGLBuffer _texCoord0Buffer;
    QOpenGLBuffer _texCoord1Buffer;
    QOpenGLBuffer _texCoord2Buffer;
    QOpenGLBuffer _texCoord3Buffer;
    QOpenGLBuffer _tangentBuf;
    QOpenGLBuffer _bitangentBuf;
    QOpenGLBuffer _jointIndexBuffer;
    QOpenGLBuffer _jointWeightBuffer;
    QOpenGLBuffer _coordBuf;

    QOpenGLVertexArrayObject   _vertexArrayObject;
    std::vector<QOpenGLBuffer> _buffers;
    unsigned int               _nVerts = 0;

    QImage       _fallbackTextureImage;
    QImage       _fallbackTextureBuffer;
    unsigned int _fallbackTexture = 0;

    bool _textureBindingsDirty = true;
    bool _uniformsDirty        = true;

    QHash<QByteArray, int>  _uniformLocationCache;
    QOpenGLShaderProgram*   _vaoConfiguredProgram = nullptr;

    QMap<int, unsigned int>  _debugTextureOverrides;
    QMap<QString, QVariant>  _debugUniformOverrides;
};
