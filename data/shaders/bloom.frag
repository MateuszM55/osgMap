#version 330

uniform sampler2D color_texture;
uniform sampler2D depth_texture;

in vec2 tex_coord;
out vec4 fragColor;

void main() {
    fragColor = texture(color_texture, tex_coord) + vec4(0.0, 0.0, 0.5, 1.0);
}