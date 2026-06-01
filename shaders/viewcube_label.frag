#version 450 core

in vec2 uv;

out vec4 fragColor;

uniform sampler2D labelTexture;

void main()
{
    vec4 color = texture(labelTexture, uv);
    if (color.a < 0.01)
        discard;
    fragColor = color;
}
