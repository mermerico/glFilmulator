#version 430 core

layout(local_size_x = 64, local_size_y = 8) in;
layout(rgba32f, binding = 4) uniform image2D inputImage;
layout(rgba32f, binding = 0) uniform image2D activeCrystals;

uniform float rolloffBoundary;

void main()
{
    uint x = gl_GlobalInvocationID.x;
    uint y = gl_GlobalInvocationID.y;

    float crystalHeadroom = 65535.0-rolloffBoundary;

    vec3 thisPixel = imageLoad(inputImage, ivec2(x,y)).xyz;
    vec3 outCrystals;

    for (int c = 0; c < 3; c++)
    {
        float thisColor = thisPixel[c];
        if ( thisColor > rolloffBoundary)
        {
            outCrystals[c] = 65535.0f-(crystalHeadroom*crystalHeadroom /
                             (thisColor+crystalHeadroom-rolloffBoundary));
        }
        else
        {
            outCrystals[c] = thisColor;
        }
    }
    
    imageStore(activeCrystals, ivec2(x, y), vec4(outCrystals, 0.0f));
}
