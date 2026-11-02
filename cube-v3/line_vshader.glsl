#version 330 core

uniform mat4 mvp_matrix;

layout(location = 0) in vec4 a_position;
layout(location = 1) in vec3 a_color;

out vec3 v_color;

void main()
{
    // Same transform as the cube: model -> world -> view -> clip
    gl_Position = mvp_matrix * a_position;

    // Pass the vertex color to the fragment shader
    v_color = a_color;
}
