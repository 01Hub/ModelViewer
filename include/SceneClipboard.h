#pragma once

#include <QList>
#include <QString>
#include <QUuid>
#include <assimp/matrix4x4.h>

// ---------------------------------------------------------------------------
// ClipboardNode
//
// Lightweight, pointer-free snapshot of one SceneNode for clipboard storage.
// Holds original mesh UUIDs (sources to clone from at paste time) and
// recursively mirrors the subtree structure.  Safe to keep across scene
// modifications because it holds no live pointers.
// ---------------------------------------------------------------------------
struct ClipboardNode
{
    QString      name;
    aiMatrix4x4  localTransform;
    QList<QUuid> meshUuids;         // original UUIDs — cloned at paste time
    QList<ClipboardNode> children;
};

// ---------------------------------------------------------------------------
// ClipboardEntry
//
// One top-level item placed on the clipboard by a Copy action.
//   isLeaf == true  → user copied a single mesh leaf; leafUuid is valid.
//   isLeaf == false → user copied an assembly; assemblyRoot is valid.
// ---------------------------------------------------------------------------
struct ClipboardEntry
{
    bool isLeaf = false;

    // Valid when isLeaf == true
    QUuid leafUuid;

    // Valid when isLeaf == false
    ClipboardNode assemblyRoot;
};
