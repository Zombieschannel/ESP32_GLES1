#include "GLES.h"

#include <cfloat>
#include <algorithm>
#include <iostream>

#include "Arduino.h"
#include "Context.h"

#define LIBRARY_NAME "GLES1"
#define glGetMaxTextureSize 512
#define glGetMaxModelViewStack 16
#define glGetMaxProjectionStack 2
#define glGetMaxTextureStack 2
#define glGetMaxTextureUnits 2

#define oneDiv255 0.003921569f
#define oneDiv256 0.00390625f
#define degToRad 0.017453289f
#define oneDiv65536 0.0000152587890625f

static GLint lastError = GL_NO_ERROR;

Context* context;

namespace
{
    struct Vertex
    {
        GLfloat pos[4] = {0, 0, 0, 1};
        GLfloat col[4] = {0, 0, 0, 1};
        GLfloat tex[4] = {0, 0, 0, 1};
    };
    struct Vector2
    {
        float x, y;
    };
    uint16_t RGBto565(uint8_t r, uint8_t g, uint8_t b)
    {
        return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    }
    void RGBFloatFrom565(uint16_t color, float& r, float& g, float& b)
    {
        r = (((color & 0xF800) >> 11) * 8) * oneDiv256;
        g = (((color & 0x7E0) >> 5) * 4) * oneDiv256;
        b = ((color & 0x1F) * 8) * oneDiv256;
    }
    uint16_t swapBytes(uint16_t color)
    {
        return ((color & 0xFF00) >> 8) | ((color & 0xFF) << 8);
    }
    void multiplyMatrixVector(const std::array<GLfloat, 16>& matrix, const GLfloat vector[4],
        GLfloat result[4])
    {
        result[0] = matrix[0] * vector[0] + matrix[4] * vector[1] + matrix[8]  * vector[2] + matrix[12] * vector[3];
        result[1] = matrix[1] * vector[0] + matrix[5] * vector[1] + matrix[9]  * vector[2] + matrix[13] * vector[3];
        result[2] = matrix[2] * vector[0] + matrix[6] * vector[1] + matrix[10] * vector[2] + matrix[14] * vector[3];
        result[3] = matrix[3] * vector[0] + matrix[7] * vector[1] + matrix[11] * vector[2] + matrix[15] * vector[3];

    }
    void multiplyMatrixMatrix(const std::array<GLfloat, 16>& matA, const std::array<GLfloat, 16>& matB,
        std::array<GLfloat, 16>& result)
    {
        for (int8_t i = 0; i < 4; i++)
            for (int8_t j = 0; j < 4; j++)
            {
                result[i * 4 + j] = 0.f;
                for (int8_t k = 0; k < 4; k++)
                    result[i * 4 + j] += matA[k * 4 + j] * matB[i * 4 + k];
            }
    }
    uint8_t sizeOfType(const GLenum type)
    {
        switch (type)
        {
        case GL_FLOAT:
            return 4;
        case GL_UNSIGNED_BYTE:
            return 1;
        case GL_BYTE:
            return 1;
        case GL_UNSIGNED_SHORT:
            return 2;
        case GL_SHORT:
            return 2;
        }
    }
    void getVertexAtOffset(uint32_t vertexTotalOffset, uint32_t colorTotalOffset, uint32_t texCoordTotalOffset, Vertex* vertex)
    {
        const GLubyte* vertices = (const GLubyte*)context->glVertexPointer + vertexTotalOffset;
        const GLubyte* colors = (const GLubyte*)context->glColorPointer + colorTotalOffset;
        const GLubyte* texCoord = (const GLubyte*)context->glTextureUnit[context->glClientActiveTexture].glTexCoordPointer + texCoordTotalOffset;

        for (uint8_t j = 0; j < context->glVertexPointerSize; j++)
        {
            if (context->glVertexPointerType == GL_FLOAT)
                vertex->pos[j] = ((GLfloat*)vertices)[j];
            else if (context->glVertexPointerType == GL_SHORT)
                vertex->pos[j] = ((GLshort*)vertices)[j];
        }
        if (context->glUseColorArray)
            for (uint8_t j = 0; j < context->glColorPointerSize; j++)
            {
                if (context->glColorPointerType == GL_FLOAT)
                    vertex->col[j] = ((GLfloat*)colors)[j];
                else if (context->glColorPointerType == GL_SHORT)
                    vertex->col[j] = ((GLshort*)colors)[j];
                else if (context->glColorPointerType == GL_UNSIGNED_SHORT)
                    vertex->col[j] = ((GLushort*)colors)[j];
                else if (context->glColorPointerType == GL_BYTE)
                    vertex->col[j] = ((GLbyte*)colors)[j];
                else if (context->glColorPointerType == GL_UNSIGNED_BYTE)
                    vertex->col[j] = ((GLubyte*)colors)[j] * oneDiv255;
            }
        if (context->glTextureUnit[context->glClientActiveTexture].glUseTexCoordArray)
            for (uint8_t j = 0; j < context->glTextureUnit[context->glClientActiveTexture].glTexCoordPointerSize; j++)
            {
                if (context->glTextureUnit[context->glClientActiveTexture].glTexCoordPointerType == GL_FLOAT)
                    vertex->tex[j] = ((GLfloat*)texCoord)[j];
                else if (context->glTextureUnit[context->glClientActiveTexture].glTexCoordPointerType == GL_SHORT)
                    vertex->tex[j] = ((GLshort*)texCoord)[j];
            }
    }
    float blendFactorCalc(GLenum factor, const float src[4], const float dst[4], uint8_t component)
    {
        switch (factor)
        {
        case GL_ZERO: return 0.f;
        case GL_ONE: return 1.f;
        case GL_SRC_COLOR: return src[component];
        case GL_ONE_MINUS_SRC_COLOR: return 1.f - src[component];
        case GL_DST_COLOR: return dst[component];
        case GL_ONE_MINUS_DST_COLOR: return 1.f - dst[component];
        case GL_SRC_ALPHA: return src[3];
        case GL_ONE_MINUS_SRC_ALPHA: return 1.f - src[3];
        case GL_DST_ALPHA: return dst[3];
        case GL_ONE_MINUS_DST_ALPHA: return 1.f - dst[3];
        case GL_SRC_ALPHA_SATURATE:
            if (component == 3)
                return 1.f;
            return min(src[3], 1.f - dst[3]);
        default:
            return 1.f;
        }
    }
    void vertexShader(Vertex& targetVertex, const std::array<float, 16>& mvpMatrix)
    {
        //Now transform
        GLfloat temp[4];
        // ESP_LOGE("Pos", "%f %f %f %f", targetVertex.pos[0], targetVertex.pos[1], targetVertex.pos[2], targetVertex.pos[3]);
        multiplyMatrixVector(mvpMatrix, targetVertex.pos, temp);
        if (temp[3] != 1.f)
        {
            GLfloat oneDivW = 1.f / temp[3];
            temp[0] *= oneDivW;
            temp[1] *= oneDivW;
            temp[2] *= oneDivW;
        }
        memcpy(targetVertex.pos, temp, 4 * sizeof(GLfloat));

        multiplyMatrixVector(context->glTextureUnit[context->glActiveTexture].glTextureMatrix.back(), targetVertex.tex, temp);
        memcpy(targetVertex.tex, temp, 4 * sizeof(GLfloat));
    }

    int16_t fragmentShader(const Vertex& v0, const Vertex& v1, const Vertex& v2,
        float weight0, float weight1, float weight2, bool sameColor, uint16_t dstColor, uint8_t dstAlpha, bool& discard, uint8_t& returnAlpha)
    {
        float src[4] = { 1.f, 1.f, 1.f, 1.f };
        Context::Texture& tex = context->glTextures[context->glTextureUnit[context->glActiveTexture].glBoundTexture];
        if (context->glTextureUnit[context->glClientActiveTexture].glUseTexCoordArray && tex.data)
        {
            float texCoord[4];
            for (int8_t i = 0; i < context->glTextureUnit[context->glClientActiveTexture].glTexCoordPointerSize; i++)
                texCoord[i] = v0.tex[i] * weight0 + v1.tex[i] * weight1 + v2.tex[i] * weight2;
            for (int8_t i = 0; i < 4; i++)
                texCoord[i] = std::clamp(texCoord[i], 0.f, 1.f);
            int16_t posX = texCoord[0] * tex.width;
            int16_t posY = texCoord[1] * tex.height;
            if (posX == tex.width)
                posX--;
            if (posY == tex.height)
                posY--;
            Context::Texture::RGBA targetPixel = ((Context::Texture::RGBA*)tex.data)[posY * tex.width + posX];
            src[0] *= targetPixel.r * oneDiv255;
            src[1] *= targetPixel.g * oneDiv255;
            src[2] *= targetPixel.b * oneDiv255;
            src[3] *= targetPixel.a * oneDiv255;
        }
        if (context->glUseColorArray)
        {
            if (!sameColor)
            {
                for (uint8_t i = 0; i < 4; i++)
                    src[i] *= v0.col[i] * weight0 + v1.col[i] * weight1 + v2.col[i] * weight2;
            }
            else
            {
                for (uint8_t i = 0; i < 4; i++)
                    src[i] *= v0.col[i];
            }
        }
        if (context->glAlphaTest)
        {
            switch (context->glAlphaFunc)
            {
            case GL_NEVER: discard = true; return 0;
            case GL_LESS: if (!(src[3] < context->glAlphaRef)) { discard = true; return 0; } break;
            case GL_LEQUAL: if (!(src[3] <= context->glAlphaRef)) { discard = true; return 0; } break;
            case GL_EQUAL: if (!(src[3] == context->glAlphaRef)) { discard = true; return 0; } break;
            case GL_GREATER: if (!(src[3] > context->glAlphaRef)) { discard = true; return 0; } break;
            case GL_NOTEQUAL: if (!(src[3] != context->glAlphaRef)) { discard = true; return 0; } break;
            case GL_GEQUAL: if (!(src[3] >= context->glAlphaRef)) { discard = true; return 0; } break;
            case GL_ALWAYS:
                break;
            }
        }
        /*if (context->glStencilTest)
        {
            switch (context->glStencilFunc)
            {
            case GL_NEVER: discard = true; return 0;
            case GL_LESS: if (!(src[3] < context->glAlphaRef)) { discard = true; return 0; } break;
            case GL_LEQUAL: if (!(src[3] <= context->glAlphaRef)) { discard = true; return 0; } break;
            case GL_EQUAL: if (!(src[3] == context->glAlphaRef)) { discard = true; return 0; } break;
            case GL_GREATER: if (!(src[3] > context->glAlphaRef)) { discard = true; return 0; } break;
            case GL_NOTEQUAL: if (!(src[3] != context->glAlphaRef)) { discard = true; return 0; } break;
            case GL_GEQUAL: if (!(src[3] >= context->glAlphaRef)) { discard = true; return 0; } break;
            case GL_ALWAYS:
                break;
            }
        }*/
        /*if (context->glDepthTest)
        {
            switch (context->glDepthFunc)
            {
            case GL_NEVER: discard = true; return 0;
            case GL_LESS: if (!(src[3] < context->glAlphaRef)) { discard = true; return 0; } break;
            case GL_LEQUAL: if (!(src[3] <= context->glAlphaRef)) { discard = true; return 0; } break;
            case GL_EQUAL: if (!(src[3] == context->glAlphaRef)) { discard = true; return 0; } break;
            case GL_GREATER: if (!(src[3] > context->glAlphaRef)) { discard = true; return 0; } break;
            case GL_NOTEQUAL: if (!(src[3] != context->glAlphaRef)) { discard = true; return 0; } break;
            case GL_GEQUAL: if (!(src[3] >= context->glAlphaRef)) { discard = true; return 0; } break;
            case GL_ALWAYS:
                break;
            }
        }*/
        if (context->glBlend)
        {
            float dst[4];
            RGBFloatFrom565(dstColor, dst[0], dst[1], dst[2]);
            dst[3] = dstAlpha * oneDiv255;

            float result[4];
            for (uint8_t i = 0; i < 3; i++)
            {
                const float sf = blendFactorCalc(context->glBlendColorSrc, src, dst, i);
                const float df = blendFactorCalc(context->glBlendColorDst, src, dst, i);

                result[i] = std::clamp(sf * src[i] + df * dst[i], 0.f, 1.f);
            }
            {
                const float sf = blendFactorCalc(context->glBlendAlphaSrc, src, dst, 3);
                const float df = blendFactorCalc(context->glBlendAlphaDst, src, dst, 3);

                result[3] = std::clamp(sf * src[3] + df * dst[3], 0.f, 1.f);
            }
            for (uint8_t i = 0; i < 4; i++)
                src[i] = result[i];
        }
        returnAlpha = src[3] * 255.f;
        return RGBto565(src[0] * 255.f, src[1] * 255.f, src[2] * 255.f);
    }
    void rasterizeTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2)
    {
        auto edgeFunction = [](const Vector2& a, const Vector2 &b, const Vector2 &c){ return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x); };
        bool sameColor = (v0.col[0] == v1.col[0] && v1.col[0] == v2.col[0] && v0.col[1] == v1.col[1] && v1.col[1] == v2.col[1] && v0.col[2] == v1.col[2] && v1.col[2] == v2.col[2]);

        Vector2 vcoords[3] = { {v0.pos[0], v0.pos[1] }, {v1.pos[0], v1.pos[1] }, {v2.pos[0], v2.pos[1] }};
        if (context->glCullFace)
        {
            float area = edgeFunction(vcoords[0], vcoords[1], vcoords[2]);
            if (context->glFrontFace == GL_CCW && context->glCullFaceMode == GL_BACK ||
                context->glFrontFace == GL_CW && context->glCullFaceMode == GL_FRONT)
            {
                if (area > 0.f)
                    return;
            }
            else if (context->glFrontFace == GL_CW && context->glCullFaceMode == GL_BACK ||
                context->glFrontFace == GL_CCW && context->glCullFaceMode == GL_FRONT)
            {
                if (area < 0.f)
                    return;
            }
        }
        auto targetColor = (*context->surfaceFirstFrameBuffer ? context->pixels.first : context->pixels.second);
        auto targetAlpha = (*context->surfaceFirstFrameBuffer ? context->alpha.first : context->alpha.second);

        Vector2 amin{+FLT_MAX, +FLT_MAX};

        Vector2 amax{-FLT_MAX, -FLT_MAX};

        for (int8_t i = 0; i < 3; i++)
        {
            Vector2 p = vcoords[i];

            amin.x = min(p.x, amin.x);
            amin.y = min(p.y, amin.y);

            amax.x = max(p.x, amax.x);
            amax.y = max(p.y, amax.y);
        }

        amin.x = std::clamp(amin.x, -1.0f, +1.0f);
        amax.x = std::clamp(amax.x, -1.0f, +1.0f);

        amin.y = std::clamp(amin.y, -1.0f, +1.0f);
        amax.y = std::clamp(amax.y, -1.0f, +1.0f);

        const float doublePixelWidth = 2.0f / static_cast<float>(context->width);
        const float doublePixelHeight = 2.0f / static_cast<float>(context->height);

        const float max0 = 1 / edgeFunction(vcoords[1], vcoords[2], vcoords[0]);
        const float max1 = 1 / edgeFunction(vcoords[2], vcoords[0], vcoords[1]);
        const float max2 = 1 / edgeFunction(vcoords[0], vcoords[1], vcoords[2]);

        Vector2 p;

        int32_t minX = static_cast<int32_t>(floor((0.5f + 0.5f * amin.x) * context->width));
        int32_t maxX = static_cast<int32_t>(ceil((0.5f + 0.5f * amax.x) * context->width));
        int32_t minY = static_cast<int32_t>(floor((0.5f + 0.5f * amin.y) * context->height));
        int32_t maxY = static_cast<int32_t>(ceil((0.5f + 0.5f * amax.y) * context->height));

        for (int32_t iy = minY; iy < maxY; iy++)
        {
            p.y = -1.0f + static_cast<float>(iy) * doublePixelHeight;
            bool optimizationEnteredTriangle = false;
            for (int32_t ix = minX; ix < maxX; ix++)
            {
                p.x = -1.0f + static_cast<float>(ix) * doublePixelWidth;
                float w0 = edgeFunction(vcoords[1], vcoords[2], p);
                float w1 = edgeFunction(vcoords[2], vcoords[0], p);
                float w2 = edgeFunction(vcoords[0], vcoords[1], p);

                // clipping emulation
                // float clipping[4] = {
                //     v0.pos[0] * w0 + v1.pos[0] * w1 + v2.pos[0] * w2,
                //     v0.pos[1] * w0 + v1.pos[1] * w1 + v2.pos[1] * w2,
                //     v0.pos[2] * w0 + v1.pos[2] * w1 + v2.pos[2] * w2,
                //     v0.pos[3] * w0 + v1.pos[3] * w1 + v2.pos[3] * w2
                // };
                // if (clipping[0] > clipping[3] || clipping[0] < -clipping[3] ||
                //     clipping[1] > clipping[3] || clipping[1] < -clipping[3] ||
                //     clipping[2] > clipping[3] || clipping[2] < -clipping[3])
                //     continue;

                if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f || w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f)
                {
                    optimizationEnteredTriangle = true;
                    uint32_t iBuf = (context->height - static_cast<int32_t>(iy) - 1) * context->width + ix;
                    float weight0 = w0 * max0;
                    float weight1 = w1 * max1;
                    float weight2 = w2 * max2;
                    uint16_t dstColor = 0;
                    uint8_t dstAlpha = 255;
                    uint8_t returnAlpha = 255;
                    bool discard = false;

                    if (context->glBlend)
                    {
                        dstColor = swapBytes(targetColor[iBuf]);
                        if (targetAlpha)
                            dstAlpha = targetAlpha[iBuf];
                    }

                    uint16_t color = fragmentShader(v0, v1, v2, weight0, weight1, weight2, sameColor, dstColor, dstAlpha, discard, returnAlpha);

                    if (discard)
                        continue;

                    targetColor[iBuf] = swapBytes(color);
                    if (targetAlpha)
                        targetAlpha[iBuf] = returnAlpha;
                }
                else if (optimizationEnteredTriangle)
                    break;
            }
        }
    }
    uint8_t glGet(GLenum pname, GLfloat* data)
    {
        switch (pname)
        {
        case 0x821B: data[0] = 1; return 1;
        case 0x821C: data[0] = 1; return 1;
        case GL_FRAMEBUFFER_BINDING_OES: if (data) data[0] = 0; return 1;
        case GL_ACTIVE_TEXTURE: if (data) data[0] = context->glActiveTexture; return 1;
        // case GL_ALIASED_POINT_SIZE_RANGE: break;
        // case GL_ALIASED_LINE_WIDTH_RANGE: break;
        case GL_ALPHA_BITS: if (data) data[0] = context->config->alpha; return 1;
        case GL_ALPHA_TEST: if (data) data[0] = context->glAlphaTest; return 1;
        case GL_ALPHA_TEST_FUNC: if (data) data[0] = context->glAlphaFunc; return 1;
        case GL_ALPHA_TEST_REF: if (data) data[0] = context->glAlphaRef; return 1;
        case GL_ARRAY_BUFFER_BINDING: if (data) data[0] = context->glBoundBuffer; return 1;
        case GL_BLEND: if (data) data[0] = context->glBlend; return 1;
        // case GL_BLEND_DST: break;
        // case GL_BLEND_SRC: break;
        case GL_BLUE_BITS: if (data) data[0] = context->config->blue; return 1;
        case GL_CLIENT_ACTIVE_TEXTURE: if (data) data[0] = context->glClientActiveTexture; return 1;
        // case GL_CLIP_PLANE0: break;
        // case GL_CLIP_PLANE1: break;
        // case GL_CLIP_PLANE2: break;
        // case GL_CLIP_PLANE3: break;
        // case GL_CLIP_PLANE4: break;
        // case GL_CLIP_PLANE5: break;
        case GL_COLOR_ARRAY: if (data) data[0] = context->glUseColorArray; return 1;
        // case GL_COLOR_ARRAY_BUFFER_BINDING: break;
        case GL_COLOR_ARRAY_SIZE: if (data) data[0] = context->glColorPointerSize; return 1;
        case GL_COLOR_ARRAY_STRIDE: if (data) data[0] = context->glColorPointerStride; return 1;
        case GL_COLOR_ARRAY_TYPE: if (data) data[0] = context->glColorPointerType; return 1;
        case GL_COLOR_CLEAR_VALUE:
            if (data)
            {
                data[0] = context->glClearColorRed;
                data[1] = context->glClearColorGreen;
                data[2] = context->glClearColorBlue;
                data[3] = context->glClearColorAlpha;
            }
            return 4;
        // case GL_COLOR_LOGIC_OP: break;
        // case GL_COLOR_MATERIAL: break;
        // case GL_COLOR_WRITEMASK: break;
        // case GL_COMPRESSED_TEXTURE_FORMATS: break;
        case GL_CULL_FACE: if (data) data[0] = context->glCullFace; return 1;
        case GL_CULL_FACE_MODE: if (data) data[0] = context->glCullFaceMode; return 1;
        // case GL_CURRENT_COLOR: break;
        // case GL_CURRENT_NORMAL: break;
        // case GL_CURRENT_TEXTURE_COORDS: break;
        case GL_DEPTH_BITS: if (data) data[0] = context->config->depth; return 1;
        case GL_DEPTH_CLEAR_VALUE: if (data) data[0] = context->glClearDepth; return 1;
        // case GL_DEPTH_FUNC: break;
        // case GL_DEPTH_RANGE: break;
        // case GL_DEPTH_TEST: break;
        // case GL_DEPTH_WRITEMASK: break;
        // case GL_ELEMENT_ARRAY_BUFFER_BINDING: break;
        // case GL_FOG: break;
        // case GL_FOG_COLOR: break;
        // case GL_FOG_DENSITY: break;
        // case GL_FOG_END: break;
        // case GL_FOG_HINT: break;
        // case GL_FOG_MODE: break;
        // case GL_FOG_START: break;
        case GL_FRONT_FACE: if (data) data[0] = context->glFrontFace; return 1;
        case GL_GREEN_BITS: if (data) data[0] = context->config->green; return 1;
        // case GL_LIGHT_MODEL_AMBIENT: break;
        // case GL_LIGHT_MODEL_TWO_SIDE: break;
        // case GL_LIGHT0: break;
        // case GL_LIGHT1: break;
        // case GL_LIGHT2: break;
        // case GL_LIGHT3: break;
        // case GL_LIGHT4: break;
        // case GL_LIGHT5: break;
        // case GL_LIGHT6: break;
        // case GL_LIGHT7: break;
        // case GL_LIGHTING: break;
        // case GL_LINE_SMOOTH: break;
        // case GL_LINE_SMOOTH_HINT: break;
        // case GL_LINE_WIDTH: break;
        // case GL_LOGIC_OP_MODE: break;
        case GL_MATRIX_MODE: if (data) data[0] = context->glMatrixMode; return 1;
        // case GL_MAX_CLIP_PLANES: break;
        // case GL_MAX_LIGHTS: break;
        case GL_MAX_MODELVIEW_STACK_DEPTH: if (data) data[0] = glGetMaxModelViewStack; return 1;
        case GL_MAX_PROJECTION_STACK_DEPTH: if (data) data[0] = glGetMaxProjectionStack; return 1;
        case GL_MAX_TEXTURE_SIZE: if (data) data[0] = glGetMaxTextureSize; return 1;
        case GL_MAX_TEXTURE_STACK_DEPTH: if (data) data[0] = glGetMaxTextureStack; return 1;
        case GL_MAX_TEXTURE_UNITS: if (data) data[0] = glGetMaxTextureUnits; return 1;
        case GL_MAX_VIEWPORT_DIMS: if (data) { data[0] = context->width; data[1] = context->height; } return 2;
        case GL_MODELVIEW_MATRIX:
            if (data)
                for (uint8_t i = 0; i < 16; i++)
                    data[i] = context->glModelViewMatrix.back()[i];
            return 16;
        case GL_MODELVIEW_STACK_DEPTH: if (data) data[0] = context->glModelViewMatrix.size(); return 1;
        // case GL_MULTISAMPLE: break;
        case GL_NORMAL_ARRAY: if (data) data[0] = context->glUseNormalArray; return 1;
        // case GL_NORMAL_ARRAY_BUFFER_BINDING: break;
        case GL_NORMAL_ARRAY_STRIDE: if (data) data[0] = context->glNormalPointerStride; return 1;
        case GL_NORMAL_ARRAY_TYPE: if (data) data[0] = context->glNormalPointerType; return 1;
        // case GL_NORMALIZE: break;
        // case GL_NUM_COMPRESSED_TEXTURE_FORMATS: break;
        // case GL_PACK_ALIGNMENT: break;
        // case GL_PERSPECTIVE_CORRECTION_HINT: break;
        // case GL_POINT_DISTANCE_ATTENUATION: break;
        // case GL_POINT_FADE_THRESHOLD_SIZE: break;
        // case GL_POINT_SIZE: break;
        // case GL_POINT_SIZE_MAX: break;
        // case GL_POINT_SIZE_MIN: break;
        // case GL_POINT_SMOOTH: break;
        // case GL_POINT_SMOOTH_HINT: break;
        // case GL_POLYGON_OFFSET_FACTOR: break;
        // case GL_POLYGON_OFFSET_FILL: break;
        // case GL_POLYGON_OFFSET_UNITS: break;
        case GL_PROJECTION_MATRIX:
            if (data) for (uint8_t i = 0; i < 16; i++)
                    data[i] = context->glProjectionMatrix.back()[i];
            return 16;
        case GL_PROJECTION_STACK_DEPTH: if (data) data[0] = context->glProjectionMatrix.size(); return 1;
        case GL_RED_BITS: if (data) data[0] = context->config->red; return 1;
        // case GL_RESCALE_NORMAL: break;
        // case GL_SAMPLE_ALPHA_TO_COVERAGE: break;
        // case GL_SAMPLE_ALPHA_TO_ONE: break;
        // case GL_SAMPLE_BUFFERS: break;
        // case GL_SAMPLE_COVERAGE: break;
        // case GL_SAMPLE_COVERAGE_INVERT: break;
        // case GL_SAMPLE_COVERAGE_VALUE: break;
        // case GL_SAMPLES: break;
        // case GL_SCISSOR_BOX: break;
        // case GL_SCISSOR_TEST: break;
        // case GL_SHADE_MODEL: break;
        // case GL_SMOOTH_LINE_WIDTH_RANGE: break;
        // case GL_SMOOTH_POINT_SIZE_RANGE: break;
        case GL_STENCIL_BITS: if (data) data[0] = context->config->stencil; return 1;
        // case GL_STENCIL_CLEAR_VALUE: break;
        // case GL_STENCIL_FAIL: break;
        // case GL_STENCIL_FUNC: break;
        // case GL_STENCIL_PASS_DEPTH_FAIL: break;
        // case GL_STENCIL_PASS_DEPTH_PASS: break;
        // case GL_STENCIL_REF: break;
        // case GL_STENCIL_TEST: break;
        // case GL_STENCIL_VALUE_MASK: break;
        // case GL_STENCIL_WRITEMASK: break;
        // case GL_SUBPIXEL_BITS: break;
        case GL_TEXTURE_2D: if (data) data[0] = context->glTextureUnit[context->glActiveTexture].glEnabled; return 1;
        case GL_TEXTURE_BINDING_2D: if (data) data[0] = context->glTextureUnit[context->glActiveTexture].glBoundTexture; return 1;
        case GL_TEXTURE_COORD_ARRAY: if (data) data[0] = context->glTextureUnit[context->glClientActiveTexture].glUseTexCoordArray; return 1;
        // case GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING: break;
        case GL_TEXTURE_COORD_ARRAY_SIZE: if (data) data[0] = context->glTextureUnit[context->glClientActiveTexture].glTexCoordPointerSize; return 1;
        case GL_TEXTURE_COORD_ARRAY_STRIDE: if (data) data[0] = context->glTextureUnit[context->glClientActiveTexture].glTexCoordPointerStride; return 1;
        case GL_TEXTURE_COORD_ARRAY_TYPE: if (data) data[0] = context->glTextureUnit[context->glClientActiveTexture].glTexCoordPointerType; return 1;
        case GL_TEXTURE_MATRIX:
            if (data)
                for (uint8_t i = 0; i < 16; i++)
                    data[i] = context->glTextureUnit[context->glActiveTexture].glTextureMatrix.back()[i];
            return 16;
        case GL_TEXTURE_STACK_DEPTH: if (data) data[0] = context->glTextureUnit[context->glActiveTexture].glTextureMatrix.size(); return 1;
        // case GL_UNPACK_ALIGNMENT: break;
        case GL_VIEWPORT:
            if (data)
            {
                data[0] = context->glViewportX;
                data[1] = context->glViewportY;
                data[2] = context->glViewportWidth;
                data[3] = context->glViewportHeight;
            }
            return 4;
        case GL_VERTEX_ARRAY: if (data) data[0] = context->glUseVertexArray; return 1;
        // case GL_VERTEX_ARRAY_BUFFER_BINDING: break;
        case GL_VERTEX_ARRAY_SIZE: if (data) data[0] = context->glVertexPointerSize; break;
        case GL_VERTEX_ARRAY_STRIDE: if (data) data[0] = context->glVertexPointerStride; break;
        case GL_VERTEX_ARRAY_TYPE: if (data) data[0] = context->glVertexPointerType; break;
        default:
            ESP_LOGE(LIBRARY_NAME, "glGet Enum not implemented: %d", pname);
            break;
        }
    }
    void glSet(GLenum cap, bool value)
    {
        switch (cap)
        {
        case GL_ALPHA_TEST: context->glAlphaTest = value; break;
        case GL_BLEND: context->glBlend = value; break;
        // case GL_COLOR_LOGIC_OP: break;
        // case GL_COLOR_MATERIAL: break;
        case GL_CULL_FACE: context->glCullFace = value; break;
        // case GL_DEPTH_TEST: break;
        // case GL_DITHER: break;
        // case GL_FOG: break;
        // case GL_LIGHTING: break;
        // case GL_LINE_SMOOTH: break;
        // case GL_MULTISAMPLE: break;
        // case GL_NORMALIZE: break;
        // case GL_POINT_SMOOTH: break;
        // case GL_POLYGON_OFFSET_FILL: break;
        // case GL_RESCALE_NORMAL: break;
        // case GL_SAMPLE_ALPHA_TO_COVERAGE: break;
        // case GL_SAMPLE_ALPHA_TO_ONE: break;
        // case GL_SAMPLE_COVERAGE: break;
        // case GL_SCISSOR_TEST: break;
        // case GL_STENCIL_TEST: break;
        case GL_TEXTURE_2D: context->glTextureUnit[context->glActiveTexture].glEnabled = value; break;
        default:
            ESP_LOGE(LIBRARY_NAME, "glSet Enum not implemented: %d", cap);
            break;
        }
    }
    void glTexParameter(GLenum target, GLenum pname, GLint param)
    {
        if (target != GL_TEXTURE_2D)
        {
            lastError = GL_INVALID_ENUM;
            return;
        }
        switch (pname)
        {
        case GL_TEXTURE_MIN_FILTER:
            switch (param)
            {
            case GL_NEAREST:
            case GL_LINEAR:
            case GL_NEAREST_MIPMAP_NEAREST:
            case GL_LINEAR_MIPMAP_NEAREST:
            case GL_NEAREST_MIPMAP_LINEAR:
            case GL_LINEAR_MIPMAP_LINEAR:
                context->glTextureUnit[context->glActiveTexture].glTextureMagFilter = param;
                return;
            default:
                lastError = GL_INVALID_ENUM;
                return;
            }
        case GL_TEXTURE_MAG_FILTER:
            switch (param)
            {
            case GL_NEAREST:
            case GL_LINEAR:
                context->glTextureUnit[context->glActiveTexture].glTextureMagFilter = param;
                return;
            default:
                lastError = GL_INVALID_ENUM;
                return;
            }
        case GL_TEXTURE_WRAP_S:
            switch (param)
            {
            case GL_REPEAT:
            case GL_CLAMP_TO_EDGE:
                context->glTextureUnit[context->glActiveTexture].glTextureWrapS = param;
                return;
            default:
                lastError = GL_INVALID_ENUM;
                return;
            }
            case GL_TEXTURE_WRAP_T:
            switch (param)
            {
            case GL_REPEAT:
            case GL_CLAMP_TO_EDGE:
                context->glTextureUnit[context->glActiveTexture].glTextureWrapT = param;
                return;
            default:
                lastError = GL_INVALID_ENUM;
                return;
            }
            case GL_GENERATE_MIPMAP:
            ESP_LOGE(LIBRARY_NAME, "glTexParameterf not implemented");
            break;
        default:
            lastError = GL_INVALID_ENUM;
            return;
        }
    }

}

void glActiveTexture(GLenum texture)
{
    if (texture < GL_TEXTURE0 || texture > GL_TEXTURE0 + glGetMaxTextureUnits - 1)
    {
        lastError = GL_INVALID_ENUM;
        return;
    }
    context->glActiveTexture = texture - GL_TEXTURE0;
}

void glAlphaFunc(GLenum func, GLfloat ref)
{
    switch (func)
    {
    case GL_NEVER:
    case GL_LESS:
    case GL_LEQUAL:
    case GL_EQUAL:
    case GL_GREATER:
    case GL_NOTEQUAL:
    case GL_GEQUAL:
    case GL_ALWAYS:
        context->glAlphaFunc = func;
        context->glAlphaRef = std::clamp(ref, 0.f, 1.f);
        return;
    default:
        lastError = GL_INVALID_ENUM;
        return;
    }
}

void glAlphaFuncx(GLenum func, GLfixed ref)
{
    glAlphaFunc(func, ref * oneDiv65536);
}

void glBindBuffer(GLenum target, GLuint buffer)
{
    switch (target)
    {
    case GL_ARRAY_BUFFER: context->glBoundBuffer = buffer; return;
    case GL_ELEMENT_ARRAY_BUFFER: context->glBoundElementBuffer = buffer; return;
    default:
        lastError = GL_INVALID_ENUM;
        return;
    }
}

void glBindTexture(GLenum target, GLuint texture)
{
    switch (target)
    {
    case GL_TEXTURE_2D: context->glTextureUnit[context->glActiveTexture].glBoundTexture = texture; return;
    default:
        lastError = GL_INVALID_ENUM;
        return;
    }
}

void glBlendFunc(GLenum sfactor, GLenum dfactor)
{
    switch (sfactor)
    {
    case GL_ZERO:
    case GL_ONE:
    case GL_DST_COLOR:
    case GL_ONE_MINUS_DST_COLOR:
    case GL_SRC_ALPHA:
    case GL_ONE_MINUS_SRC_ALPHA:
    case GL_DST_ALPHA:
    case GL_ONE_MINUS_DST_ALPHA:
    case GL_SRC_ALPHA_SATURATE:
        context->glBlendColorSrc = sfactor;
        context->glBlendAlphaSrc = sfactor;
        break;
    default:
        lastError = GL_INVALID_ENUM;
        return;
    }
    switch (dfactor)
    {
    case GL_ZERO:
    case GL_ONE:
    case GL_SRC_COLOR:
    case GL_ONE_MINUS_SRC_COLOR:
    case GL_SRC_ALPHA:
    case GL_ONE_MINUS_SRC_ALPHA:
    case GL_DST_ALPHA:
    case GL_ONE_MINUS_DST_ALPHA:
        context->glBlendColorDst = dfactor;
        context->glBlendAlphaDst = dfactor;
        break;
    default:
        lastError = GL_INVALID_ENUM;
        return;
    }
}

void glBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage)
{
    ESP_LOGE(LIBRARY_NAME, "glBufferData unimplemented");
}

void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data)
{
    ESP_LOGE(LIBRARY_NAME, "glBufferSubData unimplemented");
}

void glClear(GLbitfield mask)
{
    if (!(mask & GL_COLOR_BUFFER_BIT) && !(mask & GL_DEPTH_BUFFER_BIT) && !(mask & GL_STENCIL_BUFFER_BIT))
    {
        lastError = GL_INVALID_VALUE;
        return;
    }
    if (mask & GL_COLOR_BUFFER_BIT)
    {
        uint16_t val = swapBytes(RGBto565(context->glClearColorRed * 255.f,
            context->glClearColorGreen * 255.f,
            context->glClearColorBlue * 255.f));
        auto& color = *context->surfaceFirstFrameBuffer ? context->pixels.first : context->pixels.second;
        auto& alpha = *context->surfaceFirstFrameBuffer ? context->alpha.first : context->alpha.second;
        for (uint32_t i = 0; i < context->width * context->height; i++)
        {
            color[i] = val;
            if (alpha)
                alpha[i] = context->glClearColorAlpha * 255.f;
        }
    }
    if (mask & GL_DEPTH_BUFFER_BIT)
    {
        uint16_t val = context->glClearDepth * 65535;
            auto& buff = *context->surfaceFirstFrameBuffer ? context->depth.first : context->depth.second;
            for (uint32_t i = 0; i < context->width * context->height; i++)
                buff[i] = val;
    }
    if (mask & GL_STENCIL_BUFFER_BIT)
    {
        auto& buff = *context->surfaceFirstFrameBuffer ? context->stencil.first : context->stencil.second;
        for (uint32_t i = 0; i < context->width * context->height; i++)
            buff[i] = context->glClearStencil;
    }
}

void glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    context->glClearColorRed = std::clamp(red, 0.f, 1.f);
    context->glClearColorGreen = std::clamp(green, 0.f, 1.f);
    context->glClearColorBlue = std::clamp(blue, 0.f, 1.f);
    context->glClearColorAlpha = std::clamp(alpha, 0.f, 1.f);
}

void glClearColorx(GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha)
{
    glClearColor(red * oneDiv65536, green * oneDiv65536, blue * oneDiv65536, alpha * oneDiv65536);
}

void glClearDepthf(GLfloat d)
{
    context->glClearDepth = std::clamp(d, 0.f, 1.f);
}

void glClearDepthx(GLfixed depth)
{
    glClearDepthf(depth * oneDiv65536);
}

void glClearStencil(GLint s)
{
    context->glClearStencil = std::clamp(s, 0, static_cast<int>(std::pow(2, context->config->stencil) - 1));
}
void glClientActiveTexture(GLenum texture)
{
    if (texture < GL_TEXTURE0 || texture > GL_TEXTURE0 + glGetMaxTextureUnits - 1)
    {
        lastError = GL_INVALID_ENUM;
        return;
    }
    context->glClientActiveTexture = texture - GL_TEXTURE0;
}

void glClipPlanef(GLenum p, const GLfloat* eqn)
{
    ESP_LOGE(LIBRARY_NAME, "glClipPlanef unimplemented");
}

void glClipPlanex(GLenum plane, const GLfixed* equation)
{
    ESP_LOGE(LIBRARY_NAME, "glClipPlane unimplemented");
}

void glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    ESP_LOGE(LIBRARY_NAME, "glColor4f unimplemented");
}

void glColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
    ESP_LOGE(LIBRARY_NAME, "glColor4ub unimplemented");
}

void glColor4x(GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha)
{
    glColor4f(red * oneDiv65536, green * oneDiv65536, blue * oneDiv65536, alpha * oneDiv65536);
}

void glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
    ESP_LOGE(LIBRARY_NAME, "glColorMask unimplemented");
}

void glColorPointer(GLint size, GLenum type, GLsizei stride, const void* pointer)
{
    if (size != 3 && size != 4 || stride < 0)
    {
        lastError = GL_INVALID_VALUE;
        return;
    }
    if (type != GL_BYTE && type != GL_UNSIGNED_BYTE && type != GL_UNSIGNED_SHORT &&
        type != GL_SHORT && type != GL_FLOAT)
    {
        lastError = GL_INVALID_ENUM;
        return;
    }
    context->glColorPointerSize = size;
    context->glColorPointerType = type;
    context->glColorPointerStride = stride;
    context->glColorPointer = pointer;
}

void glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height,
    GLint border, GLsizei imageSize, const void* data)
{
    ESP_LOGE(LIBRARY_NAME, "glCompressedTexImage2D unimplemented");
}

void glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLsizei imageSize, const void* data)
{
    ESP_LOGE(LIBRARY_NAME, "glCompressedTexSubImage2D unimplemented");
}

void glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width,
    GLsizei height, GLint border)
{
    ESP_LOGE(LIBRARY_NAME, "glCopyTexImage2D unimplemented");
}

void glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y,
    GLsizei width, GLsizei height)
{
    ESP_LOGE(LIBRARY_NAME, "glCopyTexSubImage2D unimplemented");
}

void glCullFace(GLenum mode)
{
    switch (mode)
    {
    case GL_FRONT:
    case GL_BACK:
    case GL_FRONT_AND_BACK:
        context->glCullFace = mode;
        return;
    default:
        lastError = GL_INVALID_ENUM;
        return;
    }
}

void glDeleteBuffers(GLsizei n, const GLuint* buffers)
{
    if (n < 0)
    {
        lastError = GL_INVALID_VALUE;
        return;
    }
    if (buffers == nullptr)
        return;

    for (uint16_t i = 0; i < n; i++)
    {
        context->glBuffers.erase(buffers[i]);
        if (context->glBoundBuffer == buffers[i])
            context->glBoundBuffer = 0;
        if (context->glBoundElementBuffer == buffers[i])
            context->glBoundElementBuffer = 0;
    }
}

void glDeleteTextures(GLsizei n, const GLuint* textures)
{
    if (n < 0)
    {
        lastError = GL_INVALID_VALUE;
        return;
    }
    if (textures == nullptr)
        return;

    for (uint16_t i = 0; i < n; i++)
    {
        if (context->glTextures[textures[i]].data)
            heap_caps_free(context->glTextures[textures[i]].data);
        context->glTextures.erase(textures[i]);
        for (int8_t j = 0; j < 2; j++)
            if (context->glTextureUnit[j].glBoundTexture == textures[i])
                context->glTextureUnit[j].glBoundTexture = 0;
    }
}

void glDepthFunc(GLenum func)
{
    ESP_LOGE(LIBRARY_NAME, "glDepthFunc unimplemented");
}

void glDepthMask(GLboolean flag)
{
    ESP_LOGE(LIBRARY_NAME, "glDepthMask unimplemented");
}

void glDepthRangef(GLfloat n, GLfloat f)
{
    ESP_LOGE(LIBRARY_NAME, "glDepthRangef unimplemented");
}

void glDepthRangex(GLfixed n, GLfixed f)
{
    glDepthRangef(n, f * oneDiv65536);
}

void glDisable(GLenum cap)
{
    glSet(cap, false);
}

void glDisableClientState(GLenum array)
{
    switch (array)
    {
    case GL_COLOR_ARRAY: context->glUseColorArray = false; return;
    case GL_NORMAL_ARRAY: context->glUseNormalArray = false; return;
    case GL_TEXTURE_COORD_ARRAY: context->glTextureUnit[context->glClientActiveTexture].glUseTexCoordArray = false; return;
    case GL_VERTEX_ARRAY: context->glUseVertexArray = false; return;
    default: lastError = GL_INVALID_ENUM; return;
    }
}

void glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
    if (count < 0)
    {
        lastError = GL_INVALID_VALUE;
        return;
    }
    if (!context->glUseVertexArray)
        return;
    if (mode != GL_POINTS && mode != GL_LINE_LOOP && mode != GL_LINE_STRIP && mode != GL_TRIANGLES && mode != GL_TRIANGLE_STRIP
        && mode != GL_LINES && mode != GL_TRIANGLE_FAN)
    {
        lastError = GL_INVALID_ENUM;
        return;
    }
    if (context->glCullFace && context->glCullFaceMode == GL_FRONT_AND_BACK &&
        (mode == GL_TRIANGLES || mode == GL_TRIANGLE_FAN || mode == GL_TRIANGLE_STRIP))
    {
        return;
    }
    if (context->glBoundBuffer)
    {
        ESP_LOGE(LIBRARY_NAME, "Buffer bound");
    }
    uint32_t vertexOffset = context->glVertexPointerStride ? context->glVertexPointerStride : context->glVertexPointerSize * sizeOfType(context->glVertexPointerType);
    uint32_t vertexTotalOffset = first * vertexOffset;
    uint32_t colorOffset = context->glColorPointerStride ? context->glColorPointerStride : context->glColorPointerSize * sizeOfType(context->glColorPointerType);
    uint32_t colorTotalOffset = first * colorOffset;
    uint32_t texCoordOffset = context->glTextureUnit[context->glClientActiveTexture].glTexCoordPointerStride ?
        context->glTextureUnit[context->glClientActiveTexture].glTexCoordPointerStride :
        context->glTextureUnit[context->glClientActiveTexture].glTexCoordPointerSize * sizeOfType(context->glTextureUnit[context->glClientActiveTexture].glTexCoordPointerType);
    uint32_t texCoordTotalOffset = first * texCoordOffset;
    std::array<GLfloat, 16> mvpMatrix;
    multiplyMatrixMatrix(context->glProjectionMatrix.back(), context->glModelViewMatrix.back(), mvpMatrix);

    Vertex ver[3];
    uint8_t index = 0;
    uint32_t counter = 0;
    bool stripFanOrder = true;
    for (uint32_t i = first; i < first + count; i++)
    {
        ver[index] = Vertex();
        getVertexAtOffset(vertexTotalOffset, colorTotalOffset, texCoordTotalOffset, &ver[index]);

        vertexShader(ver[index], mvpMatrix);
        //Now render
        switch (mode)
        {
        case GL_POINTS:
            if (ver[index].pos[0] >= -1.f && ver[index].pos[1] >= -1.f &&
                ver[index].pos[0] < 1.f && ver[index].pos[1] < 1.f)
            {
                const int16_t posX = (ver[index].pos[0] + 1.f) * (context->width / 2);
                const int16_t posY = (-ver[index].pos[1] + 1.f) * (context->height / 2);
                const int16_t color = swapBytes(RGBto565(ver[index].col[0] * 255, ver[index].col[1] * 255, ver[index].col[2] * 255));
                if (*context->surfaceFirstFrameBuffer)
                    context->pixels.first[posX + posY * context->width] = color;
                else
                    context->pixels.second[posX + posY * context->width] = color;
            }
            break;
        case GL_TRIANGLES:
            if (index % 3 == 2)
                rasterizeTriangle(ver[0], ver[1], ver[2]);
            index++;
            index %= 3;
            break;
        case GL_TRIANGLE_FAN:
            if (counter >= 2)
            {
                if (!stripFanOrder)
                    rasterizeTriangle(ver[0], ver[1], ver[2]);
                else
                    rasterizeTriangle(ver[0], ver[2], ver[1]);
            }
            if (index == 2)
            {
                stripFanOrder = true;
                index = 1;
            }
            else if (index == 1)
            {
                stripFanOrder = false;
                index = 2;
            }
            else
                index++;
            break;
        case GL_TRIANGLE_STRIP:
            if (counter >= 2)
                rasterizeTriangle(ver[(index + 1) % 3], ver[(index + 2) % 3], ver[(index) % 3]);
            index++;
            index %= 3;
            break;
        default:
            ESP_LOGE(LIBRARY_NAME, "Well shit");
            break;
        }
        counter++;
        vertexTotalOffset += vertexOffset;
        colorTotalOffset += colorOffset;
        texCoordTotalOffset += texCoordOffset;
    }
}

void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices)
{
    ESP_LOGE(LIBRARY_NAME, "glDrawElements unimplemented");
}

void glEnable(GLenum cap)
{
    glSet(cap, true);
}

void glEnableClientState(GLenum array)
{
    switch (array)
    {
    case GL_COLOR_ARRAY: context->glUseColorArray = true; return;
    case GL_NORMAL_ARRAY: context->glUseNormalArray = true; return;
    case GL_TEXTURE_COORD_ARRAY: context->glTextureUnit[context->glClientActiveTexture].glUseTexCoordArray = true; return;
    case GL_VERTEX_ARRAY: context->glUseVertexArray = true; return;
    default: lastError = GL_INVALID_ENUM; return;
    }
}

void glFinish()
{
    ESP_LOGE(LIBRARY_NAME, "glFinish unimplemented");
}

void glFlush()
{
    return;
}

void glFogf(GLenum pname, GLfloat param)
{
    ESP_LOGE(LIBRARY_NAME, "glFogf unimplemented");
}

void glFogfv(GLenum pname, const GLfloat* params)
{
    ESP_LOGE(LIBRARY_NAME, "glFogfv unimplemented");
}

void glFogx(GLenum pname, GLfixed param)
{
    glFogf(pname, param * oneDiv65536);
}

void glFogxv(GLenum pname, const GLfixed* param)
{
    ESP_LOGE(LIBRARY_NAME, "glFogxv unimplemented");
}

void glFrontFace(GLenum mode)
{
    switch (mode)
    {
    case GL_CCW:
    case GL_CW:
        context->glFrontFace = mode;
        return;
    default:
        lastError = GL_INVALID_ENUM;
        return;
    }
}

void glFrustumf(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f)
{
    if (l == r || b == t || n == f)
    {
        lastError = GL_INVALID_VALUE;
        return;
    }
    const float A = (r + l) / (r - l);
    const float B = (t + b) / (t - b);
    const float C = -(f + n) / (f - n);
    const float D = -2.f * f * n / (f - n);
    const std::array<float, 16> temp = {
        2.f * n / (r - l),  0.f,             0.f, 0.f,
        0.f,                2 * n / (t - b), 0.f, 0.f,
        A,                  B,               C,  -1.f,
        0.f,                0.f,             D,   0.f,
    };
    glMultMatrixf(temp.data());
}

void glFrustumx(GLfixed l, GLfixed r, GLfixed b, GLfixed t, GLfixed n, GLfixed f)
{
    glFrustumf(l * oneDiv65536, r * oneDiv65536, b * oneDiv65536, t * oneDiv65536, n * oneDiv65536, f * oneDiv65536);
}

void glGenBuffers(GLsizei n, GLuint* buffers)
{
    if (n < 0)
    {
        lastError = GL_INVALID_VALUE;
        return;
    }
    if (buffers == nullptr)
        return;

    for (uint16_t i = 0; i < n; i++)
    {
        buffers[i] = ++context->glBufferCounter;
        context->glBuffers[context->glBufferCounter];
    }
}

void glGenTextures(GLsizei n, GLuint* textures)
{
    if (n < 0)
    {
        lastError = GL_INVALID_VALUE;
        return;
    }
    if (textures == nullptr)
        return;

    for (uint16_t i = 0; i < n; i++)
    {
        textures[i] = ++context->glTextureCounter;
        context->glTextures[context->glTextureCounter];
    }
}

void glGetBooleanv(GLenum pname, GLboolean* data)
{
    if (data == nullptr)
        return;
    GLfloat res[4] = {MAXFLOAT, MAXFLOAT, MAXFLOAT, MAXFLOAT};
    uint8_t count = glGet(pname, res);
    for (int i = 0; i < 4 && res[i] != MAXFLOAT; i++)
        data[i] = res[i];
}

void glGetBufferParameteriv(GLenum target, GLenum pname, GLint* params)
{
    ESP_LOGE(LIBRARY_NAME, "glGetBufferParameteriv unimplemented");
}

void glGetClipPlanef(GLenum plane, GLfloat* equation)
{
    ESP_LOGE(LIBRARY_NAME, "glGetClipPlanef unimplemented");
}

void glGetClipPlanex(GLenum plane, GLfixed* equation)
{
    ESP_LOGE(LIBRARY_NAME, "glGetClipPlanex unimplemented");
}

GLenum glGetError()
{
    GLint error = lastError;
    lastError = GL_NO_ERROR;
    return error;
}

void glGetFixedv(GLenum pname, GLfixed* params)
{
    if (params == nullptr)
        return;
    GLfloat res[4] = {MAXFLOAT, MAXFLOAT, MAXFLOAT, MAXFLOAT};
    uint8_t count = glGet(pname, res);
    for (int i = 0; i < 4 && res[i] != MAXFLOAT; i++)
        params[i] = res[i];
}

void glGetFloatv(GLenum pname, GLfloat* data)
{
    if (data == nullptr)
        return;
    GLfloat res[16] = {MAXFLOAT, MAXFLOAT, MAXFLOAT, MAXFLOAT, MAXFLOAT, MAXFLOAT, MAXFLOAT, MAXFLOAT};
    uint8_t count = glGet(pname, res);
    for (int i = 0; i < 4 && res[i] != MAXFLOAT; i++)
        data[i] = res[i];
}

void glGetIntegerv(GLenum pname, GLint* data)
{
    if (data == nullptr)
        return;
    GLfloat res[4] = {MAXFLOAT, MAXFLOAT, MAXFLOAT, MAXFLOAT};
    uint8_t count = glGet(pname, res);
    for (int i = 0; i < 4 && res[i] != MAXFLOAT; i++)
        data[i] = res[i];
}

void glGetLightfv(GLenum light, GLenum pname, GLfloat* params)
{
    ESP_LOGE(LIBRARY_NAME, "glGetLightfv unimplemented");
}

void glGetLightxv(GLenum light, GLenum pname, GLfixed* params)
{
    ESP_LOGE(LIBRARY_NAME, "glGetLightxv unimplemented");
}

void glGetMaterialfv(GLenum face, GLenum pname, GLfloat* params)
{
    ESP_LOGE(LIBRARY_NAME, "glGetMaterialfv unimplemented");
}

void glGetMaterialxv(GLenum face, GLenum pname, GLfixed* params)
{
    ESP_LOGE(LIBRARY_NAME, "glGetMaterialxv unimplemented");
}

void glGetPointerv(GLenum pname, void** params)
{
    ESP_LOGE(LIBRARY_NAME, "glGetPointerv unimplemented");
}

const GLubyte* glGetString(GLenum name)
{
    switch (name)
    {
    case GL_VENDOR:
        return (GLubyte*)"Zombieschannel";
    case GL_RENDERER:
        return (GLubyte*)"ABCv2";
    case GL_VERSION:
        return (GLubyte*)"OpenGL ES 1.0";
    case GL_EXTENSIONS:
        return (GLubyte*)" ";
    default:
        lastError = GL_INVALID_ENUM;
        return nullptr;
    }
}

void glGetTexEnvfv(GLenum target, GLenum pname, GLfloat* params)
{
    ESP_LOGE(LIBRARY_NAME, "glGetTexEnvfv unimplemented");
}

void glGetTexEnviv(GLenum target, GLenum pname, GLint* params)
{
    ESP_LOGE(LIBRARY_NAME, "glGetTexEnviv unimplemented");
}

void glGetTexEnvxv(GLenum target, GLenum pname, GLfixed* params)
{
    ESP_LOGE(LIBRARY_NAME, "glGetTexEnvxv unimplemented");
}

void glGetTexParameterfv(GLenum target, GLenum pname, GLfloat* params)
{
    ESP_LOGE(LIBRARY_NAME, "glGetTexParameterfv unimplemented");
}

void glGetTexParameteriv(GLenum target, GLenum pname, GLint* params)
{
    ESP_LOGE(LIBRARY_NAME, "glGetTexParameteriv unimplemented");
}

void glGetTexParameterxv(GLenum target, GLenum pname, GLfixed* params)
{
    ESP_LOGE(LIBRARY_NAME, "glGetTexParameterxv unimplemented");
}

void glHint(GLenum target, GLenum mode)
{
    ESP_LOGE(LIBRARY_NAME, "glHint unimplemented");
}

GLboolean glIsBuffer(GLuint buffer)
{
    if (!buffer)
        return GL_FALSE;
    if (context->glBuffers.find(buffer) != context->glBuffers.end())
        return GL_TRUE;
    return GL_FALSE;
}

GLboolean glIsEnabled(GLenum cap)
{
    switch (cap)
    {
    // case GL_ALPHA_TEST: break;
    case GL_BLEND: return context->glBlend;
    // case GL_COLOR_LOGIC_OP: break;
    // case GL_COLOR_MATERIAL: break;
    case GL_CULL_FACE: return context->glCullFace;
    // case GL_DEPTH_TEST: break;
    // case GL_DITHER: break;
    // case GL_FOG: break;
    // case GL_LIGHTING: break;
    // case GL_LINE_SMOOTH: break;
    // case GL_MULTISAMPLE: break;
    // case GL_NORMALIZE: break;
    // case GL_POINT_SMOOTH: break;
    // case GL_POLYGON_OFFSET_FILL: break;
    // case GL_RESCALE_NORMAL: break;
    // case GL_SAMPLE_ALPHA_TO_COVERAGE: break;
    // case GL_SAMPLE_ALPHA_TO_ONE: break;
    // case GL_SAMPLE_COVERAGE: break;
    // case GL_SCISSOR_TEST: break;
    // case GL_STENCIL_TEST: break;
    case GL_TEXTURE_2D: return context->glTextureUnit[context->glActiveTexture].glEnabled;
    default:
        ESP_LOGE(LIBRARY_NAME, "glSet Enum not implemented: %d", cap);
        break;
    }
}

GLboolean glIsTexture(GLuint texture)
{
    if (!texture)
        return GL_FALSE;
    if (context->glTextures.find(texture) != context->glTextures.end())
        return GL_TRUE;
    return GL_FALSE;
}

void glLightModelf(GLenum pname, GLfloat param)
{
    ESP_LOGE(LIBRARY_NAME, "glLightModelf unimplemented");
}

void glLightModelfv(GLenum pname, const GLfloat* params)
{
    ESP_LOGE(LIBRARY_NAME, "glLightModelfv unimplemented");
}

void glLightModelx(GLenum pname, GLfixed param)
{
    glLightModelf(pname, param * oneDiv65536);
}

void glLightModelxv(GLenum pname, const GLfixed* param)
{
    ESP_LOGE(LIBRARY_NAME, "glLightModelxv unimplemented");
}

void glLightf(GLenum light, GLenum pname, GLfloat param)
{
    ESP_LOGE(LIBRARY_NAME, "glLightf unimplemented");
}

void glLightfv(GLenum light, GLenum pname, const GLfloat* params)
{
    ESP_LOGE(LIBRARY_NAME, "glLightfv unimplemented");
}

void glLightx(GLenum light, GLenum pname, GLfixed param)
{
    glLightf(light, pname, param * oneDiv65536);
}

void glLightxv(GLenum light, GLenum pname, const GLfixed* params)
{
    ESP_LOGE(LIBRARY_NAME, "glLightxv unimplemented");
}

void glLineWidth(GLfloat width)
{
    ESP_LOGE(LIBRARY_NAME, "glLineWidth unimplemented");
}

void glLineWidthx(GLfixed width)
{
    glLineWidth(width * oneDiv65536);
}

void glLoadIdentity()
{
    switch (context->glMatrixMode)
    {
    case GL_MODELVIEW:
        context->glModelViewMatrix.back() = std::array<float, 16>{
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f
        };
        return;
    case GL_PROJECTION:
        context->glProjectionMatrix.back() = std::array<float, 16>{
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f
        };
        return;
    case GL_TEXTURE:
        context->glTextureUnit[context->glActiveTexture].glTextureMatrix.back() = std::array<float, 16>{
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f
        };
        return;
    }
}

void glLoadMatrixf(const GLfloat* m)
{
    switch (context->glMatrixMode)
    {
    case GL_MODELVIEW:
        for (uint8_t i = 0; i < 16; i++)
            context->glModelViewMatrix.back()[i] = m[i];
        return;
    case GL_PROJECTION:
        for (uint8_t i = 0; i < 16; i++)
            context->glProjectionMatrix.back()[i] = m[i];
        return;
    case GL_TEXTURE:
        for (uint8_t i = 0; i < 16; i++)
            context->glTextureUnit[context->glActiveTexture].glTextureMatrix.back()[i] = m[i];
        return;
    }
}

void glLoadMatrixx(const GLfixed* m)
{
    GLfloat n[16];
    for (uint8_t i = 0; i < 16; i++)
        n[i] = m[i] * oneDiv65536;
    glLoadMatrixf(n);
}

void glLogicOp(GLenum opcode)
{
    ESP_LOGE(LIBRARY_NAME, "glLogicOp unimplemented");
}

void glMaterialf(GLenum face, GLenum pname, GLfloat param)
{
    ESP_LOGE(LIBRARY_NAME, "glMaterialf unimplemented");
}

void glMaterialfv(GLenum face, GLenum pname, const GLfloat* params)
{
    ESP_LOGE(LIBRARY_NAME, "glMaterialfv unimplemented");
}

void glMaterialx(GLenum face, GLenum pname, GLfixed param)
{
    ESP_LOGE(LIBRARY_NAME, "glMaterialx unimplemented");
}

void glMaterialxv(GLenum face, GLenum pname, const GLfixed* param)
{
    ESP_LOGE(LIBRARY_NAME, "glMaterialxv unimplemented");
}

void glMatrixMode(GLenum mode)
{
    switch (mode)
    {
    case GL_MODELVIEW: case GL_PROJECTION: case GL_TEXTURE:
        context->glMatrixMode = mode;
        break;
    default:
        lastError = GL_INVALID_ENUM;
        break;
    }
}

void glMultMatrixf(const GLfloat* m)
{
    std::array<float, 16>* current;
    switch (context->glMatrixMode)
    {
    case GL_MODELVIEW:
        current = &context->glModelViewMatrix.back();
        break;
    case GL_PROJECTION:
        current = &context->glProjectionMatrix.back();
        break;
    case GL_TEXTURE:
        current = &context->glTextureUnit[context->glActiveTexture].glTextureMatrix.back();
        break;
    }
    std::array<float, 16> tmp;
    for (int8_t i = 0; i < 4; i++)
        for (int8_t j = 0; j < 4; j++)
        {
            tmp[i * 4 + j] = 0.f;
            for (int8_t k = 0; k < 4; k++)
                tmp[i * 4 + j] += (*current)[k * 4 + j] * m[i * 4 + k];
        }
    *current = tmp;
}

void glMultMatrixx(const GLfixed* m)
{
    GLfloat n[16];
    for (uint8_t i = 0; i < 16; i++)
        n[i] = m[i] * oneDiv65536;
    glMultMatrixf(n);
}

void glMultiTexCoord4f(GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q)
{
    ESP_LOGE(LIBRARY_NAME, "glMultiTexCoord4f unimplemented");
}

void glMultiTexCoord4x(GLenum texture, GLfixed s, GLfixed t, GLfixed r, GLfixed q)
{
    ESP_LOGE(LIBRARY_NAME, "glMultiTexCoordxf unimplemented");
}

void glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz)
{
    ESP_LOGE(LIBRARY_NAME, "glNormal3f unimplemented");
}

void glNormal3x(GLfixed nx, GLfixed ny, GLfixed nz)
{
    ESP_LOGE(LIBRARY_NAME, "glNormal3x unimplemented");
}

void glNormalPointer(GLenum type, GLsizei stride, const void* pointer)
{
    if (stride < 0)
    {
        lastError = GL_INVALID_VALUE;
        return;
    }
    if (type != GL_BYTE && type != GL_SHORT && type != GL_FLOAT)
    {
        lastError = GL_INVALID_ENUM;
        return;
    }
    context->glNormalPointerType = type;
    context->glNormalPointerStride = stride;
    context->glNormalPointer = pointer;
}

void glOrthof(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f)
{
    if (l == r || b == t || n == f)
    {
        lastError = GL_INVALID_VALUE;
        return;
    }
    const float tx = -(r + l) / (r - l);
    const float ty = -(t + b) / (t - b);
    const float tz = -(f + n) / (f - n);
    const std::array<float, 16> temp = {
        2.f / (r - l),  0.f,           0.f,           0.f,
        0.f,            2.f / (t - b), 0.f,           0.f,
        0.f,            0.f,          -2.f / (f - n), 0.f,
        tx,             ty,            tz,            1.f,
    };
    glMultMatrixf(temp.data());
}

void glOrthox(GLfixed l, GLfixed r, GLfixed b, GLfixed t, GLfixed n, GLfixed f)
{
    glOrthof(l * oneDiv65536, r * oneDiv65536, b * oneDiv65536, t * oneDiv65536, f * oneDiv65536, n * oneDiv65536);
}

void glPixelStorei(GLenum pname, GLint param)
{
    ESP_LOGE(LIBRARY_NAME, "glPixelStorei unimplemented");
}

void glPointParameterf(GLenum pname, GLfloat param)
{
    ESP_LOGE(LIBRARY_NAME, "glPointParameterf unimplemented");
}

void glPointParameterfv(GLenum pname, const GLfloat* params)
{
    ESP_LOGE(LIBRARY_NAME, "glPointParameterfv unimplemented");
}

void glPointParameterx(GLenum pname, GLfixed param)
{
    ESP_LOGE(LIBRARY_NAME, "glPointParameterx unimplemented");
}

void glPointParameterxv(GLenum pname, const GLfixed* params)
{
    ESP_LOGE(LIBRARY_NAME, "glPointParameterxv unimplemented");
}

void glPointSize(GLfloat size)
{
    ESP_LOGE(LIBRARY_NAME, "glPointSize unimplemented");
}

void glPointSizex(GLfixed size)
{
    ESP_LOGE(LIBRARY_NAME, "glPointSizex unimplemented");
}

void glPolygonOffset(GLfloat factor, GLfloat units)
{
    ESP_LOGE(LIBRARY_NAME, "glPolygonOffset unimplemented");
}

void glPolygonOffsetx(GLfixed factor, GLfixed units)
{
    ESP_LOGE(LIBRARY_NAME, "glPolygonOffsetx unimplemented");
}

void glPopMatrix()
{
    switch (context->glMatrixMode)
    {
    case GL_MODELVIEW:
        if (context->glModelViewMatrix.size() <= 1)
        {
            lastError = GL_STACK_UNDERFLOW;
            return;
        }
        context->glModelViewMatrix.pop_back();
        return;
    case GL_PROJECTION:
        if (context->glProjectionMatrix.size() <= 1)
        {
            lastError = GL_STACK_UNDERFLOW;
            return;
        }
        context->glProjectionMatrix.pop_back();
        return;
    case GL_TEXTURE:
        if (context->glTextureUnit[context->glActiveTexture].glTextureMatrix.size() <= 1)
        {
            lastError = GL_STACK_UNDERFLOW;
            return;
        }
        context->glTextureUnit[context->glActiveTexture].glTextureMatrix.pop_back();
        return;
    }
}

void glPushMatrix()
{
    switch (context->glMatrixMode)
    {
    case GL_MODELVIEW:
        if (context->glModelViewMatrix.size() >= 32)
        {
            lastError = GL_STACK_OVERFLOW;
            return;
        }
        context->glModelViewMatrix.push_back(context->glModelViewMatrix.back());
        return;
    case GL_PROJECTION:
        if (context->glProjectionMatrix.size() >= 2)
        {
            lastError = GL_STACK_OVERFLOW;
            return;
        }
        context->glProjectionMatrix.push_back(context->glProjectionMatrix.back());
        return;
    case GL_TEXTURE:
        if (context->glTextureUnit[context->glActiveTexture].glTextureMatrix.size() >= 2)
        {
            lastError = GL_STACK_OVERFLOW;
            return;
        }
        context->glTextureUnit[context->glActiveTexture].glTextureMatrix.push_back(
            context->glTextureUnit[context->glActiveTexture].glTextureMatrix.back());
        return;
    }
}

void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels)
{
    ESP_LOGE(LIBRARY_NAME, "glReadPixels unimplemented");
}

void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
    const float c = cosf(angle * degToRad);
    const float s = sinf(angle * degToRad);
    const std::array<float, 16> t = {
        x * x * (1 - c) + c,     y * x * (1 - c) + z * s, x * z * (1 - c) - y * s, 0.f,
        x * y * (1 - c) - z * s, y * y * (1 - c) + c,     y * z * (1 - c) + x * s, 0.f,
        x * z * (1 - c) + y * s, y * z * (1 - c) - x * s, z * z * (1 - c) + c,     0.f,
        0.f,                     0.f,                     0.f,                     1.f,
    };
    glMultMatrixf(t.data());
}

void glRotatex(GLfixed angle, GLfixed x, GLfixed y, GLfixed z)
{
    glRotatef(angle * oneDiv65536, x * oneDiv65536, y * oneDiv65536, z * oneDiv65536);
}

void glSampleCoverage(GLfloat value, GLboolean invert)
{
    ESP_LOGE(LIBRARY_NAME, "glSampleCoverage unimplemented");
}

void glSampleCoveragex(GLclampx value, GLboolean invert)
{
    ESP_LOGE(LIBRARY_NAME, "glSampleCoveragex unimplemented");
}

void glScalef(GLfloat x, GLfloat y, GLfloat z)
{
    const std::array<float, 16> t = {
        x, 0.f, 0.f, 0.f,
        0.f, y, 0.f, 0.f,
        0.f, 0.f, z, 0.f,
        0.f, 0.f, 0.f, 1.f,
    };
    glMultMatrixf(t.data());
}

void glScalex(GLfixed x, GLfixed y, GLfixed z)
{
    glScalef(x * oneDiv65536, y * oneDiv65536, z * oneDiv65536);
}

void glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
    ESP_LOGE(LIBRARY_NAME, "glScissor unimplemented");
}

void glShadeModel(GLenum mode)
{
    ESP_LOGE(LIBRARY_NAME, "glShadeModel unimplemented");
}

void glStencilFunc(GLenum func, GLint ref, GLuint mask)
{
    ESP_LOGE(LIBRARY_NAME, "glStencilFunc unimplemented");
}

void glStencilMask(GLuint mask)
{
    ESP_LOGE(LIBRARY_NAME, "glStencilMask unimplemented");
}

void glStencilOp(GLenum fail, GLenum zfail, GLenum zpass)
{
    ESP_LOGE(LIBRARY_NAME, "glStencilOp unimplemented");
}

void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const void* pointer)
{
    if (size != 1 && size != 2 && size != 3 && size != 4 || stride < 0)
    {
        lastError = GL_INVALID_VALUE;
        return;
    }
    if (type != GL_SHORT && type != GL_FLOAT)
    {
        lastError = GL_INVALID_ENUM;
        return;
    }
    if (type != GL_FLOAT)
        ESP_LOGE(LIBRARY_NAME, "Just don't");
    context->glTextureUnit[context->glClientActiveTexture].glTexCoordPointerSize = size;
    context->glTextureUnit[context->glClientActiveTexture].glTexCoordPointerType = type;
    context->glTextureUnit[context->glClientActiveTexture].glTexCoordPointerStride = stride;
    context->glTextureUnit[context->glClientActiveTexture].glTexCoordPointer = pointer;
}

void glTexEnvf(GLenum target, GLenum pname, GLfloat param)
{
    ESP_LOGE(LIBRARY_NAME, "glTexEnvf unimplemented");
}

void glTexEnvfv(GLenum target, GLenum pname, const GLfloat* params)
{
    ESP_LOGE(LIBRARY_NAME, "glTexEnvfv unimplemented");
}

void glTexEnvi(GLenum target, GLenum pname, GLint param)
{
    ESP_LOGE(LIBRARY_NAME, "glTexEnvi unimplemented");
}

void glTexEnviv(GLenum target, GLenum pname, const GLint* params)
{
    ESP_LOGE(LIBRARY_NAME, "glTexEnviv unimplemented");
}

void glTexEnvx(GLenum target, GLenum pname, GLfixed param)
{
    ESP_LOGE(LIBRARY_NAME, "glTexEnvx unimplemented");
}

void glTexEnvxv(GLenum target, GLenum pname, const GLfixed* params)
{
    ESP_LOGE(LIBRARY_NAME, "glTexEnvxv unimplemented");
}

void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border,
    GLenum format, GLenum type, const void* pixels)
{
    if (target != GL_TEXTURE_2D)
    {
        lastError = GL_INVALID_ENUM;
        return;
    }
    if (level < 0)
    {
        lastError = GL_INVALID_VALUE;
        return;
    }
    if (border != 0)
    {
        lastError = GL_INVALID_VALUE;
        return;
    }
    if (internalformat != format)
    {
        lastError = GL_INVALID_OPERATION;
        return;
    }
    if (internalformat != GL_RGBA && internalformat != GL_RGB && internalformat != GL_ALPHA &&
        internalformat != GL_LUMINANCE && internalformat != GL_LUMINANCE_ALPHA)
    {
        lastError = GL_INVALID_ENUM;
        return;
    }
    if (type != GL_UNSIGNED_BYTE || level != 0 || internalformat != GL_RGBA)
    {
        ESP_LOGE(LIBRARY_NAME, "Just don't");
    }
    Context::Texture& tex = context->glTextures[context->glTextureUnit[context->glActiveTexture].glBoundTexture];
    tex.width = width;
    tex.height = height;

    uint8_t size = 0;
    switch (internalformat)
    {
    case GL_ALPHA: size = sizeof(Context::Texture::Alpha); break;
    case GL_RGB: size = sizeof(Context::Texture::RGB); break;
    case GL_RGBA: size = sizeof(Context::Texture::RGBA); break;
    case GL_LUMINANCE: size = sizeof(Context::Texture::Luminance); break;
    case GL_LUMINANCE_ALPHA: size = sizeof(Context::Texture::LuminanceAlpha); break;
    }
    if (!tex.data)
        tex.data = heap_caps_malloc(width * height * size, MALLOC_CAP_SPIRAM);
    if (pixels)
    {
        for (uint32_t i = 0; i < width * height * size; i++)
            static_cast<uint8_t*>(tex.data)[i] = static_cast<const uint8_t*>(pixels)[i];
    }
}

void glTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
    glTexParameter(target, pname, param);
}

void glTexParameterfv(GLenum target, GLenum pname, const GLfloat* params)
{
    glTexParameter(target, pname, params[0]);
}

void glTexParameteri(GLenum target, GLenum pname, GLint param)
{
    glTexParameter(target, pname, param);
}

void glTexParameteriv(GLenum target, GLenum pname, const GLint* params)
{
    glTexParameter(target, pname, params[0]);
}

void glTexParameterx(GLenum target, GLenum pname, GLfixed param)
{
    glTexParameter(target, pname, param);
}

void glTexParameterxv(GLenum target, GLenum pname, const GLfixed* params)
{
    glTexParameter(target, pname, params[0]);
}

void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
    GLenum format, GLenum type, const void* pixels)
{
    if (target != GL_TEXTURE_2D)
    {
        lastError = GL_INVALID_ENUM;
        return;
    }
    if (level < 0)
    {
        lastError = GL_INVALID_VALUE;
        return;
    }
    if (format != GL_RGBA && format != GL_RGB && format != GL_ALPHA &&
        format != GL_LUMINANCE && format != GL_LUMINANCE_ALPHA)
    {
        lastError = GL_INVALID_ENUM;
        return;
    }
    if (type != GL_UNSIGNED_BYTE || level != 0 || format != GL_RGBA)
    {
        ESP_LOGE(LIBRARY_NAME, "Just don't");
    }
    if (width < 0 || height < 0)
    {
        lastError = GL_INVALID_VALUE;
        return;
    }
    Context::Texture& tex = context->glTextures[context->glTextureUnit[context->glActiveTexture].glBoundTexture];
    if (xoffset < 0 || xoffset + width > tex.width || yoffset < 0 || yoffset + height > tex.height)
    {
        lastError = GL_INVALID_VALUE;
        return;
    }

    uint8_t size = 0;
    switch (format)
    {
    case GL_ALPHA: size = sizeof(Context::Texture::Alpha); break;
    case GL_RGB: size = sizeof(Context::Texture::RGB); break;
    case GL_RGBA: size = sizeof(Context::Texture::RGBA); break;
    case GL_LUMINANCE: size = sizeof(Context::Texture::Luminance); break;
    case GL_LUMINANCE_ALPHA: size = sizeof(Context::Texture::LuminanceAlpha); break;
    }
    if (!tex.data)
    {
        lastError = GL_INVALID_OPERATION;
        return;
    }
    if (pixels)
    {
        for (int16_t i = 0; i < width; i++)
            for (int16_t j = 0; j < height; j++)
            {
                for (int8_t k = 0; k < size; k++)
                {
                    uint32_t src = (j * width + i) * size + k;
                    uint32_t dst = ((j + yoffset) * tex.width + xoffset + i) * size + k;
                    static_cast<uint8_t*>(tex.data)[dst] = static_cast<const uint8_t*>(pixels)[src];
                }
            }
    }
}

void glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
    const std::array<float, 16> t = {
        1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        x,   y,   z,   1.f,
    };
    glMultMatrixf(t.data());
}

void glTranslatex(GLfixed x, GLfixed y, GLfixed z)
{
    glTranslatef(x * oneDiv65536, y * oneDiv65536, z * oneDiv65536);
}

void glVertexPointer(GLint size, GLenum type, GLsizei stride, const void* pointer)
{
    if (size != 2 && size != 3 && size != 4 || stride < 0)
    {
        lastError = GL_INVALID_VALUE;
        return;
    }
    if (type != GL_FLOAT && type != GL_SHORT)
    {
        lastError = GL_INVALID_ENUM;
        return;
    }
    if (type != GL_FLOAT)
        ESP_LOGE(LIBRARY_NAME, "Just don't");
    context->glVertexPointerSize = size;
    context->glVertexPointerType = type;
    context->glVertexPointerStride = stride;
    context->glVertexPointer = pointer;
}

void glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    if (width < 0 || height < 0)
    {
        lastError = GL_INVALID_VALUE;
        return;
    }
    context->glViewportX = x;
    context->glViewportY = y;
    context->glViewportWidth = width;
    context->glViewportHeight = height;
}

//SFML Extensions
void glBlendEquationOES(GLenum mode)
{
    ESP_LOGE(LIBRARY_NAME, "glBlendEquation not implemented");
}

void glBlendEquationEXT(GLenum mode)
{
    ESP_LOGE(LIBRARY_NAME, "glBlendEquation not implemented");
}

void glBlendFuncSeparateOES(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha)
{
    context->glBlendColorSrc = srcRGB;
    context->glBlendColorDst = dstRGB;
    context->glBlendAlphaSrc = srcAlpha;
    context->glBlendAlphaDst = dstAlpha;
}

void glBlendEquationSeparateOES(GLenum modeRGB, GLenum modeAlpha)
{
    ESP_LOGE(LIBRARY_NAME, "glBlendEquationSeparate not implemented");
}

void glBindRenderbufferOES(GLenum target, GLuint renderbuffer)
{
    ESP_LOGE(LIBRARY_NAME, "glBindRenderbuffer not implemented");
}

void glDeleteRenderbuffersOES(GLsizei n, const GLuint* renderbuffers)
{
    ESP_LOGE(LIBRARY_NAME, "glDeleteRenderbuffers not implemented");
}

void glGenRenderbuffersOES(GLsizei n, GLuint* renderbuffers)
{
    ESP_LOGE(LIBRARY_NAME, "glGenRenderbuffers not implemented");
}

void glRenderbufferStorageOES(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
    ESP_LOGE(LIBRARY_NAME, "glRenderbufferStorage not implemented");
}

void glBindFramebufferOES(GLenum target, GLuint framebuffer)
{
    ESP_LOGE(LIBRARY_NAME, "glBindFramebuffer not implemented");
}

void glDeleteFramebuffersOES(GLsizei n, const GLuint* framebuffers)
{
    ESP_LOGE(LIBRARY_NAME, "glDeleteFramebuffers not implemented");
}

void glGenFramebuffersOES(GLsizei n, GLuint* framebuffers)
{
    ESP_LOGE(LIBRARY_NAME, "glGenFramebuffers not implemented");
}

GLenum glCheckFramebufferStatusOES(GLenum target)
{
    ESP_LOGE(LIBRARY_NAME, "glCheckFramebufferStatus not implemented");
    return GL_ZERO;
}

void glFramebufferTexture2DOES(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
    ESP_LOGE(LIBRARY_NAME, "glFramebufferTexture2D not implemented");
}

void glFramebufferRenderbufferOES(GLenum target, GLenum attachment, GLenum renderbuffertarget,
    GLuint renderbuffer)
{
    ESP_LOGE(LIBRARY_NAME, "glFramebufferRenderbuffer not implemented");
}

void glGenerateMipmapOES(GLenum target)
{
    ESP_LOGE(LIBRARY_NAME, "glGenerateMipmap not implemented");
}
