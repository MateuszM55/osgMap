#version 330

uniform sampler2D color_texture;
uniform sampler2D depth_texture;

// KONFIGURACJA

// Maksymalny limit rozmycia (Wyzsza wartosc to bardziej rozmyte tlo)
uniform float u_max_blur; 

// Sila rozmycia
// Dostosuj to jesli tlo jest zbyt rozmyte lub za malo rozmyte (Wyzsza wartosc to bardziej rozmyte tlo)
uniform float u_blur_ramp;

// Bezpieczna strefa (zachowanie ostrosci pobliskiego terenu)
// Wszystko bardzo blisko pozostaje ostre
uniform float u_focus_range;

in vec2 tex_coord;
out vec4 fragColor;

void main() {
    vec4 sceneColor = texture2D(color_texture, tex_coord);
    
    // Pobranie glebi
    float depth = texture2D(depth_texture, tex_coord).r;

    // STALY FOKUS (Blisko znaczy ostro)
    // Zamiast czytac srodkowy piksel blokujemy ostrosc na 0.0 (bliska plaszczyzna)
    float focusDepth = 0.0; 

    // Obliczenie wspolczynnika rozmycia
    // Prosta logika im wieksza glebia tym wieksze rozmycie
    float dist = abs(depth - focusDepth);

    // Odjecie bezpiecznej strefy (zachowanie ostrosci pobliskiego terenu)
    float factor = max(0.0, dist - u_focus_range);

    // Ograniczenie i wzmocnienie
    factor = clamp(factor, 0.0, u_max_blur);
    float strength = factor * u_blur_ramp;
    
    // Obliczenie kroku
    vec2 blurStep = vec2(1.0/800.0) * strength; 

    // Probkowanie Rozmycia
    vec4 sum = vec4(0.0);
    
    sum += sceneColor * 4.0;
    
    sum += texture2D(color_texture, tex_coord + vec2(-blurStep.x, -blurStep.y));
    sum += texture2D(color_texture, tex_coord + vec2( 0.0,        -blurStep.y));
    sum += texture2D(color_texture, tex_coord + vec2( blurStep.x, -blurStep.y));
    
    sum += texture2D(color_texture, tex_coord + vec2(-blurStep.x,  0.0)); 
    sum += texture2D(color_texture, tex_coord + vec2( blurStep.x,  0.0));
    
    sum += texture2D(color_texture, tex_coord + vec2(-blurStep.x,  blurStep.y));
    sum += texture2D(color_texture, tex_coord + vec2( 0.0,         blurStep.y));
    sum += texture2D(color_texture, tex_coord + vec2( blurStep.x,  blurStep.y));

    fragColor = sum / 12.0;
}
