#version 410 core

in  vec2 vTexCoord;
out vec4 fragColor;

uniform sampler2D uTexture;

void main() {
    vec4 color = texture(uTexture, vTexCoord);
    // Perceptual luminance weights (ITU-R BT.601)
    float luma = dot(color.rgb, vec3(0.299, 0.587, 0.114));
    fragColor   = vec4(luma, luma, luma, 1.0);
}
