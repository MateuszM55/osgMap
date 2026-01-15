#version 330

layout (location = 0) in vec3 i_position;

out vec2 tex_coord;

void main() {
    gl_Position = vec4(i_position, 1.0);
    tex_coord = vec2(
        (i_position.x + 1.0) / 2.0,
        (i_position.y + 1.0) / 2.0
    );
}