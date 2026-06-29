#pragma once

// ---------------------------------------------------------------------------
// RenderEnums.h
//
// Render-pipeline enumerations shared between GLWidget and
// SceneRenderController.  Extracted from GLWidget.h in Phase 10 of the
// mesh/render/runtime separation refactor so that SceneRenderController.h
// can include them without creating a circular dependency.
// ---------------------------------------------------------------------------

enum class RenderingMode          { ADS_BLINN_PHONG, PHYSICALLY_BASED_RENDERING };
enum class ClippingPlaneHatchMode { PROCEDURAL, TEXTURE };
enum class HatchPattern           { DIAGONAL_45 = 0, DIAGONAL_135 = 1, HORIZONTAL = 2, VERTICAL = 3, GRID = 4, DIAGONAL_CROSS = 5 };

enum class DebugOverlayMode  { BoundingBox, VertexNormals, FaceNormals };
enum class HDRToneMapMode    { KhronosPbrNeutral, ACES_Narkowicz, ACES_Hill,
                               AECS_Hill_Exposure_Boost, Uncharted2ToneMapping, Reinhard };
enum class GroundMode        { None = 0, Floor = 1, Grid = 2 };

// Viewport enumerations (also extracted here to avoid circular includes from
// ViewportInteractionController.h; GLWidget.h replaces its inline definitions
// with #include "RenderEnums.h" for all of these).
enum class ViewMode          { TOP, BOTTOM, LEFT, RIGHT, FRONT, BACK,
                               ISOMETRIC, DIMETRIC, TRIMETRIC, NONE };
enum class ViewProjection    { ORTHOGRAPHIC, PERSPECTIVE };
enum class CornerAxisPosition { TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT };
