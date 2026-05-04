#version 460 core
in  vec3 vTexCoord;
out vec4 FragColor;

uniform samplerCube uSkybox;
uniform float       uBrightness;   /* g_brightness from main */

void main() {
    vec4 tex   = texture(uSkybox, vTexCoord);
    FragColor  = vec4(tex.rgb * uBrightness, 1.0);
}
