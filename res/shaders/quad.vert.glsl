#version 450

layout(location = 0) in vec2 a_position;
layout(location = 1) in vec2 a_tex_coord;

layout(location = 0) out vec2 frag_tex_coord;

void main() {
    gl_Position = vec4(a_position, 0.0, 1.0);
    frag_tex_coord = a_tex_coord;
}
