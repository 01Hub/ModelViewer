#ifndef GLWIDGET_H
#define GLWIDGET_H

#include "AdaptiveShadowMapper.h"
#include "BoundingSphere.h"
#include "GLCamera.h"
#include "TriangleMesh.h"
#include "TransformCommand.h"
#include "ShaderProgram.h"
#include "AssImpModelLoader.h"
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

/* Custom OpenGL Viewer Widget */

namespace Mvf { struct Document; }

class TextRenderer;
class ClippingPlanesEditor;
class AssImpModelLoader;
class Plane;
class Cube;
class Cone;
class Sphere;
class ViewCubeMesh;

class AssImpModelLoader;

class ModelViewer;

enum class ViewMode { TOP, BOTTOM, LEFT, RIGHT, FRONT, BACK, ISOMETRIC, DIMETRIC, TRIMETRIC, NONE };
enum class ViewProjection { ORTHOGRAPHIC, PERSPECTIVE };
enum class DisplayMode { SHADED, WIREFRAME, WIRESHADED, REALSHADED, FLATSHADED };
enum class RenderingMode { ADS_BLINN_PHONG, PHYSICALLY_BASED_RENDERING };
enum class CornerAxisPosition { TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT };
enum class ClippingPlaneHatchMode { PROCEDURAL, TEXTURE };
enum class HatchPattern { DIAGONAL_45 = 0, DIAGONAL_135 = 1, HORIZONTAL = 2, VERTICAL = 3, GRID = 4, DIAGONAL_CROSS = 5 };
enum class HDRToneMapMode { KhronosPbrNeutral, ACES_Narkowicz, ACES_Hill, AECS_Hill_Exposure_Boost, Uncharted2ToneMapping, Reinhard };
enum class GroundMode { None = 0, Floor = 1, Grid = 2 };

struct TextureSamplerSettings
{
	GLenum wrapS = GL_REPEAT;
	GLenum wrapT = GL_REPEAT;
	GLenum minFilter = GL_LINEAR_MIPMAP_LINEAR;
	GLenum magFilter = GL_LINEAR;

	bool operator==(const TextureSamplerSettings& other) const
	{
		return wrapS == other.wrapS &&
			wrapT == other.wrapT &&
			minFilter == other.minFilter &&
			magFilter == other.magFilter;
	}
};

struct CachedTextureEntry
{
	QImage image;
	int imageWidth = 0;
	int imageHeight = 0;
	int imageComponents = 0;
	GLenum imageFormat = GL_RGBA;

	GLuint lastGPUTexture = 0;
	TextureSamplerSettings lastSamplerSettings;
	int refCount = 0;
};

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
	void setProjection(ViewProjection proj);
	void setCameraMode(GLCamera::CameraMode mode);

	void setMultiView(bool active) { _multiViewActive = active; }
	void setRotationActive(bool active);
	void setPanningActive(bool active);
	void setZoomingActive(bool active);

	void setShowCenterAxisOverride(bool show) { _userShowAxisOverride = show; update(); }
	void setShowCornerAxisOverride(bool show) { _userShowCornerAxisOverride = show; update(); }
	void setShowViewCubeOverride(bool show) { _showViewCubeOverride = show; if (!show) _viewCubeHoveredRegionId = -1; update(); }
	void setCornerAxisPosition(CornerAxisPosition position);

	void beginWindowZoom();
	void performWindowZoom();

	void setDisplayList(const std::vector<int>& ids);
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

	void showShadows(bool show);
	void showSelfShadows(bool show);
	void showEnvironment(bool show);
	void showSkyBox(bool show);
	void blurSkyBox(bool blur);
	void setSkyBoxBlurPercent(int percent);
	void showReflections(bool show);
	void setGroundMode(GroundMode mode);
	GroundMode groundMode() const { return _groundMode; }
	void showFloor(bool show);
	bool isFloorShown() { return _groundMode == GroundMode::Floor; }
	bool isGridShown() const { return _groundMode == GroundMode::Grid; }
	void showFloorTexture(bool show);
	void setFloorTexture(QImage img);

	std::vector<TriangleMesh*> getMeshStore() const { return _meshStore; }

	void addToDisplay(TriangleMesh*);
	void removeFromDisplay(int index);
	void centerScreen(std::vector<int> selectedIDs);
	void select(int id);
	void deselect(int id);

	bool loadAssImpModel(const QString& fileName, const UVMethod& uvMethod, QString& error, bool progressiveLoading = false);

	bool generateUVsForMeshes(const std::vector<int>& ids, const UVMethod& uvMethod, const UVConfig& uvConfig, QString& error);

	aiScene* getAssImpScene() const { return _globalScene; }
	glm::mat4 getGlobalSceneTransform() const { return _globalSceneTransform; }

	void invertADSOpacityTexMap(const std::vector<int>& ids, const bool& inverted);

	void setMaterialToObjects(const std::vector<int>& ids, const GLMaterial& mat);
	void setTexturesToObjects(const std::vector<int>& ids, const GLMaterial& mat);
	void synchronizeTextureCache(const GLMaterial* material, GLMaterial::TextureType type);
	void clearTextureCache();

	void setPBRAlbedoColor(const std::vector<int>& ids, const QColor& col);
	void setPBRMetallic(const std::vector<int>& ids, const float& val);
	void setPBRRoughness(const std::vector<int>& ids, const float& val);

	void clearPBRTexMaps(const std::vector<int>& ids);
	void clearPBRAlbedoTexMap(const std::vector<int>& ids);
	void clearPBRMetallicTexMap(const std::vector<int>& ids);
	void clearPBRRoughnessTexMap(const std::vector<int>& ids);
	void clearPBRNormalTexMap(const std::vector<int>& ids);
	void clearPBRAOTexMap(const std::vector<int>& ids);

	void invertPBROpacityTexMap(const std::vector<int>& ids, const bool& inverted);
	void clearPBROpacityTexMap(const std::vector<int>& ids);

	void clearPBRHeightTexMap(const std::vector<int>& ids);

	void clearPBRTransmissionTexMap(const std::vector<int>& ids);

	void clearPBRIORTexMap(const std::vector<int>& ids);

	void clearPBRSheenColorTexMap(const std::vector<int>& ids);

	void clearPBRSheenRoughnessTexMap(const std::vector<int>& ids);

	void clearPBRClearcoatTexMap(const std::vector<int>& ids);

	void clearPBRClearcoatRoughnessTexMap(const std::vector<int>& ids);

	void clearPBRClearcoatNormalTexMap(const std::vector<int>& ids);

	void setTransformation(const std::vector<int>& ids, const QVector3D& trans, const QVector3D& rot, const QVector3D& scale);
	void bakeTransformation(const std::vector<int>& ids);
	void resetTransformation(const std::vector<int>& ids);
	void applyTransforms(const QMap<int, TransformState>& transforms);

	void setSkyBoxTextureFolder(QString folder);
	bool loadCubemapFromSingleHDR(const QString& filePath);
	bool convertEquirectangularToCubemap(const QString& filePath);
	bool convertEquirectangularToCubemapQuad(const QString& filePath);

	void renderConversionCube();

	void setAnisotropicFilteringLevel(int level) { _anisotropicFilteringLevel = level; }
	int getAnisotropicFilteringLevel() const { return _anisotropicFilteringLevel; }

	void setTransmissionEnabled(const bool& enabled);
	bool isTransmissionEnabled() const { return _transmissionEnabled; }
	void setActiveAnimation(const QString& sourceFile, int clipIndex);
	void setAnimationPlaying(bool playing);
	void seekAnimation(double timeSeconds);
	void setAnimationLooping(bool looping);
	void setAnimationPlaybackSpeed(double speed);
	void refreshAnimationMaterialState(const QString& sourceFile);
	QString activeAnimationFile() const { return _activeAnimationFile; }
	int activeAnimationClip() const { return _activeAnimationClip; }
	double currentAnimationTimeSeconds() const { return _animationCurrentTimeSeconds; }
	bool isAnimationPlaying() const { return _animationPlaying; }
	bool isAnimationLooping() const { return _animationLooping; }
	double animationPlaybackSpeed() const { return _animationPlaybackSpeed; }

	// glTF camera switching
	void activateGltfCamera(const QString& sourceFile, int cameraIndex);
	void resetToSystemCamera();
	bool isGltfCameraActive()     const { return _activeGltfCameraIndex >= 0; }
	QString activeGltfCameraFile()  const { return _activeGltfCameraFile; }
	int     activeGltfCameraIndex() const { return _activeGltfCameraIndex; }

public:
	float getXTran() const;
	void setXTran(const float& xTran);

	float getYTran() const;
	void setYTran(const float& yTran);

	float getZTran() const;
	void setZTran(const float& zTran);

	float getXRot() const;
	void setXRot(const float& xRot);

	float getYRot() const;
	void setYRot(const float& yRot);

	float getZRot() const;
	void setZRot(const float& zRot);

	float getXScale() const;
	void setXScale(const float& xScale);

	float getYScale() const;
	void setYScale(const float& yScale);

	float getZScale() const;
	void setZScale(const float& zScale);

	QVector4D getDefaultLightColor() const;
	void setDefaultLightColor(const QVector4D& defaultLightColor);

	QVector3D getLightPosition() const;
	void setLightOffset(const QVector3D& offset);

	std::vector<GPULight> getParsedLights() const { return _originalParsedLights; }

	float getFloorSize() const { return _floorSize; }

	bool isShaded() const;
	DisplayMode getDisplayMode() const;
	void setDisplayMode(DisplayMode mode);

	bool isVertexNormalsShown() const;
	void setShowVertexNormals(bool showVertexNormals);

	bool isFaceNormalsShown() const;
	void setShowFaceNormals(bool showFaceNormals);

	std::vector<int> getDisplayedObjectsIds() const;

	bool isVisibleSwapped() const;

	BoundingSphere getBoundingSphere() const;
	int processSelection(const QPoint& pixel);

	QColor getBgTopColor() const;
	void setBgTopColor(const QColor& bgTopColor);


	QColor getBgBotColor() const;
	void setBgBotColor(const QColor& bgBotColor);

	int getBgGradientStyle() const { return _gradientStyle; }
	void setBgGradientStyle(int style) { _gradientStyle = style; }

	RenderingMode getRenderingMode() const;
	void setRenderingMode(const RenderingMode& renderingMode);

	void setCappingPlanesEnabled(const bool& enabled) { _cappingEnabled = enabled; }
	bool cappingPlanesEnabled() const { return _cappingEnabled; }

	void setYZClippingEnabled(const bool& enabled) { _clipYZEnabled = enabled; }
	bool yzClippingEnabled() const { return _clipYZEnabled; }
	void setZXClippingEnabled(const bool& enabled) { _clipZXEnabled = enabled; }
	bool zxClippingEnabled() const { return _clipZXEnabled; }
	void setXYClippingEnabled(const bool& enabled) { _clipXYEnabled = enabled; }
	bool xyClippingEnabled() const { return _clipXYEnabled; }

	void setClippingXFlipped(const bool& flipped) { _clipXFlipped = flipped; }
	bool clippingXFlipped() const { return _clipXFlipped; }
	void setClippingYFlipped(const bool& flipped) { _clipYFlipped = flipped; }
	bool clippingYFlipped() const { return _clipYFlipped; }
	void setClippingZFlipped(const bool& flipped) { _clipZFlipped = flipped; }
	bool clippingZFlipped() const { return _clipZFlipped; }

	void setClippingXCoeff(const float& coeff) { _clipXCoeff = coeff; }
	float clippingXCoeff() const { return _clipXCoeff; }
	void setClippingYCoeff(const float& coeff) { _clipYCoeff = coeff; }
	float clippingYCoeff() const { return _clipYCoeff; }
	void setClippingZCoeff(const float& coeff) { _clipZCoeff = coeff; }
	float clippingZCoeff() const { return _clipZCoeff; }

	bool getHdrToneMapping() const;
	bool getGammaCorrection() const;
	float getScreenGamma() const;

	// Environment mapping accessors
	// index: 0 = ViewerIBL, 1 = Studio, 2 = Outdoor, 3 = Office
	GLuint getEnvironmentMap(int index = 0, bool regenerate = false);
	GLuint getIrradianceMap(int index = 0, bool regenerate = false);
	GLuint getPrefilterMap(int index = 0, bool regenerate = false);
	GLuint getSheenPrefilterMap(int index = 0, bool regenerate = false);
	unsigned int getPrefilterMipLevels() const { return _prefilterMipLevels; }
	unsigned int getSheenPrefilterMipLevels() const { return _sheenPrefilterMipLevels; }
	GLuint getBrdfLUT() const { return _brdfLUTTexture; }
	GLuint getCharlieLUT() const { return _charlieLUTTexture; }
	GLuint getSheenELUT() const { return _sheenELUTTexture; }
	bool isEnvironmentMapEnabled() const { return _envMapEnabled; }
	bool isIBLEnabled() const { return _useIBL; }
	float getIBLExposure() const { return _iblExposure; }
	QString getCurrentSkyboxFolder() const { return _currentSkyboxFolder; }

	ViewToolbar* getViewToolbar() const { return _viewToolbar; }

	bool areLightsShown() const;

	void cleanUpShaders();

	// Recycle bin operations (used by DeleteCommand)
	void moveToRecycleBin(const QUuid& uuid, int originalIndex);
	bool restoreFromRecycleBin(const QUuid& uuid);
	void permanentlyDeleteFromBin(const QUuid& uuid);

	// Query methods
	bool isInRecycleBin(const QUuid& uuid) const;
	QVector<QUuid> getRecycleBinUuids() const;

	// UUID lookup methods
	TriangleMesh* getMeshByUuid(const QUuid& uuid) const;
	TriangleMesh* getMeshByIndex(int index) const;
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
		std::vector<Vertex>       vertices;
		std::vector<unsigned int> indices;
		GLMaterial   material;
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

	/// GL-only upload: creates AssImpMesh objects, uploads VBOs and
	/// textures, and populates the display list.  Must run on the main
	/// (GL) thread.  Updates the progress bar between meshes.
	bool uploadPreparedMvfMeshes(const QVector<PreparedMvfMesh>& meshes);

	// Legacy combined entry point (kept for compatibility).
	bool loadMvfMeshes(const Mvf::Document& document,
	                    const QByteArray& geometryChunk,
	                    const QByteArray& imageChunk);

	/// Accessor for the foreground shader (for pre-load shader validation).
	ShaderProgram* getShader() const { return _fgShader.get(); }

	void setSectionCapsDynamicEnabled(bool enabled);

	struct RuntimeNodeTransform
	{
		QVector3D translation = QVector3D(0.0f, 0.0f, 0.0f);
		QQuaternion rotation;
		QVector3D scale = QVector3D(1.0f, 1.0f, 1.0f);
	};

	struct RuntimeAnimationFileState
	{
		GltfAnimationData data;
		QHash<QString, RuntimeNodeTransform> defaultNodeTransforms;
		QHash<QString, QVector<float>> defaultNodeMorphWeights;
		QHash<QUuid, GLMaterial> defaultMeshMaterials;
		QMultiHash<int, QUuid> meshUuidsByMaterialIndex;
	};

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
	// Forwarded from SelectionManager so external panels (e.g. TextureDebugPanel)
	// can react to mesh selection changes without needing access to SelectionManager.
	void selectionChanged(const QList<int>& selectedIds);
	// Emitted by requestTextureReadback() once the GL readback is complete.
	void textureReadbackReady(QVector<TextureSlotInfo> slots, QString meshName);

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
	void setSkyBoxZRotation(int index);
	void setSkyBoxTextureHDRI(bool hdrSet);
	void enableHDRToneMapping(bool hdrToneMapping);
	void enableGammaCorrection(bool gammaCorrection);
	void setScreenGamma(double screenGamma);
	void setHDRToneMappingMode(HDRToneMapMode mode);
	void setEnvMapExposure(double exposure);
	void setIBLExposure(double exposure);

	// Getters for tone mapping and gamma settings
	bool isHDRToneMappingEnabled() const { return _hdrToneMapping; }
	bool isGammaCorrectionEnabled() const { return _gammaCorrection; }
	HDRToneMapMode getHDRToneMappingMode() const { return _toneMappingMode; }
	void showLights(bool showLights);
	void useDefaultLights(bool useDefaultLights);
	void usePunctualLights(bool usePunctualLights);
	void useIBL(bool useIBL);
	void showFileReadingProgress(float percent);
	void showMeshLoadingProgress(float percent);
	void showNodeMeshLoadingProgress(int processedNodes, int totalNodes, int processedMeshes, int totalMeshes, bool uvProcessed);
	void swapVisible(bool checked);
	void cancelAssImpModelLoading();
	void onAnimationTick();

	// Accessors for SelectionManager
	QMatrix4x4 getViewMatrix() const { return _viewMatrix; }
	QMatrix4x4 getProjectionMatrix() const { return _projectionMatrix; }
	QMatrix4x4 getModelViewMatrix() const { return _modelViewMatrix; }
	bool isMultiViewActive() const { return _multiViewActive; }
	ShaderProgram* getSelectionShader() const { return _selectionShader.get(); }
	SelectionManager* getSelectionManager() const { return _selectionManager; }
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

	// Emits selectionChanged() with the given list.  Called by
	// ModelViewer::handleTreeWidgetSelectionChanged() so that panels connected
	// to this signal (e.g. TextureDebugPanel) stay in sync with tree-widget
	// driven selection changes, including full deselection (empty list).
	void broadcastSelectionChanged(const QList<int>& ids) { emit selectionChanged(ids); }

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
	void loadGrid();
	void applyFloorPlaneMaterialSettings();
	void syncFloorPlaneAlbedoTexture();
	void updateMainLightPosition(float halfObjectSize);
	float updateFloorGeometry();
	void syncDefaultLightColorUniforms();

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
	bool isMeshOutsideFrustum(const TriangleMesh* mesh) const;
	bool isMeshFullyClipped_X(const TriangleMesh* mesh) const;
	bool isMeshFullyClipped_Y(const TriangleMesh* mesh) const;
	bool isMeshFullyClipped_Z(const TriangleMesh* mesh) const;
	bool isMeshFullyKept_X(const TriangleMesh* mesh) const;
	bool isMeshFullyKept_Y(const TriangleMesh* mesh) const;
	bool isMeshFullyKept_Z(const TriangleMesh* mesh) const;
	bool isMeshStraddlesCapPlane(const TriangleMesh* mesh, int planeIndex) const;
	bool isMeshInvisibleInAllClipPasses(const TriangleMesh* mesh) const;
	bool isMeshAnimationVisible(const TriangleMesh* mesh) const;
	bool isMeshVisible(const TriangleMesh* mesh, int activeClipPlaneIndex) const;

	void drawSectionCapping();
	void drawFloor(const bool& drawReflection = true);
	void drawGrid();
	void drawSkyBox();
	void drawVertexNormals();
	void drawFaceNormals();
	void drawAxis();
	void drawCornerAxis(CornerAxisPosition position);
	void drawViewCube();
	void drawViewCubeLabels(const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix, float cubeScale);
	void drawLights();

	void bindIBLTextures();

	void render(GLCamera* camera);
	void renderToShadowBuffer();
	void renderQuad();
	void renderMeshWithDisplayMode(TriangleMesh* mesh, DisplayMode mode);

	void gradientBackground(float top_r, float top_g, float top_b, float top_a,
		float bot_r, float bot_g, float bot_b, float bot_a, int gradientStyle);

	void loadBgColorSettings();
	float groundPlaneZ();
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

	float highestModelZ();
	float lowestModelZ();

	QList<int> sweepSelect(const QPoint& pixel, bool addToSelection = false);  // Sweep selection using rubber band
	unsigned int colorToIndex(const QColor& color);
	QColor indexToColor(const unsigned int& index);
	QRect getViewportFromPoint(const QPoint& pixel);
	QRect getClientRectFromPoint(const QPoint& pixel);
	QVector3D get3dTranslationVectorFromMousePoints(const QPoint& start, const QPoint& end);
	unsigned int loadTextureFromFile(const char* path,
		GLenum wrapS = GL_REPEAT, GLenum wrapT = GL_REPEAT,
		GLenum minFilter = GL_LINEAR_MIPMAP_LINEAR, GLenum magFilter = GL_LINEAR,
		bool flipY = false);
	void setupClippingUniforms(QOpenGLShaderProgram* prog, QVector3D pos);

	void onMeshBatchReady(const std::vector<AssImpMeshData>& batch);
	AssImpMesh* createMeshFromData(const AssImpMeshData& meshData);
	void syncFileNodeTransforms(const QString& sourceFile);
	void applyAnimationPose(const QString& sourceFile, int clipIndex, double timeSeconds);
	void resetAnimationPose(const QString& sourceFile);
	void updateAnimatedMeshState(const QString& sourceFile,
		const QHash<QString, QMatrix4x4>& worldTransforms);

	GLuint createGPUTextureFromImage(const QImage& image, const TextureSamplerSettings& samplers);
	GLuint uploadDecodedTextureImage(const QImage& image, const TextureSamplerSettings& samplers);
	GLuint uploadKtx2TextureImage(const QString& path, const std::string& mapType, const TextureSamplerSettings& samplers);
	GLuint uploadDecodedTexture(GLMaterial::Texture& texture, const QImage& image);
	GLuint uploadKtx2Texture(const QString& path, const std::string& mapType, GLMaterial::Texture& texture);
	UVMethod promptLargeModelUVDecision(int totalTriangles, UVMethod currentMethod);
	unsigned int getOrCreateTextureCached(const QString& cacheKey,
		const QImage& image,
		const TextureSamplerSettings& samplers = TextureSamplerSettings());
	unsigned int getOrLoadKtx2TextureCached(const QString& path,
		const std::string& mapType,
		const TextureSamplerSettings& samplers = TextureSamplerSettings());
	unsigned int getOrLoadTextureCached(const QString& path,
		const TextureSamplerSettings& samplers = TextureSamplerSettings());
	void retainTexture(unsigned int texId);
	void releaseTexture(unsigned int texId);
		
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

	GLuint _whiteTexture = 0;
	void createWhiteTexture();

	void generateCubemapMipmaps(GLuint cubemapTexture);

	void setSectionCapsInteractionSuppressed(bool suppressed);
	float groundPlaneScaleFactor() const;
	float groundPlaneExtent() const;

private:
	ViewToolbar* _viewToolbar;

	QSet<int> _keys;
	DisplayMode _displayMode;
	RenderingMode _renderingMode;
	QColor      _bgTopColor;
	QColor      _bgBotColor;
	int _gradientStyle = 0; // 0=Vertical, 1=Horizontal, 2=TopLeftToBottomRight, 3=TopRightToBottomLeft
	bool _windowZoomActive;
	bool _viewZooming;
	bool _viewPanning;
	bool _viewRotating;
	int _modelNum;
	QImage _texImage, _texBuffer;
	float _floorTexRepeatS, _floorTexRepeatT;
	TextRenderer* _textRenderer;
	TextRenderer* _axisTextRenderer;
	QString _labelTop, _labelFront, _labelLeft, _labelIsometric, _labelDimetric, _labelTrimetric;
	QString _labelAxisX, _labelAxisY, _labelAxisZ;
	QString _labelNumMeshes;
	QString _modelName;

	QVector3D _currentTranslation;
	QQuaternion _currentRotation;
	QQuaternion _customViewTargetRotation;
	float _slerpStep;
	float _slerpFrac;

	float _currentViewRange;
	float _scaleFrac;

	float _viewRange;
	float _viewBoundingSphereDia;
	float _FOV;

	bool _autoFitViewOnUpdate;
	bool _selectionHighlighting;

	QPoint _leftButtonPoint;
	QPoint _rightButtonPoint;
	QPoint _middleButtonPoint;

	QPoint _lastPanPoint;
	int _lastZoomDirection = 0; // +1 for zoom in, -1 for zoom out, 0 for none
	float _lastZoomStep = 1.05f;
	QVector3D _lastZoomPanVector;
	QVector3D _inertiaZoomPanVelocity = QVector3D(0, 0, 0);

	// Inertia state for mouse actions
	QVector2D _inertiaPanVelocity;
	float _inertiaZoomVelocity = 0.0f;
	QVector2D _inertiaRotateVelocity;
	QTimer* _inertiaTimer = nullptr;
	float _inertiaDamping = 0.8f; // Damping factor (tweak as needed)

	bool _mouseMovedSincePress = false;
	qint64 _lastMouseMoveTime = 0;

	QPoint _lastMousePos;
	qint64 _lastMouseTime = 0;

	QRubberBand* _rubberBand;
	QRubberBand* _selectRect;
	QVector3D _rubberBandPan;
	GLfloat _rubberBandZoomRatio;
	float _rubberBandRadius;
	QVector3D _rubberBandCenter;
	QList<int> _selectedIDs;

	// Selection manager instance (owns all selection logic and state)
	SelectionManager* _selectionManager = nullptr;

	// FBO resources for color picking (managed by GLWidget)
	unsigned int _selectionFBO = 0;
	unsigned int _selectionRBO = 0;        // Color render buffer
	unsigned int _selectionDBO = 0;        // Depth render buffer

	// Selection state tracking
	bool _shiftDragActive = false;          // Track if Shift was held during drag start
	QPoint _sweepStartPoint;                // Track sweep selection start point for rubber band

	bool _multiViewActive;

	bool _showAxis;
	bool _userShowAxisOverride;
	bool _userShowCornerAxisOverride;

	float _clipXCoeff;
	float _clipYCoeff;
	float _clipZCoeff;

	float _clipDX;
	float _clipDY;
	float _clipDZ;

	bool _clipYZEnabled;
	bool _clipZXEnabled;
	bool _clipXYEnabled;

	bool _clipXFlipped;
	bool _clipYFlipped;
	bool _clipZFlipped;

	// Frustum planes extracted each frame for AABB culling (Gribb-Hartmann, world space)
	QVector4D _frustumPlanes[6];

	bool _showVertexNormals;
	bool _showFaceNormals;

	bool _envMapEnabled;
	bool _shadowsEnabled;
	bool _selfShadowsEnabled;
	bool _reflectionsEnabled;
	GroundMode _groundMode;
	bool _floorTextureDisplayed;
	bool _skyBoxEnabled;
	int  _skyBoxBlurPercent;

	bool _lowResEnabled;
	bool _sectionCapsSuppressedDuringInteraction = false;
	bool _dynamicCappingEnabled = false;

	unsigned int _shadowWidth;
	unsigned int _shadowHeight;

	bool _shadowMapNeedsInitialization = true;

	float _xTran;
	float _yTran;
	float _zTran;

	float _xRot;
	float _yRot;
	float _zRot;

	float _xScale;
	float _yScale;
	float _zScale;

	QVector4D _defaultLightColor;
	QVector4D _ambientLight;
	QVector4D _diffuseLight;
	QVector4D _specularLight;

	QVector3D _lightPosition;
	float _lightOffsetX;
	float _lightOffsetY;
	float _lightOffsetZ;

	QMatrix4x4 _lightSpaceMatrix;

	QMatrix4x4 _projectionMatrix, _viewMatrix, _modelMatrix;
	QMatrix4x4 _modelViewMatrix;
	QMatrix4x4 _viewportMatrix;

	std::unique_ptr<ShaderProgram> _fgShader;
	std::unique_ptr<ShaderProgram> _axisShader;
	std::unique_ptr<ShaderProgram> _vertexNormalShader;
	std::unique_ptr<ShaderProgram> _faceNormalShader;
	std::unique_ptr<ShaderProgram> _shadowMappingShader;
	std::unique_ptr<ShaderProgram> _skyBoxShader;
	std::unique_ptr<ShaderProgram> _gridShader;
	std::unique_ptr<ShaderProgram> _irradianceShader;
	std::unique_ptr<ShaderProgram> _prefilterShader;
	std::unique_ptr<ShaderProgram> _sheenPrefilterShader;
	std::unique_ptr<ShaderProgram> _brdfShader;
	std::unique_ptr<ShaderProgram> _lightCubeShader;
	std::unique_ptr<ShaderProgram> _viewCubeShader;
	std::unique_ptr<ShaderProgram> _viewCubeLabelShader;
	std::unique_ptr<ShaderProgram> _clippingPlaneShader;
	std::unique_ptr<ShaderProgram> _clippedMeshShader;
	std::unique_ptr<ShaderProgram> _selectionShader;
	std::unique_ptr<ShaderProgram> _equirectToCubeShader;
	std::unique_ptr<ShaderProgram> _equirectToCubeQuadShader;
	std::unique_ptr<ShaderProgram> _downsampleShader;

	unsigned int             _environmentMap  = 0;
	unsigned int             _shadowMap       = 0;
	unsigned int             _shadowMapFBO    = 0;
	unsigned int             _irradianceMap   = 0;
	unsigned int             _prefilterMap    = 0;
	unsigned int             _sheenPrefilterMap   = 0;
	unsigned int             _prefilterMipLevels  = 5; // Effective LOD levels: lod = roughness * (mipLevels - 1)
	unsigned int             _sheenPrefilterMipLevels = 5; // Effective mip count for sheen LOD formula
	unsigned int             _brdfLUTTexture  = 0;
	unsigned int             _charlieLUTTexture   = 0;
	unsigned int             _sheenELUTTexture    = 0;
	QString					 _currentSkyboxFolder;  // Track the current skybox folder path for environment map regeneration

	// Preset environment maps (index 1=Studio, 2=Outdoor, 3=Office)
	unsigned int             _studioEnvironmentMap = 0;
	unsigned int             _studioIrradianceMap = 0;
	unsigned int             _studioPrefilterMap = 0;
	unsigned int             _studioSheenPrefilterMap = 0;

	unsigned int             _outdoorEnvironmentMap = 0;
	unsigned int             _outdoorIrradianceMap = 0;
	unsigned int             _outdoorPrefilterMap = 0;
	unsigned int             _outdoorSheenPrefilterMap = 0;

	unsigned int             _officeEnvironmentMap = 0;
	unsigned int             _officeIrradianceMap = 0;
	unsigned int             _officePrefilterMap = 0;
	unsigned int             _officeSheenPrefilterMap = 0;
	unsigned int			 _skyboxFBO = 0;
	unsigned int			 _skyboxDepthBuffer = 0;

	// --- Transmission Buffer Resources ---
	GLuint _transmissionFBO = 0;              // Framebuffer object
	GLuint _transmissionColorTexture = 0;     // RGBA32F: opaque scene capture
	GLuint _transmissionDepthTexture = 0;     // DEPTH32F: for Phase 2 calculations
	int _transmissionTextureWidth = 0;        // Current FBO width
	int _transmissionTextureHeight = 0;       // Current FBO height
	int _transmissionMipLevels = 0;			  // Number of mip levels
	bool _transmissionEnabled = true;         // Toggle for feature

	// --- SSS (Subsurface Scattering) Buffer Resources ---
	GLuint _sssFBO = 0;                       // Capture FBO: SSS diffuse irradiance
	GLuint _sssCaptureTexture = 0;            // RGBA16F: SSS diffuse capture (also V-blur output)
	GLuint _sssDepthTexture = 0;              // DEPTH32F: depth for capture pass occlusion
	GLuint _sssBlurFBO = 0;                   // Blur FBO: H-blur output
	GLuint _sssBlurTexture = 0;               // RGBA16F: H-blur result (V-blur input)
	int _sssTextureWidth = 0;                 // Current FBO width
	int _sssTextureHeight = 0;                // Current FBO height
	bool _sssEnabled = false;                 // True when any loaded mesh has hasVolumeScattering

	// --- Debug texture placeholders (TextureDebugPanel) ---
	// Created once in initializeGL; owned by this widget.
	// _debugNeutralTex : 1×1 white RGBA         — replacement for disabled multiplicative slots
	// _debugNormalTex  : 1×1 (128,128,255,255) — replacement for disabled normal-map slots
	// _debugBlackTex   : 1×1 black RGBA         — replacement for disabled emissive slot
	// Note: contributions are silenced by zeroing the scalar uniforms (setScalarOverridesForUnit),
	//       not by the replacement texture value, so _debugNeutralTex is used for all non-normal slots.
	GLuint _debugNeutralTex = 0;
	GLuint _debugNormalTex  = 0;
	GLuint _debugBlackTex   = 0;
	int    _globalDebugChannel = 0;  // active channel ID for TextureDebugPanel dropdown; 0 = normal rendering

	QImage					 _floorTexImage;
	float                    _floorSize;
	float 					 _floorSizeFactor;
	float					 _floorOffsetPercent;
	QVector3D                _floorCenter;

	std::unique_ptr<ShaderProgram> _textShader;

	std::unique_ptr<ShaderProgram> _bgShader;
	QOpenGLVertexArrayObject _bgVAO;

	std::unique_ptr<ShaderProgram> _bgSplitShader;
	QOpenGLVertexArrayObject _bgSplitVAO;
	QOpenGLBuffer _bgSplitVBO;

	QOpenGLVertexArrayObject _axisVAO;
	QOpenGLBuffer _axisVBO;
	QOpenGLBuffer _axisCBO;

	CornerAxisPosition _cornerAxisPosition = CornerAxisPosition::TOP_RIGHT;

	std::vector<TriangleMesh*> _meshStore;
	std::vector<int> _displayedObjectsIds;
	std::vector<int> _hiddenObjectsIds;

	// Accumulates mesh UUIDs in DFS load order during a single loadAssImpModel
	// call (both progressive and non-progressive paths).  Consumed by
	// SceneGraph::appendFromScene() once loading completes, then cleared.
	QList<QUuid> _pendingSceneUuids;
	std::vector<int> _centerScreenObjectIDs;
	bool _visibleSwapped;
	QPointer<QWidget> _navigationOverlayPanel;

	QVBoxLayout* _editorLayout;
	QFormLayout* _lowerLayout;
	QFormLayout* _upperLayout;

	ClippingPlanesEditor* _clippingPlanesEditor;
	Plane* _clippingPlaneXY;
	Plane* _clippingPlaneYZ;
	Plane* _clippingPlaneZX;
	bool _cappingEnabled;
	unsigned int _cappingTexture;

	ViewMode _viewMode;
	ViewProjection _projection;
	GLCamera::ProjectionType _previousProjection;

	GLCamera* _primaryCamera;
	GLCamera* _orthoViewsCamera;

	// Active glTF camera (-1 / empty = none active, system camera is in use)
	QString _activeGltfCameraFile;
	int     _activeGltfCameraIndex = -1;

	// Saved system camera state, captured when a glTF camera is first activated
	// so the user can switch back to exactly where they were.
	bool                     _systemCameraStateSaved = false;
	QVector3D                _savedCameraPos;
	QVector3D                _savedCameraDir;
	QVector3D                _savedCameraUp;
	QVector3D                _savedCameraRight;
	GLCamera::ProjectionType _savedProjectionType = GLCamera::ProjectionType::PERSPECTIVE;
	float                    _savedCameraFOV      = 45.0f;
	float                    _savedCameraViewRange = 200.0f;

	QTimer* _keyboardNavTimer;
	QTimer* _animateViewTimer;
	QTimer* _animateFitAllTimer;
	QTimer* _animateWindowZoomTimer;
	QTimer* _animateCenterScreenTimer;

	BoundingSphere _boundingSphere;
	BoundingSphere _selectionBoundingSphere;

	BoundingBox _boundingBox;
	float _visibleHighestZ = 0.0f;
	float _visibleLowestZ = 0.0f;

	Plane* _floorPlane;
	Plane* _gridPlane;
	Cube* _skyBox;
	GLuint _fsTriVAO = 0;          // Fullscreen triangle VAO
	GLuint _fsTriVBO = 0;          // Fullscreen triangle VBO
	bool _fsTriInitialized = false; // Track initialization state
	std::vector<QString> _skyBoxFaces;
	float _skyBoxFOV;
	float _skyBoxZRotation;
	bool  _skyBoxTextureHDRI;
	bool  _gammaCorrection;
	float _screenGamma;
	bool  _hdrToneMapping;
	float _envMapExposure;
	float _iblExposure;

	GLuint _conversionCubeVAO = 0;
	GLuint _conversionCubeVBO = 0;

	HDRToneMapMode _toneMappingMode;

	bool _openGLInitialized = false;  // set true only after initializeGL() succeeds
	float _anisotropicFilteringLevel = 16.0f;

	Cone* _axisCone;
	ViewCubeMesh* _viewCube = nullptr;
	int _viewCubeHoveredRegionId = -1;
	bool _customViewAnimationActive = false;
	bool _showViewCubeOverride = true;
	std::array<GLuint, 6> _viewCubeLabelTextures = { 0, 0, 0, 0, 0, 0 };
	GLuint _viewCubeLabelVAO = 0;
	GLuint _viewCubeLabelVBO = 0;

	Cube* _lightCube;
	Sphere* _lightSphere;
	bool _showLights;
	bool _useDefaultLights;
	bool _usePunctualLights;
	bool _useIBL;

	std::unique_ptr<ShaderProgram> _debugShader;

	ModelViewer* _viewer;

	unsigned int _quadVAO;
	unsigned int _quadVBO;

	unsigned long long _displayedObjectsMemSize;

	AssImpModelLoader* _assimpModelLoader;
	KTX2Loader _ktx2Loader;
	GPUCapabilities _gpuCapabilities;
	const aiScene* _assimpScene = nullptr;
	aiScene* _globalScene = nullptr; // Merged scene from multiple files
	glm::mat4 _globalSceneTransform = glm::mat4(1.0f);
	bool _progressiveLoadingEnabled = false;
	bool _cancelRequested = false;
	bool _loadCancelled = false;

	// --- Texture cache: path -> GL texture id (per GLWidget context)
	std::unordered_map<QString, CachedTextureEntry> _texCache;
	// Reference counts so we can release when maps are cleared
	std::unordered_map<unsigned int, int> _texRefCount;


	ClippingPlaneHatchMode _hatchMode = ClippingPlaneHatchMode::PROCEDURAL;
	HatchPattern _hatchPattern = HatchPattern::DIAGONAL_45;
	int   _hatchTiling = 50;
	float _hatchThickness = 0.05f;
	float _hatchIntensity = 1.0f;
	int _hatchLayers = 3;
	QVector3D _hatchLineColor = QVector3D(0.0f, 0.0f, 0.0f);
	QString _hatchTexturePath;

	AdaptiveShadowMapper shadowMapper;

	std::unique_ptr<GLLights> glLights;
	std::vector<GPULight> _originalParsedLights;      // ORIGINAL - never modified
	std::vector<GPULight> _currentRepositionedLights; // Working copy
	float _originalBoundingRadius = 1.0f;
	QString _animatedLightTransformSourceFile;
	std::vector<GPULight> _animatedParsedLights;
	QString _animatedLightVisibilitySourceFile;
	QVector<bool> _animatedLightVisibilityMask;
	QString _animatedMeshVisibilitySourceFile;
	QSet<QUuid> _animatedHiddenMeshUuids;

	// Light repositioning based on model transformation
	struct LightRepositioningBasis
	{
		glm::vec3 baselineCenter;
		float baselineRadius;
		glm::mat4 accumulatedRotation;
	} _lightRepoBasis;

	float currentModelTransformScaleFactor() const;
	void applyGltfCameraEntryTransform(const GltfCameraEntry& cam);

	// Recycle bin (internal only - not exposed to user)
	struct RecycleBinEntry
	{
		TriangleMesh* mesh;
		int originalIndex;      // For potential smart restoration
		QDateTime deletedAt;    // For debugging/logging
	};

	QMap<QUuid, RecycleBinEntry> _recycleBin;
	QHash<QString, RuntimeAnimationFileState> _runtimeAnimationsByFile;
	QString _activeAnimationFile;
	int _activeAnimationClip = -1;
	double _animationCurrentTimeSeconds = 0.0;
	bool _animationPlaying = false;
	bool _animationLooping = true;
	double _animationPlaybackSpeed = 1.0;
	QTimer* _animationTimer = nullptr;
	QElapsedTimer _animationElapsed;
};

#endif
