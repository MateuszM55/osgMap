varying vec2 texCoord;
uniform float texCoordScale;
uniform float animStrength;
uniform float animSpeed;

void main() {
    texCoord = gl_Vertex.xy * texCoordScale;
    gl_Position = ftransform();
}
