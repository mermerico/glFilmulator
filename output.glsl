#version 430 core

layout(local_size_x = 128, local_size_y = 8) in;
layout(rgba32f, binding = 0) uniform image2D activeCrystalsImage;
layout(rgba32f, binding = 2) uniform image2D crystalRadiusImage;
layout(rgba32f, binding = 4) uniform image2D outputImage;

uniform float rolloffBoundary;

void main()
{
    uint x = gl_GlobalInvocationID.x;
    uint y = gl_GlobalInvocationID.y;

    vec3 activeCrystals = imageLoad(activeCrystalsImage, ivec2(x,y)).rgb;
    vec3 crystalRadius = imageLoad(crystalRadiusImage, ivec2(x,y)).rgb;

    vec3 density = activeCrystals * crystalRadius * crystalRadius;
    
    imageStore(outputImage, ivec2(x, y), vec4(density, 1.0f));
}
