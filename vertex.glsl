# version 430 core

in vec2 position;
in vec2 uvIn;

out vec2 uv;

void main()
{
    uv = uvIn;
    gl_Position = vec4(position,0.0,1.0);
}
