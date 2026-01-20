#version 330

uniform sampler2D color_texture;
in vec2 tex_coord;
out vec4 fragColor;

// ---------------- CONFIGURATION ----------------
float THRESHOLD = 0.9; //hard threshold
float KNEE = 0.4; //lagodzi przejscie
float BLUR_STEP = 0.007;  //odleglosc poswiaty od obiektu
float BLOOM_INTENSITY = 2.0; //sila poswiaty

//filtr poswiaty
vec3 get_bloom(vec2 uv) {
    vec3 col = texture(color_texture, uv).rgb;
    float luma = dot(col, vec3(0.2126, 0.7152, 0.0722));
    //soft threshold
    float weight = clamp((luma - THRESHOLD + KNEE) / (KNEE * 2.0), 0.0, 1.0);
    return col * weight;
}

void main()
{
    vec3 sceneCol = texture(color_texture, tex_coord).rgb;
    vec3 bloom = vec3(0.0);

    int directions = 12;
    int rings = 3; 

    for(int r = 1; r <= rings; r++) {
        float currentRadius = BLUR_STEP * (float(r) / float(rings));
        for(int d = 0; d < directions; d++) {
            float angle = float(d) * (6.283185 / float(directions));
            vec2 offset = vec2(cos(angle), sin(angle)) * currentRadius;
            bloom += get_bloom(tex_coord + offset);
        }
    }

    bloom /= float(directions * rings);
    vec3 finalColor = sceneCol + (bloom * BLOOM_INTENSITY);

    fragColor = vec4(finalColor, 1.0);
}
