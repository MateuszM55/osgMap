#version 330

uniform sampler2D color_texture;
uniform sampler2D depth_texture;

in vec2 tex_coord;

void main() {
    gl_FragColor = vec4(texture(color_texture, tex_coord).r);
}
