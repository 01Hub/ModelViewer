#pragma once

#include "BoundingBox.h"
#include "GltfLightData.h"
#include "SceneMeshRecord.h"
#include "RenderableMesh.h"

#include <QDateTime>
#include <QList>
#include <QMap>
#include <QUuid>
#include <QVector>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <assimp/scene.h>

struct SceneNode;

// ---------------------------------------------------------------------------
// TextureSamplerSettings
// Sampler parameters used when uploading or looking up a cached texture.
// ---------------------------------------------------------------------------
struct TextureSamplerSettings
{
	GLenum wrapS      = GL_REPEAT;
	GLenum wrapT      = GL_REPEAT;
	GLenum minFilter  = GL_LINEAR_MIPMAP_LINEAR;
	GLenum magFilter  = GL_LINEAR;

	bool operator==(const TextureSamplerSettings& other) const
	{
		return wrapS      == other.wrapS   &&
		       wrapT      == other.wrapT   &&
		       minFilter  == other.minFilter &&
		       magFilter  == other.magFilter;
	}
};

// ---------------------------------------------------------------------------
// CachedTextureEntry
// One entry in the GLWidget texture cache (path → GPU texture).
// ---------------------------------------------------------------------------
struct CachedTextureEntry
{
	QImage  image;
	int     imageWidth      = 0;
	int     imageHeight     = 0;
	int     imageComponents = 0;
	GLenum  imageFormat     = GL_RGBA;

	GLuint                lastGPUTexture   = 0;
	TextureSamplerSettings lastSamplerSettings;
	int                   refCount         = 0;
};

// ---------------------------------------------------------------------------
// RuntimeVisibilityNode
// One node in the per-frame BVH used for frustum + visibility culling.
// ---------------------------------------------------------------------------
struct RuntimeVisibilityNode
{
	const SceneNode* sceneNode = nullptr;
	QVector<int>     children;
	QVector<int>     meshIndices;
	BoundingBox      subtreeBounds;
	bool             subtreeHasVisibleMeshes = false;
};

// ---------------------------------------------------------------------------
// SceneRuntime
//
// Groups the core scene-data fields that logically belong to a "scene
// runtime" rather than to GLWidget's rendering or interaction concerns.
// Extracted from GLWidget in Phase 5 of the mesh/render/runtime separation
// refactor.  GLWidget embeds one instance and aliases every field via
// const-ref / ref members so all existing call sites in GLWidget.cpp remain
// unchanged.
//
// All fields are public; encapsulation is deferred to a later phase when
// SceneRuntime acquires its own methods and its own .cpp translation unit.
// ---------------------------------------------------------------------------
class SceneRuntime
{
public:
	// ---- Mesh store ----
	std::vector<SceneMeshRecord> _meshStore;
	std::vector<int>           _displayedObjectsIds;
	std::vector<int>           _hiddenObjectsIds;

	// ---- Recycle bin ----
	struct RecycleBinEntry
	{
		TriangleMesh* mesh;
		int           originalIndex;
		QDateTime     deletedAt;
	};
	QMap<QUuid, RecycleBinEntry> _recycleBin;

	// ---- Texture cache ----
	std::unordered_map<QString, CachedTextureEntry> _texCache;
	std::unordered_map<unsigned int, int>           _texRefCount;

	// ---- Runtime visibility BVH ----
	QVector<RuntimeVisibilityNode> _runtimeVisibilityNodes;
	int    _runtimeVisibilityRootIndex              = -1;
	bool   _runtimeVisibilityHierarchyDirty         = true;
	int    _runtimeVisibilityMeshStoreCount         = -1;
	bool   _runtimeVisibilityPrepared               = false;
	quint64 _runtimeVisibilityBoundsRevision        = 0;
	quint64 _runtimeVisibilityMaskRevision          = 1;
	quint64 _runtimeVisibilityMaskProcessedRevision = 0;
	std::vector<unsigned char> _runtimeBaseVisibleMask;

	// ---- Assimp scene ----
	const aiScene* _assimpScene        = nullptr;
	aiScene*       _globalScene        = nullptr;
	glm::mat4      _globalSceneTransform{ 1.0f };

	// ---- Load lifecycle ----
	QList<QUuid>     _pendingSceneUuids;
	std::vector<int> _centerScreenObjectIDs;
	bool _visibleSwapped            = false;
	bool _progressiveLoadingEnabled = false;
	bool _cancelRequested           = false;
	bool _loadCancelled             = false;

	// ---- Light data ----
	GltfLightData _pendingLightData;
};
