varying vec2 texCoord;
varying vec3 viewDir;
uniform float texCoordScale;
uniform float animStrength;
uniform float animSpeed;

void main() {
    texCoord = gl_Vertex.xy * texCoordScale;
    vec4 eyePos = gl_ModelViewMatrix * gl_Vertex;
    viewDir = normalize(eyePos.xyz);
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}