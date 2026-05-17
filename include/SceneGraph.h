#pragma once

#include "SceneNode.h"
#include "GLLights.h"
#include "GltfAnimationData.h"
#include "GltfVariantData.h"

#include <QHash>
#include <QJsonArray>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QDataStream>
#include <assimp/scene.h>

// ---------------------------------------------------------------------------
// SceneGraph
//
// Owns and manages the complete scene node hierarchy.  It is the canonical
// data model for:
//   - the tree widget (reads the hierarchy, connects to structureChanged)
//   - the export path (calls reconstructAsScene() instead of walking
//     _globalScene, which may contain stale / deleted meshes)
//
// Lifetime: owned by ModelViewer.  Neither GLWidget nor any command class
// should hold or delete SceneGraph directly.
//
// Thread safety: all methods must be called from the main (UI) thread.
// ---------------------------------------------------------------------------
class SceneGraph : public QObject
{
    Q_OBJECT

public:
    explicit SceneGraph(QObject* parent = nullptr);
    ~SceneGraph();

    // -----------------------------------------------------------------------
    // Build
    // -----------------------------------------------------------------------

    // Append one imported file's hierarchy to the scene.
    //
    // scene              — the aiScene* returned by AssImpModelLoader::getScene()
    //                      BEFORE it is deep-copied and merged into _globalScene.
    // sourceFile         — absolute path of the file that was loaded.
    // meshUuidsInOrder   — UUIDs of every TriangleMesh created from this file,
    //                      in the same DFS order that
    //                      AssImpModelLoader::processNode() visited them.
    //                      The cursor-based DFS inside buildSubtree() assigns
    //                      these UUIDs to the matching aiNodes automatically.
    // lights             — optional punctual lights from KHR_lights_punctual extension.
    void appendFromScene(const aiScene*                   scene,
                         const QString&                   sourceFile,
                         const QList<QUuid>&              meshUuidsInOrder,
                         const std::vector<GPULight>&    lights = {});

    // Build a flat synthetic session node that owns all meshes directly.
    // Used as a fallback for native-session files that do not carry a full
    // hierarchy, or when loading legacy MVF files.
    void rebuildFlat(const QString& sessionName,
                     const QList<QUuid>& meshUuids);

    // Rebuild the hierarchy from an MVF3 document's flat node array.
    // documentNodes  — the parsed document.nodes QJsonArray.
    // sceneRootNodes — the scene's root-level node index array (scene["nodes"]).
    void rebuildFromMvf(const QJsonArray& documentNodes,
                        const QJsonArray& sceneRootNodes);

    // Reset to an empty graph (e.g. on "New scene").
    void clear();

    // Session persistence
    void serialize(QDataStream& out) const;
    bool deserialize(QDataStream& in);

    // -----------------------------------------------------------------------
    // Query
    // -----------------------------------------------------------------------

    // The invisible root node.  Its children are the per-file synthetic nodes.
    // Do not display this node in the tree widget — use root()->children as
    // the top-level items.
    SceneNode* root() const { return _root; }

    // Return the SceneNode that currently owns meshUuid, or nullptr if the
    // mesh has been removed (moved to recycle bin) or does not exist.
    SceneNode* findNodeForMesh(const QUuid& meshUuid) const;

    // Recursively collect all mesh UUIDs under node, including descendants.
    // The order follows a DFS pre-order traversal (node's own meshUuids first,
    // then children left-to-right), which matches the original load order.
    QList<QUuid> collectMeshUuids(const SceneNode* node) const;

    // Get all punctual lights in the scene.
    const std::vector<GPULight>& lights() const { return _lights; }

    // Set lights in the scene (called during import).
    void setLights(const std::vector<GPULight>& lights) { _lights = lights; }

    SceneNode* findFileNode(const QString& sourceFile) const;

    // -----------------------------------------------------------------------
    // KHR_materials_variants
    // -----------------------------------------------------------------------

    // Register variant data for a source file (called after import succeeds).
    void setVariantData(const QString& sourceFile, const GltfVariantData& data);

    // Remove variant data for a source file.
    void clearVariantData(const QString& sourceFile);

    // Returns the variant data for a specific source file, or an empty struct.
    GltfVariantData variantDataForFile(const QString& sourceFile) const;

    // Returns all source files that carry KHR_materials_variants data.
    QStringList filesWithVariants() const;

    // Track and retrieve the currently active variant per source file.
    // -1 = file default (no variant active).
    void setActiveVariant(const QString& sourceFile, int variantIndex);
    int  activeVariantForFile(const QString& sourceFile) const;

    // -----------------------------------------------------------------------
    // glTF animations
    // -----------------------------------------------------------------------
    void setAnimationData(const QString& sourceFile, const GltfAnimationData& data);
    void clearAnimationData(const QString& sourceFile);
    GltfAnimationData animationDataForFile(const QString& sourceFile) const;
    QStringList filesWithAnimations() const;
    void setActiveAnimationClip(const QString& sourceFile, int clipIndex);
    int activeAnimationClipForFile(const QString& sourceFile) const;

    // -----------------------------------------------------------------------
    // Mutation  (called by undo/redo command classes)
    // -----------------------------------------------------------------------

    // Remove meshUuid from its owning node and deregister it from the lookup
    // table.  The node itself is NOT deleted even if it becomes empty, so that
    // undo (restoreMeshUuid) can safely put it back without needing to
    // reconstruct the node.
    //
    // outPosition — receives the index the UUID held in node->meshUuids so
    //               the caller can pass it back to restoreMeshUuid for an
    //               exact restoration.
    //
    // Returns the owning SceneNode (caller must store it for undo), or nullptr
    // if the UUID was not found.
    SceneNode* removeMeshUuid(const QUuid& meshUuid, int& outPosition);

    // Re-insert meshUuid into node->meshUuids at position and re-register it
    // in the lookup table.  position is clamped to a valid range automatically
    // in case sibling insertions/removals shifted the list since the removal.
    void restoreMeshUuid(SceneNode* node, const QUuid& meshUuid, int position);

    // Attach newChild under parent at position (appends if position is out of
    // range).  Sets newChild->parent and registers all descendant mesh UUIDs.
    // SceneGraph takes ownership of newChild.
    void insertChildNode(SceneNode* parent, SceneNode* newChild, int position = -1);

    // Detach child from parent->children.  Does NOT free child — the caller
    // (typically PasteCommand) holds ownership until redo re-attaches it.
    // Deregisters all descendant mesh UUIDs.  outPosition receives the index
    // the child was at so redo can restore it exactly.
    void removeChildNode(SceneNode* parent, SceneNode* child, int& outPosition);

    // Return the SceneNode whose nodeUuid matches, or nullptr.  O(n) DFS.
    SceneNode* findNodeByUuid(const QUuid& nodeUuid) const;

    // Delete a subtree that is no longer attached to this SceneGraph.
    // Used by PasteCommand destructor to free cloned nodes when the command
    // is destroyed in the undone state.  Does NOT touch _meshUuidToNode.
    static void deleteDetachedSubtree(SceneNode* root);

signals:
    // Emitted after any structural change (append, clear, remove, restore).
    // The tree widget connects to this to rebuild or refresh its items.
    void structureChanged();

    // Emitted when variant data is added or removed (a file with
    // KHR_materials_variants is loaded or its meshes are cleared).
    // The MaterialVariantsPanel connects to this to refresh its tree.
    void variantDataChanged();
    void animationDataChanged();

private:
    // Recursively build a SceneNode subtree that mirrors ainode and its
    // descendants.  cursor advances through uuids as mesh UUIDs are assigned
    // to nodes — the traversal order must match processNode() exactly.
    SceneNode* buildSubtree(const aiNode*       ainode,
                            SceneNode*          parent,
                            const QList<QUuid>& uuids,
                            int&                cursor);

    void collectUuidsRecursive(const SceneNode* node, QList<QUuid>& out) const;

    // Register / deregister every mesh UUID in a subtree in _meshUuidToNode.
    void registerSubtreeUuids(SceneNode* node);
    void deregisterSubtreeUuids(SceneNode* node);

    // Delete node and all of its descendants.  Does not touch _meshUuidToNode;
    // callers are responsible for clearing the hash before calling this.
    void freeSubtree(SceneNode* node);

    // Invisible root — never shown in the UI.
    SceneNode* _root = nullptr;

    // Fast O(1) lookup from mesh UUID to the SceneNode that owns it.
    // Entries are added in appendFromScene / restoreMeshUuid and removed in
    // removeMeshUuid / clear.
    QHash<QUuid, SceneNode*> _meshUuidToNode;

    // Punctual lights from KHR_lights_punctual extension.
    std::vector<GPULight> _lights;

    // KHR_materials_variants: one entry per source file that carries the extension.
    QHash<QString, GltfVariantData> _variantDataByFile;

    // Currently active variant index per source file (-1 = file default).
    QHash<QString, int> _activeVariantByFile;
    QHash<QString, GltfAnimationData> _animationDataByFile;
    QHash<QString, int> _activeAnimationClipByFile;
};
