#version 330 core

in vec3 v_color;

layout(location = 0) out vec4 fragColor;

void main()
{
    // Lines are not textured, only colored
    fragColor = vec4(v_color, 1.0);
}
