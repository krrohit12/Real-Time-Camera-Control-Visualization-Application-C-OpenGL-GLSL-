#version 410 core

in  vec2 vTexCoord;
out vec4 fragColor;

uniform sampler2D uTexture;
uniform float     uBrightness;  // -1.0 .. 1.0, default 0.0
uniform float     uContrast;    //  0.0 .. 3.0, default 1.0

void main() {
    vec4 color = texture(uTexture, vTexCoord);

    // Apply contrast around mid-grey (0.5)
    vec3 result = (color.rgb - 0.5) * uContrast + 0.5;

    // Apply brightness offset
    result += uBrightness;

    fragColor = vec4(clamp(result, 0.0, 1.0), 1.0);
}
