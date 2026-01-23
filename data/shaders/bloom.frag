#version 330

uniform sampler2D color_texture;
uniform bool u_is_active;

in vec2 tex_coord;
out vec4 fragColor;

// ---------------- CONFIGURATION ----------------

uniform float u_threshold;          //hard threshold
uniform float u_knee;               //lagodzi przejscie
uniform float u_blur_step;          //odleglosc poswiaty od obiektu
uniform float u_bloom_intensity;    //sila poswiaty

//filtr poswiaty
vec3 get_bloom(vec2 uv) {
    vec3 col = texture(color_texture, uv).rgb;
    float luma = dot(col, vec3(0.2126, 0.7152, 0.0722));
    //soft threshold
    float weight = clamp((luma - u_threshold + u_knee) / (u_knee* 2.0), 0.0, 1.0);
    return col * weight;
}

void main()
{
    vec3 sceneCol = texture(color_texture, tex_coord).rgb;
    if (!u_is_active) { fragColor = vec4(sceneCol, 1.0); return; }

    vec3 bloom = vec3(0.0);

    int directions = 12;
    int rings = 3; 

    for(int r = 1; r <= rings; r++) {
        float currentRadius = u_blur_step * (float(r) / float(rings));
        for(int d = 0; d < directions; d++) {
            float angle = float(d) * (6.283185 / float(directions));
            vec2 offset = vec2(cos(angle), sin(angle)) * currentRadius;
            bloom += get_bloom(tex_coord + offset);
        }
    }

    bloom /= float(directions * rings);
    vec3 finalColor = sceneCol + (bloom * u_bloom_intensity);

    fragColor = vec4(finalColor, 1.0);
}
