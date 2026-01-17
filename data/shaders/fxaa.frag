#version 330 core

/**
 * Implementation taken mostly from the official nvidia paper
 * https://developer.download.nvidia.com/assets/gamedev/files/sdk/11/FXAA_WhitePaper.pdf
 *
 * The look of this effect strongly depends on screen res etc. 
 * It looks fine locally (2160 x 1080), but may be overblurred 
 * on smaller resolutions. Feel free to play around with the 
 * shader parameters to fine tune the look of it.
 */

/** FXAA settings */
#define FXAA_EDGE_THRESHOLD     1.0 / 8.0   // luma threshold over which everything is considered an edge
#define FXAA_EDGE_THRESHOLD_MIN 1.0 / 16.0  // minimum luma threshold that has to be satisfied by every edge
#define FXAA_EDGE_SEARCH_STEPS  8           // number of pixels that will be checked during end-of-edge search
#define FXAA_BLUR_CLOSE_DIST    1.0         // distance of the close pixels that will be averaged with the center pixel
#define FXAA_BLUR_FAR_DIST      1.5         // distance of the far pixels that will be averaged with the center pixel

/** Debug defines */
// #define FXAA_DBG_SPLIT_SCREEN        // only process the left half of the viewport
// #define FXAA_DBG_DISCARD_NON_EDGE    // discard everything that's not an edge
// #define FXAA_DBG_EDGE_DETECTION      // draw a red line along the detected edges
// #define FXAA_DBG_EDGE_DIRECTION      // draw a red/green line depending on the direction of the edge
// #define FXAA_DBG_EDGE_DISTANCE       // draw a red gradient near edges - more red, more edge (hehe)

uniform sampler2D color_texture;
uniform sampler2D depth_texture;
uniform vec2 u_resolution;

in vec2 tex_coord;
out vec4 fragColor;

vec3 getPixel(in vec2 offset) { return texture(color_texture, tex_coord + offset).xyz; }
float getLuma(in vec3 color) { return dot(color, vec3(0.299, 0.587, 0.114)); }

vec3 fxaa() {

    #ifdef FXAA_DBG_SPLIT_SCREEN
        if (tex_coord.x >= 0.5) {
            return getPixel(vec2(0.0, 0.0));
        }
    #endif

    vec2 pixel_size = vec2(1.0 / u_resolution.x, 1.0 / u_resolution.y);

    vec3 rgb_m  = getPixel(vec2( 0.0,  0.0));
    vec3 rgb_n  = getPixel(vec2( 0.0, -1.0) * pixel_size);
    vec3 rgb_s  = getPixel(vec2( 0.0,  1.0) * pixel_size);
    vec3 rgb_e  = getPixel(vec2( 1.0,  0.0) * pixel_size);
    vec3 rgb_w  = getPixel(vec2(-1.0,  0.0) * pixel_size);
    vec3 rgb_ne = getPixel(vec2( 1.0, -1.0) * pixel_size);
    vec3 rgb_nw = getPixel(vec2(-1.0, -1.0) * pixel_size);
    vec3 rgb_se = getPixel(vec2( 1.0,  1.0) * pixel_size);
    vec3 rgb_sw = getPixel(vec2(-1.0,  1.0) * pixel_size);

    float luma_m    = getLuma(rgb_m);
    float luma_n    = getLuma(rgb_n);
    float luma_s    = getLuma(rgb_s);
    float luma_e    = getLuma(rgb_e);
    float luma_w    = getLuma(rgb_w);
    float luma_ne   = getLuma(rgb_ne);
    float luma_nw   = getLuma(rgb_nw);
    float luma_se   = getLuma(rgb_se);
    float luma_sw   = getLuma(rgb_sw);

    float luma_min = min(luma_m, min(luma_n, min(luma_s, min(luma_e, luma_w))));
    float luma_max = max(luma_m, max(luma_n, max(luma_s, max(luma_e, luma_w))));
    float luma_range = luma_max - luma_min;

    if (luma_range < max(FXAA_EDGE_THRESHOLD_MIN, luma_max * FXAA_EDGE_THRESHOLD)) {
        #ifdef FXAA_DBG_DISCARD_NON_EDGE 
            return vec3(0.0, 0.0, 0.0);
        #else
            return rgb_m;
        #endif
    }

    #ifdef FXAA_DBG_EDGE_DETECTION 
        return vec3(1.0, 0.0, 0.0);
    #endif

    bool is_vertical = (
        abs((0.25 * luma_nw) + (-0.5 * luma_n) + (0.25 * luma_ne)) +
        abs((0.50 * luma_w ) + (-1.0 * luma_m) + (0.50 * luma_e )) +
        abs((0.25 * luma_sw) + (-0.5 * luma_s) + (0.25 * luma_se))
    ) > (
        abs((0.25 * luma_nw) + (-0.5 * luma_w) + (0.25 * luma_sw)) +
        abs((0.50 * luma_n ) + (-1.0 * luma_m) + (0.50 * luma_s )) +
        abs((0.25 * luma_ne) + (-0.5 * luma_e) + (0.25 * luma_se))
    );
    vec2 edge_tangent = is_vertical ? vec2(0.0, 1.0) : vec2(1.0, 0.0);

    #ifdef FXAA_DBG_EDGE_DIRECTION
        return vec3(edge_tangent, 0.0);
    #endif

    float luma_positive = is_vertical ? luma_n : luma_e;
    float luma_negative = is_vertical ? luma_s : luma_w;
    float gradient_positive = abs(luma_positive - luma_m);
    float gradient_negative = abs(luma_negative - luma_m);
    
    vec2 distance_positive  = vec2(0.0);
    vec2 distance_negative  = vec2(0.0);
    vec2 distance_offset    = vec2(edge_tangent.y * pixel_size.x, edge_tangent.x * pixel_size.y);

    float gradient  = max(gradient_positive, gradient_negative);
    float luma_edge = gradient_positive > gradient_negative ? luma_positive : luma_negative;

    float luma_end_positive = 0.0;
    float luma_end_negative = 0.0;
    bool done_positive = false;
    bool done_negative = false;

    for (int i = 0; i < FXAA_EDGE_SEARCH_STEPS; i++) {
        if (!done_positive) {
            distance_positive += distance_offset;
            luma_end_positive = getLuma(getPixel(distance_positive));
            done_positive = abs(luma_end_positive - luma_edge) >= gradient;
        }
        if (!done_negative) {
            distance_negative -= distance_offset;
            luma_end_negative = getLuma(getPixel(distance_negative));
            done_negative = abs(luma_end_negative - luma_edge) >= gradient;
        }

        if (done_positive && done_negative) { break; }
    }

    float length_positive = length(distance_positive);
    float length_negative = length(distance_negative);
    float offset = abs((length_positive - length_negative) 
        / (length_positive + length_negative));

    #ifdef FXAA_DBG_EDGE_DISTANCE
        return vec3(offset, 0.0, 0.0);
    #endif
    
    float bias = length_positive > length_negative ? offset : -offset;
    vec3 blurred = (
        getPixel( FXAA_BLUR_CLOSE_DIST * distance_offset) +
        getPixel( FXAA_BLUR_FAR_DIST   * distance_offset) +
        getPixel(-FXAA_BLUR_CLOSE_DIST * distance_offset) +
        getPixel(-FXAA_BLUR_FAR_DIST   * distance_offset) +
        getPixel( FXAA_BLUR_CLOSE_DIST * distance_offset * bias)
    ) / 5.0;

    return mix(rgb_m, blurred, sqrt(offset));
}

void main() {
    fragColor = vec4(fxaa(), 1.0);
}