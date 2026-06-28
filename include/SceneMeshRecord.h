#pragma once

#include <QUuid>

class SceneMesh;
// ---------------------------------------------------------------------------
// SceneMeshRecord
//
// One slot in the scene mesh store (_meshStore in SceneRuntime / GLWidget).
// Pairs a raw mesh pointer with its stable QUuid so callers can correlate
// meshes without extra lookups.
//
// Introduced in Phase 7 of the mesh/render/runtime separation refactor when
// _meshStore was changed from std::vector<SceneMesh*> to
// std::vector<SceneMeshRecord>.
//
// Compatibility shims:
//   operator->()       — _meshStore[i]->method() compiles unchanged
//   operator SceneMesh*() — SceneMesh* p = _meshStore[i] and
//                              range-for (SceneMesh* m : _meshStore) work
//                              unchanged; null-pointer conditions (if
//                              (_meshStore[i])) work via pointer bool.
//
// These will be removed once all call sites have been migrated to use the
// record fields directly (Phase 12 clean-up).
// ---------------------------------------------------------------------------
struct SceneMeshRecord
{
    SceneMesh* mesh = nullptr;
    QUuid         uuid;

    SceneMeshRecord() = default;
    SceneMeshRecord(SceneMesh* m, const QUuid& id) : mesh(m), uuid(id) {}

    // ---- Compatibility shims -----------------------------------------------
    SceneMesh* operator->()      const { return mesh; }
    operator SceneMesh*()        const { return mesh; }
    // ------------------------------------------------------------------------
};
