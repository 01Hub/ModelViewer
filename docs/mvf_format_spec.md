# MVF Format Specification

## Overview

This document defines the next-generation `.mvf` format for ModelViewer.

The design intent is:

- `.mvf` is a true native scene/project file
- imported geometry becomes part of the saved scene
- the file is self-contained and portable
- runtime OpenGL state is **not** serialized directly
- geometry, materials, textures, hierarchy, and session state are stored in a compact, deduplicated form

This specification is intentionally inspired by the glTF 2.0 scene model:

- scenes contain nodes
- nodes reference meshes and define local transforms
- meshes contain primitives with typed attribute streams
- materials reference textures and samplers
- images and binary payloads are stored separately from the logical scene graph

Reference used for the conceptual model:

- [glTF 2.0 Specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)

Relevant glTF concepts mirrored here:

- scenes / nodes / hierarchy
- buffers / bufferViews / accessors
- meshes / primitives
- materials / textures / images / samplers
- GLB-style chunked container layout

## Why A New MVF Design Is Needed

The current MVF implementation in:

- [ModelViewer.cpp](D:\work\progs\Qt6\vcpkg\ModelViewer-Qt\src\ModelViewer.cpp)
- [GLWidget.cpp](D:\work\progs\Qt6\vcpkg\ModelViewer-Qt\src\GLWidget.cpp)
- [AssImpMesh.cpp](D:\work\progs\Qt6\vcpkg\ModelViewer-Qt\src\AssImpMesh.cpp)
- [GLMaterial.cpp](D:\work\progs\Qt6\vcpkg\ModelViewer-Qt\src\GLMaterial.cpp)

serializes an expanded runtime mesh representation directly. That has several problems:

- file size explodes compared to source assets
- textures and materials are reconstructed through runtime-oriented logic
- data is duplicated at the mesh level instead of being asset-referenced
- loading becomes slow and fragile
- the format is too tightly coupled to current in-memory classes

The next MVF format should instead serialize a **scene asset model**, then rebuild runtime objects on load.

## Design Goals

1. Self-contained
- no dependency on original imported files
- all geometry, images, materials, and scene state are contained in the `.mvf`

2. Compact
- deduplicate meshes, materials, textures, and images
- compress heavy binary payload blocks

3. Deterministic
- stable object ids
- explicit versioning
- chunked layout with clear validation rules

4. Runtime-independent
- store scene data, not GPU ids, temporary paths, or cache-only state

5. Extensible
- support existing ModelViewer material features
- allow future animation, cameras, and app-specific extras without breaking core readers

## Non-Goals

- Binary compatibility with the current MVF stream layout
- Lossless preservation of ephemeral runtime-only state such as:
  - OpenGL object ids
  - shader program state
  - temporary extracted embedded-texture file paths
  - transient caches

## File Identity

Extension:

- `.mvf`

Magic:

- `MVF3`

Endianness:

- little-endian

Container style:

- GLB-inspired chunked binary container

## High-Level Container Layout

The file layout should be:

1. Fixed header
2. One mandatory JSON metadata chunk
3. One or more binary data chunks
4. Optional future chunks

Recommended binary layout:

```text
+---------------------------+
| Header                    |
+---------------------------+
| Chunk 0: JSON             |
+---------------------------+
| Chunk 1: BIN_GEOMETRY     |
+---------------------------+
| Chunk 2: BIN_IMAGES       |
+---------------------------+
| Chunk 3: BIN_OPTIONAL     |
+---------------------------+
```

### Header

Suggested structure:

```c
struct MvfHeader
{
    char     magic[4];      // "MVF3"
    uint32_t version;       // format version
    uint32_t fileLength;    // total bytes
    uint32_t flags;         // reserved
};
```

### Chunk Header

Suggested structure:

```c
struct MvfChunkHeader
{
    uint32_t chunkLength;
    uint32_t chunkType;
};
```

Suggested chunk types:

- `JSON`
- `GEOM`
- `IMGS`
- `AUX0`

## Core Data Model

MVF should use a glTF-like logical model, with ModelViewer-specific extensions.

Top-level logical sections in the JSON chunk:

- `asset`
- `scene`
- `scenes`
- `nodes`
- `meshes`
- `materials`
- `textures`
- `images`
- `samplers`
- `buffers`
- `bufferViews`
- `accessors`
- `extensionsUsed`
- `extensionsRequired`
- `mvfSession`

This mirrors glTF closely on purpose. It lowers conceptual complexity and makes the format easier to validate.

## Asset Section

The `asset` object should include:

- `version`
- `generator`
- `minReaderVersion`
- `createdUtc`
- `lastSavedUtc`

Example:

```json
{
  "asset": {
    "version": "3.0",
    "generator": "ModelViewer",
    "minReaderVersion": "3.0"
  }
}
```

## Scene And Node Model

This should align closely with the current [SceneGraph](D:\work\progs\Qt6\vcpkg\ModelViewer-Qt\include\SceneGraph.h) and [SceneNode](D:\work\progs\Qt6\vcpkg\ModelViewer-Qt\include\SceneNode.h) design.

### Scenes

`scenes[]` contains root node references.

`scene` is the default active scene index.

### Nodes

Each node should store:

- `id`
- `name`
- `children`
- one of:
  - `matrix`
  - or `translation`, `rotation`, `scale`
- optional `meshBindings`
- optional `extras`

Important MVF difference from glTF:

- glTF nodes normally reference at most one `mesh`
- MVF should allow a node to reference multiple mesh bindings, because the current scene graph stores `meshUuids` directly on a node

Recommended node property:

- `meshBindings`: array of mesh binding objects

Each mesh binding should store:

- `mesh`
- `materialOverride` optional
- `primitiveMask` optional
- `uuid`
- `nameOverride` optional
- `visibility` optional

This preserves stable per-mesh identity without forcing every node/mesh relation into glTF’s one-node-one-mesh assumption.

## Geometry Model

MVF geometry should be stored like glTF mesh primitives, not like current `AssImpMesh` runtime dumps.

### Meshes

Each `mesh` contains:

- `id`
- `name`
- `primitives`
- optional `extras`

### Primitives

Each primitive contains:

- `attributes`
- `indices`
- `material`
- `mode`
- optional `extras`

Supported attributes should include:

- `POSITION`
- `NORMAL`
- `TANGENT`
- `TEXCOORD_0`
- `TEXCOORD_1`
- `TEXCOORD_2`
- `TEXCOORD_3`
- `COLOR_0`

Only attributes actually present should be written.

This is a crucial difference from the current MVF path:

- current MVF writes full per-vertex runtime structs for every mesh
- new MVF should write typed attribute streams referenced through accessors

### Accessors / Buffer Views / Buffers

MVF should reuse glTF’s accessor idea directly:

- accessors describe typed array views
- bufferViews describe byte ranges within binary chunks
- buffers describe logical binary stores

This enables:

- compact packing
- no C++-struct padding issues
- future compression per buffer region
- validation of count, type, and component type

## Material Model

The logical material design should be glTF-inspired, but richer where ModelViewer already supports more features.

The material object should include:

- `name`
- `shadingModel`
- `blendMode`
- `doubleSided`
- `alphaMode`
- `alphaCutoff`
- `opacity`

### Canonical PBR Block

Use a canonical PBR section similar to glTF:

- `baseColorFactor`
- `baseColorTexture`
- `metallicFactor`
- `roughnessFactor`
- `metallicRoughnessTexture`
- `normalTexture`
- `occlusionTexture`
- `emissiveTexture`
- `emissiveFactor`

### ModelViewer Material Extensions

ModelViewer currently supports features beyond core glTF, including:

- transmission
- ior
- sheen
- clearcoat
- specular
- anisotropy
- iridescence
- volume/thickness
- diffuse transmission
- specular-glossiness workflow compatibility
- ADS legacy compatibility

Those should be represented in an `extensions` object with stable MVF names.

Recommended extension blocks:

- `MVF_material_ads`
- `MVF_material_transmission`
- `MVF_material_clearcoat`
- `MVF_material_sheen`
- `MVF_material_specular`
- `MVF_material_anisotropy`
- `MVF_material_iridescence`
- `MVF_material_volume`
- `MVF_material_diffuseTransmission`
- `MVF_material_specularGlossiness`

This mirrors glTF’s extension style while staying native to ModelViewer.

### ADS Compatibility

ADS data should not be the primary shading definition, but it should still be preserved for:

- legacy material workflows
- direct round-trip of ModelViewer-specific user edits

ADS should live in `MVF_material_ads`, containing:

- ambient
- diffuse
- specular
- emissive
- shininess
- metallic boolean compatibility

## Texture, Image, And Sampler Model

MVF should follow glTF’s split:

- `textures` reference `images` and `samplers`
- `images` reference binary payload
- `samplers` define filtering and wrapping

### Images

Each image stores:

- `id`
- `name`
- `mimeType`
- `bufferView`
- `byteLength`
- optional hash

No runtime file path should be required for a valid MVF file.

Optional informational metadata may include:

- original source uri
- import-time file name
- source asset hash

But the binary image payload inside MVF is authoritative.

### Texture Deduplication

Images should be deduplicated by content hash.

Textures should be deduplicated by:

- image id
- sampler id
- UV transform
- texCoord index

This avoids writing the same embedded or shared texture payload repeatedly.

### Samplers

Samplers should store:

- `magFilter`
- `minFilter`
- `wrapS`
- `wrapT`

These map well to the metadata already carried in `GLMaterial::Texture`.

## Binary Payload Storage

Binary chunks should be separated by domain:

- geometry data in `GEOM`
- images in `IMGS`

This keeps large image blobs independent from mesh data and allows future streaming improvements.

### Compression

Compression should be part of the design from the start.

Recommended options:

- per-bufferView compression flag
- whole-chunk compression flag

Suggested initial approach:

- chunk-level compression using zstd or zlib
- clear metadata in JSON describing:
  - compressed byte length
  - uncompressed byte length
  - codec

Without compression, MVF will continue to be unacceptably large for imported assets.

## Session State

The `mvfSession` section stores ModelViewer-specific state that glTF does not define.

It should include:

- visible mesh UUID set
- selected mesh UUID set
- active render mode / shading mode
- camera state
- environment / HDRI settings if desired
- floor / light settings if desired
- tree expansion state optionally

### Visibility And Selection

Visibility and selection should be stored against stable mesh binding UUIDs, not row indices and not transient mesh-store positions.

This matches the current architecture:

- [ModelViewer.cpp](D:\work\progs\Qt6\vcpkg\ModelViewer-Qt\src\ModelViewer.cpp)
- [SceneGraph.cpp](D:\work\progs\Qt6\vcpkg\ModelViewer-Qt\src\SceneGraph.cpp)

## UUID Model

Every persistent scene-level object should have a stable id.

Recommended:

- scene nodes: UUID
- mesh bindings: UUID
- meshes: UUID or integer asset id
- materials: UUID or integer asset id
- images/textures: integer asset id plus optional hash

Important distinction:

- mesh asset identity is not the same as mesh instance identity
- a mesh binding is the instance in the scene

This separation is necessary for future instancing support.

## Relationship To Current Code

The current code already contains pieces of the new conceptual model:

- [SceneGraph](D:\work\progs\Qt6\vcpkg\ModelViewer-Qt\include\SceneGraph.h) is a node hierarchy with transforms and mesh references
- [AssImpMeshExporter](D:\work\progs\Qt6\vcpkg\ModelViewer-Qt\src\AssImpMeshExporter.cpp) already thinks in terms of scene, materials, textures, and deduped export assets
- [GltfPostProcessor](D:\work\progs\Qt6\vcpkg\ModelViewer-Qt\src\GltfPostProcessor.cpp) already maps ModelViewer material state to a glTF-like material model
- [TextureLocationManager](D:\work\progs\Qt6\vcpkg\ModelViewer-Qt\src\TextureLocationManager.cpp) already handles packaging and dedup cues for texture assets

The new MVF format should be designed to sit closer to those export concepts, and farther away from direct `AssImpMesh` runtime serialization.

## Recommended Save Pipeline

When saving MVF:

1. Freeze current scene state
2. Build a temporary MVF asset graph
3. Deduplicate:
- images
- textures
- samplers
- materials
- mesh assets
4. Build JSON metadata
5. Pack binary geometry and image chunks
6. Compress heavy chunks
7. Write header + chunks

Important:

- no OpenGL texture ids
- no temporary extracted texture paths
- no renderer caches

## Recommended Load Pipeline

When loading MVF:

1. Read and validate header
2. Read chunk table
3. Parse JSON metadata
4. Validate references
5. Decompress binary chunks
6. Build asset tables:
- images
- samplers
- textures
- materials
- mesh assets
7. Build scene graph
8. Instantiate runtime meshes from mesh assets + material bindings
9. Rebuild tree and session state
10. Build GPU state

This order avoids the current problem of mixing runtime texture loading and mesh deserialization too early.

## Compatibility Strategy

This should be a new format generation, not an incremental patch on the current one.

Recommended compatibility plan:

- current MVF stream layout remains legacy `MVF2`
- new format becomes `MVF3`
- readers may continue to support `MVF2` as best-effort legacy import
- writers should default to `MVF3`

## Migration Strategy

### Phase 1

Implement the new MVF container and metadata model:

- header
- JSON chunk
- geometry chunk
- image chunk
- scene graph
- material/texture assets
- session state

### Phase 2

Switch save/load to use the new asset graph builder instead of direct `AssImpMesh::serialize()`.

### Phase 3

Optionally add:

- mesh instancing
- cameras
- animations
- undo/redo snapshots or document metadata

## Validation Rules

An MVF reader should reject or warn on:

- invalid magic or version
- missing JSON chunk
- invalid accessor bounds
- invalid bufferView ranges
- dangling references
- unknown required MVF extensions
- missing image payload for referenced image
- material texture references pointing to non-existent textures

## Summary

The next MVF format should be:

- self-contained like GLB
- scene-structured like glTF
- extension-friendly for ModelViewer-specific material/session data
- compact and compressed
- independent of runtime OpenGL objects

That gives ModelViewer a professional native format that matches the intended product behavior:

- imported geometry becomes part of the scene
- the file reopens without depending on source assets
- hierarchy, materials, textures, and session state round-trip cleanly
- file size and load cost remain tractable

