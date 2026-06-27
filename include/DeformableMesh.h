#pragma once

#include "TriangleMesh.h"

// ---------------------------------------------------------------------------
// DeformableMesh
//
// Subclass of TriangleMesh that carries the additional static geometry data
// required for skeletal skinning and morph-target animation:
//
//   _vertices / _baseVertices   — interleaved Vertex array (AssImpMesh path);
//                                  _baseVertices is the unmodified morph base.
//   _morphTargets               — per-vertex delta arrays (static after load)
//   _defaultMorphWeights        — default blend weights matching each target
//
// AssImpMesh inherits from DeformableMesh and provides the concrete
// render() / applyMorphWeights() / resetMorphTargets() implementations.
//
// TriangleMesh retains the virtual interface for all morph/skin queries with
// safe no-op defaults so that non-deformable TriangleMesh instances continue
// to compile and link without change.
//
// Introduced in Phase 12a of the mesh/render/runtime separation refactor.
// Phase 12b will convert AssImpMesh into a factory (AssImpMeshBuilder) that
// produces DeformableMesh instances directly.
// ---------------------------------------------------------------------------
class DeformableMesh : public TriangleMesh
{
public:
    using TriangleMesh::TriangleMesh;   // inherit all constructors

    // ---- Morph-target accessors (override TriangleMesh no-op defaults) ----
    bool hasMorphTargets() const override { return !_morphTargets.isEmpty(); }

    QVector<float> defaultMorphWeights() const override { return _defaultMorphWeights; }

    const QVector<MorphTargetData>& getMorphTargets() const override { return _morphTargets; }

protected:
    // ---- Interleaved CPU geometry (AssImpMesh path) -------------------------
    // _vertices is the working copy modified by applyMorphWeights().
    // _baseVertices is the unmodified snapshot used as the morph-delta base.
    // Both are split into separate float arrays by initBuffers() for the GPU.
    std::vector<Vertex> _vertices;
    std::vector<Vertex> _baseVertices;

    // ---- Morph-target data (geometry asset, static after load) -------------
    QVector<MorphTargetData> _morphTargets;
    QVector<float>           _defaultMorphWeights;
};
