#pragma once

// ---------------------------------------------------------------------------
// RenderEnums.h
//
// Render-pipeline enumerations shared between GLWidget and
// SceneRenderController.  Extracted from GLWidget.h in Phase 10 of the
// mesh/render/runtime separation refactor so that SceneRenderController.h
// can include them without creating a circular dependency.
// ---------------------------------------------------------------------------

enum class DebugOverlayMode  { BoundingBox, VertexNormals, FaceNormals };
enum class HDRToneMapMode    { KhronosPbrNeutral, ACES_Narkowicz, ACES_Hill,
                               AECS_Hill_Exposure_Boost, Uncharted2ToneMapping, Reinhard };
enum class GroundMode        { None = 0, Floor = 1, Grid = 2 };
