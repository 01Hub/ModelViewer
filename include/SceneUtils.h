#pragma once

#include <assimp/postprocess.h>
#include <assimp/scene.h>

class SceneUtils
{

public:
	static void mergeScene(aiScene** globalScene, aiScene* source);
	static aiScene* deepCopyScene(const aiScene* source);
	static aiMaterial* copyMaterial(const aiMaterial* src);
	static void deleteScene(aiScene* scene);
	static void deleteNodeRecursive(aiNode* node);
};