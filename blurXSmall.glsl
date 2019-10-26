#version 430 core

layout(local_size_x = 64, local_size_y = 8) in;
layout(binding = 1) uniform sampler2D developer;
layout(r32f, binding = 5) uniform image2D smallBlurred;

uniform uint mipLevel;
uniform float sigma;

float relCoord(uint absCoord,uint dim)
{
    ivec2 outputSize = imageSize(smallBlurred);
    return float(absCoord)/float(outputSize[dim] - 1);
}

void main()
{
    uint x = gl_GlobalInvocationID.x;
    uint y = gl_GlobalInvocationID.y;

    float rx = relCoord(x,0);
    float ry = relCoord(y,1);

    float sum = 0;
    float weightSum = 0;
    for(int i = -5; i < 6; i++)
    {
        float weight = exp(-0.5*pow(i/sigma,2));
        sum += textureLod(developer, vec2(relCoord(x+i, 0), ry), mipLevel).x * weight;
        weightSum += weight;
    }
    imageStore(smallBlurred,ivec2(x,y),vec4(sum/weightSum,0.0,0.0,0.0));
}