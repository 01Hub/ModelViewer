#pragma once

#include "RenderEnums.h"
#include "ShaderProgram.h"

#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QString>

#include <array>
#include <memory>
#include <vector>

// ---------------------------------------------------------------------------
// SceneRenderController
//
// Groups all GPU render-pipeline state that was previously scattered through
// GLWidget's private section: shader programs, IBL/environment maps, shadow
// map, transmission and SSS buffers, utility render geometry, and the render
// settings flags that gate each pass.
//
// GLWidget embeds one instance and aliases every field via reference members
// so all existing call sites in GLWidget.cpp compile unchanged.
//
// Introduced in Phase 10 of the mesh/render/runtime separation refactor.
//
// Fields use plain `unsigned int` for GL resource handles so this header
// needs no GL headers; the aliases in GLWidget.h carry the original GLuint
// type where callers expect it.
// ---------------------------------------------------------------------------
class SceneRenderController
{
public:
    // ---- Shader programs ---------------------------------------------------
    std::unique_ptr<ShaderProgram> _bgShader;
    std::unique_ptr<ShaderProgram> _bgSplitShader;
    std::unique_ptr<ShaderProgram> _fgShader;
    std::unique_ptr<ShaderProgram> _fgFlatShader;
    std::unique_ptr<ShaderProgram> _wireframeShader;
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
    std::unique_ptr<ShaderProgram> _textShader;
    std::unique_ptr<ShaderProgram> _debugShader;

    // ---- IBL / environment maps --------------------------------------------
    unsigned int _environmentMap        = 0;
    unsigned int _irradianceMap         = 0;
    unsigned int _prefilterMap          = 0;
    unsigned int _sheenPrefilterMap     = 0;
    unsigned int _prefilterMipLevels    = 5;
    unsigned int _sheenPrefilterMipLevels = 5;
    unsigned int _brdfLUTTexture        = 0;
    unsigned int _charlieLUTTexture     = 0;
    unsigned int _sheenELUTTexture      = 0;
    QString      _currentSkyboxFolder;

    // Preset environment maps (index 1=Studio, 2=Outdoor, 3=Office)
    unsigned int _studioEnvironmentMap      = 0;
    unsigned int _studioIrradianceMap       = 0;
    unsigned int _studioPrefilterMap        = 0;
    unsigned int _studioSheenPrefilterMap   = 0;

    unsigned int _outdoorEnvironmentMap     = 0;
    unsigned int _outdoorIrradianceMap      = 0;
    unsigned int _outdoorPrefilterMap       = 0;
    unsigned int _outdoorSheenPrefilterMap  = 0;

    unsigned int _officeEnvironmentMap      = 0;
    unsigned int _officeIrradianceMap       = 0;
    unsigned int _officePrefilterMap        = 0;
    unsigned int _officeSheenPrefilterMap   = 0;

    unsigned int _skyboxFBO          = 0;
    unsigned int _skyboxDepthBuffer  = 0;

    // ---- Shadow map --------------------------------------------------------
    unsigned int _shadowMap                    = 0;
    unsigned int _shadowMapFBO                 = 0;
    unsigned int _shadowWidth                  = 0;
    unsigned int _shadowHeight                 = 0;
    float        _shadowFarDist                = 1.0f;
    float        _shadowFrustumExtentW         = -1.0f;
    float        _shadowFrustumExtentH         = -1.0f;
    bool         _shadowMapNeedsInitialization = true;

    // ---- Transmission buffer -----------------------------------------------
    unsigned int _transmissionFBO           = 0;
    unsigned int _transmissionColorTexture  = 0;
    unsigned int _transmissionDepthTexture  = 0;
    int          _transmissionTextureWidth  = 0;
    int          _transmissionTextureHeight = 0;
    int          _transmissionMipLevels     = 0;
    bool         _transmissionEnabled       = true;

    // ---- SSS (subsurface scattering) buffer --------------------------------
    unsigned int _sssFBO            = 0;
    unsigned int _sssCaptureTexture = 0;
    unsigned int _sssDepthTexture   = 0;
    unsigned int _sssBlurFBO        = 0;
    unsigned int _sssBlurTexture    = 0;
    int          _sssTextureWidth   = 0;
    int          _sssTextureHeight  = 0;
    bool         _sssEnabled        = false;

    // ---- Debug / utility textures ------------------------------------------
    unsigned int _debugNeutralTex   = 0;
    unsigned int _debugNormalTex    = 0;
    unsigned int _debugBlackTex     = 0;
    int          _globalDebugChannel = 0;
    unsigned int _whiteTexture      = 0;

    // ---- Utility render geometry -------------------------------------------
    unsigned int _debugOverlayBoxVAO = 0;
    unsigned int _debugOverlayBoxVBO = 0;

    QOpenGLVertexArrayObject _bgVAO;
    QOpenGLVertexArrayObject _bgSplitVAO;
    QOpenGLBuffer            _bgSplitVBO;

    QOpenGLVertexArrayObject _axisVAO;
    QOpenGLBuffer            _axisVBO;
    QOpenGLBuffer            _axisCBO;

    unsigned int _conversionCubeVAO = 0;
    unsigned int _conversionCubeVBO = 0;

    unsigned int _quadVAO = 0;
    unsigned int _quadVBO = 0;

    unsigned int _fsTriVAO         = 0;
    unsigned int _fsTriVBO         = 0;
    bool         _fsTriInitialized = false;

    std::array<unsigned int, 6> _viewCubeLabelTextures = { 0, 0, 0, 0, 0, 0 };
    unsigned int _viewCubeLabelVAO = 0;
    unsigned int _viewCubeLabelVBO = 0;

    // ---- Capping -----------------------------------------------------------
    bool         _cappingEnabled = false;
    unsigned int _cappingTexture = 0;

    // ---- Render settings ---------------------------------------------------
    bool         _openGLInitialized                  = false;
    bool         _envMapEnabled                      = false;
    bool         _shadowsEnabled                     = false;
    bool         _selfShadowsEnabled                 = false;
    bool         _reflectionsEnabled                 = false;
    GroundMode   _groundMode                         = GroundMode::None;
    bool         _floorTextureDisplayed              = false;
    bool         _skyBoxEnabled                      = false;
    int          _skyBoxBlurPercent                  = 0;
    std::vector<QString> _skyBoxFaces;
    float        _skyBoxFOV                          = 0.0f;
    float        _skyBoxZRotation                    = 0.0f;
    bool         _skyBoxTextureHDRI                  = false;
    bool         _gammaCorrection                    = false;
    float        _screenGamma                        = 2.2f;
    bool         _hdrToneMapping                     = false;
    HDRToneMapMode _toneMappingMode                  = HDRToneMapMode::KhronosPbrNeutral;
    float        _envMapExposure                     = 1.0f;
    float        _iblExposure                        = 1.0f;
    bool         _lowResEnabled                      = false;
    bool         _sectionCapsSuppressedDuringInteraction = false;
    bool         _dynamicCappingEnabled              = false;
    float        _anisotropicFilteringLevel          = 16.0f;
    bool         _useDefaultLights                   = false;
    bool         _usePunctualLights                  = false;
    bool         _useIBL                             = true;

    // ---- Debug overlay display flags ---------------------------------------
    bool             _showVertexNormals          = false;
    bool             _showFaceNormals            = false;
    bool             _showBoundingBox            = false;
    bool             _debugOverlayEnabled        = false;
    DebugOverlayMode _debugOverlayMode           = DebugOverlayMode::BoundingBox;
    bool             _debugBoundingBoxAvailable  = true;
    bool             _debugVertexNormalsAvailable = true;
    bool             _debugFaceNormalsAvailable  = true;
};
