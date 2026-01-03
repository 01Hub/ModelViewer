#include "KTX2Loader.h"
#include <fstream>
#include <QDebug>
#include <QOpenGLContext>
#include <cstring>

#ifndef GL_COMPRESSED_RGBA_BC7_UNORM
#define GL_COMPRESSED_RGBA_BC7_UNORM 0x8E8D
#endif
#ifndef GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB
#define GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB 0x8E8F
#endif

KTX2Loader::KTX2Loader()
{
}

KTX2Loader::~KTX2Loader()
{
}

bool KTX2Loader::initializeOpenGL()
{
    // Initialize the OpenGL function pointers
    QOpenGLContext* context = QOpenGLContext::currentContext();
    if (!context)
    {
        qWarning() << " No OpenGL context current";
        return false;
    }

    // Initialize function pointers
    initializeOpenGLFunctions();
    glInitialized = true;

    // Initialize basisu transcoder (only once)
    if (!basisuInitialized)
    {
        basist::basisu_transcoder_init();
        basisuInitialized = true;
        qDebug() << " Basisu transcoder initialized";
    }

    return true;
}

bool KTX2Loader::readFileToMemory(const std::string& filePath, std::vector<uint8_t>& outData)
{
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        qDebug() << " Failed to open KTX2 file:" << QString::fromStdString(filePath);
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    outData.resize(size);
    if (!file.read(reinterpret_cast<char*>(outData.data()), size))
    {
        qDebug() << " Failed to read KTX2 file data";
        return false;
    }

    qDebug() << " Loaded KTX2 file" << QString::fromStdString(filePath)
        << "(" << (size / 1024 / 1024) << "MB)";
    return true;
}

GPUCapabilities KTX2Loader::detectGPUCapabilities()
{
    GPUCapabilities caps;

    QOpenGLContext* context = QOpenGLContext::currentContext();
    if (!context)
    {
        qWarning() << " No OpenGL context for capability detection";
        return caps;
    }

    // Create a local object to call member functions
    QOpenGLFunctions_4_5_Core functions;
    functions.initializeOpenGLFunctions();

    // Call glGetString through the object
    const GLubyte* extensionsPtr = functions.glGetString(GL_EXTENSIONS);
    const char* extensions = (const char*)extensionsPtr;

    if (extensions)
    {
        std::string extString(extensions);

        if (extString.find("GL_EXT_texture_compression_s3tc") != std::string::npos)
        {
            caps.supportsBC7 = true;
            qDebug() << " GPU supports BC7";
        }
        if (extString.find("GL_KHR_texture_compression_astc_ldr") != std::string::npos)
        {
            caps.supportsASTC = true;
            qDebug() << " GPU supports ASTC";
        }
        if (extString.find("GL_ARB_texture_compression_bptc") != std::string::npos)
        {
            caps.supportsBC6H = true;
            qDebug() << " GPU supports BC6H";
        }
    }

    if (!caps.supportsBC7 && !caps.supportsASTC)
    {
        qDebug() << " GPU doesn't support BC7 or ASTC";
    }

    return caps;
}

basist::transcoder_texture_format KTX2Loader::selectBestFormat(
    const basist::ktx2_transcoder& transcoder,
    const GPUCapabilities& gpuCaps,
    const std::string& mapType)
{
    // For packed textures (metallic/roughness/occlusion/anisotropy), always use uncompressed
    // to preserve channel precision
    if (mapType == "metallic" || 
        mapType == "roughness" || 
        mapType == "occlusion" ||
        mapType == "anisotropy" ||
        mapType == "anisotropyMap")
    {
        qDebug() << "Using RGBA32 for" << QString::fromStdString(mapType) << "(packed texture)";
        return basist::transcoder_texture_format::cTFRGBA32;
    }

    // For HDR textures, use BC6H if available
    if (transcoder.is_hdr())
    {
        if (gpuCaps.supportsBC6H)
        {
            qDebug() << " Selected BC6H (HDR)";
            return basist::transcoder_texture_format::cTFBC6H;
        }
        // Fallback: uncompressed float half
        qDebug() << " Selected RGBA_HALF (HDR fallback)";
        return basist::transcoder_texture_format::cTFRGBA_HALF;
    }

    // For LDR textures: BC7 > ASTC > RGBA32
    if (gpuCaps.supportsBC7)
    {
        qDebug() << " Selected BC7";
        return basist::transcoder_texture_format::cTFBC7_RGBA;
    }

    if (gpuCaps.supportsASTC)
    {
        qDebug() << " Selected ASTC 4x4";
        return basist::transcoder_texture_format::cTFASTC_4x4_RGBA;
    }

    qDebug() << " Selected RGBA32 (fallback)";
    return basist::transcoder_texture_format::cTFRGBA32;
}

uint32_t KTX2Loader::calculateBlockCount(uint32_t width, uint32_t height,
    basist::transcoder_texture_format format)
{
    switch (format)
    {
    case basist::transcoder_texture_format::cTFBC7_RGBA:
    case basist::transcoder_texture_format::cTFASTC_4x4_RGBA:
    case basist::transcoder_texture_format::cTFBC6H:
    {
        // 4x4 blocks
        uint32_t blocksX = (width + 3) / 4;
        uint32_t blocksY = (height + 3) / 4;
        return blocksX * blocksY;
    }

    case basist::transcoder_texture_format::cTFRGBA32:
        // No blocks - direct pixel data
        return width * height;

    case basist::transcoder_texture_format::cTFRGBA_HALF:
        // Half-float RGBA
        return width * height;

    default:
        qDebug() << " Unknown format in calculateBlockCount";
        return 0;
    }
}

void KTX2Loader::setGLFormatInfo(TranscodedTexture& texture)
{
    texture.isCompressed = false;

    switch (texture.format)
    {
    case basist::transcoder_texture_format::cTFBC7_RGBA:
        texture.glInternalFormat = GL_COMPRESSED_RGBA_BC7_UNORM;
        texture.glFormat = GL_RGBA;
        texture.glType = GL_UNSIGNED_BYTE;
        texture.isCompressed = true;
        break;

    case basist::transcoder_texture_format::cTFASTC_4x4_RGBA:
        texture.glInternalFormat = GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
        texture.glFormat = GL_RGBA;
        texture.glType = GL_UNSIGNED_BYTE;
        texture.isCompressed = true;
        break;

    case basist::transcoder_texture_format::cTFBC6H:
        texture.glInternalFormat = GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB;
        texture.glFormat = GL_RGB;
        texture.glType = GL_UNSIGNED_BYTE;
        texture.isCompressed = true;
        break;

    case basist::transcoder_texture_format::cTFRGBA32:
        texture.glInternalFormat = GL_RGBA8;
        texture.glFormat = GL_RGBA;
        texture.glType = GL_UNSIGNED_BYTE;
        texture.isCompressed = false;
        break;

    case basist::transcoder_texture_format::cTFRGBA_HALF:
        texture.glInternalFormat = GL_RGBA16F;
        texture.glFormat = GL_RGBA;
        texture.glType = GL_HALF_FLOAT;
        texture.isCompressed = false;
        break;

    default:
        qDebug() << " Unknown format in setGLFormatInfo";
        texture.glInternalFormat = GL_RGBA8;
        texture.glFormat = GL_RGBA;
        texture.glType = GL_UNSIGNED_BYTE;
        break;
    }
}

bool KTX2Loader::transcodeMipmapLevel(
    basist::ktx2_transcoder& transcoder,
    uint32_t levelIndex,
    basist::transcoder_texture_format format,
    std::vector<uint8_t>& outData)
{
    // Get dimensions for this mipmap level
    uint32_t width = transcoder.get_width();
    uint32_t height = transcoder.get_height();

    // Account for mipmap scaling
    for (uint32_t i = 0; i < levelIndex; i++)
    {
        width = std::max(1u, width / 2);
        height = std::max(1u, height / 2);
    }

    uint32_t blockCount = calculateBlockCount(width, height, format);
    if (blockCount == 0)
    {
        qDebug() << " Invalid block count for mipmap level" << levelIndex;
        return false;
    }

    // Allocate output buffer
    uint32_t requiredSize = 0;
    if (format == basist::transcoder_texture_format::cTFRGBA32)
    {
        requiredSize = blockCount * 4;  // 4 bytes per pixel
    }
    else if (format == basist::transcoder_texture_format::cTFRGBA_HALF)
    {
        requiredSize = blockCount * 8;  // 8 bytes per pixel (4 half-floats)
    }
    else
    {
        // Compressed formats: 16 bytes per 4x4 block
        requiredSize = blockCount * 16;
    }

    outData.resize(requiredSize);

    // Transcode the level
    bool status = transcoder.transcode_image_level(
        0,          // image index
        levelIndex,
        0,          // layer index
        outData.data(),
        blockCount,
        format,
        0           // flags
    );

    if (!status)
    {
        qDebug() << " Failed to transcode mipmap level" << levelIndex;
        return false;
    }

    return true;
}

bool KTX2Loader::loadKTX2(
    const std::string& filePath,
    TranscodedTexture& outTexture,
    const GPUCapabilities& gpuCaps,
    const std::string& mapType)
{
    if (!glInitialized)
    {
        qWarning() << " KTX2Loader not initialized - call initializeOpenGL() first";
        return false;
    }

    qDebug() << "\n=== Loading KTX2 File ===";

    // Step 1: Load file into memory
    std::vector<uint8_t> ktx2Data;
    if (!readFileToMemory(filePath, ktx2Data))
    {
        return false;
    }

    // Step 2: Create and initialize transcoder
    basist::ktx2_transcoder transcoder;
    if (!transcoder.init(ktx2Data.data(), ktx2Data.size()))
    {
        qDebug() << " Failed to initialize KTX2 transcoder";
        return false;
    }

    // Step 3: Query texture properties
    uint32_t width = transcoder.get_width();
    uint32_t height = transcoder.get_height();
    uint32_t numLevels = transcoder.get_levels();

    qDebug() << "KTX2 Properties:";
    qDebug() << "  Dimensions:" << width << "x" << height;
    qDebug() << "  Mipmap levels:" << numLevels;
    qDebug() << "  Is HDR:" << (transcoder.is_hdr() ? "Yes" : "No");

    // Step 4: Select best transcoding format for this GPU
    basist::transcoder_texture_format selectedFormat = selectBestFormat(transcoder, gpuCaps, mapType);

    // Step 5: Start transcoding
    transcoder.start_transcoding();

    // Step 6: Transcode first mipmap level (main texture)
    std::vector<uint8_t> levelData;
    if (!transcodeMipmapLevel(transcoder, 0, selectedFormat, levelData))
    {
        return false;
    }

    // Step 7: Fill output structure
    outTexture.data = levelData;
    outTexture.width = width;
    outTexture.height = height;
    outTexture.mipLevels = numLevels;
    outTexture.format = selectedFormat;

    // Step 8: Set OpenGL format information
    setGLFormatInfo(outTexture);

    // Step 9: Calculate memory info
    uint32_t compressedSize = levelData.size();
    uint32_t uncompressedSize = width * height * 4;  // RGBA uncompressed
    float compressionRatio = (float)compressedSize / uncompressedSize * 100.0f;

    qDebug() << "Transcoding Results:";
    qDebug() << "  Format:" << (int)selectedFormat;
    qDebug() << "  Output size:" << (compressedSize / 1024) << "KB";
    qDebug() << "  Uncompressed equivalent:" << (uncompressedSize / 1024) << "KB";
    qDebug() << "  Compression ratio:" << compressionRatio << "%";

    qDebug() << " KTX2 loading SUCCESSFUL";
    return true;
}

GLuint KTX2Loader::uploadToGPU(const TranscodedTexture& texture)
{
    if (!glInitialized)
    {
        qWarning() << " KTX2Loader not initialized";
        return 0;
    }

    qDebug() << "\n=== Uploading Texture to GPU ===";

    // Create OpenGL texture
    GLuint textureID = 0;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // NOTE: Samplers are NOT set here - they are set by the caller
    // This allows the caller to apply glTF sampler settings

    // Upload texture data
    if (texture.isCompressed)
    {
        // Compressed format
        qDebug() << "Uploading compressed texture (internalFormat:" << texture.glInternalFormat << ")";

        glCompressedTexImage2D(
            GL_TEXTURE_2D,
            0,  // mipmap level
            texture.glInternalFormat,
            texture.width,
            texture.height,
            0,  // border
            texture.data.size(),
            texture.data.data()
        );
    }
    else
    {
        // Uncompressed format
        qDebug() << "Uploading uncompressed texture (format:" << texture.glFormat << ")";

        glTexImage2D(
            GL_TEXTURE_2D,
            0,  // mipmap level
            texture.glInternalFormat,
            texture.width,
            texture.height,
            0,  // border
            texture.glFormat,
            texture.glType,
            texture.data.data()
        );
    }

    // Let GPU generate remaining mipmaps
    glGenerateMipmap(GL_TEXTURE_2D);

    // Unbind texture
    glBindTexture(GL_TEXTURE_2D, 0);

    qDebug() << " Texture uploaded to GPU, ID:" << textureID;
    return textureID;
}
