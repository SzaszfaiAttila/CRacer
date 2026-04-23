#version 460 core
in  vec2 vUV;
out vec4 FragColor;

uniform sampler2D uFont;
uniform vec4  uColor;
uniform int   uMode;   /* 0 = filled rect,  1 = font glyph */

void main() {
    if (uMode == 1) {
        float a = texture(uFont, vUV).r;
        if (a < 0.02) discard;
        FragColor = vec4(uColor.rgb, uColor.a * a);
    } else {
        FragColor = uColor;
    }
}
