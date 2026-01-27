varying vec2 texCoord;
uniform float osg_FrameTime;
uniform float texCoordScale;
uniform float animStrength;
uniform float animSpeed;

void main() {
    texCoord = gl_Vertex.xy * texCoordScale;
    float speed = animSpeed;
    float strength = animStrength;
    texCoord.x += sin(osg_FrameTime * speed) * strength;
    texCoord.y += cos(osg_FrameTime * speed * 0.5) * strength;
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}