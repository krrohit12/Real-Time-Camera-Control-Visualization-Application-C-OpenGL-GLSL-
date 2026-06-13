#version 410 core

in  vec2 vTexCoord;
out vec4 fragColor;

uniform sampler2D uTexture;
uniform vec2      uTexelSize;   // (1/width, 1/height)
uniform float     uEdgeStrength;

// 3x3 Sobel operator
void main() {
    float dx = uTexelSize.x;
    float dy = uTexelSize.y;

    // Sample 3x3 neighbourhood — luminance only
    float tl = dot(texture(uTexture, vTexCoord + vec2(-dx,  dy)).rgb, vec3(0.299,0.587,0.114));
    float tc = dot(texture(uTexture, vTexCoord + vec2(  0,  dy)).rgb, vec3(0.299,0.587,0.114));
    float tr = dot(texture(uTexture, vTexCoord + vec2( dx,  dy)).rgb, vec3(0.299,0.587,0.114));
    float ml = dot(texture(uTexture, vTexCoord + vec2(-dx,   0)).rgb, vec3(0.299,0.587,0.114));
    float mr = dot(texture(uTexture, vTexCoord + vec2( dx,   0)).rgb, vec3(0.299,0.587,0.114));
    float bl = dot(texture(uTexture, vTexCoord + vec2(-dx, -dy)).rgb, vec3(0.299,0.587,0.114));
    float bc = dot(texture(uTexture, vTexCoord + vec2(  0, -dy)).rgb, vec3(0.299,0.587,0.114));
    float br = dot(texture(uTexture, vTexCoord + vec2( dx, -dy)).rgb, vec3(0.299,0.587,0.114));

    float gx = -tl - 2.0*ml - bl + tr + 2.0*mr + br;
    float gy = -tl - 2.0*tc - tr + bl + 2.0*bc + br;

    float edge = clamp(sqrt(gx*gx + gy*gy) * uEdgeStrength, 0.0, 1.0);
    fragColor  = vec4(edge, edge, edge, 1.0);
}
