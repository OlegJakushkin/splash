/*
 * Copyright (C) 2014 Emmanuel Durand
 *
 * This file is part of Splash.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * blobserver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with blobserver.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * @shaderSources.h
 * Contains the sources for all shaders used in Splash
 */

#ifndef SPLASH_SHADERSOURCES_H
#define SPLASH_SHADERSOURCES_H

#include <string>
#include <map>

namespace Splash
{

struct ShaderSources
{
    const std::map<std::string, std::string> INCLUDES {
        //
        // Project a point wrt a mvp matrix, and check if it is in the view frustum
        {"projectAndCheckVisibility", R"(
            bool projectAndCheckVisibility(inout vec4 p, in mat4 mvp, in float margin, out vec2 dist)
            {
                vec4 projected = mvp * vec4(p.xyz, 1.0);
                projected /= projected.w;
                p = projected;

                if (projected.z >= 0.0)
                {
                    projected = abs(projected);
                    dist = projected.xy;
                    bvec4 isVisible = lessThanEqual(projected, vec4(1.0 + margin));
                    if (isVisible.x && isVisible.y && isVisible.z)
                        return true;
                }

                return false;
            }
        )"},
        //
        // Compute a normal vector from three vectors
        {"normalVector", R"(
            vec3 normalVector(vec3 u, vec3 v, vec3 w)
            {
                vec3 n = normalize(cross(v - u, w - u));
                return n;
            }
        )"}
    };

    /**
     * Version directive, included at the start of all shaders
     */
    const std::string VERSION_DIRECTIVE_330 {R"(
        #version 330 core
    )"};

    const std::string VERSION_DIRECTIVE_430 {R"(
        #version 430 core
    )"};

    /**************************/
    // COMPUTE
    /**************************/

    /**
     * Default compute shader
     */
    const std::string COMPUTE_SHADER_DEFAULT {R"(
        #extension GL_ARB_compute_shader : enable
        #extension GL_ARB_shader_storage_buffer_object : enable

        layout(local_size_x = 32, local_size_y = 32) in;

        layout (std430, binding = 0) buffer vertexBuffer
        {
            vec4 vertex[];
        };

        layout (std430, binding = 1) buffer texcoordsBuffer
        {
            vec2 texcoords[];
        };

        layout (std430, binding = 2) buffer normalBuffer
        {
            vec4 normal[];
        };

        layout (std430, binding = 3) buffer annexeBuffer
        {
            vec4 annexe[];
        };

        uniform int _vertexNbr;

        void main(void)
        {
            uvec3 pos = gl_GlobalInvocationID;
            int globalID = int(gl_WorkGroupID.x * 32 * 32 + gl_LocalInvocationIndex);

            if (globalID < _vertexNbr)
            {
                vertex[globalID].x += 0.001;
                vertex[globalID].y += 0.001;
                vertex[globalID].z += 0.001;
            }
        }
    )"};

    /**
     * Compute shader to reset all camera contribution to zero
     */
    const std::string COMPUTE_SHADER_RESET_VISIBILITY {R"(
        #extension GL_ARB_compute_shader : enable
        #extension GL_ARB_shader_storage_buffer_object : enable

        layout(local_size_x = 32, local_size_y = 32) in;

        layout (std430, binding = 3) buffer annexeBuffer
        {
            vec4 annexe[];
        };

        uniform int _vertexNbr;

        void main(void)
        {
            uvec3 pos = gl_GlobalInvocationID;
            int globalID = int(gl_WorkGroupID.x * 32 * 32 + gl_LocalInvocationIndex);

            if (globalID < _vertexNbr / 3)
            {
                for (int idx = 0; idx < 3; ++idx)
                {
                    int vertexId = globalID * 3 + idx;
                    annexe[vertexId] = vec4(0.0);
                }
            }
        }
    )"};

    /**
     * Compute shader to compute the contribution of a specific camera
     */
    const std::string COMPUTE_SHADER_COMPUTE_VISIBILITY {R"(
        #extension GL_ARB_compute_shader : enable
        #extension GL_ARB_shader_storage_buffer_object : enable

        #include normalVector
        #include projectAndCheckVisibility

        layout(local_size_x = 32, local_size_y = 32) in;

        layout (std430, binding = 0) buffer vertexBuffer
        {
            vec4 vertex[];
        };

        layout (std430, binding = 2) buffer normalBuffer
        {
            vec4 normal[];
        };

        layout (std430, binding = 3) buffer annexeBuffer
        {
            vec4 annexe[];
        };

        uniform int _vertexNbr;
        uniform mat4 _mvp;
        uniform mat4 _mNormal;

        void main(void)
        {
            uvec3 pos = gl_GlobalInvocationID;
            int globalID = int(gl_WorkGroupID.x * 32 * 32 + gl_LocalInvocationIndex);
            vec4 screenVertex[3];
            bool vertexVisible[3];

            if (globalID < _vertexNbr / 3)
            {
                bool allVerticesVisible = true;
                for (int idx = 0; idx < 3; ++idx)
                {
                    int vertexId = globalID * 3 + idx;

                    vec2 dist;
                    vec4 normalizedSpaceVertex = vertex[vertexId];
                    bool isVisible = projectAndCheckVisibility(normalizedSpaceVertex, _mvp, 0.005, dist);
                    screenVertex[idx] = normalizedSpaceVertex;
                    if (isVisible)
                    {
                        vertexVisible[idx] = true;
                    }
                    else
                    {
                        vertexVisible[idx] = false;
                        allVerticesVisible = false;
                    }
                }

                vec3 projectedNormal = normalVector(screenVertex[0].xyz, screenVertex[1].xyz, screenVertex[2].xyz);
                if (allVerticesVisible && projectedNormal.z <= 0.0)
                {
                    for (int idx = 0; idx < 3; ++idx)
                    {
                        int vertexId = globalID * 3 + idx;
                        annexe[vertexId].x += 1.0;

                        if (vertexVisible[idx] == false)
                        {
                            annexe[vertexId].y += 1.0;
                        }

                        { // Smooth blending
                        //vec2 normalizedPos = vec2(screenVertex[idx].x / 2.0 + 0.5, screenVertex[idx].y / 2.0 + 0.5);
                        //vec2 distDoubleInvert = vec2(min(normalizedPos.x, 1.0 - normalizedPos.x), min(normalizedPos.y, 1.0 - normalizedPos.y));
                        //distDoubleInvert = clamp(distDoubleInvert / 0.1, vec2(0.0), vec2(1.0));
                        //float weight = 1.0 / (1.0 / distDoubleInvert.x + 1.0 / distDoubleInvert.y);
                        //float dist = pow(max(0.0, min(1.0, weight)), 2.0);
                        //annexe[vertexId].x = dist;
                        }
                    }
                }
            }
        }
    )"};

    /**************************/
    // FEEDBACK
    /**************************/

    /**
     * Default vertex shader with feedback
     */
    const std::string VERTEX_SHADER_FEEDBACK_DEFAULT {R"(
        layout (location = 0) in vec4 _vertex;
        layout (location = 1) in vec2 _texcoord;
        layout (location = 2) in vec4 _normal;
        layout (location = 3) in vec4 _annexe;

        uniform mat4 _mvp;
        uniform mat4 _mNormal;

        out VS_OUT
        {
            smooth vec4 vertex;
            smooth vec2 texcoord;
            smooth vec4 normal;
            smooth vec4 annexe;
        } vs_out;

        void main(void)
        {
            vs_out.vertex = _vertex;
            vs_out.texcoord = _texcoord;
            vs_out.normal = _normal;
            vs_out.annexe = _annexe;
        }
    )"};

    /**
     * Default feedback tessellation shader
     */
    const std::string TESS_CTRL_SHADER_FEEDBACK_DEFAULT {R"(
        #include projectAndCheckVisibility

        layout (vertices = 3) out;

        in VS_OUT
        {
            vec4 vertex;
            vec2 texcoord;
            vec4 normal;
            vec4 annexe;
        } tcs_in[];

        out TCS_OUT
        {
            vec4 vertex;
            vec2 texcoord;
            vec4 normal;
            vec4 annexe;
        } tcs_out[];

        uniform mat4 _mvp;
        uniform mat4 _mNormal;

        void main(void)
        {
            if (gl_InvocationID == 0)
            {
                bool isVisible = false;
                vec4 projectedVertices[3];
                float maxDist = 0.0;
                for (int i = 0; i < 3; ++i)
                {
                    vec2 dist;
                    projectedVertices[i] = tcs_in[i].vertex;
                    if (projectAndCheckVisibility(projectedVertices[i], _mvp, 0.1, dist))
                        isVisible = true;
                    maxDist = max(maxDist, max(dist.x, dist.y));
                }

                gl_TessLevelInner[0] = 1.0;
                gl_TessLevelOuter[0] = 1.0;
                gl_TessLevelOuter[1] = 1.0;
                gl_TessLevelOuter[2] = 1.0;

                vec3 projectedNormal = cross((projectedVertices[1] - projectedVertices[0]).xyz, (projectedVertices[2] - projectedVertices[0]).xyz);
                if (isVisible && projectedNormal.z <= 0.0)
                {
                    float maxLength = 0.0;
                    maxLength = max(length(projectedVertices[1].xy - projectedVertices[0].xy),
                                    length(projectedVertices[2].xy - projectedVertices[1].xy));
                    maxLength = max(maxLength, length(projectedVertices[2].xy - projectedVertices[0].xy));
                    float tessLevel = max(1.0, maxLength / 0.2);
                    tessLevel = mix(1.0, tessLevel, smoothstep(0.9, 1.0, maxDist));
                    gl_TessLevelInner[0] = tessLevel;
                    gl_TessLevelOuter[0] = tessLevel;
                    gl_TessLevelOuter[1] = tessLevel;
                    gl_TessLevelOuter[2] = tessLevel;
                }
            }

            tcs_out[gl_InvocationID].vertex = tcs_in[gl_InvocationID].vertex;
            tcs_out[gl_InvocationID].texcoord = tcs_in[gl_InvocationID].texcoord;
            tcs_out[gl_InvocationID].normal = tcs_in[gl_InvocationID].normal;
            tcs_out[gl_InvocationID].annexe = tcs_in[gl_InvocationID].annexe;

            gl_out[gl_InvocationID].gl_Position = tcs_out[gl_InvocationID].vertex;
        }
    )"};

    const std::string TESS_EVAL_SHADER_FEEDBACK_DEFAULT {R"(
        //layout (triangles, fractional_odd_spacing) in;
        layout (triangles) in;

        in TCS_OUT
        {
            vec4 vertex;
            vec2 texcoord;
            vec4 normal;
            vec4 annexe;
        } tes_in[];

        out TES_OUT
        {
            vec4 vertex;
            vec2 texcoord;
            vec4 normal;
            vec4 annexe;
        } tes_out;

        void main(void)
        {
            tes_out.vertex = (gl_TessCoord.x * tes_in[0].vertex) +
                             (gl_TessCoord.y * tes_in[1].vertex) +
                             (gl_TessCoord.z * tes_in[2].vertex);
            tes_out.texcoord = (gl_TessCoord.x * tes_in[0].texcoord) +
                               (gl_TessCoord.y * tes_in[1].texcoord) +
                               (gl_TessCoord.z * tes_in[2].texcoord);
            tes_out.normal = (gl_TessCoord.x * tes_in[0].normal) +
                             (gl_TessCoord.y * tes_in[1].normal) +
                             (gl_TessCoord.z * tes_in[2].normal);
            tes_out.annexe = (gl_TessCoord.x * tes_in[0].annexe) +
                             (gl_TessCoord.y * tes_in[1].annexe) +
                             (gl_TessCoord.z * tes_in[2].annexe);

            gl_Position = tes_out.vertex;
        }
    )"};

    /**
     * Default feedback geometry shader
     */
    const std::string GEOMETRY_SHADER_FEEDBACK_DEFAULT {R"(
        #include normalVector
        #include projectAndCheckVisibility

        in TES_OUT
        {
            vec4 vertex;
            vec2 texcoord;
            vec4 normal;
            vec4 annexe;
        } geom_in[];

        out GEOM_OUT
        {
            vec4 vertex;
            vec2 texcoord;
            vec4 normal;
            vec4 annexe;
        } geom_out;

        layout (triangles) in;
        layout (triangle_strip, max_vertices = 9) out;

        const int cutTable[6*9] = {
            0, 3, 4, 3, 1, 4, 1, 2, 4,
            0, 3, 4, 3, 1, 4, 4, 2, 0,
            0, 1, 3, 3, 4, 0, 3, 2, 4,
            0, 1, 3, 3, 4, 0, 3, 2, 4,
            0, 3, 4, 3, 1, 4, 4, 2, 0,
            0, 3, 4, 3, 1, 4, 1, 2, 4
        };

        uniform mat4 _mvp;

        void main(void)
        {
            vec4 projectedVertices[3];
            bool side[3]; // true = inside, false = outside
            vec2 distToBoundary[3];
            float verticesInside = 0;
            int cutCase = 0;
            for (int i = 0; i < 3; ++i)
            {
                vec2 dist;
                projectedVertices[i] = geom_in[i].vertex;
                bool isVisible = projectAndCheckVisibility(projectedVertices[i], _mvp, 0.0 + 0.001, dist);
                side[i] = isVisible;
                distToBoundary[i] = dist - vec2(1.0);
                if (side[i])
                {
                    verticesInside++;
                    cutCase += 1 << i;
                }
            }
            cutCase -= 1; // The table starts at 0...

            vec3 normal = normalVector(projectedVertices[0].xyz, projectedVertices[1].xyz, projectedVertices[2].xyz);
            // If all vertices are on the same side, and if the face is correctly oriented
            if (verticesInside == 0 || verticesInside == 3 || normal.z >= 0.0)
            {
                for (int i = 0; i < 3; ++i)
                {
                    gl_Position = geom_in[i].vertex;
                    geom_out.vertex = geom_in[i].vertex;
                    geom_out.texcoord = geom_in[i].texcoord;
                    geom_out.normal = geom_in[i].normal;
                    geom_out.annexe = geom_in[i].annexe;
                    EmitVertex();
                }

                EndPrimitive();
            }
            // ... if not
            else
            {
                vec4 vertices[5];
                vec2 texcoords[5];
                vec4 normals[5];
                for (int i = 0; i < 3; ++i)
                {
                    vertices[i] = geom_in[i].vertex;
                    texcoords[i] = geom_in[i].texcoord;
                    normals[i] = geom_in[i].normal;
                }
                    
                // Create the additional points
                int nextVertex = 3;
                for (int i = 0; i < 3; ++i)
                {
                    int nextId = (i + 1) % 3;
                    if (side[i] != side[nextId])
                    {
                        float ratio;
                        // These cases can handle corners better
                        if (sign(distToBoundary[i][0]) != sign(distToBoundary[nextId][0]))
                            ratio = abs(distToBoundary[i][0]) / (abs(distToBoundary[i][0]) + abs(distToBoundary[nextId][0]));
                        else
                            ratio = abs(distToBoundary[i][1]) / (abs(distToBoundary[i][1]) + abs(distToBoundary[nextId][1]));

                        vertices[nextVertex] = mix(vertices[i], vertices[nextId], ratio);
                        texcoords[nextVertex] = mix(texcoords[i], texcoords[nextId], ratio);
                        normals[nextVertex] = mix(normals[i], normals[nextId], ratio);
                        nextVertex++;
                    }
                }

                // Create the triangles from the cut case
                for (int t = 0; t < 3; ++t)
                {
                    for (int v = 0; v < 3; ++v)
                    {
                        int currentIndex = cutTable[cutCase * 9 + t * 3 + v];
                        gl_Position = vertices[currentIndex];
                        geom_out.vertex = vertices[currentIndex];
                        geom_out.texcoord = texcoords[currentIndex];
                        geom_out.normal = normals[currentIndex];
                        geom_out.annexe = vec4(0.0);
                        EmitVertex();
                    }

                    EndPrimitive();
                }
            }
        }
    )"};

    /**************************/
    // GRAPHICS
    /**************************/

    /**
     * Default vertex shader
     */
    const std::string VERTEX_SHADER_DEFAULT {R"(
        layout(location = 0) in vec4 _vertex;
        layout(location = 1) in vec2 _texcoord;
        layout(location = 2) in vec4 _normal;
        layout(location = 3) in vec4 _annexe;
        uniform mat4 _modelViewProjectionMatrix;
        uniform mat4 _normalMatrix;
        uniform vec3 _scale = vec3(1.0, 1.0, 1.0);

        out VertexData
        {
            vec4 position;
            vec2 texCoord;
            vec4 normal;
            vec4 annexe;
        } vertexOut;

        out BlendingData
        {
            smooth float totalBlend;
        } blendOut;

        void main(void)
        {
            vertexOut.position.xyz = _vertex.xyz * _scale;
            vertexOut.position = _modelViewProjectionMatrix * vec4(vertexOut.position.xyz, 1.0);
            gl_Position = vertexOut.position;
            vertexOut.normal = normalize(_normalMatrix * _normal);
            vertexOut.texCoord = _texcoord;
            vertexOut.annexe = _annexe;

            vec4 projectedVertex = vertexOut.position / vertexOut.position.w;
            if (projectedVertex.z >= 0.0)
            {
                projectedVertex = abs(projectedVertex);

                bvec4 vertexVisible = lessThanEqual(projectedVertex, vec4(1.0));
                if (vertexVisible.x && vertexVisible.y && vertexVisible.z)
                    blendOut.totalBlend = _annexe.x;// - _annexe.y;
                else
                    blendOut.totalBlend = _annexe.x;
            }
        }
    )"};

    /**
     * Textured fragment shader
     */
    const std::string FRAGMENT_SHADER_TEXTURE {R"(
        #define PI 3.14159265359

    #ifdef TEXTURE_RECT
        uniform sampler2DRect _tex0;
    #else
        uniform sampler2D _tex0;
    #endif

    #ifdef BLENDING
        uniform sampler2D _tex1;
    #endif
        uniform vec2 _tex0_size = vec2(1.0);

        uniform int _sideness = 0;
        uniform int _textureNbr = 0;
        uniform vec3 _cameraAttributes = vec3(0.05, 0.0, 1.0); // blendWidth, blackLevel and brightness
        uniform vec4 _fovAndColorBalance = vec4(0.0, 0.0, 1.0, 1.0); // fovX and fovY, r/g and b/g
        uniform int _isColorLUT = 0;
        uniform vec3 _colorLUT[256];
        uniform mat3 _colorMixMatrix = mat3(1.0, 0.0, 0.0,
                                            0.0, 1.0, 0.0,
                                            0.0, 0.0, 1.0);

        in VertexData
        {
            vec4 position;
            vec2 texCoord;
            vec4 normal;
            vec4 annexe;
        } vertexIn;

        in BlendingData
        {
            float totalBlend;
        } blendIn;

        out vec4 fragColor;
        // Texture transformation
        uniform int _tex0_flip = 0;
        uniform int _tex0_flop = 0;
        // HapQ specific parameters
        uniform int _tex0_YCoCg = 0;

        void main(void)
        {
            float blendWidth = _cameraAttributes.x;
            float blackLevel = _cameraAttributes.y;
            float brightness = _cameraAttributes.z;

            vec4 position = vertexIn.position;
            vec2 texCoord = vertexIn.texCoord;
            vec4 normal = vertexIn.normal;

            vec2 screenPos = vec2(position.x / position.w, position.y / position.w);

            /************ TEST ***************/
            //fragColor.rgb = pow(vec3(vertexIn.annexe.x / 3.0), vec3(1.0 / 2.2));
            //fragColor.rgb = pow(vec3(1.0 / blendIn.totalBlend / 3.0), vec3(1.0 / 2.2));
            ////fragColor.rgb = vec3(vertexIn.annexe.x);
            //fragColor.a = 1.0;
            //return;
            /******* END OF TEST ************/

            // Compute the real texture coordinates, according to flip / flop
            vec2 realCoords;
            if (_tex0_flip == 1 && _tex0_flop == 0)
                realCoords = vec2(texCoord.x, 1.0 - texCoord.y);
            else if (_tex0_flip == 0 && _tex0_flop == 1)
                realCoords = vec2(1.0 - texCoord.x, texCoord.y);
            else if (_tex0_flip == 1 && _tex0_flop == 1)
                realCoords = vec2(1.0 - texCoord.x, 1.0 - texCoord.y);
            else
                realCoords = texCoord;

            vec4 color = texture(_tex0, realCoords * _tex0_size);

            // If the color is expressed as YCoCg (for HapQ compression), extract RGB color from it
            if (_tex0_YCoCg == 1)
            {
                float scale = (color.z * (255.0 / 8.0)) + 1.0;
                float Co = (color.x - (0.5 * 256.0 / 255.0)) / scale;
                float Cg = (color.y - (0.5 * 256.0 / 255.0)) / scale;
                float Y = color.w;
                color.rgba = vec4(Y + Co - Cg, Y + Cg, Y - Co - Cg, 1.0);
                color.rgb = pow(color.rgb, vec3(2.2));
            }

            float maxBalanceRatio = max(_fovAndColorBalance.z, _fovAndColorBalance.w);
            color.r *= _fovAndColorBalance.z / maxBalanceRatio;
            color.g *= 1.0 / maxBalanceRatio;
            color.b *= _fovAndColorBalance.w / maxBalanceRatio;

            // Black level
            float blackCorrection = max(min(blackLevel, 1.0), 0.0);
            color.rgb = color.rgb * (1.0 - blackLevel) + blackLevel;
            
            // If there is a blending map
        #ifdef BLENDING
            int blendFactor = int(texture(_tex1, texCoord).r * 65536.0);
            // Extract the number of cameras
            int camNbr = blendFactor / 4096;
            blendFactor = blendFactor - camNbr * 4096;
            float blendFactorFloat = 0.0;

            // If the max channel value is higher than 2*blacklevel, we smooth the blending edges
            bool smoothBlend = false;
            if (color.r > blackLevel * 2.0 || color.g > blackLevel * 2.0 || color.b > blackLevel * 2.0)
                smoothBlend = true;

            if (blendFactor == 0)
                blendFactorFloat = 0.05; // The non-visible part is kinda hidden
            else if (blendWidth > 0.0 && smoothBlend == true)
            {
                vec2 normalizedPos = vec2(screenPos.x / 2.0 + 0.5, screenPos.y / 2.0 + 0.5);
                vec2 distDoubleInvert = vec2(min(normalizedPos.x, 1.0 - normalizedPos.x), min(normalizedPos.y, 1.0 - normalizedPos.y));
                distDoubleInvert = clamp(distDoubleInvert / blendWidth, vec2(0.0), vec2(1.0));
                float weight = 1.0 / (1.0 / distDoubleInvert.x + 1.0 / distDoubleInvert.y);
                float dist = pow(max(0.0, min(1.0, weight)), 2.0);
                blendFactorFloat = 256.0 * dist / float(blendFactor);
            }
            else
            {
                blendFactorFloat = 1.0 / float(camNbr);
            }
            color.rgb = color.rgb * min(1.0, blendFactorFloat);
        #endif

        #ifdef VERTEXBLENDING
            float blendFactor = blendIn.totalBlend == 0.0 ? 0.05 : 1.0 / blendIn.totalBlend;
            color.rgb = color.rgb * blendFactor;
        #endif

            // Brightness correction
            color.rgb = color.rgb * brightness;

            // Color correction through a LUT
            if (_isColorLUT != 0)
            {
                ivec3 icolor = ivec3(round(color.rgb * 255.f));
                color.rgb = vec3(_colorLUT[icolor.r].r, _colorLUT[icolor.g].g, _colorLUT[icolor.b].b);
                //color.rgb = clamp(_colorMixMatrix * color.rgb, vec3(0.0), vec3(1.0));
            }
            
            fragColor.rgb = color.rgb;
            fragColor.a = 1.0;
        }
    )"};

    /**
     * Single color fragment shader
     */
    const std::string FRAGMENT_SHADER_COLOR {R"(
        #define PI 3.14159265359

        uniform int _sideness = 0;
        uniform vec4 _fovAndColorBalance = vec4(0.0, 0.0, 1.0, 1.0); // fovX and fovY, r/g and b/g
        uniform vec4 _color = vec4(0.0, 1.0, 0.0, 1.0);

        in VertexData
        {
            vec4 position;
            vec2 texCoord;
            vec4 normal;
        } vertexIn;

        out vec4 fragColor;

        void main(void)
        {
            vec4 position = vertexIn.position;
            vec2 texCoord = vertexIn.texCoord;
            vec4 normal = vertexIn.normal;

            fragColor = _color;
        }
    )"};

    /**
     * UV drawing fragment shader
     * UV coordinates are encoded on 2 channels each, to get 16bits precision
     */
    const std::string FRAGMENT_SHADER_UV {R"(
        #define PI 3.14159265359

        uniform int _sideness = 0;
        uniform vec4 _fovAndColorBalance = vec4(0.0, 0.0, 1.0, 1.0); // fovX and fovY, r/g and b/g

        in VertexData
        {
            vec4 position;
            vec2 texCoord;
            vec4 normal;
        } vertexIn;

        out vec4 fragColor;

        void main(void)
        {
            vec4 position = vertexIn.position;
            vec2 texCoord = vertexIn.texCoord;
            vec4 normal = vertexIn.normal;

            float U = texCoord.x * 65536.0;
            float V = texCoord.y * 65536.0;

            fragColor.rg = vec2(floor(U / 256.0) / 256.0, (U - floor(U / 256.0) * 256.0) / 256.0);
            fragColor.ba = vec2(floor(V / 256.0) / 256.0, (V - floor(V / 256.0) * 256.0) / 256.0);
        }
    )"};

    /**
     * Wireframe rendering
     */
    const std::string VERTEX_SHADER_WIREFRAME {R"(
        layout(location = 0) in vec4 _vertex;
        layout(location = 1) in vec2 _texcoord;
        layout(location = 2) in vec4 _normal;
        uniform mat4 _modelViewProjectionMatrix;

        out VertexData
        {
            vec4 vertex;
            vec4 normal;
            vec2 texcoord;
        } vertexOut;

        void main()
        {
            vertexOut.vertex = _vertex;
            vertexOut.normal = _normal;
            vertexOut.texcoord = _texcoord;
        }
    )"};

    const std::string GEOMETRY_SHADER_WIREFRAME {R"(
        layout(triangles) in;
        layout(triangle_strip, max_vertices = 3) out;
        uniform mat4 _modelViewProjectionMatrix;
        uniform mat4 _normalMatrix;
        uniform vec3 _scale = vec3(1.0, 1.0, 1.0);

        in VertexData
        {
            vec4 vertex;
            vec4 normal;
            vec2 texcoord;
        } vertexIn[];

        out VertexData
        {
            vec2 texcoord;
            vec4 normal;
            vec3 bcoord;
            vec4 position;
        } vertexOut;

        void main()
        {
            vec4 v = _modelViewProjectionMatrix * vec4(vertexIn[0].vertex.xyz * _scale.xyz, 1.0);
            gl_Position = v;
            vertexOut.texcoord = vertexIn[0].texcoord;
            vertexOut.bcoord = vec3(1.0, 0.0, 0.0);
            vertexOut.position = v;
            EmitVertex();

            v = _modelViewProjectionMatrix * vec4(vertexIn[1].vertex.xyz * _scale.xyz, 1.0);
            gl_Position = v;
            vertexOut.texcoord = vertexIn[1].texcoord;
            vertexOut.bcoord = vec3(0.0, 1.0, 0.0);
            vertexOut.position = v;
            EmitVertex();

            v = _modelViewProjectionMatrix * vec4(vertexIn[2].vertex.xyz * _scale.xyz, 1.0);
            gl_Position = v;
            vertexOut.texcoord = vertexIn[2].texcoord;
            vertexOut.bcoord = vec3(0.0, 0.0, 1.0);
            vertexOut.position = v;
            EmitVertex();

            EndPrimitive();
        }
    )"};

    const std::string FRAGMENT_SHADER_WIREFRAME {R"(
        #define PI 3.14159265359

        in VertexData
        {
            vec2 texcoord;
            vec4 normal;
            vec3 bcoord;
            vec4 position;
        } vertexIn;

        uniform int _sideness = 0;
        uniform vec4 _fovAndColorBalance = vec4(0.0, 0.0, 1.0, 1.0); // fovX and fovY, r/g and b/g
        out vec4 fragColor;

        void main(void)
        {
            vec4 position = vertexIn.position;
            vec4 normal = vertexIn.normal;

            vec3 b = vertexIn.bcoord;
            float minDist = min(min(b[0], b[1]), b[2]);
            vec4 matColor = vec4(0.3, 0.3, 0.3, 1.0);
            if (minDist < 0.025)
                fragColor.rgba = mix(vec4(1.0), matColor, (minDist - 0.0125) / 0.0125);
            else
                fragColor.rgba = matColor;
        }
    )"};

    /**
     * Rendering of the output windows
     */
    const std::string VERTEX_SHADER_WINDOW {R"(
        layout(location = 0) in vec4 _vertex;
        layout(location = 1) in vec2 _texcoord;
        //layout(location = 2) in vec3 _normal;
        //uniform mat4 _modelViewProjectionMatrix;
        //uniform vec3 _scale = vec3(1.0, 1.0, 1.0);
        smooth out vec2 texCoord;

        void main(void)
        {
            //gl_Position = _modelViewProjectionMatrix * vec4(_vertex.x * _scale.x, _vertex.y * _scale.y, _vertex.z * _scale.z, 1.0);
            gl_Position = vec4(_vertex.x, _vertex.y, _vertex.z, 1.0);
            texCoord = _texcoord;
        }
    )"};

    const std::string FRAGMENT_SHADER_WINDOW {R"(
        #define PI 3.14159265359

    #ifdef TEX_1
        uniform sampler2D _tex0;
    #ifdef TEX_2
        uniform sampler2D _tex1;
    #ifdef TEX_3
        uniform sampler2D _tex2;
    #ifdef TEX_4
        uniform sampler2D _tex3;
    #endif
    #endif
    #endif
    #endif
        uniform int _textureNbr = 0;
        uniform ivec4 _layout = ivec4(0, 1, 2, 3);
        uniform vec2 _gamma = vec2(1.0, 2.2);
        in vec2 texCoord;
        out vec4 fragColor;

        void main(void)
        {
            float frames = float(_textureNbr);
            for (int i = 0; i < _textureNbr; ++i)
            {
                int value = _layout[i];
                for (int j = i + 1; j < _textureNbr; ++j)
                {
                    if (_layout[j] == value)
                    {
                        frames--;
                        break;
                    }
                }
            }

            fragColor.rgba = vec4(0.0);
    #ifdef TEX_1
            if (_textureNbr > 0 && texCoord.x > float(_layout[0]) / frames && texCoord.x < (float(_layout[0]) + 1.0) / frames)
            {
                fragColor = texture(_tex0, vec2((texCoord.x - float(_layout[0]) / frames) * frames, texCoord.y));
            }
    #ifdef TEX_2
            if (_textureNbr > 1 && texCoord.x > float(_layout[1]) / frames && texCoord.x < (float(_layout[1]) + 1.0) / frames)
            {
                vec4 color = texture(_tex1, vec2((texCoord.x - float(_layout[1]) / frames) * frames, texCoord.y));
                fragColor.rgb = mix(fragColor.rgb, color.rgb, color.a);
                fragColor.a = max(fragColor.a, color.a);
            }
    #ifdef TEX_3
            if (_textureNbr > 2 && texCoord.x > float(_layout[2]) / frames && texCoord.x < (float(_layout[2]) + 1.0) / frames)
            {
                vec4 color = texture(_tex2, vec2((texCoord.x - float(_layout[2]) / frames) * frames, texCoord.y));
                fragColor.rgb = mix(fragColor.rgb, color.rgb, color.a);
                fragColor.a = max(fragColor.a, color.a);
            }
    #ifdef TEX_4
            if (_textureNbr > 3 && texCoord.x > float(_layout[3]) / frames && texCoord.x < (float(_layout[3]) + 1.0) / frames)
            {
                vec4 color = texture(_tex3, vec2((texCoord.x - float(_layout[3]) / frames) * frames, texCoord.y));
                fragColor.rgb = mix(fragColor.rgb, color.rgb, color.a);
                fragColor.a = max(fragColor.a, color.a);
            }
    #endif
    #endif
    #endif
    #endif

            if (_gamma.x != 1.0)
                fragColor.rgb = pow(fragColor.rgb, vec3(1.0 / _gamma.y));
        }
    )"};

} ShaderSources;

} // end of namespace

#endif // SPLASH_SHADERSOURCES_H
