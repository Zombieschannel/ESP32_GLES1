#include "Context.h"

#include "Arduino.h"
#include "esp_heap_caps.h"

Context::Context(const void* config)
{
    this->config = static_cast<const Config*>(config);
    this->pixels.first = nullptr;
    this->pixels.second = nullptr;
    this->depth.first = nullptr;
    this->depth.second = nullptr;
    this->stencil.first = nullptr;
    this->stencil.second = nullptr;
}

Context::~Context()
{
    if (pixels.first)
        heap_caps_free(pixels.first);
    if (pixels.second)
        heap_caps_free(pixels.second);
    if (depth.first)
        heap_caps_free(depth.first);
    if (depth.second)
        heap_caps_free(depth.second);
    if (stencil.first)
        heap_caps_free(stencil.first);
    if (stencil.second)
        heap_caps_free(stencil.second);
    if (alpha.first)
        heap_caps_free(alpha.first);
    if (alpha.second)
        heap_caps_free(alpha.second);
}

void Context::init()
{
    if (inited)
        return;
    pixels.first = static_cast<uint16_t*>(heap_caps_malloc(width * height * sizeof(uint16_t), MALLOC_CAP_SPIRAM));
    pixels.second = static_cast<uint16_t*>(heap_caps_malloc(width * height * sizeof(uint16_t), MALLOC_CAP_SPIRAM));

    if (config->depth)
    {
        depth.first = static_cast<uint16_t*>(heap_caps_malloc(width * height * sizeof(uint16_t), MALLOC_CAP_SPIRAM));
        depth.second = static_cast<uint16_t*>(heap_caps_malloc(width * height * sizeof(uint16_t), MALLOC_CAP_SPIRAM));
    }
    if (config->stencil)
    {
        stencil.first = static_cast<uint8_t*>(heap_caps_malloc(width * height * sizeof(uint8_t), MALLOC_CAP_SPIRAM));
        stencil.second = static_cast<uint8_t*>(heap_caps_malloc(width * height * sizeof(uint8_t), MALLOC_CAP_SPIRAM));
    }
    if (config->alpha)
    {
        alpha.first = static_cast<uint8_t*>(heap_caps_malloc(width * height * sizeof(uint8_t), MALLOC_CAP_SPIRAM));
        alpha.second = static_cast<uint8_t*>(heap_caps_malloc(width * height * sizeof(uint8_t), MALLOC_CAP_SPIRAM));
    }
    glViewportHeight = height;
    glViewportWidth = width;
}
