#ifndef GLWIDGET_H
#define GLWIDGET_H

#include "AdaptiveShadowMapper.h"
#include "AnimationRuntimeController.h"
#include "BoundingSphere.h"
#include "ExplodedViewRuntimeController.h"
#include "SceneRenderController.h"
#include "ViewportInteractionController.h"
#include "GLCamera.h"
#include "PlaneRenderable.h"
#include "FloorPlane.h"
#include "SceneRuntime.h"
#include "RenderableMesh.h"
#include "TransformCommand.h"
#include "ShaderProgram.h"
#include "AssImpModelLoader.h"
#include "AssemblyRelationGraph.h"
#include <math.h>
#include <QColor>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFormLayout>
#include <QImage>
#include <QMultiHash>
#include <QOpenGLFunctions_4_5_Core>
#include <QOpenGLWidget>
#include <QPointer>
#include <QRubberBand>
#include <QSet>
#include <array>
#include "ViewToolbar.h"
#include "SceneUtils.h"
#include "GLLights.h"
#include "KTX2Loader.h"
#include "SelectionManager.h"
#include "TransformGizmo.h"

/* Custom OpenGL Viewer Widget */

namespace Mvf { struct Document; }

class TextRenderer;
class ClippingPlanesEditor;
class ExplodedViewPanel;
class ExplodedViewManager;

#include "RenderEnums.h"
class AssImpModelLoader;
class ConeRenderable;
class CubeRenderable;
class SphereRenderable;
class ViewCubeMesh;
class AssImpModelLoader;
struct SceneNode;

class ModelViewer;

// ViewMode, ViewProjection, CornerAxisPosition → RenderEnums.h (Phase 11)
enum class DisplayMode { SHADED, HOLLOW_MESH, MESH_EDGES, WIREFRAME, SHADED_WITH_EDGES, REALSHADED, FLATSHADED };
enum class RenderingMode { ADS_BLINN_PHONG, PHYSICALLY_BASED_RENDERING };
enum class ClippingPlaneHatchMode { PROCEDURAL, TEXTURE };
enum class HatchPattern { DIAGONAL_45 = 0, DIAGONAL_135 = 1, HORIZONTAL = 2, VERTICAL = 3, GRID = 4, DIAGONAL_CROSS = 5 };

// ---------------------------------------------------------------------------
// TextureSlotInfo
// Describes one texture slot as seen by the GPU — used by TextureDebugPanel.
// Built inside GLWidget::requestTextureReadback() via glGetTexImage readback.
// ---------------------------------------------------------------------------
struct TextureSlotInfo
{
	QString  slotName;              // human-readable name ("albedoMap", "normalMap", …)
	int      unitIndex  = -1;       // GL texture unit index (0, 6, 10–31)
	GLuint   textureId  = 0;        // GL object ID; 0 = slot not populated
	QPixmap  thumbnail;             // 64×64 readback pixmap; null when textureId == 0
	bool     isActive        = false; // textureId != 0 (a texture is bound)
	bool     extensionEnabled = false;// the parent KHR extension is active (may be true even
	                                  // when no texture is bound — e.g. sheen colour factor set)
	bool     isMarker        = false; // synthetic slot used only for scalar-driven activity
	                                  // detection; never shown in the thumbnail grid
};
Q_DECLARE_METATYPE(QVector<TextureSlotInfo>)

class GLWidget : public QOpenGLWidget, QOpenGLFunctions_4_5_Core
{
	Q_OBJECT
public:
	GLWidget(QWidget* parent = 0, const char* name = 0);
	~GLWidget();

	void retranslateUI();

	void updateView();

	void resizeView(int w, int h) { resizeGL(w, h); }
	void setViewMode(ViewMode mode);
	void setCameraUpAxisZUp(bool zUp, bool syncToolbar = true);
	bool isCameraUpAxisZUp() const { return _viewCtrl.cameraUpAxisZUp(); }
	void setProjection(ViewProjection proj);
	void setCameraMode(GLCamera::CameraMode mode);
	GLCamera::CameraMode cameraMode() const;

	void setMultiView(bool active) { _viewCtrl.setMultiViewActive(active); }
	void setRotationActive(bool active);
	void setPanningActive(bool active);
	void setZoomingActive(bool active);

	void setShowCenterAxisOverride(bool show) { _viewCtrl.setUserShowAxisOverride(show); update(); }
	void setShowCornerAxisOverride(bool show) { _viewCtrl.setUserShowCornerAxisOverride(show); update(); }
	void setShowViewCubeOverride(bool show) { _viewCtrl.setShowViewCubeOverride(show); update(); }
	void setCornerAxisPosition(CornerAxisPosition position);

	void beginWindowZoom();
	void performWindowZoom();

	void setDisplayList(const std::vector<int>& ids);
	GltfCameraData cameraDataForMvfSave(const GltfCameraData& source) const;
	void triggerShadowRecomputation();
	void setShadowQuality(AdaptiveShadowMapper::QualityLevel quality);
	float calculateLightDistance();

	QVector<QUuid> duplicateObjects(const std::vector<int>& ids);

	void updateFloorPlane();
	void updateBoundingSphere();

	void updateBoundingBox();

	int getModelNum() const
	{
		return _modelNum;
	}

	void updateClippingPlane();
	void showClippingPlaneEditor(bool show);
	void showExplodedViewPanel(bool show);
	ExplodedViewPanel* getExplodedViewPanel() const { return _explodedViewPanel; }
	void updateExplosion();
	QWidget* attachOverlayPanel(QWidget* contentWidget, const QRect& geometry,
	                            Qt::Alignment alignment = Qt::AlignTop | Qt::AlignLeft,
	                            const QString& objectName = QString());
	QWidget* takeOverlayPanel(QWidget* contentWidget);
	void refreshDetachedNavigationOverlayTheme();
	void setClippingPlaneHatchMode(ClippingPlaneHatchMode mode);
	void setClippingPlaneHatchPattern(HatchPattern pattern);
	void setHatchTiling(int tiling);
	void setHatchLineThickness(float width);
	void setHatchIntensity(float spacing);
	void setHatchLayers(int layers);
	void setHatchLineColor(const QColor& color);
	void setHatchTexture(const QString& path);

	void showAxis(bool show);
	void showTransformGizmoForSelection(bool show);
	bool beginExplodedViewManualPlacement(const QVector<QUuid>& selectionUuids = {});
	void finishExplodedViewManualPlacement();
	void clearExplodedViewManualPlacement();
	bool isExplodedViewManualPlacementActive() const { return _explodedViewCtrl.isManualPlacementActive(); }
	bool hasExplodedViewManualPlacement() const { return !_explodedViewCtrl.manualOriginalStates().isEmpty(); }
	bool hasExplodedViewManualTransformChanges() const;
	QSet<QUuid> explodedViewManualPlacementUuids() const;
	QVector3D explodedViewManualPlacementTranslationDelta() const;
	QVector3D explodedViewManualPlacementRotationDelta() const;
	void setExplodedViewManualPlacementTranslationDelta(const QVector3D& delta);
	void setExplodedViewManualPlacementRotationDelta(const QVector3D& delta);
	QMap<QUuid, TransformState> explodedViewManualStates() const;
	void restoreExplodedViewManualStates(const QMap<QUuid, TransformState>& states);
	bool userModelTransformForFile(const QString& sourceFile,
	                               QMatrix4x4& outTransform) const;

	void showShadows(bool show);
	void showSelfShadows(bool show);
	void showEnvironment(bool show);
	void showSkyBox(bool show);
	void blurSkyBox(bool blur);
	void setSkyBoxBlurPercent(int percent);
	void showReflections(bool show);
	void setGroundMode(GroundMode mode);
	GroundMode groundMode() const { return _renderCtrl.groundMode(); }
	void showFloor(bool show);
	bool isFloorShown() { return _renderCtrl.groundMode() == GroundMode::Floor; }
	bool isGridShown() const { return _renderCtrl.groundMode() == GroundMode::Grid; }
	void showFloorTexture(bool show);
	void setFloorTexture(QImage img);

	std::vector<SceneMesh*> getMeshStore() const
	{
		return _sceneRuntime.meshPointers();
	}

	void addToDisplay(SceneMesh*);
	void removeFromDisplay(int index);
	void centerScreen(std::vector<int> selectedIDs);
	void select(int id);
	void deselect(int id);
	void syncMeshSelectionVisualState();

	bool loadAssImpModel(const QString& fileName, const UVMethod& uvMethod, QString& error, bool progressiveLoading = false);

	bool generateUVsForMeshes(const std::vector<int>& ids, const UVMethod& uvMethod, const UVConfig& uvConfig, QString& error);

	aiScene* getAssImpScene() const { return _sceneRuntime.globalScene(); }
	glm::mat4 getGlobalSceneTransform() const { return _sceneRuntime.globalSceneTransform(); }

	void invertADSOpacityTexMap(const std::vector<int>& ids, const bool& inverted);

	void setMaterialToObjects(const std::vector<int>& ids, const GLMaterial& mat);
	void setTexturesToObjects(const std::vector<int>& ids, const GLMaterial& mat);
	void synchronizeTextureCache(const GLMaterial* material, GLMaterial::TextureType type);
	void clearTextureCache();

	void setTransformation(const std::vector<int>& ids, const QVector3D& trans, const QVector3D& rot, const QVector3D& scale);
	void resetTransformation(const std::vector<int>& ids);
	void applyTransforms(const QMap<int, TransformState>& transforms, bool fitView = true);
	void applyExplodedViewTransforms(const QMap<int, TransformState>& transforms, bool fitView = false);

	void setSkyBoxTextureFolder(QString folder);
	bool loadCubemapFromSingleHDR(const QString& filePath);
	bool convertEquirectangularToCubemap(const QString& filePath);
	bool convertEquirectangularToCubemapQuad(const QString& filePath);

	void renderConversionCube();

	void setAnisotropicFilteringLevel(int level) { _renderCtrl.setAnisotropicFilteringLevel(level); }
	int getAnisotropicFilteringLevel() const { return _renderCtrl.anisotropicFilteringLevel(); }

	void setTransmissionEnabled(const bool& enabled);
	bool isTransmissionEnabled() const { return _renderCtrl.transmissionEnabled(); }
	void setActiveAnimation(const QString& sourceFile, int clipIndex);
	void setAnimationPlaying(bool playing);

	// Drop the cached animation runtime for a file whose meshes were removed
	// from the document, stopping playback if that file was the active one.
	// Without this the stale runtime (old UUIDs) survives deletion and can be
	// picked up when the same file is imported again.
	void clearAnimationRuntimeForFile(const QString& sourceFile);
	void seekAnimation(double timeSeconds);
	void setAnimationLooping(bool looping);
	void setAnimationPlaybackSpeed(double speed);
	void syncRuntimeNodeTransforms(const QString& sourceFile);
	void refreshAnimationMaterialState(const QString& sourceFile);
	QString activeAnimationFile() const { return _animCtrl.activeAnimationFile(); }
	int activeAnimationClip() const { return _animCtrl.activeAnimationClip(); }
	double currentAnimationTimeSeconds() const { return _animCtrl.animationCurrentTimeSeconds(); }
	bool isAnimationPlaying() const { return _animCtrl.isPlaying(); }
	bool isAnimationLooping() const { return _animCtrl.isLooping(); }
	double animationPlaybackSpeed() const { return _animCtrl.playbackSpeed(); }

	// glTF camera switching
	void activateGltfCamera(const QString& sourceFile, int cameraIndex);
	void resetToSystemCamera();
	bool isGltfCameraActive()     const { return _animCtrl.activeGltfCameraIndex() >= 0; }
	QString activeGltfCameraFile()  const { return _animCtrl.activeGltfCameraFile(); }
	int     activeGltfCameraIndex() const { return _animCtrl.activeGltfCameraIndex(); }

public:
	QVector4D getDefaultLightColor() const;
	void setDefaultLightColor(const QVector4D& defaultLightColor);

	QVector3D getLightPosition() const;
	QVector3D getLightOffset() const { return QVector3D(_lightOffsetX, _lightOffsetY, _lightOffsetZ); }
	void setLightOffset(const QVector3D& offset);

	float getFloorSize() const { return _floorSize; }

	bool isShaded() const;
	DisplayMode getDisplayMode() const;
	void setDisplayMode(DisplayMode mode);

	bool isVertexNormalsShown() const;
	void setShowVertexNormals(bool showVertexNormals);
	bool isBoundingBoxShown() const;
	void setShowBoundingBox(bool showBoundingBox);
	DebugOverlayMode debugOverlayMode() const;
	void setDebugOverlayMode(DebugOverlayMode mode);
	bool isDebugOverlayEnabled() const;
	void setDebugOverlayEnabled(bool enabled);
	void setDebugOverlayAvailability(bool boundingBox, bool vertexNormals, bool faceNormals);

	bool isFaceNormalsShown() const;
	void setShowFaceNormals(bool showFaceNormals);

	std::vector<int> getDisplayedObjectsIds() const;
	std::vector<int> getHiddenObjectsIds() const;
	const std::vector<int>& currentVisibleObjectIds() const { return _sceneRuntime.currentVisibleObjectIds(); }

	bool isVisibleSwapped() const;

	BoundingSphere getBoundingSphere() const;

	QColor getBgTopColor() const;
	void setBgTopColor(const QColor& bgTopColor);


	QColor getBgBotColor() const;
	void setBgBotColor(const QColor& bgBotColor);

	int getBgGradientStyle() const { return _gradientStyle; }
	void setBgGradientStyle(int style) { _gradientStyle = style; }

	RenderingMode getRenderingMode() const;
	void setRenderingMode(const RenderingMode& renderingMode);

	void setCappingPlanesEnabled(const bool& enabled) { _renderCtrl.setCappingEnabled(enabled); }
	bool cappingPlanesEnabled() const { return _renderCtrl.cappingEnabled(); }

	void setYZClippingEnabled(const bool& enabled) { _renderCtrl.setYZClippingEnabled(enabled); }
	bool yzClippingEnabled() const { return _renderCtrl.yzClippingEnabled(); }
	void setZXClippingEnabled(const bool& enabled) { _renderCtrl.setZXClippingEnabled(enabled); }
	bool zxClippingEnabled() const { return _renderCtrl.zxClippingEnabled(); }
	void setXYClippingEnabled(const bool& enabled) { _renderCtrl.setXYClippingEnabled(enabled); }
	bool xyClippingEnabled() const { return _renderCtrl.xyClippingEnabled(); }

	void setClippingXFlipped(const bool& flipped) { _renderCtrl.setClippingXFlipped(flipped); }
	bool clippingXFlipped() const { return _renderCtrl.clippingXFlipped(); }
	void setClippingYFlipped(const bool& flipped) { _renderCtrl.setClippingYFlipped(flipped); }
	bool clippingYFlipped() const { return _renderCtrl.clippingYFlipped(); }
	void setClippingZFlipped(const bool& flipped) { _renderCtrl.setClippingZFlipped(flipped); }
	bool clippingZFlipped() const { return _renderCtrl.clippingZFlipped(); }

	void setClippingXCoeff(const float& coeff) { _renderCtrl.setClippingXCoeff(coeff); }
	float clippingXCoeff() const { return _renderCtrl.clippingXCoeff(); }
	void setClippingYCoeff(const float& coeff) { _renderCtrl.setClippingYCoeff(coeff); }
	float clippingYCoeff() const { return _renderCtrl.clippingYCoeff(); }
	void setClippingZCoeff(const float& coeff) { _renderCtrl.setClippingZCoeff(coeff); }
	float clippingZCoeff() const { return _renderCtrl.clippingZCoeff(); }

	bool getHdrToneMapping() const;
	bool getGammaCorrection() const;
	float getScreenGamma() const;

	// Environment mapping accessors
	// index: 0 = ViewerIBL, 1 = Studio, 2 = Outdoor, 3 = Office
	GLuint getEnvironmentMap(int index = 0, bool regenerate = false);
	GLuint getIrradianceMap(int index = 0, bool regenerate = false);
	GLuint getPrefilterMap(int index = 0, bool regenerate = false);
	GLuint getSheenPrefilterMap(int index = 0, bool regenerate = false);
	unsigned int getPrefilterMipLevels() const { return _renderCtrl.prefilterMipLevels(); }
	unsigned int getSheenPrefilterMipLevels() const { return _renderCtrl.sheenPrefilterMipLevels(); }
	GLuint getBrdfLUT() const { return _renderCtrl.brdfLUTTexture(); }
	GLuint getCharlieLUT() const { return _renderCtrl.charlieLUTTexture(); }
	GLuint getSheenELUT() const { return _renderCtrl.sheenELUTTexture(); }
	bool isEnvironmentMapEnabled() const { return _renderCtrl.envMapEnabled(); }
	bool isIBLEnabled() const { return _renderCtrl.useIBL(); }
	float getIBLExposure() const { return _renderCtrl.iblExposure(); }
	float getEnvMapExposure() const { return _renderCtrl.envMapExposure(); }
	QString getCurrentSkyboxFolder() const { return _renderCtrl.currentSkyboxFolder(); }
	bool isSkyBoxShown() const { return _renderCtrl.skyBoxEnabled(); }
	bool isSkyBoxHDRIEnabled() const { return _renderCtrl.skyBoxTextureHDRI(); }
	int getSkyBoxBlurPercent() const { return _renderCtrl.skyBoxBlurPercent(); }
	float getSkyBoxFOV() const { return _renderCtrl.skyBoxFOV(); }
	float getPerspFOV()  const { return _viewCtrl.FOV(); }
	float getSkyBoxZRotationDegrees() const { return _renderCtrl.skyBoxZRotation(); }
	bool areReflectionsEnabled() const { return _renderCtrl.reflectionsEnabled(); }
	bool isFloorTextureShown() const { return _renderCtrl.floorTextureDisplayed(); }
	bool areShadowsEnabled() const { return _renderCtrl.shadowsEnabled(); }
	bool areSelfShadowsEnabled() const { return _renderCtrl.selfShadowsEnabled(); }
	bool areDefaultLightsEnabled() const { return _renderCtrl.useDefaultLights(); }
	bool arePunctualLightsEnabled() const { return _renderCtrl.usePunctualLights(); }
	bool areLightsShown() const;

	ViewToolbar* getViewToolbar() const { return _viewToolbar; }

	void cleanUpShaders();

	// Recycle bin operations (used by DeleteCommand)
	void moveToRecycleBin(const QUuid& uuid, int originalIndex);
	bool restoreFromRecycleBin(const QUuid& uuid);
	void permanentlyDeleteFromBin(const QUuid& uuid);

	// Query methods
	bool isInRecycleBin(const QUuid& uuid) const;
	QVector<QUuid> getRecycleBinUuids() const;

	// UUID lookup methods
	SceneMesh* getMeshByUuid(const QUuid& uuid) const;
	SceneMesh* getMeshByIndex(int index) const;
	int getIndexByUuid(const QUuid& uuid) const;
	QUuid getUuidByIndex(int index) const;

	// Generate a name that doesn't clash with any existing mesh name.
	QString generateUniqueMeshName(const QString& baseName);

	// ---- MVF mesh loading (split into CPU preparation + GL upload) ----

	/// Pre-computed mesh data produced by prepareMvfMeshes().
	/// All fields are plain data — no GL resources — so the struct is safe
	/// to construct on any thread.
	struct PreparedMvfMesh
	{
		QString      name;
		QUuid        uuid;
		GLenum       primitiveMode = GL_TRIANGLES;
		int          sceneIndex    = -1;
		bool         hasNegativeScale = false;
		int          originalMaterialIndex = -1;
		QString      sourceFile;
		QString      sourceNodeName;
		QVector<GltfVariantMapping> variantMappings;
		QMap<int, GLMaterial> allVariantMaterials;
		std::vector<Vertex>       vertices;
		std::vector<unsigned int> indices;
		GLMaterial   material;
		QVector<GltfSkinJoint>   skinJoints;  ///< Skeleton joints for skinned meshes

		// Morph targets (blend shapes) — position/normal/tangent deltas.
		QVector<MorphTargetData> morphTargets;
		QVector<float>           defaultMorphWeights;

		// OCC B-Rep edge segments and per-topological-edge boundaries.
		// Populated for STEP/IGES/BREP meshes, empty otherwise.
		std::vector<float> occEdgeSegments;
		std::vector<int>   occEdgeBoundaries;

		// Per-mesh user transform (gizmo TRS).  Non-identity when the user has moved,
		// rotated, or scaled the mesh via the gizmo.  Applied by uploadPreparedMvfMeshes
		// after the mesh is created so interactive transforms survive save/load.
		QVector3D   meshTranslation  = QVector3D(0.0f, 0.0f, 0.0f);
		QVector3D   meshRotation     = QVector3D(0.0f, 0.0f, 0.0f);  // Euler display
		QQuaternion meshRotationQuat = QQuaternion();
		QVector3D   meshScale        = QVector3D(1.0f, 1.0f, 1.0f);
	};

	/// CPU-only preparation: reads geometry streams, builds vertex arrays,
	/// reconstructs materials.  Thread-safe — may be called from a worker
	/// thread.  Returns the prepared list (moved, not copied).
	static QVector<PreparedMvfMesh> prepareMvfMeshes(
	    const Mvf::Document& document,
	    const QByteArray& geometryChunk,
	    const QByteArray& imageChunk);

	/// Clear the mesh store and display list (safe to call from main thread).
	/// Called before uploading new MVF meshes to replace any existing geometry.
	void clearMeshStore();

	/// Single-mesh GL upload for use with BlockingQueuedConnection.
	/// Called once per mesh from the main thread while worker waits.
	void uploadOneMvfMesh(const PreparedMvfMesh& pm);

	/// GL-only upload: creates SceneMesh objects, uploads VBOs and
	/// textures, and populates the display list.  Must run on the main
	/// (GL) thread.  Updates the progress bar between meshes.
	bool uploadPreparedMvfMeshes(const QVector<PreparedMvfMesh>& meshes);

	// Legacy combined entry point (kept for compatibility).
	bool loadMvfMeshes(const Mvf::Document& document,
	                   const QByteArray& geometryChunk,
	                   const QByteArray& imageChunk);
	void setParsedLights(const GltfLightData& lights);

	/// Rebuilds the parsed light baseline from all SceneGraph-registered light data.
	/// Connected to SceneGraph::lightDataChanged to stay current on model add/remove.
	void onSceneLightDataChanged();

	/// Thin slot that delegates to SceneRuntime. Qt::UniqueConnection requires a
	/// member-function pointer — it does not work with lambda connections in Qt6.
	void onSceneStructureChanged();

	/// Accessor for the foreground shader (for pre-load shader validation).
	ShaderProgram* getShader() const { return _renderCtrl.fgShader(); }

	void setSectionCapsDynamicEnabled(bool enabled);

	// Moved to AnimationRuntimeController (Phase 8); aliases preserve external access.
	using RuntimeNodeTransform       = AnimationRuntimeController::RuntimeNodeTransform;
	using RuntimeAnimationFileState  = AnimationRuntimeController::RuntimeAnimationFileState;

signals:
	void windowZoomEnded();
	void rotationsSet();
	void zoomAndPanSet();
	void viewSet();
	void displayListSet();
	void singleSelectionDone(int);
	void sweepSelectionDone(QList<int>);
	void floorShown(bool);
	void visibleSwapped(bool);
	void loadingAssImpModelCancelled();
	void displayModeChanged(int);
	void renderingModeChanged(int);
	void animationStateChanged();
	void explodedViewManualPlacementChanged();
	void backgroundColorChanged(const QColor& topColor, const QColor& bottomColor);
	// Forwarded from SelectionManager so external panels (e.g. TextureDebugPanel)
	// can react to mesh selection changes without needing access to SelectionManager.
	void selectionChanged(const QList<int>& selectedIds);
	// Emitted by requestTextureReadback() once the GL readback is complete.
	void textureReadbackReady(QVector<TextureSlotInfo> slots, QString meshName);
	void cameraUpAxisChanged(bool zUp);

public slots:
	void animateViewChange();
	void animateFitAll();
	void animateWindowZoom();
	void animateCenterScreen();
	void onInertiaTimer();
	void stopAnimations();
	void checkAndStopTimers();
	void fitAll();
	void setAutoFitViewOnUpdate(bool update);
	void setSelectionHighlighting(bool highlight);
	void performKeyboardNav();
	void disableLowRes();
	void disableSectionCapsInteractionSuppression();
	void setFloorTexRepeatS(double floorTexRepeatS);
	void setFloorTexRepeatT(double floorTexRepeatT);
	void setFloorOffsetPercent(double value);
	void setSkyBoxFOV(double fov);
	void setPerspFOV(int fovDegrees);
	void setSkyBoxZRotation(int index);
	void setSkyBoxTextureHDRI(bool hdrSet);
	void enableHDRToneMapping(bool hdrToneMapping);
	void enableGammaCorrection(bool gammaCorrection);
	void setScreenGamma(double screenGamma);
	void setHDRToneMappingMode(HDRToneMapMode mode);
	void setEnvMapExposure(double exposure);
	void setIBLExposure(double exposure);

	// Getters for tone mapping and gamma settings
	bool isHDRToneMappingEnabled() const { return _renderCtrl.hdrToneMapping(); }
	bool isGammaCorrectionEnabled() const { return _renderCtrl.gammaCorrection(); }
	HDRToneMapMode getHDRToneMappingMode() const { return _renderCtrl.toneMappingMode(); }
	void showLights(bool showLights);
	void useDefaultLights(bool useDefaultLights);
	void usePunctualLights(bool usePunctualLights);

	// Upload a new GPU light list (e.g. after a per-light checkbox toggle) and
	// sync the hasPunctualLights / lightCount shader uniforms in one call.
	void applyEnabledLightList(const std::vector<GPULight>& enabledLights);
	void useIBL(bool useIBL);
	void showFileReadingProgress(float percent);
	void showMeshLoadingProgress(float percent);
	void showNodeMeshLoadingProgress(int processedNodes, int totalNodes, int processedMeshes, int totalMeshes, bool uvProcessed);
	void swapVisible(bool checked);
	void cancelAssImpModelLoading();
	void onAnimationTick();

	// Accessors for SelectionManager
	QMatrix4x4 getViewMatrix() const { return _viewCtrl.viewMatrix(); }
	QMatrix4x4 getProjectionMatrix() const { return _viewCtrl.projectionMatrix(); }
	QMatrix4x4 getModelViewMatrix() const { return _viewCtrl.modelViewMatrix(); }
	QMatrix4x4 getModelMatrix() const { return _viewCtrl.modelMatrix(); }
	bool isMultiViewActive() const { return _viewCtrl.multiViewActive(); }
	ShaderProgram* getSelectionShader() const { return _renderCtrl.selectionShader(); }
	SelectionManager* getSelectionManager() const { return _selectionManager; }
	bool isMeshAnimationVisibleForSelection(const SceneMesh* mesh) const { return isMeshAnimationVisible(mesh); }
	// Returns the camera configured for the viewport that contains 'pixel'.
	// In multi-view mode the ortho camera is set to the correct orientation
	// (Top/Front/Left) before being returned; the isometric viewport returns
	// the primary camera.  In single-view mode the primary camera is returned.
	GLCamera* getCameraForPoint(const QPoint& pixel);

	static GLMaterial resolveMaterialTextures(GLWidget* w, const GLMaterial& src);

	// Reads back all per-mesh texture slots for meshId via glGetTexImage and
	// emits textureReadbackReady().  Must be called on the GL thread (or the
	// method calls makeCurrent/doneCurrent internally).  meshId is a _meshStore
	// index; pass -1 to emit an empty result and clear the debug panel.
	void requestTextureReadback(int meshId);

	// Enable or disable a specific texture unit for meshId during rendering.
	// When disabled the unit is replaced with a neutral placeholder texture
	// (white for colour channels, tangent-space neutral for normal maps) so the
	// shader still runs but that channel contributes a neutral value.
	// Calls update() to trigger a repaint.
	void setDebugTextureEnabled(int meshId, int unitIndex, bool enabled);

	// Full-state apply for the checkbox panel.  enabledUnits = currently active
	// (not disabled by user); allUnits = all units with real textures on this mesh.
	// Handles the emissive-only special case (uses in-shader isolation automatically).
	void applyDebugTextureState(int meshId,
	                             const QSet<int>& enabledUnits,
	                             const QSet<int>& allUnits);

	// Global single-channel isolation for the channel dropdown.
	// Applies to every mesh in the scene — no selection required.
	// channelId matches shader IDs (1-9 = geometry, 10+ = texture units).
	// channelId == 0 restores normal rendering on all meshes.
	void setGlobalDebugChannel(int channelId);

	// Remove all debug texture overrides for meshId and repaint.
	void clearDebugTextureOverrides(int meshId);

	// Remove all debug texture AND uniform overrides for meshId and repaint.
	void clearAllDebugOverrides(int meshId);

	// Disable/re-enable an entire KHR extension for meshId by zeroing the
	// relevant scalar factor uniforms and neutral-binding its texture units.
	// extensionKey is one of: "Sheen", "Clearcoat", "Iridescence",
	// "Volume / SSS", "Specular", "Anisotropy", "Transmission",
	// "Diffuse Transmission".
	void setDebugExtensionEnabled(int meshId, const QString& extensionKey, bool enabled);

	// Remove all extension-level debug uniform+texture overrides for meshId.
	void clearDebugExtensionOverrides(int meshId);

private slots:
	void showContextMenu(const QPoint& pos);
	void centerDisplayList();
	void setBackgroundColor();
	
protected:
	void initializeGL();
	void createCappingPlanes();
	void resizeGL(int width, int height);
	void paintGL();

	void renderSingleView(QColor& topColor, QColor& botColor);

	void renderMultiView(QColor& topColor, QColor& botColor);
	void applyOverlayPanelStyle(QWidget* wrapper, const QString& objectName);
	void refreshNavigationOverlayStyle();

	void resizeEvent(QResizeEvent* event);
	void mousePressEvent(QMouseEvent*);
	void mouseReleaseEvent(QMouseEvent*);
	void mouseMoveEvent(QMouseEvent*);
	void wheelEvent(QWheelEvent*);
	void keyPressEvent(QKeyEvent* event);
	void keyReleaseEvent(QKeyEvent* event);
	void closeEvent(QCloseEvent* event);

private:

	void createShaderPrograms();
	void syncUniformsToFlatShader();
	void createLights();

	// Fullscreen triangle methods for IBL
	void createFullscreenTriangle();
	void drawFullscreenTriangle();
	void setIBLFaceBasis(QOpenGLShaderProgram* prog, int faceIndex);
	void updateEnvMapRotationMatrix();

	void loadEnvMap();
	void loadIrradianceMap();
	GLuint loadPresetEnvironmentMap(const QString& hdrFilePath);
	bool generatePresetIBLMaps(GLuint sourceCubemap, GLuint& outIrradianceMap, GLuint& outPrefilterMap, GLuint& outSheenPrefilterMap);
	void loadFloor();
	void ensureShadowMapResources();
	void loadGrid();
	void applyFloorPlaneMaterialSettings();
	void syncFloorPlaneAlbedoTexture();
	QVector3D effectiveWorldLightOffset() const;
	QVector3D effectiveWorldLightPosition() const;
	void updateMainLightPosition(float halfObjectSize);
	float updateFloorGeometry();
	void syncDefaultLightColorUniforms();
	void syncPunctualLightUniforms(int lightCount, bool hasPunctualLights);
	bool shouldUseFallbackLightForVisibleScene() const;

	void updatePunctualLights();  // Update lights based on bounding sphere changes
	void setAnimatedLightVisibilityState(const QString& sourceFile, const QVector<bool>& visibleByParsedLight);
	void setAnimatedLightTransformState(const QString& sourceFile, const std::vector<GPULight>& animatedLights);
	void clearAnimatedLightTransformState(const QString& sourceFile);
	void clearAnimatedLightVisibilityState(const QString& sourceFile);
	void setAnimatedMeshVisibilityState(const QString& sourceFile, const QSet<QUuid>& hiddenMeshUuids);
	void clearAnimatedMeshVisibilityState(const QString& sourceFile);
	void recalculateVisibleSceneStats(bool updateMemorySize = false);

	// activeCapPlaneIndex: -1 = no culling, 0 = YZ, 1 = ZX, 2 = XY
	void drawMesh(QOpenGLShaderProgram* prog, int activeCapPlaneIndex = -1);

	// activeClipPlaneIndex: -1 = no clipping (frustum only), 0 = YZ, 1 = ZX, 2 = XY
	void drawOpaqueMeshes(QOpenGLShaderProgram* prog, int activeClipPlaneIndex = -1);
	void drawTransparentMeshes(QOpenGLShaderProgram* prog, int activeClipPlaneIndex = -1);
	void drawMeshesWithClipping(QOpenGLShaderProgram* prog, bool transparentPass);
	void setCommonUniforms(QOpenGLShaderProgram* prog, GLCamera* camera);

	// Visibility culling
	void extractFrustumPlanes();
	bool  isBoundingBoxOutsideFrustum(const BoundingBox& bb) const;
	bool  isMeshOutsideFrustum(const SceneMesh* mesh) const;
	bool  isMeshFullyInsideFrustum(const SceneMesh* mesh) const;
	float computeFullyVisibleMinMeshRadius() const;
	void  updateZoomInLimit();
	bool isBoundingBoxFullyClipped_X(const BoundingBox& bb) const;
	bool isBoundingBoxFullyClipped_Y(const BoundingBox& bb) const;
	bool isBoundingBoxFullyClipped_Z(const BoundingBox& bb) const;
	bool isBoundingBoxFullyKept_X(const BoundingBox& bb) const;
	bool isBoundingBoxFullyKept_Y(const BoundingBox& bb) const;
	bool isBoundingBoxFullyKept_Z(const BoundingBox& bb) const;
	bool isBoundingBoxStraddlesCapPlane(const BoundingBox& bb, int planeIndex) const;
	bool isBoundingBoxInvisibleInAllClipPasses(const BoundingBox& bb) const;
	bool isMeshFullyClipped_X(const SceneMesh* mesh) const;
	bool isMeshFullyClipped_Y(const SceneMesh* mesh) const;
	bool isMeshFullyClipped_Z(const SceneMesh* mesh) const;
	bool isMeshFullyKept_X(const SceneMesh* mesh) const;
	bool isMeshFullyKept_Y(const SceneMesh* mesh) const;
	bool isMeshFullyKept_Z(const SceneMesh* mesh) const;
	bool isMeshStraddlesCapPlane(const SceneMesh* mesh, int planeIndex) const;
	bool isMeshInvisibleInAllClipPasses(const SceneMesh* mesh) const;
	bool isMeshAnimationVisible(const SceneMesh* mesh) const;
	bool isMeshVisible(const SceneMesh* mesh, int activeClipPlaneIndex) const;
	bool sceneHasVisibleTransmissionMaterials() const;
	bool sceneHasVisibleSSSMaterials() const;
	void collectVisibleMeshIdsForPass(int nodeIndex,
	                                  int activeClipPlaneIndex,
	                                  bool wantTransparent,
	                                  std::vector<int>& out) const;

	void drawSectionCapping();
	void drawFloor(const bool& drawReflection = true);
	void drawGrid();
	void drawSkyBox();
	void drawVertexNormals();
	void drawFaceNormals();
	void drawBoundingBoxOverlay();
	void drawDebugOverlay(GLCamera* camera);
	void drawAxis(GLCamera* camera);
	void drawCornerAxis(CornerAxisPosition position);
	void drawTransformGizmo(GLCamera* camera);
	void drawViewCube();
	void drawViewCubeLabels(const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix, float cubeScale);
	BoundingSphere computeTransformGizmoSelectionSphere() const;
	QVector3D computeTransformGizmoPivot() const;
	std::vector<int> activeTransformGizmoSelectionIds() const;
	void applyExplodedViewManualPlacementSessionTransform();
	void syncTransformGizmoToSelection();
	bool beginTransformGizmoDrag(TransformGizmo::Handle handle, const QPoint& pixel);
	bool beginTransformGizmoTranslationDrag(TransformGizmo::Handle handle, const QPoint& pixel);
	void updateTransformGizmoTranslationDrag(const QPoint& pixel);
	void finishTransformGizmoTranslationDrag(bool commit);
	bool beginTransformGizmoScaleDrag(TransformGizmo::Handle handle, const QPoint& pixel, bool uniformScale);
	void updateTransformGizmoScaleDrag(const QPoint& pixel);
	void finishTransformGizmoScaleDrag(bool commit);
	bool beginTransformGizmoRotationDrag(TransformGizmo::Handle handle, const QPoint& pixel);
	void updateTransformGizmoRotationDrag(const QPoint& pixel);
	void finishTransformGizmoRotationDrag(bool commit);
	void drawLights();

	void bindIBLTextures();

	void render(GLCamera* camera);
	void renderToShadowBuffer();
	void renderQuad();
	void renderMeshWithDisplayMode(SceneMesh* mesh, DisplayMode mode);
	void renderMeshExploded(SceneMesh* mesh, DisplayMode mode);

	void gradientBackground(float top_r, float top_g, float top_b, float top_a,
		float bot_r, float bot_g, float bot_b, float bot_a, int gradientStyle);
	void syncCameraWorldUp();
	void rotateCurrentCameraAroundWorldX(float degrees);
	QString sceneUpAxisLabel(SceneUpAxis sceneUpAxis) const;
	void applyAutoOrientCameraConvention(SceneUpAxis sceneUpAxis);
	void warnOnConflictingImportedSceneUpAxis(const QString& fileName, SceneUpAxis sceneUpAxis);

	void loadBgColorSettings();
	QRect viewCubeRect() const;
	QRect viewCubeScreenRect() const;
	void initializeViewCubeLabels();
	bool computeViewCubeRenderState(QRect& viewportRect,
	                               QMatrix4x4& viewMatrix,
	                               QMatrix4x4& projectionMatrix,
	                               QMatrix4x4& modelMatrix,
	                               float& cubeScale) const;
	bool pickViewCubeRegionAtPixel(const QPoint& pixel, QVector3D& outwardNormal, int* regionId = nullptr) const;
	bool handleViewCubeClick(const QPoint& pixel);
	void updateViewCubeHover(const QPoint& pixel, Qt::MouseButtons buttons);
	bool orientCameraToViewCubeNormal(const QVector3D& outwardNormal);

	void splitScreen();

	void animateToRotation(const QQuaternion& targetRotation);
	void setRotations(float xRot, float yRot, float zRot);
	void setZoomAndPan(float zoom, QVector3D pan);
	void setView(QVector3D viewPos, QVector3D viewDir, QVector3D upDir, QVector3D rightDir);
	void fitBoxToScreen(const BoundingBox& box);

	// Collect a sampled set of world-space vertex positions from every visible
	// mesh (≤ 1024 samples per mesh for performance).  Using actual vertices
	// instead of 8 per-mesh AABB corners gives a genuinely tight silhouette:
	// phantom corners from combining per-axis extremes that never coexist in
	// real geometry are eliminated, so the fit zooms in as tight as possible.
	std::vector<QVector3D> collectVisibleCorners() const;

	// Core fit computation on an explicit corner set + explicit view axes.
	// Separating axes from corners lets setViewMode() pass the *destination*
	// quaternion's axes so rotation and zoom animate concurrently.
	// If outCenter is non-null it receives the projected visual centre of the
	// geometry (midpoint of view-space extents), which callers should set as
	// the new orbit/pan target so the scene appears centred on screen.
	float computeFitViewRange(const std::vector<QVector3D>& corners,
	                          const QVector3D& right, const QVector3D& up,
	                          const QVector3D& viewDir,
	                          QVector3D* outCenter = nullptr) const;

	// Convenience: collects visible corners, then calls the core with the
	// provided axes (used by setViewMode with target-orientation axes).
	float computeFitViewRange(const QVector3D& right, const QVector3D& up,
	                          const QVector3D& viewDir,
	                          QVector3D* outCenter = nullptr) const;

	// Convenience: collects visible corners + reads axes from the current
	// view matrix (used by fitAll and projection-toggle).
	float computeFitViewRange(QVector3D* outCenter = nullptr) const;
	float computeOrthographicFitViewRangeForViewport(
		const std::vector<QVector3D>& corners,
		const QVector3D& right,
		const QVector3D& up,
		const QVector3D& viewDir,
		int viewportWidth,
		int viewportHeight,
		QVector3D* outCenter = nullptr,
		const QVector3D& eyePos = QVector3D(0, 0, 0)) const;
	QVector3D computeVisibleWorldCenter(const std::vector<QVector3D>& corners) const;
	float computeSharedOrthographicMultiViewRange(
		const std::vector<QVector3D>& corners,
		int viewportWidth,
		int viewportHeight,
		const QVector3D& eyePos = QVector3D(0, 0, 0)) const;
	void configureOrthoSubviewCamera(ViewMode viewMode,
		const std::vector<QVector3D>& corners,
		int viewportWidth,
		int viewportHeight,
		const QVector3D& sharedCenter,
		float sharedViewRange);

	float highestModelZ();
	float lowestModelZ();
	bool positionGameplayCameraForScene(GLCamera::CameraMode mode);

	QList<int> sweepSelect(const QPoint& pixel, bool addToSelection = false);  // Sweep selection using rubber band
	QVector3D get3dTranslationVectorFromMousePoints(const QPoint& start, const QPoint& end);
	unsigned int loadTextureFromFile(const char* path,
		GLenum wrapS = GL_REPEAT, GLenum wrapT = GL_REPEAT,
		GLenum minFilter = GL_LINEAR_MIPMAP_LINEAR, GLenum magFilter = GL_LINEAR,
		bool flipY = false);
	void setupClippingUniforms(QOpenGLShaderProgram* prog, QVector3D pos);

	void onMeshBatchReady(const std::vector<AssImpMeshData>& batch);
	SceneMesh* createMeshFromData(const AssImpMeshData& meshData);
	void syncFileNodeTransforms(const QString& sourceFile);
	void reapplyGltfCameraAfterTransform();
	void applyNodeTransformsToMeshes(const QString& sourceFile,
		const AnimationRuntimeController::RuntimeAnimationFileState& runtime,
		AnimationRuntimeController::AnimationSampleResult& result,
		SceneNode* fileNode);
	void applyMorphTargetWeights(const QString& sourceFile,
		const AnimationRuntimeController::AnimationSampleResult& result);
	void applyAnimatedMaterialChanges(const AnimationRuntimeController::AnimationSampleResult& result);
	void applyAnimatedMeshVisibility(const QString& sourceFile,
		const AnimationRuntimeController::RuntimeAnimationFileState& runtime,
		const AnimationRuntimeController::AnimationSampleResult& result,
		SceneNode* fileNode);
	void applyAnimatedLightTransforms(const QString& sourceFile,
		const AnimationRuntimeController::RuntimeAnimationFileState& runtime,
		const AnimationRuntimeController::AnimationSampleResult& result,
		SceneNode* fileNode);
	void applyAnimatedCamera(const QString& sourceFile,
		const AnimationRuntimeController::RuntimeAnimationFileState& runtime,
		const AnimationRuntimeController::AnimationSampleResult& result);
	void applyAnimationPose(const QString& sourceFile, int clipIndex, double timeSeconds);
	void resetAnimationPose(const QString& sourceFile);
	void updateAnimatedMeshState(const QString& sourceFile,
		const QHash<QUuid, QMatrix4x4>& worldTransformsByNodeUuid);

	GLuint createGPUTextureFromImage(const QImage& image, const TextureSamplerSettings& samplers);
	GLuint uploadDecodedTextureImage(const QImage& image, const TextureSamplerSettings& samplers);
	GLuint uploadKtx2TextureImage(const QString& path, const std::string& mapType, const TextureSamplerSettings& samplers);
	GLuint uploadDecodedTexture(GLMaterial::Texture& texture, const QImage& image);
	GLuint uploadKtx2Texture(const QString& path, const std::string& mapType, GLMaterial::Texture& texture);
	UVMethod promptLargeModelUVDecision(int totalTriangles, UVMethod currentMethod);
	void retainTexture(unsigned int texId);
	void releaseTexture(unsigned int texId);

public:
	unsigned int getOrCreateTextureCached(const QString& cacheKey,
		const QImage& image,
		const TextureSamplerSettings& samplers = TextureSamplerSettings());
	unsigned int getOrLoadKtx2TextureCached(const QString& path,
		const std::string& mapType,
		const TextureSamplerSettings& samplers = TextureSamplerSettings());
	unsigned int getOrLoadTextureCached(const QString& path,
		const TextureSamplerSettings& samplers = TextureSamplerSettings());

private:
		
	// --- Transmission Buffer Methods ---
	void initTransmissionBuffer();
	void renderToTransmissionBuffer(GLCamera* camera, const QColor& topColor, const QColor& botColor);
	void cleanupTransmissionBuffer();
	void resizeTransmissionBuffer(int width, int height);

	// --- SSS (Subsurface Scattering) Buffer Methods ---
	void initSSSBuffer();
	void renderToSSSBuffer(GLCamera* camera);
	void resizeSSSBuffer(int width, int height);
	void cleanupSSSBuffer();

	void createWhiteTexture();

	void generateCubemapMipmaps(GLuint cubemapTexture);

	void setSectionCapsInteractionSuppressed(bool suppressed);
private:
	SceneRuntime _sceneRuntime;

	AnimationRuntimeController _animCtrl;

	ExplodedViewRuntimeController _explodedViewCtrl;

	// Render-pipeline resources — owned here; GLWidget aliases every field by
	// reference so all existing call sites in GLWidget.cpp remain unchanged.
	// Declaration order: _renderCtrl must come before all its aliases.
	SceneRenderController _renderCtrl;

	// Viewport interaction state — owned here; GLWidget aliases every field by
	// reference so all existing call sites in GLWidget.cpp remain unchanged.
	// Declaration order: _viewCtrl must come before all its aliases.
	ViewportInteractionController _viewCtrl;


	ViewToolbar* _viewToolbar;

	QSet<int> _keys;
	DisplayMode _displayMode;
	RenderingMode _renderingMode;
	QColor      _bgTopColor;
	QColor      _bgBotColor;
	int _gradientStyle = 0; // 0=Vertical, 1=Horizontal, 2=TopLeftToBottomRight, 3=TopRightToBottomLeft
	int _modelNum;
	QImage _texImage, _texBuffer;
	float _floorTexRepeatS, _floorTexRepeatT;
	TextRenderer* _textRenderer;
	TextRenderer* _axisTextRenderer;
	QString _labelTop, _labelFront, _labelLeft, _labelIsometric, _labelDimetric, _labelTrimetric;
	QString _labelAxisX, _labelAxisY, _labelAxisZ;
	QString _labelNumMeshes;
	QString _modelName;

	bool _selectionHighlighting;

	QRubberBand* _rubberBand;
	QRubberBand* _selectRect;
	QTimer* _inertiaTimer = nullptr;

	// Selection manager instance (owns all selection logic and state)
	SelectionManager* _selectionManager = nullptr;

	QVector4D _defaultLightColor;
	QVector4D _ambientLight;
	QVector4D _diffuseLight;
	QVector4D _specularLight;

	QVector3D _lightPosition;
	float _lightOffsetX;
	float _lightOffsetY;
	float _lightOffsetZ;

	QMatrix4x4 _lightSpaceMatrix;



	QImage					 _floorTexImage;
	float                    _floorSize;
	float 					 _floorSizeFactor;
	float					 _floorOffsetPercent;
	float                    _floorPlaneZ;
	QVector3D                _floorCenter;


	// _meshStore, _displayedObjectsIds, _hiddenObjectsIds → SceneRuntime (Phase 5)
	// RuntimeVisibilityNode struct + BVH fields → SceneRuntime (Phase 5)
	// _pendingSceneUuids, _centerScreenObjectIDs, _visibleSwapped → SceneRuntime (Phase 5)
	QPointer<QWidget> _navigationOverlayPanel;

	QVBoxLayout* _editorLayout;
	QFormLayout* _lowerLayout;
	QFormLayout* _upperLayout;

	ClippingPlanesEditor* _clippingPlanesEditor;
	ExplodedViewPanel*    _explodedViewPanel;
	// ExplodedViewManager + hints cache + manual session → ExplodedViewRuntimeController (Phase 9)
	PlaneRenderable* _clippingPlaneXY;
	PlaneRenderable* _clippingPlaneYZ;
	PlaneRenderable* _clippingPlaneZX;


	GLCamera* _primaryCamera;
	GLCamera* _orthoViewsCamera;

	// Active glTF camera → AnimationRuntimeController (Phase 8)


	QTimer* _keyboardNavTimer;
	QTimer* _animateViewTimer;
	QTimer* _animateFitAllTimer;
	QTimer* _animateWindowZoomTimer;
	QTimer* _animateCenterScreenTimer;


	FloorPlane* _floorPlane;
	PlaneRenderable* _gridPlane;
	CubeRenderable* _skyBox;
	// _fsTriVAO/VBO, _skyBoxFaces, _skyBoxFOV/_skyBoxZRotation, gamma/HDR/tone-map settings,

	ConeRenderable* _axisCone;
	ViewCubeMesh* _viewCube = nullptr;
	TransformGizmo* _transformGizmo = nullptr;
	// Gizmo drag state, viewCubeHoveredRegionId, customViewAnimationActive,
	// Manual placement session fields → ExplodedViewRuntimeController (Phase 9)
	CubeRenderable* _lightCube;
	SphereRenderable* _lightSphere;
	bool _showLights;

	ModelViewer* _viewer;


	unsigned long long _displayedObjectsMemSize;

	AssImpModelLoader* _assimpModelLoader;
	KTX2Loader _ktx2Loader;
	GPUCapabilities _gpuCapabilities;
	// _assimpScene, _globalScene, _globalSceneTransform → SceneRuntime (Phase 5)
	// _progressiveLoadingEnabled, _cancelRequested, _loadCancelled → SceneRuntime (Phase 5)
	// _texCache, _texRefCount → SceneRuntime (Phase 5)


	ClippingPlaneHatchMode _hatchMode = ClippingPlaneHatchMode::PROCEDURAL;
	HatchPattern _hatchPattern = HatchPattern::DIAGONAL_45;
	int   _hatchTiling = 50;
	float _hatchThickness = 0.05f;
	float _hatchIntensity = 1.0f;
	int _hatchLayers = 3;
	QVector3D _hatchLineColor = QVector3D(0.0f, 0.0f, 0.0f);
	QString _hatchTexturePath;

	AdaptiveShadowMapper shadowMapper;

	float _originalBoundingRadius = 1.0f;
	// _animatedLight* / _animatedMesh* → AnimationRuntimeController (Phase 8)

	// Derive the user model transform for one file directly from its meshes'
	// TRS state.  Returns true (and fills outTransform) only when every mesh
	// of the file carries the same non-identity transformation — i.e. the
	// user applied a model-level transform.  Lights and glTF cameras of that
	// file follow this exact matrix; the visible-scene bounding sphere plays
	// no role, so hide/show/delete of other models cannot disturb them.
	void updateOverlayEditorTheme();

	void applyGltfCameraEntryTransform(const GltfCameraEntry& cam);

	// Animation playback fields → AnimationRuntimeController (Phase 8)
};

#endif
