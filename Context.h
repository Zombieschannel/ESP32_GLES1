#pragma once
#include <vector>
#include <array>
#include <unordered_map>
#include "GLES.h"

struct Config
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
    uint8_t depth;
    uint8_t stencil;
    uint8_t sampleBuffers;
    uint8_t samples;
    uint16_t caveat;
    uint8_t surfaceType;
    uint8_t renderableType;
};
struct Context
{
    struct Buffer
    {
        std::uint8_t* data;
    };
    struct Texture
    {
        struct RGBA { uint8_t r, g, b, a; };
        struct RGB { uint8_t r, g, b; };
        struct Luminance { uint8_t a; };
        struct LuminanceAlpha { uint8_t a; };
        struct Alpha { uint8_t a; };
        void* data = nullptr;
        uint16_t width;
        uint16_t height;
        uint16_t internalformat;
    };
    struct TextureUnit
    {
        std::vector<std::array<GLfloat, 16>> glTextureMatrix =  { {1.f, 0.f, 0.f, 0.f,
                                                            0.f, 1.f, 0.f, 0.f,
                                                            0.f, 0.f, 1.f, 0.f,
                                                            0.f, 0.f, 0.f, 1.f} };
        uint32_t glBoundTexture = 0;
        bool glEnabled = false;
        //tex environment
        uint16_t glTextureMinFilter = GL_NEAREST_MIPMAP_LINEAR;
        uint16_t glTextureMagFilter = GL_LINEAR;
        uint16_t glTextureWrapS = GL_REPEAT;
        uint16_t glTextureWrapT = GL_REPEAT;

        bool glUseTexCoordArray = false;
        uint8_t glTexCoordPointerSize = 4;
        uint16_t glTexCoordPointerType = GL_FLOAT;
        uint32_t glTexCoordPointerStride = 0;
        const void* glTexCoordPointer = nullptr;
    };
    std::pair<uint16_t*, uint16_t*> pixels;
    std::pair<uint16_t*, uint16_t*> depth;
    std::pair<uint8_t*, uint8_t*> stencil;
    std::pair<uint8_t*, uint8_t*> alpha;
    const Config* config;
    bool inited = false;

    bool* surfaceFirstFrameBuffer = nullptr;
    uint16_t width = 0;
    uint16_t height = 0;
    GLfloat glClearColorRed = 0.f, glClearColorGreen = 0.f, glClearColorBlue = 0.f, glClearColorAlpha = 0.f;
    GLfloat glClearDepth = 1.f;
    uint16_t glClearStencil = 0;
    std::vector<std::array<GLfloat, 16>> glModelViewMatrix = { {1.f, 0.f, 0.f, 0.f,
                                                            0.f, 1.f, 0.f, 0.f,
                                                            0.f, 0.f, 1.f, 0.f,
                                                            0.f, 0.f, 0.f, 1.f} };
    std::vector<std::array<GLfloat, 16>> glProjectionMatrix = { {1.f, 0.f, 0.f, 0.f,
                                                            0.f, 1.f, 0.f, 0.f,
                                                            0.f, 0.f, 1.f, 0.f,
                                                            0.f, 0.f, 0.f, 1.f} };

    std::unordered_map<uint32_t, Buffer> glBuffers;
    std::unordered_map<uint32_t, Texture> glTextures;
    uint32_t glBufferCounter = 0;
    uint32_t glTextureCounter = 0;
    uint32_t glBoundBuffer = 0;
    uint32_t glBoundElementBuffer = 0;
    TextureUnit glTextureUnit[2];
    uint16_t glActiveTexture = 0;
    uint16_t glClientActiveTexture = 0;

    uint16_t glMatrixMode = GL_MODELVIEW;
    uint16_t glViewportX = 0;
    uint16_t glViewportY = 0;
    uint16_t glViewportWidth = 0;
    uint16_t glViewportHeight = 0;

    bool glUseVertexArray = false;
    uint8_t glVertexPointerSize = 4;
    uint16_t glVertexPointerType = GL_FLOAT;
    uint32_t glVertexPointerStride = 0;
    const void* glVertexPointer = nullptr;

    bool glUseColorArray = false;
    uint8_t glColorPointerSize = 4;
    uint16_t glColorPointerType = GL_FLOAT;
    uint32_t glColorPointerStride = 0;
    const void* glColorPointer = nullptr;

    bool glUseNormalArray = false;
    uint16_t glNormalPointerType = GL_FLOAT;
    uint32_t glNormalPointerStride = 0;
    const void* glNormalPointer = nullptr;

    bool glCullFace = false;
    uint16_t glCullFaceMode = GL_BACK;
    uint16_t glFrontFace = GL_CCW;

    bool glBlend = false;
    uint16_t glBlendColorSrc = GL_ONE;
    uint16_t glBlendAlphaSrc = GL_ONE;
    uint16_t glBlendColorDst = GL_ZERO;
    uint16_t glBlendAlphaDst = GL_ZERO;

    bool glAlphaTest = false;
    uint16_t glAlphaFunc = GL_ALWAYS;
    GLfloat glAlphaRef = 0.f;

    Context(const void* config);

    ~Context();

    void init();
};
