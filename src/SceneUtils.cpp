#include "SceneUtils.h"

#include <iostream>
#include <algorithm>
#include <functional>

void SceneUtils::mergeScene(aiScene** globalScene, aiScene* source)
{
	if (!source || !source->mRootNode)
		return;

	if (!(*globalScene))
	{
		(*globalScene) = source;
		return;
	}

	// --- Merge meshes ---
	unsigned int oldMeshCount = (*globalScene)->mNumMeshes;
	unsigned int newMeshCount = source->mNumMeshes;
	aiMesh** mergedMeshes = new aiMesh * [oldMeshCount + newMeshCount];
	std::copy((*globalScene)->mMeshes, (*globalScene)->mMeshes + oldMeshCount, mergedMeshes);
	std::copy(source->mMeshes, source->mMeshes + newMeshCount, mergedMeshes + oldMeshCount);
	delete[] (*globalScene)->mMeshes;
	(*globalScene)->mMeshes = mergedMeshes;
	(*globalScene)->mNumMeshes += newMeshCount;

	// --- Merge materials ---
	unsigned int oldMatCount = (*globalScene)->mNumMaterials;
	unsigned int newMatCount = source->mNumMaterials;
	aiMaterial** mergedMats = new aiMaterial * [oldMatCount + newMatCount];
	std::copy((*globalScene)->mMaterials, (*globalScene)->mMaterials + oldMatCount, mergedMats);
	std::copy(source->mMaterials, source->mMaterials + newMatCount, mergedMats + oldMatCount);
	delete[] (*globalScene)->mMaterials;
	(*globalScene)->mMaterials = mergedMats;
	(*globalScene)->mNumMaterials += newMatCount;

	// --- Remap mesh indices in source nodes ---
	std::function<void(aiNode*)> remapMeshIndices = [&](aiNode* node) {
		for (unsigned int i = 0; i < node->mNumMeshes; ++i)
			node->mMeshes[i] += oldMeshCount;
		for (unsigned int i = 0; i < node->mNumChildren; ++i)
			remapMeshIndices(node->mChildren[i]);
		};
	remapMeshIndices(source->mRootNode);

	// --- Merge root nodes into a new one ---
	aiNode* newRoot = new aiNode("MergedRoot");

	// Add old and new roots as children
	newRoot->mChildren = new aiNode * [2];
	newRoot->mChildren[0] = (*globalScene)->mRootNode;
	newRoot->mChildren[1] = source->mRootNode;
	newRoot->mNumChildren = 2;

	// Assign the new root
	(*globalScene)->mRootNode = newRoot;
}

#ifdef _WIN32
#include <windows.h>

// Safe memory copy function with exception handling
bool SafeCopyTextureData(aiTexel* dest, const aiTexel* src, size_t byteSize)
{
	__try
	{
		std::memcpy(dest, src, byteSize);
		return true;
	}
	__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
		EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
		printf("SafeCopyTextureData: Access violation caught - invalid source data at %p\n", src);
		return false;
	}
}
#else
// For non-Windows platforms
bool SafeCopyTextureData(aiTexel* dest, const aiTexel* src, size_t byteSize)
{
	if (!dest || !src) return false;
	try
	{
		std::memcpy(dest, src, byteSize);
		return true;
	}
	catch (...)
	{
		printf("SafeCopyTextureData: Access violation caught - invalid source data at %p\n", src);
		return false;
	}
}
#endif

aiScene* SceneUtils::deepCopyScene(const aiScene* source)
{
	if (!source) return nullptr;

	aiScene* copy = new aiScene();

	// Copy flags
	copy->mFlags = source->mFlags;

	// ---- Copy Meshes ----
	copy->mNumMeshes = source->mNumMeshes;
	if (copy->mNumMeshes > 0)
	{
		copy->mMeshes = new aiMesh * [copy->mNumMeshes];
		for (unsigned int i = 0; i < copy->mNumMeshes; ++i)
		{
			const aiMesh* srcMesh = source->mMeshes[i];
			aiMesh* dstMesh = new aiMesh();

			dstMesh->mPrimitiveTypes = srcMesh->mPrimitiveTypes;
			dstMesh->mNumVertices = srcMesh->mNumVertices;
			dstMesh->mNumFaces = srcMesh->mNumFaces;
			dstMesh->mMaterialIndex = srcMesh->mMaterialIndex;
			dstMesh->mName = srcMesh->mName;

			// Copy vertex attributes
			if (srcMesh->mVertices)
			{
				dstMesh->mVertices = new aiVector3D[dstMesh->mNumVertices];
				std::memcpy(dstMesh->mVertices, srcMesh->mVertices, sizeof(aiVector3D) * dstMesh->mNumVertices);
			}

			if (srcMesh->mNormals)
			{
				dstMesh->mNormals = new aiVector3D[dstMesh->mNumVertices];
				std::memcpy(dstMesh->mNormals, srcMesh->mNormals, sizeof(aiVector3D) * dstMesh->mNumVertices);
			}

			if (srcMesh->mTangents)
			{
				dstMesh->mTangents = new aiVector3D[dstMesh->mNumVertices];
				std::memcpy(dstMesh->mTangents, srcMesh->mTangents, sizeof(aiVector3D) * dstMesh->mNumVertices);
			}

			if (srcMesh->mBitangents)
			{
				dstMesh->mBitangents = new aiVector3D[dstMesh->mNumVertices];
				std::memcpy(dstMesh->mBitangents, srcMesh->mBitangents, sizeof(aiVector3D) * dstMesh->mNumVertices);
			}

			for (int c = 0; c < AI_MAX_NUMBER_OF_COLOR_SETS; ++c)
			{
				if (srcMesh->HasVertexColors(c))
				{
					dstMesh->mColors[c] = new aiColor4D[dstMesh->mNumVertices];
					std::memcpy(dstMesh->mColors[c], srcMesh->mColors[c], sizeof(aiColor4D) * dstMesh->mNumVertices);
				}
			}

			for (int t = 0; t < AI_MAX_NUMBER_OF_TEXTURECOORDS; ++t)
			{
				if (srcMesh->HasTextureCoords(t))
				{
					dstMesh->mNumUVComponents[t] = srcMesh->mNumUVComponents[t];
					dstMesh->mTextureCoords[t] = new aiVector3D[dstMesh->mNumVertices];
					std::memcpy(dstMesh->mTextureCoords[t], srcMesh->mTextureCoords[t], sizeof(aiVector3D) * dstMesh->mNumVertices);
				}
			}

			// Copy faces
			dstMesh->mFaces = new aiFace[dstMesh->mNumFaces];
			for (unsigned int j = 0; j < dstMesh->mNumFaces; ++j)
			{
				const aiFace& srcFace = srcMesh->mFaces[j];
				aiFace& dstFace = dstMesh->mFaces[j];

				dstFace.mNumIndices = srcFace.mNumIndices;
				dstFace.mIndices = new unsigned int[dstFace.mNumIndices];
				std::memcpy(dstFace.mIndices, srcFace.mIndices, sizeof(unsigned int) * dstFace.mNumIndices);
			}

			copy->mMeshes[i] = dstMesh;
		}
	}

	// ---- Copy Materials ----
	copy->mNumMaterials = source->mNumMaterials;
	if (copy->mNumMaterials > 0)
	{
		copy->mMaterials = new aiMaterial * [copy->mNumMaterials];
		for (unsigned int i = 0; i < copy->mNumMaterials; ++i)
		{
			const aiMaterial* src = source->mMaterials[i];
			aiMaterial* dest = new aiMaterial();
			dest->mNumAllocated = src->mNumAllocated;
			dest->mNumProperties = src->mNumProperties;

			if (src->mNumProperties > 0)
			{
				dest->mProperties = new aiMaterialProperty * [src->mNumProperties];
				for (unsigned int p = 0; p < src->mNumProperties; ++p)
				{
					const aiMaterialProperty* sprop = src->mProperties[p];
					aiMaterialProperty* prop = new aiMaterialProperty();

					prop->mKey = sprop->mKey;
					prop->mSemantic = sprop->mSemantic;
					prop->mIndex = sprop->mIndex;
					prop->mType = sprop->mType;
					prop->mDataLength = sprop->mDataLength;

					if (sprop->mDataLength > 0 && sprop->mData)
					{
						prop->mData = new char[sprop->mDataLength];
						std::memcpy(prop->mData, sprop->mData, sprop->mDataLength);
					}
					else
					{
						prop->mData = nullptr;
					}

					dest->mProperties[p] = prop;
				}
			}
			else
			{
				dest->mProperties = nullptr;
			}

			copy->mMaterials[i] = dest;
		}
	}

	// ---- Copy Embedded Textures ----
	copy->mNumTextures = source->mNumTextures;
	if (copy->mNumTextures > 0)
	{
		copy->mTextures = new aiTexture * [copy->mNumTextures];

		// Initialize all pointers to nullptr for safe cleanup
		for (unsigned int i = 0; i < copy->mNumTextures; ++i)
		{
			copy->mTextures[i] = nullptr;
		}

		try
		{
			for (unsigned int i = 0; i < copy->mNumTextures; ++i)
			{
				const aiTexture* srcTex = source->mTextures[i];
				if (!srcTex)
				{
					continue; // Skip null source textures
				}

				aiTexture* dstTex = new aiTexture();
				dstTex->mWidth = srcTex->mWidth;
				dstTex->mHeight = srcTex->mHeight;
				dstTex->pcData = nullptr; // Initialize to safe state

				// Safe format hint copying
				dstTex->achFormatHint[0] = '\0';
				std::memcpy(dstTex->achFormatHint, srcTex->achFormatHint, sizeof(dstTex->achFormatHint));
				dstTex->achFormatHint[sizeof(dstTex->achFormatHint) - 1] = '\0'; // Ensure null termination

				// Texture data copying with strong guards
				if (srcTex->pcData && srcTex->mWidth > 0)
				{
					size_t width = static_cast<size_t>(srcTex->mWidth);
					size_t height = static_cast<size_t>(srcTex->mHeight);
					size_t size = (height == 0) ? width : width * height;

					// Sanity checks for reasonable texture size
					if (size > 0 && size < SIZE_MAX / sizeof(aiTexel) && size < 100000000) // 100M texel limit
					{
						size_t byteSize = size * sizeof(aiTexel);
						dstTex->pcData = new(std::nothrow) aiTexel[size];

						if (dstTex->pcData)
						{
							// Strong guard - attempt safe copy
							if (!SafeCopyTextureData(dstTex->pcData, srcTex->pcData, byteSize))
							{
								// Copy failed due to invalid source data - clean up and continue
								delete[] dstTex->pcData;
								dstTex->pcData = nullptr;
								printf("Warning: Skipping corrupted texture %u in model (invalid source data)\n", i);
							}
						}
						else
						{
							printf("Warning: Failed to allocate memory for texture %u\n", i);
						}
					}
					else
					{
						dstTex->pcData = nullptr;
						printf("Warning: Invalid texture size (%zu) for texture %u, skipping\n", size, i);
					}
				}

				copy->mTextures[i] = dstTex;
			}
		}
		catch (...)
		{
			// Cleanup on any exception during the loop
			for (unsigned int i = 0; i < copy->mNumTextures; ++i)
			{
				if (copy->mTextures[i])
				{
					delete[] copy->mTextures[i]->pcData;
					delete copy->mTextures[i];
				}
			}
			delete[] copy->mTextures;
			copy->mTextures = nullptr;
			copy->mNumTextures = 0;
			throw; // Re-throw the exception
		}
	}

	// ---- Copy Node Hierarchy ----
	std::function<aiNode* (const aiNode*)> cloneNode = [&](const aiNode* src) -> aiNode* {
		aiNode* dst = new aiNode();
		dst->mName = src->mName;
		dst->mTransformation = src->mTransformation;
		dst->mNumMeshes = src->mNumMeshes;
		dst->mNumChildren = src->mNumChildren;

		if (src->mNumMeshes > 0)
		{
			dst->mMeshes = new unsigned int[dst->mNumMeshes];
			std::memcpy(dst->mMeshes, src->mMeshes, sizeof(unsigned int) * dst->mNumMeshes);
		}

		if (src->mNumChildren > 0)
		{
			dst->mChildren = new aiNode * [dst->mNumChildren];
			for (unsigned int i = 0; i < dst->mNumChildren; ++i)
			{
				dst->mChildren[i] = cloneNode(src->mChildren[i]);
				dst->mChildren[i]->mParent = dst;
			}
		}

		return dst;
		};

	copy->mRootNode = cloneNode(source->mRootNode);

	return copy;
}


aiMaterial* SceneUtils::copyMaterial(const aiMaterial* src)
{
	if (!src) return nullptr;

	aiMaterial* dest = new aiMaterial();
	dest->mNumAllocated = src->mNumAllocated;
	dest->mNumProperties = src->mNumProperties;

	if (src->mNumProperties > 0)
	{
		dest->mProperties = new aiMaterialProperty * [src->mNumProperties];
		for (unsigned int i = 0; i < src->mNumProperties; ++i)
		{
			const aiMaterialProperty* sprop = src->mProperties[i];
			if (!sprop) continue;

			aiMaterialProperty* prop = new aiMaterialProperty();
			prop->mKey = sprop->mKey;
			prop->mSemantic = sprop->mSemantic;
			prop->mIndex = sprop->mIndex;
			prop->mType = sprop->mType;
			prop->mDataLength = sprop->mDataLength;

			if (sprop->mDataLength > 0 && sprop->mData)
			{
				prop->mData = new char[sprop->mDataLength];
				std::memcpy(prop->mData, sprop->mData, sprop->mDataLength);
			}
			else
			{
				prop->mData = nullptr;
			}

			dest->mProperties[i] = prop;
		}
	}
	else
	{
		dest->mProperties = nullptr;
	}

	return dest;
}



void SceneUtils::deleteScene(aiScene* scene)
{
	if (!scene) return;

	try
	{
		// ---- Delete animations ----
		if (scene->mAnimations)
		{
			for (unsigned int i = 0; i < scene->mNumAnimations; ++i)
			{
				aiAnimation* anim = scene->mAnimations[i];
				if (anim)
				{
					// Delete channels
					if (anim->mChannels)
					{
						for (unsigned int c = 0; c < anim->mNumChannels; ++c)
						{
							aiNodeAnim* channel = anim->mChannels[c];
							if (channel)
							{
								delete[] channel->mPositionKeys;
								delete[] channel->mRotationKeys;
								delete[] channel->mScalingKeys;
								delete channel;
							}
						}
						delete[] anim->mChannels;
					}

					// Delete mesh channels
					if (anim->mMeshChannels)
					{
						for (unsigned int mc = 0; mc < anim->mNumMeshChannels; ++mc)
						{
							aiMeshAnim* meshChannel = anim->mMeshChannels[mc];
							if (meshChannel)
							{
								delete[] meshChannel->mKeys;
								delete meshChannel;
							}
						}
						delete[] anim->mMeshChannels;
					}

					delete anim;
					scene->mAnimations[i] = nullptr;
				}
			}
			delete[] scene->mAnimations;
			scene->mAnimations = nullptr;
		}
		scene->mNumAnimations = 0;

		// ---- Delete meshes ----
		if (scene->mMeshes)
		{
			for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
			{
				aiMesh* mesh = scene->mMeshes[i];
				if (mesh)
				{
					// Delete vertex data arrays
					delete[] mesh->mVertices;
					mesh->mVertices = nullptr;

					delete[] mesh->mNormals;
					mesh->mNormals = nullptr;

					delete[] mesh->mTangents;
					mesh->mTangents = nullptr;

					delete[] mesh->mBitangents;
					mesh->mBitangents = nullptr;

					// Delete color sets
					for (unsigned int c = 0; c < AI_MAX_NUMBER_OF_COLOR_SETS; ++c)
					{
						delete[] mesh->mColors[c];
						mesh->mColors[c] = nullptr;
					}

					// Delete texture coordinate sets
					for (unsigned int t = 0; t < AI_MAX_NUMBER_OF_TEXTURECOORDS; ++t)
					{
						delete[] mesh->mTextureCoords[t];
						mesh->mTextureCoords[t] = nullptr;
					}

					// Delete faces
					if (mesh->mFaces)
					{
						for (unsigned int f = 0; f < mesh->mNumFaces; ++f)
						{
							aiFace& face = mesh->mFaces[f];
							delete[] face.mIndices;
							face.mIndices = nullptr;
							face.mNumIndices = 0;
						}
						delete[] mesh->mFaces;
						mesh->mFaces = nullptr;
					}
					mesh->mNumFaces = 0;

					// Delete bones
					if (mesh->mBones)
					{
						for (unsigned int b = 0; b < mesh->mNumBones; ++b)
						{
							aiBone* bone = mesh->mBones[b];
							if (bone)
							{
								delete[] bone->mWeights;
								delete bone;
							}
						}
						delete[] mesh->mBones;
						mesh->mBones = nullptr;
					}
					mesh->mNumBones = 0;

					// Delete animation meshes
					if (mesh->mAnimMeshes)
					{
						for (unsigned int am = 0; am < mesh->mNumAnimMeshes; ++am)
						{
							aiAnimMesh* animMesh = mesh->mAnimMeshes[am];
							if (animMesh)
							{
								delete[] animMesh->mVertices;
								delete[] animMesh->mNormals;
								delete[] animMesh->mTangents;
								delete[] animMesh->mBitangents;
								for (unsigned int c = 0; c < AI_MAX_NUMBER_OF_COLOR_SETS; ++c)
								{
									delete[] animMesh->mColors[c];
								}
								for (unsigned int t = 0; t < AI_MAX_NUMBER_OF_TEXTURECOORDS; ++t)
								{
									delete[] animMesh->mTextureCoords[t];
								}
								delete animMesh;
							}
						}
						delete[] mesh->mAnimMeshes;
						mesh->mAnimMeshes = nullptr;
					}
					mesh->mNumAnimMeshes = 0;

					delete mesh;
					scene->mMeshes[i] = nullptr;
				}
			}
			delete[] scene->mMeshes;
			scene->mMeshes = nullptr;
		}
		scene->mNumMeshes = 0;

		// ---- Delete materials ----
		if (scene->mMaterials)
		{
			for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
			{
				aiMaterial* mat = scene->mMaterials[i];
				if (mat)
				{
					if (mat->mProperties)
					{
						for (unsigned int p = 0; p < mat->mNumProperties; ++p)
						{
							aiMaterialProperty* prop = mat->mProperties[p];
							if (prop)
							{
								delete[] prop->mData;
								prop->mData = nullptr;
								delete prop;
							}
						}
						delete[] mat->mProperties;
						mat->mProperties = nullptr;
					}
					mat->mNumProperties = 0;
					delete mat;
					scene->mMaterials[i] = nullptr;
				}
			}
			delete[] scene->mMaterials;
			scene->mMaterials = nullptr;
		}
		scene->mNumMaterials = 0;

		// ---- Delete textures ----
		if (scene->mTextures)
		{
			for (unsigned int i = 0; i < scene->mNumTextures; ++i)
			{
				aiTexture* tex = scene->mTextures[i];
				if (tex)
				{
					delete[] tex->pcData;
					tex->pcData = nullptr;
					delete tex;
					scene->mTextures[i] = nullptr;
				}
			}
			delete[] scene->mTextures;
			scene->mTextures = nullptr;
		}
		scene->mNumTextures = 0;

		// ---- Delete lights ----
		if (scene->mLights)
		{
			for (unsigned int i = 0; i < scene->mNumLights; ++i)
			{
				delete scene->mLights[i];
				scene->mLights[i] = nullptr;
			}
			delete[] scene->mLights;
			scene->mLights = nullptr;
		}
		scene->mNumLights = 0;

		// ---- Delete cameras ----
		if (scene->mCameras)
		{
			for (unsigned int i = 0; i < scene->mNumCameras; ++i)
			{
				delete scene->mCameras[i];
				scene->mCameras[i] = nullptr;
			}
			delete[] scene->mCameras;
			scene->mCameras = nullptr;
		}
		scene->mNumCameras = 0;

		// ---- Delete node hierarchy ----
		if (scene->mRootNode)
		{
			deleteNodeRecursive(scene->mRootNode);
			scene->mRootNode = nullptr;
		}

		// ---- Delete metadata ----
		if (scene->mMetaData)
		{
			delete scene->mMetaData;
			scene->mMetaData = nullptr;
		}

		// Finally delete the scene itself
		delete scene;

	}
	catch (const std::exception& e)
	{
		std::cerr << "Exception during scene deletion: " << e.what() << std::endl;
		// Try to at least delete the main scene object to prevent memory leak
		delete scene;
	}
	catch (...)
	{
		std::cerr << "Unknown exception during scene deletion" << std::endl;
		// Try to at least delete the main scene object to prevent memory leak
		delete scene;
	}
}

void SceneUtils::deleteNodeRecursive(aiNode* node)
{
	if (!node) return;

	try
	{
		// Delete children first
		if (node->mChildren)
		{
			for (unsigned int i = 0; i < node->mNumChildren; ++i)
			{
				if (node->mChildren[i])
				{
					deleteNodeRecursive(node->mChildren[i]);
					node->mChildren[i] = nullptr;
				}
			}
			delete[] node->mChildren;
			node->mChildren = nullptr;
		}
		node->mNumChildren = 0;

		// Delete mesh indices array
		delete[] node->mMeshes;
		node->mMeshes = nullptr;
		node->mNumMeshes = 0;

		// Delete metadata
		if (node->mMetaData)
		{
			delete node->mMetaData;
			node->mMetaData = nullptr;
		}

		// Finally delete the node itself
		delete node;

	}
	catch (...)
	{
		// Even if something goes wrong, try to delete the node
		delete node;
	}
}
