#version 410 core

in  vec2 vTexCoord;
out vec4 fragColor;

uniform sampler2D uTexture;
uniform vec2      uTexelSize;
uniform int       uBlurRadius;  // 1..10

// Simple box blur — intentionally naive for Phase 1 baseline
void main() {
    vec4  sum   = vec4(0.0);
    int   count = 0;
    int   r     = clamp(uBlurRadius, 1, 10);

    for (int x = -r; x <= r; ++x) {
        for (int y = -r; y <= r; ++y) {
            sum   += texture(uTexture, vTexCoord + vec2(x, y) * uTexelSize);
            count += 1;
        }
    }
    fragColor = sum / float(count);
}
