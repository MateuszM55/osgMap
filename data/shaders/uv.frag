#version 420 compatibility

in vec2 tex_coord;
out vec4 fragColor;

void main() {
	fragColor = vec4(tex_coord, 0.0, 1.0);
}