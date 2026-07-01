# GLWidget Method Ownership Map
**Branch:** `refactor/mesh-render-runtime-separation`  
**Source of truth:** `include/GLWidget.h` as audited on 2026-06-28

Each method is listed with its exact signature and bare method name for quick lookup.  
Legend: **STAY** = GL-context or Qt-event affinity; **MOVE** = clean extraction; **SPLIT** = partial extraction.

---

## Destination: STAY in GLWidget

### Constructor / Destructor
| Signature | Method Name | Access |
|---|---|---|
| `GLWidget(QWidget* parent = 0, const char* name = 0)` | `GLWidget` | public |
| `~GLWidget()` | `~GLWidget` | public |

### Qt i18n / UI wiring
| Signature | Method Name | Access |
|---|---|---|
| `void retranslateUI()` | `retranslateUI` | public |
| `void updateView()` | `updateView` | public |
| `void resizeView(int w, int h)` | `resizeView` | public |
| `ViewToolbar* getViewToolbar() const` | `getViewToolbar` | public |
| `void cleanUpShaders()` | `cleanUpShaders` | public |
| `SelectionManager* getSelectionManager() const` | `getSelectionManager` | public slot |
| `void showFileReadingProgress(float percent)` | `showFileReadingProgress` | public slot |
| `void showMeshLoadingProgress(float percent)` | `showMeshLoadingProgress` | public slot |
| `void showNodeMeshLoadingProgress(int processedNodes, int totalNodes, int processedMeshes, int totalMeshes, bool uvProcessed)` | `showNodeMeshLoadingProgress` | public slot |
| `void cancelAssImpModelLoading()` | `cancelAssImpModelLoading` | public slot |
| `void showContextMenu(const QPoint& pos)` | `showContextMenu` | private slot |
| `void centerDisplayList()` | `centerDisplayList` | private slot |
| `void setBackgroundColor()` | `setBackgroundColor` | private slot |
| `void updateOverlayEditorTheme()` | `updateOverlayEditorTheme` | private |
| `void applyOverlayPanelStyle(QWidget* wrapper, const QString& objectName)` | `applyOverlayPanelStyle` | protected |
| `void refreshNavigationOverlayStyle()` | `refreshNavigationOverlayStyle` | protected |
| `void loadBgColorSettings()` | `loadBgColorSettings` | private |

### Overlay panels
| Signature | Method Name | Access |
|---|---|---|
| `QWidget* attachOverlayPanel(QWidget* contentWidget, const QRect& geometry, Qt::Alignment alignment, const QString& objectName)` | `attachOverlayPanel` | public |
| `QWidget* takeOverlayPanel(QWidget* contentWidget)` | `takeOverlayPanel` | public |
| `void refreshDetachedNavigationOverlayTheme()` | `refreshDetachedNavigationOverlayTheme` | public |

### Exploded view panel UI
| Signature | Method Name | Access |
|---|---|---|
| `void showExplodedViewPanel(bool show)` | `showExplodedViewPanel` | public |
| `ExplodedViewPanel* getExplodedViewPanel() const` | `getExplodedViewPanel` | public |

### Camera & view orchestration (GL context required)
| Signature | Method Name | Access |
|---|---|---|
| `void setViewMode(ViewMode mode)` | `setViewMode` | public |
| `void setCameraUpAxisZUp(bool zUp, bool syncToolbar = true)` | `setCameraUpAxisZUp` | public |
| `void setProjection(ViewProjection proj)` | `setProjection` | public |
| `void setCameraMode(GLCamera::CameraMode mode)` | `setCameraMode` | public |
| `void setMultiView(bool active)` | `setMultiView` | public |
| `void setShowCenterAxisOverride(bool show)` | `setShowCenterAxisOverride` | public |
| `void setShowCornerAxisOverride(bool show)` | `setShowCornerAxisOverride` | public |
| `void setShowViewCubeOverride(bool show)` | `setShowViewCubeOverride` | public |
| `void showAxis(bool show)` | `showAxis` | public |
| `void showTransformGizmoForSelection(bool show)` | `showTransformGizmoForSelection` | public |
| `GltfCameraData cameraDataForMvfSave(const GltfCameraData& source) const` | `cameraDataForMvfSave` | public |
| `void centerScreen(std::vector<int> selectedIDs)` | `centerScreen` | public |
| `void animateViewChange()` | `animateViewChange` | public slot |
| `void animateFitAll()` | `animateFitAll` | public slot |
| `void animateWindowZoom()` | `animateWindowZoom` | public slot |
| `void animateCenterScreen()` | `animateCenterScreen` | public slot |
| `void fitAll()` | `fitAll` | public slot |
| `void setAutoFitViewOnUpdate(bool update)` | `setAutoFitViewOnUpdate` | public slot |
| `void performKeyboardNav()` | `performKeyboardNav` | public slot |
| `void disableLowRes()` | `disableLowRes` | public slot |
| `GLCamera* getCameraForPoint(const QPoint& pixel)` | `getCameraForPoint` | public slot |
| `void setView(QVector3D viewPos, QVector3D viewDir, QVector3D upDir, QVector3D rightDir)` | `setView` | private |
| `void animateToRotation(const QQuaternion& targetRotation)` | `animateToRotation` | private |
| `void syncCameraWorldUp()` | `syncCameraWorldUp` | private |
| `void rotateCurrentCameraAroundWorldX(float degrees)` | `rotateCurrentCameraAroundWorldX` | private |
| `void applyAutoOrientCameraConvention(SceneUpAxis sceneUpAxis)` | `applyAutoOrientCameraConvention` | private |
| `bool positionGameplayCameraForScene(GLCamera::CameraMode mode)` | `positionGameplayCameraForScene` | private |
| `void configureOrthoSubviewCamera(ViewMode viewMode, const std::vector<QVector3D>& corners, int viewportWidth, int viewportHeight, const QVector3D& sharedCenter, float sharedViewRange)` | `configureOrthoSubviewCamera` | private |
| `void splitScreen()` | `splitScreen` | private |
| `void updateZoomInLimit()` | `updateZoomInLimit` | private |
| `QRect getViewportFromPoint(const QPoint& pixel)` | `getViewportFromPoint` | private |
| `QRect getClientRectFromPoint(const QPoint& pixel)` | `getClientRectFromPoint` | private |
| `void applyGltfCameraEntryTransform(const GltfCameraEntry& cam)` | `applyGltfCameraEntryTransform` | private |

### Inertia / animation timers
| Signature | Method Name | Access |
|---|---|---|
| `void onInertiaTimer()` | `onInertiaTimer` | public slot |
| `void stopAnimations()` | `stopAnimations` | public slot |
| `void checkAndStopTimers()` | `checkAndStopTimers` | public slot |

### Shadow & lighting orchestration
| Signature | Method Name | Access |
|---|---|---|
| `void triggerShadowRecomputation()` | `triggerShadowRecomputation` | public |
| `float calculateLightDistance()` | `calculateLightDistance` | public |
| `void updateFloorPlane()` | `updateFloorPlane` | public |
| `void updateBoundingSphere()` | `updateBoundingSphere` | public |
| `void updateBoundingBox()` | `updateBoundingBox` | public |
| `void showLights(bool showLights)` | `showLights` | public slot |
| `void applyEnabledLightList(const std::vector<GPULight>& enabledLights)` | `applyEnabledLightList` | public slot |
| `void onSceneLightDataChanged()` | `onSceneLightDataChanged` | public |
| `void syncDefaultLightColorUniforms()` | `syncDefaultLightColorUniforms` | private |
| `void syncPunctualLightUniforms(int lightCount, bool hasPunctualLights)` | `syncPunctualLightUniforms` | private |
| `bool shouldUseFallbackLightForVisibleScene() const` | `shouldUseFallbackLightForVisibleScene` | private |
| `void updatePunctualLights()` | `updatePunctualLights` | private |
| `void updateMainLightPosition(float halfObjectSize)` | `updateMainLightPosition` | private |
| `QVector3D effectiveWorldLightOffset() const` | `effectiveWorldLightOffset` | private |
| `QVector3D effectiveWorldLightPosition() const` | `effectiveWorldLightPosition` | private |

### Clipping / caps orchestration
| Signature | Method Name | Access |
|---|---|---|
| `void updateClippingPlane()` | `updateClippingPlane` | public |
| `void showClippingPlaneEditor(bool show)` | `showClippingPlaneEditor` | public |
| `void updateExplosion()` | `updateExplosion` | public |

### Model loading
| Signature | Method Name | Access | Notes |
|---|---|---|---|
| `bool loadAssImpModel(const QString& fileName, const UVMethod& uvMethod, QString& error, bool progressiveLoading = false)` | `loadAssImpModel` | public | SPLIT — mesh store population → SceneRuntime |
| `bool generateUVsForMeshes(const std::vector<int>& ids, const UVMethod& uvMethod, const UVConfig& uvConfig, QString& error)` | `generateUVsForMeshes` | public | SPLIT |
| `void onMeshBatchReady(const std::vector<AssImpMeshData>& batch)` | `onMeshBatchReady` | private | |
| `void uploadOneMvfMesh(const PreparedMvfMesh& pm)` | `uploadOneMvfMesh` | public | |
| `bool uploadPreparedMvfMeshes(const QVector<PreparedMvfMesh>& meshes)` | `uploadPreparedMvfMeshes` | public | |
| `bool loadMvfMeshes(const Mvf::Document& document, const QByteArray& geometryChunk, const QByteArray& imageChunk)` | `loadMvfMeshes` | public | SPLIT |
| `UVMethod promptLargeModelUVDecision(int totalTriangles, UVMethod currentMethod)` | `promptLargeModelUVDecision` | private | |

### Selection & picking (GL thread or event coordination)
| Signature | Method Name | Access |
|---|---|---|
| `QList<int> sweepSelect(const QPoint& pixel, bool addToSelection = false)` | `sweepSelect` | private |

### Animation playback orchestration
| Signature | Method Name | Access |
|---|---|---|
| `void onAnimationTick()` | `onAnimationTick` | public slot |
| `void syncFileNodeTransforms(const QString& sourceFile)` | `syncFileNodeTransforms` | private |
| `void applyAnimationPose(const QString& sourceFile, int clipIndex, double timeSeconds)` | `applyAnimationPose` | private |
| `void resetAnimationPose(const QString& sourceFile)` | `resetAnimationPose` | private |
| `void updateAnimatedMeshState(const QString& sourceFile, const QHash<QUuid, QMatrix4x4>& worldTransformsByNodeUuid)` | `updateAnimatedMeshState` | private |
| `void applyExplodedViewManualPlacementSessionTransform()` | `applyExplodedViewManualPlacementSessionTransform` | private |

### Gizmo interaction (mouse events + GL pick)
| Signature | Method Name | Access |
|---|---|---|
| `BoundingSphere computeTransformGizmoSelectionSphere() const` | `computeTransformGizmoSelectionSphere` | private |
| `QVector3D computeTransformGizmoPivot() const` | `computeTransformGizmoPivot` | private |
| `void syncTransformGizmoToSelection()` | `syncTransformGizmoToSelection` | private |
| `bool beginTransformGizmoDrag(TransformGizmo::Handle handle, const QPoint& pixel)` | `beginTransformGizmoDrag` | private |
| `bool beginTransformGizmoTranslationDrag(TransformGizmo::Handle handle, const QPoint& pixel)` | `beginTransformGizmoTranslationDrag` | private |
| `void updateTransformGizmoTranslationDrag(const QPoint& pixel)` | `updateTransformGizmoTranslationDrag` | private |
| `void finishTransformGizmoTranslationDrag(bool commit)` | `finishTransformGizmoTranslationDrag` | private |
| `bool beginTransformGizmoScaleDrag(TransformGizmo::Handle handle, const QPoint& pixel, bool uniformScale)` | `beginTransformGizmoScaleDrag` | private |
| `void updateTransformGizmoScaleDrag(const QPoint& pixel)` | `updateTransformGizmoScaleDrag` | private |
| `void finishTransformGizmoScaleDrag(bool commit)` | `finishTransformGizmoScaleDrag` | private |
| `bool beginTransformGizmoRotationDrag(TransformGizmo::Handle handle, const QPoint& pixel)` | `beginTransformGizmoRotationDrag` | private |
| `void updateTransformGizmoRotationDrag(const QPoint& pixel)` | `updateTransformGizmoRotationDrag` | private |
| `void finishTransformGizmoRotationDrag(bool commit)` | `finishTransformGizmoRotationDrag` | private |

### View cube (GL rendering + Qt event)
| Signature | Method Name | Access |
|---|---|---|
| `void initializeViewCubeLabels()` | `initializeViewCubeLabels` | private |
| `bool computeViewCubeRenderState(QRect& viewportRect, QMatrix4x4& viewMatrix, QMatrix4x4& projectionMatrix, QMatrix4x4& modelMatrix, float& cubeScale) const` | `computeViewCubeRenderState` | private |
| `bool pickViewCubeRegionAtPixel(const QPoint& pixel, QVector3D& outwardNormal, int* regionId = nullptr) const` | `pickViewCubeRegionAtPixel` | private |
| `bool handleViewCubeClick(const QPoint& pixel)` | `handleViewCubeClick` | private |
| `void updateViewCubeHover(const QPoint& pixel, Qt::MouseButtons buttons)` | `updateViewCubeHover` | private |
| `bool orientCameraToViewCubeNormal(const QVector3D& outwardNormal)` | `orientCameraToViewCubeNormal` | private |

### GL resource creation & render passes
| Signature | Method Name | Access |
|---|---|---|
| `void initializeGL()` | `initializeGL` | protected |
| `void createCappingPlanes()` | `createCappingPlanes` | protected |
| `void resizeGL(int width, int height)` | `resizeGL` | protected |
| `void paintGL()` | `paintGL` | protected |
| `void renderSingleView(QColor& topColor, QColor& botColor)` | `renderSingleView` | protected |
| `void renderMultiView(QColor& topColor, QColor& botColor)` | `renderMultiView` | protected |
| `void createShaderPrograms()` | `createShaderPrograms` | private |
| `void syncUniformsToFlatShader()` | `syncUniformsToFlatShader` | private |
| `void createLights()` | `createLights` | private |
| `void createFullscreenTriangle()` | `createFullscreenTriangle` | private |
| `void drawFullscreenTriangle()` | `drawFullscreenTriangle` | private |
| `void setIBLFaceBasis(QOpenGLShaderProgram* prog, int faceIndex)` | `setIBLFaceBasis` | private |
| `void loadEnvMap()` | `loadEnvMap` | private |
| `void loadIrradianceMap()` | `loadIrradianceMap` | private |
| `GLuint loadPresetEnvironmentMap(const QString& hdrFilePath)` | `loadPresetEnvironmentMap` | private |
| `bool generatePresetIBLMaps(GLuint sourceCubemap, GLuint& outIrradianceMap, GLuint& outPrefilterMap, GLuint& outSheenPrefilterMap)` | `generatePresetIBLMaps` | private |
| `void loadFloor()` | `loadFloor` | private |
| `void ensureShadowMapResources()` | `ensureShadowMapResources` | private |
| `void loadGrid()` | `loadGrid` | private |
| `void applyFloorPlaneMaterialSettings()` | `applyFloorPlaneMaterialSettings` | private |
| `void syncFloorPlaneAlbedoTexture()` | `syncFloorPlaneAlbedoTexture` | private |
| `float updateFloorGeometry()` | `updateFloorGeometry` | private |
| `void drawMesh(QOpenGLShaderProgram* prog, int activeCapPlaneIndex = -1)` | `drawMesh` | private |
| `void drawOpaqueMeshes(QOpenGLShaderProgram* prog, int activeClipPlaneIndex = -1)` | `drawOpaqueMeshes` | private |
| `void drawTransparentMeshes(QOpenGLShaderProgram* prog, int activeClipPlaneIndex = -1)` | `drawTransparentMeshes` | private |
| `void drawMeshesWithClipping(QOpenGLShaderProgram* prog, bool transparentPass)` | `drawMeshesWithClipping` | private |
| `void setCommonUniforms(QOpenGLShaderProgram* prog, GLCamera* camera)` | `setCommonUniforms` | private |
| `void extractFrustumPlanes()` | `extractFrustumPlanes` | private |
| `bool sceneHasVisibleTransmissionMaterials() const` | `sceneHasVisibleTransmissionMaterials` | private |
| `bool sceneHasVisibleSSSMaterials() const` | `sceneHasVisibleSSSMaterials` | private |
| `void drawSectionCapping()` | `drawSectionCapping` | private |
| `void drawFloor(const bool& drawReflection = true)` | `drawFloor` | private |
| `void drawGrid()` | `drawGrid` | private |
| `void drawSkyBox()` | `drawSkyBox` | private |
| `void drawVertexNormals()` | `drawVertexNormals` | private |
| `void drawFaceNormals()` | `drawFaceNormals` | private |
| `void drawBoundingBoxOverlay()` | `drawBoundingBoxOverlay` | private |
| `void drawDebugOverlay(GLCamera* camera)` | `drawDebugOverlay` | private |
| `void drawAxis(GLCamera* camera)` | `drawAxis` | private |
| `void drawCornerAxis(CornerAxisPosition position)` | `drawCornerAxis` | private |
| `void drawTransformGizmo(GLCamera* camera)` | `drawTransformGizmo` | private |
| `void drawViewCube()` | `drawViewCube` | private |
| `void drawViewCubeLabels(const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix, float cubeScale)` | `drawViewCubeLabels` | private |
| `void drawLights()` | `drawLights` | private |
| `void bindIBLTextures()` | `bindIBLTextures` | private |
| `void render(GLCamera* camera)` | `render` | private |
| `void renderToShadowBuffer()` | `renderToShadowBuffer` | private |
| `void renderQuad()` | `renderQuad` | private |
| `void renderMeshWithDisplayMode(SceneMesh* mesh, DisplayMode mode)` | `renderMeshWithDisplayMode` | private |
| `void renderMeshExploded(SceneMesh* mesh, DisplayMode mode)` | `renderMeshExploded` | private |
| `void gradientBackground(float top_r, float top_g, float top_b, float top_a, float bot_r, float bot_g, float bot_b, float bot_a, int gradientStyle)` | `gradientBackground` | private |
| `void setupClippingUniforms(QOpenGLShaderProgram* prog, QVector3D pos)` | `setupClippingUniforms` | private |
| `void setSkyBoxTextureFolder(QString folder)` | `setSkyBoxTextureFolder` | public |
| `bool loadCubemapFromSingleHDR(const QString& filePath)` | `loadCubemapFromSingleHDR` | public |
| `bool convertEquirectangularToCubemap(const QString& filePath)` | `convertEquirectangularToCubemap` | public |
| `bool convertEquirectangularToCubemapQuad(const QString& filePath)` | `convertEquirectangularToCubemapQuad` | public |
| `void renderConversionCube()` | `renderConversionCube` | public |
| `void initTransmissionBuffer()` | `initTransmissionBuffer` | private |
| `void renderToTransmissionBuffer(GLCamera* camera, const QColor& topColor, const QColor& botColor)` | `renderToTransmissionBuffer` | private |
| `void cleanupTransmissionBuffer()` | `cleanupTransmissionBuffer` | private |
| `void resizeTransmissionBuffer(int width, int height)` | `resizeTransmissionBuffer` | private |
| `void initSSSBuffer()` | `initSSSBuffer` | private |
| `void renderToSSSBuffer(GLCamera* camera)` | `renderToSSSBuffer` | private |
| `void resizeSSSBuffer(int width, int height)` | `resizeSSSBuffer` | private |
| `void cleanupSSSBuffer()` | `cleanupSSSBuffer` | private |
| `void createWhiteTexture()` | `createWhiteTexture` | private |
| `void generateCubemapMipmaps(GLuint cubemapTexture)` | `generateCubemapMipmaps` | private |
| `unsigned int loadTextureFromFile(const char* path, GLenum wrapS, GLenum wrapT, GLenum minFilter, GLenum magFilter, bool flipY)` | `loadTextureFromFile` | private |
| `GLuint createGPUTextureFromImage(const QImage& image, const TextureSamplerSettings& samplers)` | `createGPUTextureFromImage` | private |
| `GLuint uploadDecodedTextureImage(const QImage& image, const TextureSamplerSettings& samplers)` | `uploadDecodedTextureImage` | private |
| `GLuint uploadKtx2TextureImage(const QString& path, const std::string& mapType, const TextureSamplerSettings& samplers)` | `uploadKtx2TextureImage` | private |
| `GLuint uploadDecodedTexture(GLMaterial::Texture& texture, const QImage& image)` | `uploadDecodedTexture` | private |
| `GLuint uploadKtx2Texture(const QString& path, const std::string& mapType, GLMaterial::Texture& texture)` | `uploadKtx2Texture` | private |

### Debug readback (GL glGetTexImage)
| Signature | Method Name | Access |
|---|---|---|
| `void requestTextureReadback(int meshId)` | `requestTextureReadback` | public slot |
| `void setDebugTextureEnabled(int meshId, int unitIndex, bool enabled)` | `setDebugTextureEnabled` | public slot |
| `void applyDebugTextureState(int meshId, const QSet<int>& enabledUnits, const QSet<int>& allUnits)` | `applyDebugTextureState` | public slot |
| `void setGlobalDebugChannel(int channelId)` | `setGlobalDebugChannel` | public slot |
| `void clearDebugTextureOverrides(int meshId)` | `clearDebugTextureOverrides` | public slot |
| `void clearAllDebugOverrides(int meshId)` | `clearAllDebugOverrides` | public slot |
| `void setDebugExtensionEnabled(int meshId, const QString& extensionKey, bool enabled)` | `setDebugExtensionEnabled` | public slot |
| `void clearDebugExtensionOverrides(int meshId)` | `clearDebugExtensionOverrides` | public slot |

### Qt event handlers
| Signature | Method Name | Access |
|---|---|---|
| `void resizeEvent(QResizeEvent* event)` | `resizeEvent` | protected |
| `void mousePressEvent(QMouseEvent*)` | `mousePressEvent` | protected |
| `void mouseReleaseEvent(QMouseEvent*)` | `mouseReleaseEvent` | protected |
| `void mouseMoveEvent(QMouseEvent*)` | `mouseMoveEvent` | protected |
| `void wheelEvent(QWheelEvent*)` | `wheelEvent` | protected |
| `void keyPressEvent(QKeyEvent* event)` | `keyPressEvent` | protected |
| `void keyReleaseEvent(QKeyEvent* event)` | `keyReleaseEvent` | protected |
| `void closeEvent(QCloseEvent* event)` | `closeEvent` | protected |

### Signals (always stay in declaring class)
| Signature | Method Name |
|---|---|
| `void windowZoomEnded()` | `windowZoomEnded` |
| `void rotationsSet()` | `rotationsSet` |
| `void zoomAndPanSet()` | `zoomAndPanSet` |
| `void viewSet()` | `viewSet` |
| `void displayListSet()` | `displayListSet` |
| `void singleSelectionDone(int)` | `singleSelectionDone` |
| `void sweepSelectionDone(QList<int>)` | `sweepSelectionDone` |
| `void floorShown(bool)` | `floorShown` |
| `void visibleSwapped(bool)` | `visibleSwapped` |
| `void loadingAssImpModelCancelled()` | `loadingAssImpModelCancelled` |
| `void displayModeChanged(int)` | `displayModeChanged` |
| `void renderingModeChanged(int)` | `renderingModeChanged` |
| `void animationStateChanged()` | `animationStateChanged` |
| `void explodedViewManualPlacementChanged()` | `explodedViewManualPlacementChanged` |
| `void backgroundColorChanged(const QColor& topColor, const QColor& bottomColor)` | `backgroundColorChanged` |
| `void selectionChanged(const QList<int>& selectedIds)` | `selectionChanged` |
| `void textureReadbackReady(QVector<TextureSlotInfo> slots, QString meshName)` | `textureReadbackReady` |
| `void cameraUpAxisChanged(bool zUp)` | `cameraUpAxisChanged` |

---

## Destination: `AnimationRuntimeController`

| Signature | Method Name | Access | Notes |
|---|---|---|---|
| `void setActiveAnimation(const QString& sourceFile, int clipIndex)` | `setActiveAnimation` | public | 100% delegates to `_animCtrl` |
| `void setAnimationPlaying(bool playing)` | `setAnimationPlaying` | public | |
| `void clearAnimationRuntimeForFile(const QString& sourceFile)` | `clearAnimationRuntimeForFile` | public | |
| `void seekAnimation(double timeSeconds)` | `seekAnimation` | public | |
| `void setAnimationLooping(bool looping)` | `setAnimationLooping` | public | |
| `void setAnimationPlaybackSpeed(double speed)` | `setAnimationPlaybackSpeed` | public | |
| `void syncRuntimeNodeTransforms(const QString& sourceFile)` | `syncRuntimeNodeTransforms` | public | |
| `void refreshAnimationMaterialState(const QString& sourceFile)` | `refreshAnimationMaterialState` | public | |
| `QString activeAnimationFile() const` | `activeAnimationFile` | public | pure accessor |
| `int activeAnimationClip() const` | `activeAnimationClip` | public | |
| `double currentAnimationTimeSeconds() const` | `currentAnimationTimeSeconds` | public | |
| `bool isAnimationPlaying() const` | `isAnimationPlaying` | public | |
| `bool isAnimationLooping() const` | `isAnimationLooping` | public | |
| `double animationPlaybackSpeed() const` | `animationPlaybackSpeed` | public | |
| `void activateGltfCamera(const QString& sourceFile, int cameraIndex)` | `activateGltfCamera` | public | |
| `void resetToSystemCamera()` | `resetToSystemCamera` | public | |
| `bool isGltfCameraActive() const` | `isGltfCameraActive` | public | |
| `QString activeGltfCameraFile() const` | `activeGltfCameraFile` | public | |
| `int activeGltfCameraIndex() const` | `activeGltfCameraIndex` | public | |
| `std::vector<GPULight> getParsedLights() const` | `getParsedLights` | public | |
| `std::vector<GPULight> getRepositionedLights() const` | `getRepositionedLights` | public | |
| `QVector<LightOrigin> getLightFileIndexMap() const` | `getLightFileIndexMap` | public | |
| `void setParsedLights(const GltfLightData& lights)` | `setParsedLights` | public | |
| `void setAnimatedLightVisibilityState(const QString& sourceFile, const QVector<bool>& visibleByParsedLight)` | `setAnimatedLightVisibilityState` | private | |
| `void setAnimatedLightTransformState(const QString& sourceFile, const std::vector<GPULight>& animatedLights)` | `setAnimatedLightTransformState` | private | |
| `void clearAnimatedLightTransformState(const QString& sourceFile)` | `clearAnimatedLightTransformState` | private | |
| `void clearAnimatedLightVisibilityState(const QString& sourceFile)` | `clearAnimatedLightVisibilityState` | private | |
| `void setAnimatedMeshVisibilityState(const QString& sourceFile, const QSet<QUuid>& hiddenMeshUuids)` | `setAnimatedMeshVisibilityState` | private | |
| `void clearAnimatedMeshVisibilityState(const QString& sourceFile)` | `clearAnimatedMeshVisibilityState` | private | |

**Type aliases to dissolve** (currently `using X = AnimationRuntimeController::X` in GLWidget):
- `using LightOrigin = AnimationRuntimeController::LightOrigin;`
- `using RuntimeNodeTransform = AnimationRuntimeController::RuntimeNodeTransform;`
- `using RuntimeAnimationFileState = AnimationRuntimeController::RuntimeAnimationFileState;`

---

## Destination: `SceneRenderController`

| Signature | Method Name | Access | Notes |
|---|---|---|---|
| `void setCornerAxisPosition(CornerAxisPosition position)` | `setCornerAxisPosition` | public | render state enum |
| `void setShadowQuality(AdaptiveShadowMapper::QualityLevel quality)` | `setShadowQuality` | public | |
| `void setClippingPlaneHatchMode(ClippingPlaneHatchMode mode)` | `setClippingPlaneHatchMode` | public | |
| `void setClippingPlaneHatchPattern(HatchPattern pattern)` | `setClippingPlaneHatchPattern` | public | |
| `void setHatchTiling(int tiling)` | `setHatchTiling` | public | |
| `void setHatchLineThickness(float width)` | `setHatchLineThickness` | public | |
| `void setHatchIntensity(float spacing)` | `setHatchIntensity` | public | |
| `void setHatchLayers(int layers)` | `setHatchLayers` | public | |
| `void setHatchLineColor(const QColor& color)` | `setHatchLineColor` | public | |
| `void setHatchTexture(const QString& path)` | `setHatchTexture` | public | |
| `void showShadows(bool show)` | `showShadows` | public | |
| `void showSelfShadows(bool show)` | `showSelfShadows` | public | |
| `void showEnvironment(bool show)` | `showEnvironment` | public | |
| `void showSkyBox(bool show)` | `showSkyBox` | public | |
| `void blurSkyBox(bool blur)` | `blurSkyBox` | public | |
| `void setSkyBoxBlurPercent(int percent)` | `setSkyBoxBlurPercent` | public | |
| `void showReflections(bool show)` | `showReflections` | public | |
| `void setGroundMode(GroundMode mode)` | `setGroundMode` | public | |
| `GroundMode groundMode() const` | `groundMode` | public | |
| `void showFloor(bool show)` | `showFloor` | public | |
| `bool isFloorShown()` | `isFloorShown` | public | |
| `bool isGridShown() const` | `isGridShown` | public | |
| `void showFloorTexture(bool show)` | `showFloorTexture` | public | |
| `void setFloorTexture(QImage img)` | `setFloorTexture` | public | state only; GL upload stays in GLWidget |
| `void setAnisotropicFilteringLevel(int level)` | `setAnisotropicFilteringLevel` | public | |
| `int getAnisotropicFilteringLevel() const` | `getAnisotropicFilteringLevel` | public | |
| `void setTransmissionEnabled(const bool& enabled)` | `setTransmissionEnabled` | public | |
| `bool isTransmissionEnabled() const` | `isTransmissionEnabled` | public | |
| `QVector4D getDefaultLightColor() const` | `getDefaultLightColor` | public | |
| `void setDefaultLightColor(const QVector4D& defaultLightColor)` | `setDefaultLightColor` | public | |
| `QVector3D getLightPosition() const` | `getLightPosition` | public | |
| `QVector3D getLightOffset() const` | `getLightOffset` | public | |
| `void setLightOffset(const QVector3D& offset)` | `setLightOffset` | public | |
| `float getFloorSize() const` | `getFloorSize` | public | |
| `bool isShaded() const` | `isShaded` | public | |
| `DisplayMode getDisplayMode() const` | `getDisplayMode` | public | |
| `void setDisplayMode(DisplayMode mode)` | `setDisplayMode` | public | |
| `bool isVertexNormalsShown() const` | `isVertexNormalsShown` | public | |
| `void setShowVertexNormals(bool showVertexNormals)` | `setShowVertexNormals` | public | |
| `bool isBoundingBoxShown() const` | `isBoundingBoxShown` | public | |
| `void setShowBoundingBox(bool showBoundingBox)` | `setShowBoundingBox` | public | |
| `DebugOverlayMode debugOverlayMode() const` | `debugOverlayMode` | public | |
| `void setDebugOverlayMode(DebugOverlayMode mode)` | `setDebugOverlayMode` | public | |
| `bool isDebugOverlayEnabled() const` | `isDebugOverlayEnabled` | public | |
| `void setDebugOverlayEnabled(bool enabled)` | `setDebugOverlayEnabled` | public | |
| `void setDebugOverlayAvailability(bool boundingBox, bool vertexNormals, bool faceNormals)` | `setDebugOverlayAvailability` | public | |
| `bool isFaceNormalsShown() const` | `isFaceNormalsShown` | public | |
| `void setShowFaceNormals(bool showFaceNormals)` | `setShowFaceNormals` | public | |
| `QColor getBgTopColor() const` | `getBgTopColor` | public | |
| `void setBgTopColor(const QColor& bgTopColor)` | `setBgTopColor` | public | |
| `QColor getBgBotColor() const` | `getBgBotColor` | public | |
| `void setBgBotColor(const QColor& bgBotColor)` | `setBgBotColor` | public | |
| `int getBgGradientStyle() const` | `getBgGradientStyle` | public | |
| `void setBgGradientStyle(int style)` | `setBgGradientStyle` | public | |
| `RenderingMode getRenderingMode() const` | `getRenderingMode` | public | |
| `void setRenderingMode(const RenderingMode& renderingMode)` | `setRenderingMode` | public | |
| `void setCappingPlanesEnabled(const bool& enabled)` | `setCappingPlanesEnabled` | public | |
| `bool cappingPlanesEnabled() const` | `cappingPlanesEnabled` | public | |
| `void setYZClippingEnabled(const bool& enabled)` | `setYZClippingEnabled` | public | |
| `bool yzClippingEnabled() const` | `yzClippingEnabled` | public | |
| `void setZXClippingEnabled(const bool& enabled)` | `setZXClippingEnabled` | public | |
| `bool zxClippingEnabled() const` | `zxClippingEnabled` | public | |
| `void setXYClippingEnabled(const bool& enabled)` | `setXYClippingEnabled` | public | |
| `bool xyClippingEnabled() const` | `xyClippingEnabled` | public | |
| `void setClippingXFlipped(const bool& flipped)` | `setClippingXFlipped` | public | |
| `bool clippingXFlipped() const` | `clippingXFlipped` | public | |
| `void setClippingYFlipped(const bool& flipped)` | `setClippingYFlipped` | public | |
| `bool clippingYFlipped() const` | `clippingYFlipped` | public | |
| `void setClippingZFlipped(const bool& flipped)` | `setClippingZFlipped` | public | |
| `bool clippingZFlipped() const` | `clippingZFlipped` | public | |
| `void setClippingXCoeff(const float& coeff)` | `setClippingXCoeff` | public | |
| `float clippingXCoeff() const` | `clippingXCoeff` | public | |
| `void setClippingYCoeff(const float& coeff)` | `setClippingYCoeff` | public | |
| `float clippingYCoeff() const` | `clippingYCoeff` | public | |
| `void setClippingZCoeff(const float& coeff)` | `setClippingZCoeff` | public | |
| `float clippingZCoeff() const` | `clippingZCoeff` | public | |
| `bool getHdrToneMapping() const` | `getHdrToneMapping` | public | |
| `bool getGammaCorrection() const` | `getGammaCorrection` | public | |
| `float getScreenGamma() const` | `getScreenGamma` | public | |
| `GLuint getEnvironmentMap(int index = 0, bool regenerate = false)` | `getEnvironmentMap` | public | |
| `GLuint getIrradianceMap(int index = 0, bool regenerate = false)` | `getIrradianceMap` | public | |
| `GLuint getPrefilterMap(int index = 0, bool regenerate = false)` | `getPrefilterMap` | public | |
| `GLuint getSheenPrefilterMap(int index = 0, bool regenerate = false)` | `getSheenPrefilterMap` | public | |
| `unsigned int getPrefilterMipLevels() const` | `getPrefilterMipLevels` | public | |
| `unsigned int getSheenPrefilterMipLevels() const` | `getSheenPrefilterMipLevels` | public | |
| `GLuint getBrdfLUT() const` | `getBrdfLUT` | public | |
| `GLuint getCharlieLUT() const` | `getCharlieLUT` | public | |
| `GLuint getSheenELUT() const` | `getSheenELUT` | public | |
| `bool isEnvironmentMapEnabled() const` | `isEnvironmentMapEnabled` | public | |
| `bool isIBLEnabled() const` | `isIBLEnabled` | public | |
| `float getIBLExposure() const` | `getIBLExposure` | public | |
| `float getEnvMapExposure() const` | `getEnvMapExposure` | public | |
| `QString getCurrentSkyboxFolder() const` | `getCurrentSkyboxFolder` | public | |
| `bool isSkyBoxShown() const` | `isSkyBoxShown` | public | |
| `bool isSkyBoxHDRIEnabled() const` | `isSkyBoxHDRIEnabled` | public | |
| `int getSkyBoxBlurPercent() const` | `getSkyBoxBlurPercent` | public | |
| `float getSkyBoxFOV() const` | `getSkyBoxFOV` | public | |
| `float getSkyBoxZRotationDegrees() const` | `getSkyBoxZRotationDegrees` | public | |
| `bool areReflectionsEnabled() const` | `areReflectionsEnabled` | public | |
| `bool isFloorTextureShown() const` | `isFloorTextureShown` | public | |
| `bool areShadowsEnabled() const` | `areShadowsEnabled` | public | |
| `bool areSelfShadowsEnabled() const` | `areSelfShadowsEnabled` | public | |
| `bool areDefaultLightsEnabled() const` | `areDefaultLightsEnabled` | public | |
| `bool arePunctualLightsEnabled() const` | `arePunctualLightsEnabled` | public | |
| `bool areLightsShown() const` | `areLightsShown` | public | |
| `ShaderProgram* getShader() const` | `getShader` | public | |
| `void setSectionCapsDynamicEnabled(bool enabled)` | `setSectionCapsDynamicEnabled` | public | |
| `void disableSectionCapsInteractionSuppression()` | `disableSectionCapsInteractionSuppression` | public slot | |
| `void setFloorTexRepeatS(double floorTexRepeatS)` | `setFloorTexRepeatS` | public slot | |
| `void setFloorTexRepeatT(double floorTexRepeatT)` | `setFloorTexRepeatT` | public slot | |
| `void setFloorOffsetPercent(double value)` | `setFloorOffsetPercent` | public slot | |
| `void setSkyBoxFOV(double fov)` | `setSkyBoxFOV` | public slot | |
| `void setSkyBoxZRotation(int index)` | `setSkyBoxZRotation` | public slot | |
| `void setSkyBoxTextureHDRI(bool hdrSet)` | `setSkyBoxTextureHDRI` | public slot | |
| `void enableHDRToneMapping(bool hdrToneMapping)` | `enableHDRToneMapping` | public slot | |
| `void enableGammaCorrection(bool gammaCorrection)` | `enableGammaCorrection` | public slot | |
| `void setScreenGamma(double screenGamma)` | `setScreenGamma` | public slot | |
| `void setHDRToneMappingMode(HDRToneMapMode mode)` | `setHDRToneMappingMode` | public slot | |
| `void setEnvMapExposure(double exposure)` | `setEnvMapExposure` | public slot | |
| `void setIBLExposure(double exposure)` | `setIBLExposure` | public slot | |
| `bool isHDRToneMappingEnabled() const` | `isHDRToneMappingEnabled` | public slot | |
| `bool isGammaCorrectionEnabled() const` | `isGammaCorrectionEnabled` | public slot | |
| `HDRToneMapMode getHDRToneMappingMode() const` | `getHDRToneMappingMode` | public slot | |
| `void useDefaultLights(bool useDefaultLights)` | `useDefaultLights` | public slot | |
| `void usePunctualLights(bool usePunctualLights)` | `usePunctualLights` | public slot | |
| `void useIBL(bool useIBL)` | `useIBL` | public slot | |
| `void updateEnvMapRotationMatrix()` | `updateEnvMapRotationMatrix` | private | env map rotation state |
| `void setSectionCapsInteractionSuppressed(bool suppressed)` | `setSectionCapsInteractionSuppressed` | private | |

---

## Destination: `ViewportInteractionController`

| Signature | Method Name | Access | Notes |
|---|---|---|---|
| `void setRotationActive(bool active)` | `setRotationActive` | public | toggles `_viewCtrl` flags only |
| `void setPanningActive(bool active)` | `setPanningActive` | public | |
| `void setZoomingActive(bool active)` | `setZoomingActive` | public | |
| `void beginWindowZoom()` | `beginWindowZoom` | public | interactive zoom gesture state |
| `void performWindowZoom()` | `performWindowZoom` | public | |
| `bool isCameraUpAxisZUp() const` | `isCameraUpAxisZUp` | public | pure `_viewCtrl` accessor |
| `GLCamera::CameraMode cameraMode() const` | `cameraMode` | public | |
| `float getPerspFOV() const` | `getPerspFOV` | public | |
| `QMatrix4x4 getViewMatrix() const` | `getViewMatrix` | public slot | |
| `QMatrix4x4 getProjectionMatrix() const` | `getProjectionMatrix` | public slot | |
| `QMatrix4x4 getModelViewMatrix() const` | `getModelViewMatrix` | public slot | |
| `bool isMultiViewActive() const` | `isMultiViewActive` | public slot | |
| `void setPerspFOV(int fovDegrees)` | `setPerspFOV` | public slot | |
| `void setRotations(float xRot, float yRot, float zRot)` | `setRotations` | private | |
| `void setZoomAndPan(float zoom, QVector3D pan)` | `setZoomAndPan` | private | |
| `void fitBoxToScreen(const BoundingBox& box)` | `fitBoxToScreen` | private | |

---

## Destination: `SceneRuntime`

| Signature | Method Name | Access | Notes |
|---|---|---|---|
| `void setDisplayList(const std::vector<int>& ids)` | `setDisplayList` | public | |
| `int getModelNum() const` | `getModelNum` | public | |
| `std::vector<SceneMesh*> getMeshStore() const` | `getMeshStore` | public | |
| `void addToDisplay(SceneMesh*)` | `addToDisplay` | public | |
| `void removeFromDisplay(int index)` | `removeFromDisplay` | public | |
| `aiScene* getAssImpScene() const` | `getAssImpScene` | public | |
| `glm::mat4 getGlobalSceneTransform() const` | `getGlobalSceneTransform` | public | |
| `void invertADSOpacityTexMap(const std::vector<int>& ids, const bool& inverted)` | `invertADSOpacityTexMap` | public | |
| `void setMaterialToObjects(const std::vector<int>& ids, const GLMaterial& mat)` | `setMaterialToObjects` | public | Phase 2 Item 2 |
| `void setTexturesToObjects(const std::vector<int>& ids, const GLMaterial& mat)` | `setTexturesToObjects` | public | Phase 2 Item 2 |
| `void clearTextureCache()` | `clearTextureCache` | public | drain GPU IDs → GLWidget for `glDeleteTextures` |
| `bool userModelTransformForFile(const QString& sourceFile, QMatrix4x4& outTransform) const` | `userModelTransformForFile` | public | |
| `std::vector<int> getDisplayedObjectsIds() const` | `getDisplayedObjectsIds` | public | |
| `bool isVisibleSwapped() const` | `isVisibleSwapped` | public | |
| `BoundingSphere getBoundingSphere() const` | `getBoundingSphere` | public | |
| `void moveToRecycleBin(const QUuid& uuid, int originalIndex)` | `moveToRecycleBin` | public | |
| `bool restoreFromRecycleBin(const QUuid& uuid)` | `restoreFromRecycleBin` | public | |
| `void permanentlyDeleteFromBin(const QUuid& uuid)` | `permanentlyDeleteFromBin` | public | |
| `bool isInRecycleBin(const QUuid& uuid) const` | `isInRecycleBin` | public | |
| `QVector<QUuid> getRecycleBinUuids() const` | `getRecycleBinUuids` | public | |
| `SceneMesh* getMeshByUuid(const QUuid& uuid) const` | `getMeshByUuid` | public | |
| `SceneMesh* getMeshByIndex(int index) const` | `getMeshByIndex` | public | |
| `int getIndexByUuid(const QUuid& uuid) const` | `getIndexByUuid` | public | |
| `QUuid getUuidByIndex(int index) const` | `getUuidByIndex` | public | |
| `QString generateUniqueMeshName(const QString& baseName)` | `generateUniqueMeshName` | public | |
| `void clearMeshStore()` | `clearMeshStore` | public | |
| `void swapVisible(bool checked)` | `swapVisible` | public slot | |
| `void invalidateRuntimeVisibilityHierarchy()` | `invalidateRuntimeVisibilityHierarchy` | private | BVH management |
| `void rebuildRuntimeVisibilityHierarchy()` | `rebuildRuntimeVisibilityHierarchy` | private | |
| `bool ensureRuntimeVisibilityHierarchy()` | `ensureRuntimeVisibilityHierarchy` | private | |
| `void refreshRuntimeVisibilityCacheForCurrentView()` | `refreshRuntimeVisibilityCacheForCurrentView` | private | |
| `int buildRuntimeVisibilityNodeRecursive(const SceneNode* node, const QHash<QUuid, int>& meshIndexByUuid)` | `buildRuntimeVisibilityNodeRecursive` | private | |
| `bool refreshRuntimeVisibilityNodeBounds(int nodeIndex, const std::vector<unsigned char>& baseVisibleMask, bool refreshBounds)` | `refreshRuntimeVisibilityNodeBounds` | private | |
| `void collectVisibleMeshIdsForPass(int nodeIndex, int activeClipPlaneIndex, bool wantTransparent, std::vector<int>& out) const` | `collectVisibleMeshIdsForPass` | private | |
| `SceneMesh* createMeshFromData(const AssImpMeshData& meshData)` | `createMeshFromData` | private | pure construction; no GL affinity |
| `float highestModelZ()` | `highestModelZ` | private | bounding box query |
| `float lowestModelZ()` | `lowestModelZ` | private | bounding box query |

**TRS property accessors — DEAD CODE** — the backing fields (`_xTran`, `_yTran`, `_zTran`, `_xRot`, `_yRot`, `_zRot`, `_xScale`, `_yScale`, `_zScale`) are initialised in the constructor and referenced only inside their own getter/setter bodies. Zero callers exist anywhere in `src/`. These predate `setTransformation()`/`applyTransforms()` and were never removed. Safe to delete outright.

| Signature | Method Name | Access | Verdict |
|---|---|---|---|
| `float getXTran() const` | `getXTran` | public | DELETE |
| `void setXTran(const float& xTran)` | `setXTran` | public | DELETE |
| `float getYTran() const` | `getYTran` | public | DELETE |
| `void setYTran(const float& yTran)` | `setYTran` | public | DELETE |
| `float getZTran() const` | `getZTran` | public | DELETE |
| `void setZTran(const float& zTran)` | `setZTran` | public | DELETE |
| `float getXRot() const` | `getXRot` | public | DELETE |
| `void setXRot(const float& xRot)` | `setXRot` | public | DELETE |
| `float getYRot() const` | `getYRot` | public | DELETE |
| `void setYRot(const float& yRot)` | `setYRot` | public | DELETE |
| `float getZRot() const` | `getZRot` | public | DELETE |
| `void setZRot(const float& zRot)` | `setZRot` | public | DELETE |
| `float getXScale() const` | `getXScale` | public | DELETE |
| `void setXScale(const float& xScale)` | `setXScale` | public | DELETE |
| `float getYScale() const` | `getYScale` | public | DELETE |
| `void setYScale(const float& yScale)` | `setYScale` | public | DELETE |
| `float getZScale() const` | `getZScale` | public | DELETE |
| `void setZScale(const float& zScale)` | `setZScale` | public | DELETE |

---

## Destination: `SelectionManager`

| Signature | Method Name | Access | Notes |
|---|---|---|---|
| `void select(int id)` | `select` | public | |
| `void deselect(int id)` | `deselect` | public | |
| `void syncMeshSelectionVisualState()` | `syncMeshSelectionVisualState` | public | |
| `int processSelection(const QPoint& pixel)` | `processSelection` | public | color-pick ID resolution |
| `void setSelectionHighlighting(bool highlight)` | `setSelectionHighlighting` | public slot | |
| `void broadcastSelectionChanged(const QList<int>& ids)` | `broadcastSelectionChanged` | public slot | just re-emits signal |
| `ShaderProgram* getSelectionShader() const` | `getSelectionShader` | public slot | dependency injection accessor |
| `std::vector<int> activeTransformGizmoSelectionIds() const` | `activeTransformGizmoSelectionIds` | private | query of selection state |

---

## Destination: `ExplodedViewRuntimeController`

| Signature | Method Name | Access |
|---|---|---|
| `bool beginExplodedViewManualPlacement(const QVector<QUuid>& selectionUuids = {})` | `beginExplodedViewManualPlacement` | public |
| `void finishExplodedViewManualPlacement()` | `finishExplodedViewManualPlacement` | public |
| `void clearExplodedViewManualPlacement()` | `clearExplodedViewManualPlacement` | public |
| `bool isExplodedViewManualPlacementActive() const` | `isExplodedViewManualPlacementActive` | public |
| `bool hasExplodedViewManualPlacement() const` | `hasExplodedViewManualPlacement` | public |
| `bool hasExplodedViewManualTransformChanges() const` | `hasExplodedViewManualTransformChanges` | public |
| `QSet<QUuid> explodedViewManualPlacementUuids() const` | `explodedViewManualPlacementUuids` | public |
| `QVector3D explodedViewManualPlacementTranslationDelta() const` | `explodedViewManualPlacementTranslationDelta` | public |
| `QVector3D explodedViewManualPlacementRotationDelta() const` | `explodedViewManualPlacementRotationDelta` | public |
| `void setExplodedViewManualPlacementTranslationDelta(const QVector3D& delta)` | `setExplodedViewManualPlacementTranslationDelta` | public |
| `void setExplodedViewManualPlacementRotationDelta(const QVector3D& delta)` | `setExplodedViewManualPlacementRotationDelta` | public |
| `QMap<QUuid, TransformState> explodedViewManualStates() const` | `explodedViewManualStates` | public |
| `void restoreExplodedViewManualStates(const QMap<QUuid, TransformState>& states)` | `restoreExplodedViewManualStates` | public |

---

## Destination: `VisibilityComputationHelper` (new)

Stateless free functions or a stateless helper class. All are pure math — no GL state changes.

| Signature | Method Name | Access |
|---|---|---|
| `bool isBoundingBoxOutsideFrustum(const BoundingBox& bb) const` | `isBoundingBoxOutsideFrustum` | private |
| `bool isMeshOutsideFrustum(const SceneMesh* mesh) const` | `isMeshOutsideFrustum` | private |
| `bool isMeshFullyInsideFrustum(const SceneMesh* mesh) const` | `isMeshFullyInsideFrustum` | private |
| `float computeFullyVisibleMinMeshRadius() const` | `computeFullyVisibleMinMeshRadius` | private |
| `bool isBoundingBoxFullyClipped_X(const BoundingBox& bb) const` | `isBoundingBoxFullyClipped_X` | private |
| `bool isBoundingBoxFullyClipped_Y(const BoundingBox& bb) const` | `isBoundingBoxFullyClipped_Y` | private |
| `bool isBoundingBoxFullyClipped_Z(const BoundingBox& bb) const` | `isBoundingBoxFullyClipped_Z` | private |
| `bool isBoundingBoxFullyKept_X(const BoundingBox& bb) const` | `isBoundingBoxFullyKept_X` | private |
| `bool isBoundingBoxFullyKept_Y(const BoundingBox& bb) const` | `isBoundingBoxFullyKept_Y` | private |
| `bool isBoundingBoxFullyKept_Z(const BoundingBox& bb) const` | `isBoundingBoxFullyKept_Z` | private |
| `bool isBoundingBoxStraddlesCapPlane(const BoundingBox& bb, int planeIndex) const` | `isBoundingBoxStraddlesCapPlane` | private |
| `bool isBoundingBoxInvisibleInAllClipPasses(const BoundingBox& bb) const` | `isBoundingBoxInvisibleInAllClipPasses` | private |
| `bool isMeshFullyClipped_X(const SceneMesh* mesh) const` | `isMeshFullyClipped_X` | private |
| `bool isMeshFullyClipped_Y(const SceneMesh* mesh) const` | `isMeshFullyClipped_Y` | private |
| `bool isMeshFullyClipped_Z(const SceneMesh* mesh) const` | `isMeshFullyClipped_Z` | private |
| `bool isMeshFullyKept_X(const SceneMesh* mesh) const` | `isMeshFullyKept_X` | private |
| `bool isMeshFullyKept_Y(const SceneMesh* mesh) const` | `isMeshFullyKept_Y` | private |
| `bool isMeshFullyKept_Z(const SceneMesh* mesh) const` | `isMeshFullyKept_Z` | private |
| `bool isMeshStraddlesCapPlane(const SceneMesh* mesh, int planeIndex) const` | `isMeshStraddlesCapPlane` | private |
| `bool isMeshInvisibleInAllClipPasses(const SceneMesh* mesh) const` | `isMeshInvisibleInAllClipPasses` | private |
| `bool isMeshAnimationVisible(const SceneMesh* mesh) const` | `isMeshAnimationVisible` | private |
| `bool isMeshVisible(const SceneMesh* mesh, int activeClipPlaneIndex) const` | `isMeshVisible` | private |
| `std::vector<QVector3D> collectVisibleCorners() const` | `collectVisibleCorners` | private |
| `float computeFitViewRange(const std::vector<QVector3D>& corners, const QVector3D& right, const QVector3D& up, const QVector3D& viewDir, QVector3D* outCenter = nullptr) const` | `computeFitViewRange` | private |
| `float computeFitViewRange(const QVector3D& right, const QVector3D& up, const QVector3D& viewDir, QVector3D* outCenter = nullptr) const` | `computeFitViewRange` | private |
| `float computeFitViewRange(QVector3D* outCenter = nullptr) const` | `computeFitViewRange` | private |
| `float computeOrthographicFitViewRangeForViewport(const std::vector<QVector3D>& corners, const QVector3D& right, const QVector3D& up, const QVector3D& viewDir, int viewportWidth, int viewportHeight, QVector3D* outCenter = nullptr, const QVector3D& eyePos = QVector3D(0,0,0)) const` | `computeOrthographicFitViewRangeForViewport` | private |
| `QVector3D computeVisibleWorldCenter(const std::vector<QVector3D>& corners) const` | `computeVisibleWorldCenter` | private |
| `float computeSharedOrthographicMultiViewRange(const std::vector<QVector3D>& corners, int viewportWidth, int viewportHeight, const QVector3D& eyePos = QVector3D(0,0,0)) const` | `computeSharedOrthographicMultiViewRange` | private |
| `QRect viewCubeRect() const` | `viewCubeRect` | private |
| `QRect viewCubeScreenRect() const` | `viewCubeScreenRect` | private |

---

## Destination: `CoordinateSystemHelper` (new)

Stateless free functions. Pure coordinate convention math; no GL state.

| Signature | Method Name | Access |
|---|---|---|
| `Plane::Orientation floorPlaneOrientation() const` | `floorPlaneOrientation` | private |
| `QVector3D currentWorldUpVector() const` | `currentWorldUpVector` | private |
| `float coordinateAlongCurrentWorldUp(const QVector3D& point) const` | `coordinateAlongCurrentWorldUp` | private |
| `void setCoordinateAlongCurrentWorldUp(QVector3D& point, float value) const` | `setCoordinateAlongCurrentWorldUp` | private |
| `QQuaternion cameraUpAxisConventionRotation() const` | `cameraUpAxisConventionRotation` | private |
| `QVector3D transformVectorForCameraUpAxis(const QVector3D& vector) const` | `transformVectorForCameraUpAxis` | private |
| `void standardViewBasis(ViewMode mode, QVector3D& viewDir, QVector3D& upDir, QVector3D& rightDir) const` | `standardViewBasis` | private |
| `QQuaternion standardViewRotation(ViewMode mode) const` | `standardViewRotation` | private |
| `bool sceneUpAxisIsZUp(SceneUpAxis sceneUpAxis) const` | `sceneUpAxisIsZUp` | private |
| `QString sceneUpAxisLabel(SceneUpAxis sceneUpAxis) const` | `sceneUpAxisLabel` | private |
| `void warnOnConflictingImportedSceneUpAxis(const QString& fileName, SceneUpAxis sceneUpAxis)` | `warnOnConflictingImportedSceneUpAxis` | private |
| `float groundPlaneZ()` | `groundPlaneZ` | private |
| `float groundPlaneScaleFactor() const` | `groundPlaneScaleFactor` | private |
| `float groundPlaneExtent() const` | `groundPlaneExtent` | private |

---

## Destination: `MvfMeshPreparationWorker` (new)

CPU-only, fully thread-safe, no GL affinity.

| Signature | Method Name | Access | Notes |
|---|---|---|---|
| `static QVector<PreparedMvfMesh> prepareMvfMeshes(const Mvf::Document& document, const QByteArray& geometryChunk, const QByteArray& imageChunk)` | `prepareMvfMeshes` | public | static; thread-safe |
| `struct PreparedMvfMesh` | `PreparedMvfMesh` | (inner type) | moves with the worker |

---

## Destination: `TextureCacheManager` (new)

Cache lookup and reference counting. GL upload calls stay in GLWidget.

| Signature | Method Name | Access |
|---|---|---|
| `unsigned int getOrCreateTextureCached(const QString& cacheKey, const QImage& image, const TextureSamplerSettings& samplers)` | `getOrCreateTextureCached` | public |
| `unsigned int getOrLoadKtx2TextureCached(const QString& path, const std::string& mapType, const TextureSamplerSettings& samplers)` | `getOrLoadKtx2TextureCached` | public |
| `unsigned int getOrLoadTextureCached(const QString& path, const TextureSamplerSettings& samplers)` | `getOrLoadTextureCached` | public |
| `void synchronizeTextureCache(const GLMaterial* material, GLMaterial::TextureType type)` | `synchronizeTextureCache` | public |
| `void retainTexture(unsigned int texId)` | `retainTexture` | private |
| `void releaseTexture(unsigned int texId)` | `releaseTexture` | private |

---

## Destination: `PickingHelper` (new)

Pure math; no GL state.

| Signature | Method Name | Access |
|---|---|---|
| `unsigned int colorToIndex(const QColor& color)` | `colorToIndex` | private |
| `QColor indexToColor(const unsigned int& index)` | `indexToColor` | private |
| `QVector3D get3dTranslationVectorFromMousePoints(const QPoint& start, const QPoint& end)` | `get3dTranslationVectorFromMousePoints` | private |

---

## Summary count

| Destination | Method count |
|---|---|
| STAY in GLWidget | ~115 |
| `AnimationRuntimeController` | 29 |
| `SceneRenderController` | 73 |
| `ViewportInteractionController` | 16 |
| `SceneRuntime` | 35 |
| DELETE (dead TRS accessors) | 18 |
| `SelectionManager` | 8 |
| `ExplodedViewRuntimeController` | 13 |
| `VisibilityComputationHelper` (new) | 31 |
| `CoordinateSystemHelper` (new) | 14 |
| `MvfMeshPreparationWorker` (new) | 1 + struct |
| `TextureCacheManager` (new) | 6 |
| `PickingHelper` (new) | 3 |
| **Total declared methods** | **~344** |
