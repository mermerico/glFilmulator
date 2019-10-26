#version 430 core

layout(local_size_x = 64, local_size_y = 8) in;
layout(rgba32f, binding = 0) uniform image2D activeCrystalsImage;
layout(r32f, binding = 1) uniform image2D developerConcentrationOutputImage;
layout(binding = 5) uniform sampler2D developerConcentrationSampler;
layout(rgba32f, binding = 2) uniform image2D crystalRadiusImage;
layout(rgba32f, binding = 3) uniform image2D silverSaltDensityImage;

uniform float crystalGrowthRate;
uniform float developerConsumptionRate;
uniform float silverSaltConsumptionRate;

void main()
{
    uint x = gl_GlobalInvocationID.x;
    uint y = gl_GlobalInvocationID.y;

    ivec2 fullSize = imageSize(activeCrystalsImage);
    float rx = float(x)/float(fullSize.x - 1);
    float ry = float(y)/float(fullSize.y - 1);

    float developerConcentration = texture(developerConcentrationSampler,vec2(rx,ry)).r;
    //float developerConcentration = imageLoad(developerConcentrationOutputImage, ivec2(x,y)).x;
    vec3 silverSaltDensity = imageLoad(silverSaltDensityImage, ivec2(x,y)).xyz;
    vec3 crystalRadius = imageLoad(crystalRadiusImage, ivec2(x,y)).xyz;
    vec3 activeCrystals = imageLoad(activeCrystalsImage, ivec2(x,y)).xyz;

    vec3 dCrystalRad = developerConcentration * silverSaltDensity * crystalGrowthRate;
    vec3 dCrystalVol = dCrystalRad * crystalRadius * crystalRadius * activeCrystals;

    crystalRadius += dCrystalRad;
    silverSaltDensity -= silverSaltConsumptionRate * dCrystalVol;
    developerConcentration -= developerConsumptionRate * (dCrystalVol.x + dCrystalVol.y + dCrystalVol.z);
    silverSaltDensity = max(silverSaltDensity,vec3(0,0,0));
    if (developerConcentration < 0)
    {
        developerConcentration = 0;
    } 
    //developerConcentration = 1;

    imageStore(crystalRadiusImage, ivec2(x, y), vec4(crystalRadius, 0.0f));
    imageStore(silverSaltDensityImage, ivec2(x, y), vec4(silverSaltDensity, 0.0f));
    imageStore(developerConcentrationOutputImage, ivec2(x,y), vec4(developerConcentration, 0.0f, 0.0f, 0.0f));

}