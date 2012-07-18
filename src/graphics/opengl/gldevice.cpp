// * This file is part of the COLOBOT source code
// * Copyright (C) 2012, Polish Portal of Colobot (PPC)
// *
// * This program is free software: you can redistribute it and/or modify
// * it under the terms of the GNU General Public License as published by
// * the Free Software Foundation, either version 3 of the License, or
// * (at your option) any later version.
// *
// * This program is distributed in the hope that it will be useful,
// * but WITHOUT ANY WARRANTY; without even the implied warranty of
// * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// * GNU General Public License for more details.
// *
// * You should have received a copy of the GNU General Public License
// * along with this program. If not, see  http://www.gnu.org/licenses/.

// gldevice.cpp

#include "common/image.h"
#include "graphics/opengl/gldevice.h"
#include "math/geometry.h"

#define GL_GLEXT_PROTOTYPES

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glext.h>

#include <SDL/SDL.h>

#include <assert.h>

namespace Gfx {

struct GLDevicePrivate
{
    void (APIENTRY* glMultiTexCoord2fARB)(GLenum target, GLfloat s, GLfloat t);
    void (APIENTRY* glActiveTextureARB)(GLenum texture);

    GLDevicePrivate()
    {
        glMultiTexCoord2fARB = NULL;
        glActiveTextureARB   = NULL;
    }
};

}; // namespace Gfx


void Gfx::GLDeviceConfig::LoadDefault()
{
    Gfx::DeviceConfig::LoadDefault();

    hardwareAccel = true;

    redSize = 8;
    blueSize = 8;
    greenSize = 8;
    alphaSize = 8;
    depthSize = 24;
}




Gfx::CGLDevice::CGLDevice()
{
    m_private = new Gfx::GLDevicePrivate();
    m_wasInit = false;
    m_texturing = false;
}


Gfx::CGLDevice::~CGLDevice()
{
    delete m_private;
    m_private = NULL;
}

bool Gfx::CGLDevice::GetWasInit()
{
    return m_wasInit;
}

std::string Gfx::CGLDevice::GetError()
{
    return m_error;
}

bool Gfx::CGLDevice::Create()
{
    /* First check for extensions
       These should be available in standard OpenGL 1.3
       But every distribution is different
       So we're loading them dynamically through SDL_GL_GetProcAddress() */

    std::string extensions = std::string( (char*) glGetString(GL_EXTENSIONS));

    if (extensions.find("GL_ARB_multitexture") == std::string::npos)
    {
        m_error = "Required extension GL_ARB_multitexture not supported";
        return false;
    }

    if (extensions.find("GL_EXT_texture_env_combine") == std::string::npos)
    {
        m_error = "Required extension GL_EXT_texture_env_combine not supported";
        return false;
    }

    /*m_private->glMultiTexCoord2fARB = (PFNGLMULTITEXCOORD2FARBPROC) SDL_GL_GetProcAddress("glMultiTexCoord2fARB");
    m_private->glActiveTextureARB   = (PFNGLACTIVETEXTUREARBPROC)   SDL_GL_GetProcAddress("glActiveTextureARB");

    if ((m_private->glMultiTexCoord2fARB == NULL) || (m_private->glActiveTextureARB == NULL))
    {
        m_error = "Could not load extension functions, even though they seem supported";
        return false;
    }*/

    m_wasInit = true;

    // This is mostly done in all modern hardware by default
    // DirectX doesn't even allow the option to turn off perspective correction anymore
    // So turn it on permanently
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

    // To use separate specular color in drawing primitives
    glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);

    // Set just to be sure
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();


    m_lights        = std::vector<Gfx::Light>(GL_MAX_LIGHTS, Gfx::Light());
    m_lightsEnabled = std::vector<bool>      (GL_MAX_LIGHTS, false);

    int maxTextures = 0;
    glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &maxTextures);

    m_textures         = std::vector<Gfx::Texture*>     (maxTextures, (Gfx::Texture*)(NULL));
    m_texturesEnabled  = std::vector<bool>              (maxTextures, false);
    m_texturesParams   = std::vector<Gfx::TextureParams>(maxTextures, Gfx::TextureParams());

    return true;
}

void Gfx::CGLDevice::Destroy()
{
    /*m_private->glMultiTexCoord2fARB = NULL;
    m_private->glActiveTextureARB   = NULL;*/

    // Delete the remaining textures
    // Should not be strictly necessary, but just in case
    DestroyAllTextures();

    m_lights.clear();
    m_lightsEnabled.clear();

    m_textures.clear();
    m_texturesEnabled.clear();
    m_texturesParams.clear();

    m_wasInit = false;
}

void Gfx::CGLDevice::BeginScene()
{
    Clear();

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(m_projectionMat.Array());

    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(m_modelviewMat.Array());
}

void Gfx::CGLDevice::EndScene()
{
    glFlush();
}

void Gfx::CGLDevice::Clear()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Gfx::CGLDevice::SetTransform(Gfx::TransformType type, const Math::Matrix &matrix)
{
    if      (type == Gfx::TRANSFORM_WORLD)
    {
        m_worldMat = matrix;
        m_modelviewMat = Math::MultiplyMatrices(m_viewMat, m_worldMat);
        glMatrixMode(GL_MODELVIEW);
        glLoadMatrixf(m_modelviewMat.Array());
    }
    else if (type == Gfx::TRANSFORM_VIEW)
    {
        m_viewMat = matrix;
        m_modelviewMat = Math::MultiplyMatrices(m_viewMat, m_worldMat);
        glMatrixMode(GL_MODELVIEW);
        glLoadMatrixf(m_modelviewMat.Array());
    }
    else if (type == Gfx::TRANSFORM_PROJECTION)
    {
        m_projectionMat = matrix;
        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(m_projectionMat.Array());
    }
    else
    {
        assert(false);
    }
}

const Math::Matrix& Gfx::CGLDevice::GetTransform(Gfx::TransformType type)
{
    if      (type == Gfx::TRANSFORM_WORLD)
        return m_worldMat;
    else if (type == Gfx::TRANSFORM_VIEW)
        return m_viewMat;
    else if (type == Gfx::TRANSFORM_PROJECTION)
        return m_projectionMat;
    else
        assert(false);

    return m_worldMat; // to avoid warning
}

void Gfx::CGLDevice::MultiplyTransform(Gfx::TransformType type, const Math::Matrix &matrix)
{
    if      (type == Gfx::TRANSFORM_WORLD)
    {
        m_worldMat = Math::MultiplyMatrices(m_worldMat, matrix);
        m_modelviewMat = Math::MultiplyMatrices(m_viewMat, m_worldMat);
        glMatrixMode(GL_MODELVIEW);
        glLoadMatrixf(m_modelviewMat.Array());
    }
    else if (type == Gfx::TRANSFORM_VIEW)
    {
        m_viewMat = Math::MultiplyMatrices(m_viewMat, matrix);
        m_modelviewMat = Math::MultiplyMatrices(m_viewMat, m_worldMat);
        glMatrixMode(GL_MODELVIEW);
        glLoadMatrixf(m_modelviewMat.Array());
    }
    else if (type == Gfx::TRANSFORM_PROJECTION)
    {
        m_projectionMat = Math::MultiplyMatrices(m_projectionMat, matrix);
        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(m_projectionMat.Array());
    }
    else
    {
        assert(false);
    }
}

void Gfx::CGLDevice::SetMaterial(Gfx::Material &material)
{
    m_material = material;

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT,  m_material.ambient.Array());
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE,  m_material.diffuse.Array());
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, m_material.specular.Array());
}

const Gfx::Material& Gfx::CGLDevice::GetMaterial()
{
    return m_material;
}

int Gfx::CGLDevice::GetMaxLightCount()
{
    return m_lights.size();
}

void Gfx::CGLDevice::SetLight(int index, Gfx::Light &light)
{
    assert(index >= 0);
    assert(index < (int)m_lights.size());

    m_lights[index] = light;

    // Indexing from GL_LIGHT0 should always work
    glLightfv(GL_LIGHT0 + index, GL_AMBIENT,  light.ambient.Array());
    glLightfv(GL_LIGHT0 + index, GL_DIFFUSE,  light.diffuse.Array());
    glLightfv(GL_LIGHT0 + index, GL_SPECULAR, light.specular.Array());

    GLfloat position[4] = { light.position.x, light.position.y, light.position.z, 0.0f };
    if (light.type == LIGHT_DIRECTIONAL)
        position[3] = 0.0f;
    else
        position[3] = 1.0f;
    glLightfv(GL_LIGHT0 + index, GL_POSITION, position);

    GLfloat direction[4] = { light.direction.x, light.direction.y, light.direction.z, 0.0f };
    glLightfv(GL_LIGHT0 + index, GL_SPOT_DIRECTION, direction);

    glLightf(GL_LIGHT0 + index, GL_SPOT_CUTOFF, light.range);

    // TODO: falloff?, phi?, theta?

    glLightf(GL_LIGHT0 + index, GL_CONSTANT_ATTENUATION,  light.attenuation0);
    glLightf(GL_LIGHT0 + index, GL_LINEAR_ATTENUATION,    light.attenuation1);
    glLightf(GL_LIGHT0 + index, GL_QUADRATIC_ATTENUATION, light.attenuation2);
}

const Gfx::Light& Gfx::CGLDevice::GetLight(int index)
{
    assert(index >= 0);
    assert(index < (int)m_lights.size());

    return m_lights[index];
}

void Gfx::CGLDevice::SetLightEnabled(int index, bool enabled)
{
    assert(index >= 0);
    assert(index < (int)m_lights.size());

    m_lightsEnabled[index] = enabled;

    glEnable(GL_LIGHT0 + index);
}

bool Gfx::CGLDevice::GetLightEnabled(int index)
{
    assert(index >= 0);
    assert(index < (int)m_lights.size());

    return m_lightsEnabled[index];
}

/** If image is invalid, returns NULL.
    Otherwise, returns pointer to new Gfx::Texture struct.
    This struct must not be deleted in other way than through DeleteTexture() */
Gfx::Texture* Gfx::CGLDevice::CreateTexture(CImage *image, const Gfx::TextureCreateParams &params)
{
    ImageData *data = image->GetData();
    if (data == NULL)
        return NULL;

    Gfx::Texture *result = new Gfx::Texture();
    result->valid = true;

    // Use & enable 1st texture stage
    glActiveTextureARB(GL_TEXTURE0_ARB);
    glEnable(GL_TEXTURE_2D);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glGenTextures(1, &result->id);
    glBindTexture(GL_TEXTURE_2D, result->id);

    // Set params

    GLint minF = 0;
    if      (params.minFilter == Gfx::TEX_MIN_FILTER_NEAREST)                minF = GL_NEAREST;
    else if (params.minFilter == Gfx::TEX_MIN_FILTER_LINEAR)                 minF = GL_LINEAR;
    else if (params.minFilter == Gfx::TEX_MIN_FILTER_NEAREST_MIPMAP_NEAREST) minF = GL_NEAREST_MIPMAP_NEAREST;
    else if (params.minFilter == Gfx::TEX_MIN_FILTER_LINEAR_MIPMAP_NEAREST)  minF = GL_LINEAR_MIPMAP_NEAREST;
    else if (params.minFilter == Gfx::TEX_MIN_FILTER_NEAREST_MIPMAP_LINEAR)  minF = GL_NEAREST_MIPMAP_LINEAR;
    else if (params.minFilter == Gfx::TEX_MIN_FILTER_LINEAR_MIPMAP_LINEAR)   minF = GL_LINEAR_MIPMAP_LINEAR;
    else  assert(false);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minF);

    GLint magF = 0;
    if      (params.magFilter == Gfx::TEX_MAG_FILTER_NEAREST) magF = GL_NEAREST;
    else if (params.magFilter == Gfx::TEX_MAG_FILTER_LINEAR)  magF = GL_LINEAR;
    else  assert(false);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magF);

    if      (params.wrapS == Gfx::TEX_WRAP_CLAMP)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    else if (params.wrapS == Gfx::TEX_WRAP_REPEAT)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    else  assert(false);

    if      (params.wrapT == Gfx::TEX_WRAP_CLAMP)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    else if (params.wrapT == Gfx::TEX_WRAP_REPEAT)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    else  assert(false);


    if (params.mipmap)
        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
    else
        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);

    GLenum sourceFormat = 0;
    if (params.format == Gfx::TEX_IMG_RGB)
        sourceFormat = GL_RGB;
    else if (params.format == Gfx::TEX_IMG_BGR)
        sourceFormat = GL_BGR;
    else if (params.format == Gfx::TEX_IMG_RGBA)
        sourceFormat = GL_RGBA;
    else if (params.format == Gfx::TEX_IMG_BGRA)
        sourceFormat = GL_BGRA;
    else
        assert(false);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, data->surface->w, data->surface->h,
                 0, sourceFormat, GL_UNSIGNED_BYTE, data->surface->pixels);


    // Restore the previous state of 1st stage
    if (m_textures[0] == NULL)
        glBindTexture(GL_TEXTURE_2D, 0);
    else
        glBindTexture(GL_TEXTURE_2D, m_textures[0]->id);

    if ( (! m_texturing) || (! m_texturesEnabled[0]) )
        glDisable(GL_TEXTURE_2D);

    return result;
}

void Gfx::CGLDevice::DestroyTexture(Gfx::Texture *texture)
{
    std::set<Gfx::Texture*>::iterator it = m_allTextures.find(texture);
    if (it != m_allTextures.end())
        m_allTextures.erase(it);

    // Unbind the texture if in use anywhere
    for (int index = 0; index < (int)m_textures.size(); ++index)
    {
        if (m_textures[index] == texture)
            SetTexture(index, NULL);
    }

    glDeleteTextures(1, &texture->id);
}

void Gfx::CGLDevice::DestroyAllTextures()
{
    std::set<Gfx::Texture*> allCopy = m_allTextures;
    std::set<Gfx::Texture*>::iterator it;
    for (it = allCopy.begin(); it != allCopy.end(); ++it)
    {
        DestroyTexture(*it);
        delete *it;
    }
}

int Gfx::CGLDevice::GetMaxTextureCount()
{
    return m_textures.size();
}

/**
  If \a texture is \c NULL or invalid, unbinds the given texture.
  If valid, binds the texture and enables the given texture stage.
  The setting is remembered, even if texturing is disabled at the moment. */
void Gfx::CGLDevice::SetTexture(int index, Gfx::Texture *texture)
{
    assert(index >= 0);
    assert(index < (int)m_textures.size());

    // Enable the given texture stage
    glActiveTextureARB(GL_TEXTURE0_ARB + index);
    glEnable(GL_TEXTURE_2D);

    if ((texture == NULL) || (! texture->valid))
    {
        glBindTexture(GL_TEXTURE_2D, 0); // unbind texture
        m_textures[index] = NULL;        // remember the changes
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, texture->id);        // bind the texture
        m_textures[index] = texture;                      // remember the changes
        SetTextureParams(index, m_texturesParams[index]); // texture params need to be re-set for the new texture
    }

    // Disable the stage if it is set so
    if ( (! m_texturing) || (! m_texturesEnabled[index]) )
        glDisable(GL_TEXTURE_2D);
}

/**
  Returns the previously assigned texture or \c NULL if the given stage is not enabled. */
Gfx::Texture* Gfx::CGLDevice::GetTexture(int index)
{
    assert(index >= 0);
    assert(index < (int)m_textures.size());

    return m_textures[index];
}

void Gfx::CGLDevice::SetTextureEnabled(int index, bool enabled)
{
    assert(index >= 0);
    assert(index < (int)m_textures.size());

    m_texturesEnabled[index] = enabled;

    glActiveTextureARB(GL_TEXTURE0_ARB + index);
    if (enabled)
        glEnable(GL_TEXTURE_2D);
    else
        glDisable(GL_TEXTURE_2D);
}

bool Gfx::CGLDevice::GetTextureEnabled(int index)
{
    assert(index >= 0);
    assert(index < (int)m_textures.size());

    return m_texturesEnabled[index];
}

/**
  Sets the texture parameters for the given texture stage.
  If the given texture was not set (bound) yet, nothing happens.
  The settings are remembered, even if texturing is disabled at the moment. */
void Gfx::CGLDevice::SetTextureParams(int index, const Gfx::TextureParams &params)
{
    assert(index >= 0);
    assert(index < (int)m_textures.size());

    // Remember the settings
    m_texturesParams[index] = params;

    // Don't actually do anything if texture not set
    if (m_textures[index] == NULL)
        return;

    // Enable the given stage
    glActiveTextureARB(GL_TEXTURE0_ARB + index);
    glEnable(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, m_textures[index]->id);

    // Selection of operation and arguments
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);

    // Color operation
    if      (params.colorOperation == Gfx::TEX_MIX_OPER_MODULATE)
        glTexEnvi(GL_TEXTURE_2D, GL_COMBINE_RGB_ARB, GL_MODULATE);
    else if (params.colorOperation == Gfx::TEX_MIX_OPER_ADD)
        glTexEnvi(GL_TEXTURE_2D, GL_COMBINE_RGB_ARB, GL_ADD);
    else  assert(false);

    // Color arg1
    if (params.colorArg1 == Gfx::TEX_MIX_ARG_CURRENT)
    {
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS); // that's right - stupid D3D enum values
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
    }
    else if (params.colorArg1 == Gfx::TEX_MIX_ARG_TEXTURE)
    {
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
    }
    else if (params.colorArg1 == Gfx::TEX_MIX_ARG_DIFFUSE)
    {
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PRIMARY_COLOR); // here as well
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
    }
    else  assert(false);

    // Color arg2
    if (params.colorArg2 == Gfx::TEX_MIX_ARG_CURRENT)
    {
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR);
    }
    else if (params.colorArg2 == Gfx::TEX_MIX_ARG_TEXTURE)
    {
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR);
    }
    else if (params.colorArg2 == Gfx::TEX_MIX_ARG_DIFFUSE)
    {
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PRIMARY_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR);
    }
    else  assert(false);

    // Alpha operation
    if      (params.alphaOperation == Gfx::TEX_MIX_OPER_MODULATE)
        glTexEnvi(GL_TEXTURE_2D, GL_COMBINE_ALPHA_ARB, GL_MODULATE);
    else if (params.alphaOperation == Gfx::TEX_MIX_OPER_ADD)
        glTexEnvi(GL_TEXTURE_2D, GL_COMBINE_ALPHA_ARB, GL_ADD);
    else  assert(false);

    // Alpha arg1
    if (params.alphaArg1 == Gfx::TEX_MIX_ARG_CURRENT)
    {
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_PREVIOUS);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB, GL_SRC_ALPHA);
    }
    else if (params.alphaArg1 == Gfx::TEX_MIX_ARG_TEXTURE)
    {
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_TEXTURE);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB, GL_SRC_ALPHA);
    }
    else if (params.alphaArg1 == Gfx::TEX_MIX_ARG_DIFFUSE)
    {
        glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_PRIMARY_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
    }
    else  assert(false);

    // Alpha arg2
    if (params.alphaArg2 == Gfx::TEX_MIX_ARG_CURRENT)
    {
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_PREVIOUS);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_SRC_ALPHA);
    }
    else if (params.alphaArg2 == Gfx::TEX_MIX_ARG_TEXTURE)
    {
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_TEXTURE);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_SRC_ALPHA);
    }
    else if (params.alphaArg2 == Gfx::TEX_MIX_ARG_DIFFUSE)
    {
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_PRIMARY_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_SRC_ALPHA);
    }
    else  assert(false);

    // Disable the stage if it is set so
    if ( (! m_texturing) || (! m_texturesEnabled[index]) )
        glDisable(GL_TEXTURE_2D);
}

Gfx::TextureParams Gfx::CGLDevice::GetTextureParams(int index)
{
    assert(index >= 0);
    assert(index < (int)m_textures.size());

    return m_texturesParams[index];
}

void Gfx::CGLDevice::SetTextureFactor(Gfx::Color &color)
{
    // Needs to be set for all texture stages
    for (int index = 0; index < (int)m_textures.size(); ++index)
    {
        // Activate stage
        glActiveTextureARB(GL_TEXTURE0_ARB + index);
        glEnable(GL_TEXTURE_2D);

        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color.Array());

        // Disable the stage if it is set so
        if ( (! m_texturing) || (! m_texturesEnabled[index]) )
            glDisable(GL_TEXTURE_2D);
    }
}

Gfx::Color Gfx::CGLDevice::GetTextureFactor()
{
    // Get from 1st stage (should be the same for all stages)
    glActiveTextureARB(GL_TEXTURE0_ARB);
    glEnable(GL_TEXTURE_2D);

    GLfloat color[4] = { 0.0f };
    glGetTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);

    // Disable the 1st stage if it is set so
    if ( (! m_texturing) || (! m_texturesEnabled[0]) )
        glDisable(GL_TEXTURE_2D);

    return Gfx::Color(color[0], color[1], color[2], color[3]);
}

void Gfx::CGLDevice::DrawPrimitive(Gfx::PrimitiveType type, Vertex *vertices, int vertexCount)
{
    if (type == Gfx::PRIMITIVE_LINES)
        glBegin(GL_LINES);
    else if (type == Gfx::PRIMITIVE_TRIANGLES)
        glBegin(GL_TRIANGLES);
    else if (type == Gfx::PRIMITIVE_TRIANGLE_STRIP)
        glBegin(GL_TRIANGLE_STRIP);

    glColor3f(1.0f, 1.0f, 1.0f);

    for (int i = 0; i < vertexCount; ++i)
    {
        glNormal3fv((GLfloat*)vertices[i].normal.Array());
        glMultiTexCoord2fvARB(GL_TEXTURE0_ARB, (GLfloat*)vertices[i].texCoord.Array());
        glVertex3fv((GLfloat*)vertices[i].coord.Array());
    }

    glEnd();
}

void Gfx::CGLDevice::DrawPrimitive(Gfx::PrimitiveType type, Gfx::VertexCol *vertices, int vertexCount)
{
    if (type == Gfx::PRIMITIVE_LINES)
        glBegin(GL_LINES);
    else if (type == Gfx::PRIMITIVE_TRIANGLES)
        glBegin(GL_TRIANGLES);
    else if (type == Gfx::PRIMITIVE_TRIANGLE_STRIP)
        glBegin(GL_TRIANGLE_STRIP);

    for (int i = 0; i < vertexCount; ++i)
    {
        glColor4fv((GLfloat*)vertices[i].color.Array());
        glSecondaryColor3fv((GLfloat*)vertices[i].specular.Array());
        glMultiTexCoord2fvARB(GL_TEXTURE0_ARB, (GLfloat*)vertices[i].texCoord.Array());
        glVertex3fv((GLfloat*)vertices[i].coord.Array());
    }

    glEnd();
}

void Gfx::CGLDevice::DrawPrimitive(Gfx::PrimitiveType type, VertexTex2 *vertices, int vertexCount)
{
    if (type == Gfx::PRIMITIVE_LINES)
        glBegin(GL_LINES);
    else if (type == Gfx::PRIMITIVE_TRIANGLES)
        glBegin(GL_TRIANGLES);
    else if (type == Gfx::PRIMITIVE_TRIANGLE_STRIP)
        glBegin(GL_TRIANGLE_STRIP);

    for (int i = 0; i < vertexCount; ++i)
    {
        glNormal3fv((GLfloat*) vertices[i].normal.Array());
        glMultiTexCoord2fARB(GL_TEXTURE0_ARB, vertices[i].texCoord.x, vertices[i].texCoord.y);
        glMultiTexCoord2fARB(GL_TEXTURE1_ARB, vertices[i].texCoord2.x, vertices[i].texCoord2.y);
        glVertex3fv((GLfloat*) vertices[i].coord.Array());
    }

    glEnd();
}

bool InPlane(Math::Vector normal, float originPlane, Math::Vector center, float radius)
{
    float distance = (originPlane + Math::DotProduct(normal, center)) / normal.Length();

    if (distance < -radius)
        return true;

    return false;
}

/*
   The implementation of ComputeSphereVisibility is taken from libwine's device.c
   Copyright of the WINE team, licensed under GNU LGPL v 2.1
 */

// TODO: testing
int Gfx::CGLDevice::ComputeSphereVisibility(Math::Vector center, float radius)
{
    Math::Matrix m;
    m.LoadIdentity();
    m = Math::MultiplyMatrices(m, m_worldMat);
    m = Math::MultiplyMatrices(m, m_viewMat);
    m = Math::MultiplyMatrices(m, m_projectionMat);

    Math::Vector vec[6];
    float originPlane[6];

    // Left plane
    vec[0].x = m.Get(4, 1) + m.Get(1, 1);
    vec[0].y = m.Get(4, 2) + m.Get(1, 2);
    vec[0].z = m.Get(4, 3) + m.Get(1, 3);
    originPlane[0] = m.Get(4, 4) + m.Get(1, 4);

    // Right plane
    vec[1].x = m.Get(4, 1) - m.Get(1, 1);
    vec[1].y = m.Get(4, 2) - m.Get(1, 2);
    vec[1].z = m.Get(4, 3) - m.Get(1, 3);
    originPlane[1] = m.Get(4, 4) - m.Get(1, 4);

    // Top plane
    vec[2].x = m.Get(4, 1) - m.Get(2, 1);
    vec[2].y = m.Get(4, 2) - m.Get(2, 2);
    vec[2].z = m.Get(4, 3) - m.Get(2, 3);
    originPlane[2] = m.Get(4, 4) - m.Get(2, 4);

    // Bottom plane
    vec[3].x = m.Get(4, 1) + m.Get(2, 1);
    vec[3].y = m.Get(4, 2) + m.Get(2, 2);
    vec[3].z = m.Get(4, 3) + m.Get(2, 3);
    originPlane[3] = m.Get(4, 4) + m.Get(2, 4);

    // Front plane
    vec[4].x = m.Get(3, 1);
    vec[4].y = m.Get(3, 2);
    vec[4].z = m.Get(3, 3);
    originPlane[4] = m.Get(3, 4);

    // Back plane
    vec[5].x = m.Get(4, 1) - m.Get(3, 1);
    vec[5].y = m.Get(4, 2) - m.Get(3, 2);
    vec[5].z = m.Get(4, 3) - m.Get(3, 3);
    originPlane[5] = m.Get(4, 4) - m.Get(3, 4);

    int result = 0;

    if (InPlane(vec[0], originPlane[0], center, radius))
        result |= Gfx::INTERSECT_PLANE_LEFT;
    if (InPlane(vec[1], originPlane[1], center, radius))
        result |= Gfx::INTERSECT_PLANE_RIGHT;
    if (InPlane(vec[2], originPlane[2], center, radius))
        result |= Gfx::INTERSECT_PLANE_TOP;
    if (InPlane(vec[3], originPlane[3], center, radius))
        result |= Gfx::INTERSECT_PLANE_BOTTOM;
    if (InPlane(vec[4], originPlane[4], center, radius))
        result |= Gfx::INTERSECT_PLANE_FRONT;
    if (InPlane(vec[5], originPlane[5], center, radius))
        result |= Gfx::INTERSECT_PLANE_BACK;

    return result;
}

void Gfx::CGLDevice::SetRenderState(Gfx::RenderState state, bool enabled)
{
    if (state == RENDER_STATE_DEPTH_WRITE)
    {
        glDepthMask(enabled ? GL_TRUE : GL_FALSE);
        return;
    }
    else if (state == RENDER_STATE_TEXTURING)
    {
        m_texturing = enabled;

        // Enable/disable stages with new setting
        for (int index = 0; index < (int)m_textures.size(); ++index)
        {
            glActiveTextureARB(GL_TEXTURE0_ARB + index);
            if (m_texturing && m_texturesEnabled[index])
                glEnable(GL_TEXTURE_2D);
            else
                glDisable(GL_TEXTURE_2D);
        }

        return;
    }

    GLenum flag = 0;

    switch (state)
    {
        case Gfx::RENDER_STATE_LIGHTING:    flag = GL_LIGHTING; break;
        case Gfx::RENDER_STATE_BLENDING:    flag = GL_BLEND; break;
        case Gfx::RENDER_STATE_FOG:         flag = GL_FOG; break;
        case Gfx::RENDER_STATE_DEPTH_TEST:  flag = GL_DEPTH_TEST; break;
        case Gfx::RENDER_STATE_ALPHA_TEST:  flag = GL_ALPHA_TEST; break;
        case Gfx::RENDER_STATE_DITHERING:   flag = GL_DITHER; break;
        default: assert(false); break;
    }

    if (enabled)
        glEnable(flag);
    else
        glDisable(flag);
}

bool Gfx::CGLDevice::GetRenderState(Gfx::RenderState state)
{
    if (state == RENDER_STATE_TEXTURING)
        return m_texturing;

    GLenum flag = 0;

    switch (state)
    {
        case Gfx::RENDER_STATE_DEPTH_WRITE: flag = GL_DEPTH_WRITEMASK; break;
        case Gfx::RENDER_STATE_LIGHTING:    flag = GL_DEPTH_WRITEMASK; break;
        case Gfx::RENDER_STATE_BLENDING:    flag = GL_BLEND; break;
        case Gfx::RENDER_STATE_FOG:         flag = GL_FOG; break;
        case Gfx::RENDER_STATE_DEPTH_TEST:  flag = GL_DEPTH_TEST; break;
        case Gfx::RENDER_STATE_ALPHA_TEST:  flag = GL_ALPHA_TEST; break;
        case Gfx::RENDER_STATE_DITHERING:   flag = GL_DITHER; break;
        default: assert(false); break;
    }

    GLboolean result = GL_FALSE;
    glGetBooleanv(flag, &result);

    return result == GL_TRUE;
}

Gfx::CompFunc TranslateGLCompFunc(GLenum flag)
{
    switch (flag)
    {
        case GL_NEVER:    return Gfx::COMP_FUNC_NEVER;
        case GL_LESS:     return Gfx::COMP_FUNC_LESS;
        case GL_EQUAL:    return Gfx::COMP_FUNC_EQUAL;
        case GL_NOTEQUAL: return Gfx::COMP_FUNC_NOTEQUAL;
        case GL_LEQUAL:   return Gfx::COMP_FUNC_LEQUAL;
        case GL_GREATER:  return Gfx::COMP_FUNC_GREATER;
        case GL_GEQUAL:   return Gfx::COMP_FUNC_GEQUAL;
        case GL_ALWAYS:   return Gfx::COMP_FUNC_ALWAYS;
        default: assert(false); break;
    }
    return Gfx::COMP_FUNC_NEVER;
}

GLenum TranslateGfxCompFunc(Gfx::CompFunc func)
{
    switch (func)
    {
        case Gfx::COMP_FUNC_NEVER:    return GL_NEVER;
        case Gfx::COMP_FUNC_LESS:     return GL_LESS;
        case Gfx::COMP_FUNC_EQUAL:    return GL_EQUAL;
        case Gfx::COMP_FUNC_NOTEQUAL: return GL_NOTEQUAL;
        case Gfx::COMP_FUNC_LEQUAL:   return GL_LEQUAL;
        case Gfx::COMP_FUNC_GREATER:  return GL_GREATER;
        case Gfx::COMP_FUNC_GEQUAL:   return GL_GEQUAL;
        case Gfx::COMP_FUNC_ALWAYS:   return GL_ALWAYS;
        default: assert(false); break;
    }
    return 0;
}

void Gfx::CGLDevice::SetDepthTestFunc(Gfx::CompFunc func)
{
    glDepthFunc(TranslateGfxCompFunc(func));
}

Gfx::CompFunc Gfx::CGLDevice::GetDepthTestFunc()
{
    GLenum flag = 0;
    glGetIntegerv(GL_DEPTH_FUNC, (GLint*)&flag);
    return TranslateGLCompFunc(flag);
}

void Gfx::CGLDevice::SetDepthBias(float factor)
{
    glPolygonOffset(factor, 0.0f);
}

float Gfx::CGLDevice::GetDepthBias()
{
    GLfloat result = 0.0f;
    glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &result);
    return result;
}

void Gfx::CGLDevice::SetAlphaTestFunc(Gfx::CompFunc func, float refValue)
{
    glAlphaFunc(TranslateGfxCompFunc(func), refValue);
}

void Gfx::CGLDevice::GetAlphaTestFunc(Gfx::CompFunc &func, float &refValue)
{
    GLenum flag = 0;
    glGetIntegerv(GL_ALPHA_TEST_FUNC, (GLint*)&flag);
    func = TranslateGLCompFunc(flag);

    glGetFloatv(GL_ALPHA_TEST_REF, (GLfloat*) &refValue);
}

Gfx::BlendFunc TranslateGLBlendFunc(GLenum flag)
{
    switch (flag)
    {
        case GL_ZERO:                return Gfx::BLEND_ZERO;
        case GL_ONE:                 return Gfx::BLEND_ONE;
        case GL_SRC_COLOR:           return Gfx::BLEND_SRC_COLOR;
        case GL_ONE_MINUS_SRC_COLOR: return Gfx::BLEND_INV_SRC_COLOR;
        case GL_DST_COLOR:           return Gfx::BLEND_DST_COLOR;
        case GL_ONE_MINUS_DST_COLOR: return Gfx::BLEND_INV_DST_COLOR;
        case GL_SRC_ALPHA:           return Gfx::BLEND_SRC_ALPHA;
        case GL_ONE_MINUS_SRC_ALPHA: return Gfx::BLEND_INV_SRC_ALPHA;
        case GL_DST_ALPHA:           return Gfx::BLEND_DST_ALPHA;
        case GL_ONE_MINUS_DST_ALPHA: return Gfx::BLEND_INV_DST_ALPHA;
        case GL_SRC_ALPHA_SATURATE:  return Gfx::BLEND_SRC_ALPHA_SATURATE;
        default: assert(false); break;
    }

    return Gfx::BLEND_ZERO;
}

GLenum TranslateGfxBlendFunc(Gfx::BlendFunc func)
{
    switch (func)
    {
        case Gfx::BLEND_ZERO:               return GL_ZERO;
        case Gfx::BLEND_ONE:                return GL_ONE;
        case Gfx::BLEND_SRC_COLOR:          return GL_SRC_COLOR;
        case Gfx::BLEND_INV_SRC_COLOR:      return GL_ONE_MINUS_SRC_COLOR;
        case Gfx::BLEND_DST_COLOR:          return GL_DST_COLOR;
        case Gfx::BLEND_INV_DST_COLOR:      return GL_ONE_MINUS_DST_COLOR;
        case Gfx::BLEND_SRC_ALPHA:          return GL_SRC_ALPHA;
        case Gfx::BLEND_INV_SRC_ALPHA:      return GL_ONE_MINUS_SRC_ALPHA;
        case Gfx::BLEND_DST_ALPHA:          return GL_DST_ALPHA;
        case Gfx::BLEND_INV_DST_ALPHA:      return GL_ONE_MINUS_DST_ALPHA;
        case Gfx::BLEND_SRC_ALPHA_SATURATE: return GL_SRC_ALPHA_SATURATE;
        default: assert(false); break;
    }
    return 0;
}

void Gfx::CGLDevice::SetBlendFunc(Gfx::BlendFunc srcBlend, Gfx::BlendFunc dstBlend)
{
    glBlendFunc(TranslateGfxBlendFunc(srcBlend), TranslateGfxBlendFunc(dstBlend));
}

void Gfx::CGLDevice::GetBlendFunc(Gfx::BlendFunc &srcBlend, Gfx::BlendFunc &dstBlend)
{
    GLenum srcFlag = 0;
    glGetIntegerv(GL_ALPHA_TEST_FUNC, (GLint*)&srcFlag);
    srcBlend = TranslateGLBlendFunc(srcFlag);

    GLenum dstFlag = 0;
    glGetIntegerv(GL_ALPHA_TEST_FUNC, (GLint*)&dstFlag);
    dstBlend = TranslateGLBlendFunc(dstFlag);
}

void Gfx::CGLDevice::SetClearColor(Gfx::Color color)
{
    glClearColor(color.r, color.g, color.b, color.a);
}

Gfx::Color Gfx::CGLDevice::GetClearColor()
{
    GLfloat color[4] = { 0.0f };
    glGetFloatv(GL_COLOR_CLEAR_VALUE, color);
    return Gfx::Color(color[0], color[1], color[2], color[3]);
}

void Gfx::CGLDevice::SetGlobalAmbient(Gfx::Color color)
{
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, color.Array());
}

Gfx::Color Gfx::CGLDevice::GetGlobalAmbient()
{
    GLfloat color[4] = { 0.0f };
    glGetFloatv(GL_LIGHT_MODEL_AMBIENT, color);
    return Gfx::Color(color[0], color[1], color[2], color[3]);
}

void Gfx::CGLDevice::SetFogParams(Gfx::FogMode mode, Gfx::Color color, float start, float end, float density)
{
    if      (mode == Gfx::FOG_LINEAR) glFogi(GL_FOG_MODE, GL_LINEAR);
    else if (mode == Gfx::FOG_EXP)    glFogi(GL_FOG_MODE, GL_EXP);
    else if (mode == Gfx::FOG_EXP2)   glFogi(GL_FOG_MODE, GL_EXP2);
    else assert(false);

    glFogf(GL_FOG_START,   start);
    glFogf(GL_FOG_END,     end);
    glFogf(GL_FOG_DENSITY, density);
}

void Gfx::CGLDevice::GetFogParams(Gfx::FogMode &mode, Gfx::Color &color, float &start, float &end, float &density)
{
    GLenum flag = 0;
    glGetIntegerv(GL_FOG_MODE, (GLint*)&flag);
    if      (flag == GL_LINEAR) mode = Gfx::FOG_LINEAR;
    else if (flag == GL_EXP)    mode = Gfx::FOG_EXP;
    else if (flag == GL_EXP2)   mode = Gfx::FOG_EXP2;
    else assert(false);

    glGetFloatv(GL_FOG_START,   (GLfloat*)&start);
    glGetFloatv(GL_FOG_END,     (GLfloat*)&end);
    glGetFloatv(GL_FOG_DENSITY, (GLfloat*)&density);
}

void Gfx::CGLDevice::SetCullMode(Gfx::CullMode mode)
{
    if      (mode == Gfx::CULL_CW)  glCullFace(GL_CW);
    else if (mode == Gfx::CULL_CCW) glCullFace(GL_CCW);
    else assert(false);
}

Gfx::CullMode Gfx::CGLDevice::GetCullMode()
{
    GLenum flag = 0;
    glGetIntegerv(GL_CULL_FACE, (GLint*)&flag);
    if      (flag == GL_CW)  return Gfx::CULL_CW;
    else if (flag == GL_CCW) return Gfx::CULL_CCW;
    else assert(false);
    return Gfx::CULL_CW;
}

void Gfx::CGLDevice::SetShadeModel(Gfx::ShadeModel model)
{
    if      (model == Gfx::SHADE_FLAT)   glShadeModel(GL_FLAT);
    else if (model == Gfx::SHADE_SMOOTH) glShadeModel(GL_SMOOTH);
    else  assert(false);
}

Gfx::ShadeModel Gfx::CGLDevice::GetShadeModel()
{
    GLenum flag = 0;
    glGetIntegerv(GL_SHADE_MODEL, (GLint*)&flag);
    if      (flag == GL_FLAT)    return Gfx::SHADE_FLAT;
    else if (flag == GL_SMOOTH)  return Gfx::SHADE_SMOOTH;
    else  assert(false);
    return Gfx::SHADE_FLAT;
}

void Gfx::CGLDevice::SetFillMode(Gfx::FillMode mode)
{
    if      (mode == Gfx::FILL_POINT) glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
    else if (mode == Gfx::FILL_LINES) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else if (mode == Gfx::FILL_FILL)  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    else assert(false);
}

Gfx::FillMode Gfx::CGLDevice::GetFillMode()
{
    GLenum flag = 0;
    glGetIntegerv(GL_POLYGON_MODE, (GLint*)&flag);
    if      (flag == GL_POINT) return Gfx::FILL_POINT;
    else if (flag == GL_LINE)  return Gfx::FILL_LINES;
    else if (flag == GL_FILL)  return Gfx::FILL_FILL;
    else  assert(false);
    return Gfx::FILL_POINT;
}