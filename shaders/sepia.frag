#version 410 core

in  vec2 vTexCoord;
out vec4 fragColor;

uniform sampler2D uTexture;

void main() {
    vec4 color = texture(uTexture, vTexCoord);
    float r = color.r, g = color.g, b = color.b;

    float outR = clamp(r * 0.393 + g * 0.769 + b * 0.189, 0.0, 1.0);
    float outG = clamp(r * 0.349 + g * 0.686 + b * 0.168, 0.0, 1.0);
    float outB = clamp(r * 0.272 + g * 0.534 + b * 0.131, 0.0, 1.0);

    fragColor = vec4(outR, outG, outB, 1.0);
}
