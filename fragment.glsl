#version 430 core

in vec2 uv;
out vec4 outColor;

layout(binding = 0) uniform sampler2D activeCrystals;
layout(binding = 1) uniform sampler2D developer;
layout(binding = 2) uniform sampler2D crystalRadius;
layout(binding = 3) uniform sampler2D silverSaltConcentration;
layout(binding = 4) uniform sampler2D input;
layout(binding = 5) uniform sampler2D developerSmall;

void main()
{
    float ds = texture(developerSmall, uv).r/2;
    float d = texture(developer, uv).r/2;
    float a = texture(activeCrystals, uv).r;
    float i = texture(input, uv).r/pow(2,16);
    float s = texture(silverSaltConcentration, uv).r;
    float cr = 100*texture(crystalRadius, uv).r;

    float den = a*cr*cr/1e2;
    vec4 aVec = texture(activeCrystals, uv);
    vec4 crVec = texture(crystalRadius, uv);
    vec4 density = aVec * crVec * crVec;
    outColor = 100*density;
    // outColor = vec4(texture(input, uv)/pow(2,16));
    // outColor = texture(input, uv);

}
