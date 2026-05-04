#version 460 core
layout(location = 0) in vec3 aPos;

out vec3 vTexCoord;

uniform mat4 uView;
uniform mat4 uProj;

void main() {
    /* Flip X and Z to rotate skybox 180° around Y so bright side
       matches the moon position.                                   */
    vTexCoord   = vec3(-aPos.x, aPos.y, -aPos.z);
    vec4 pos    = uProj * uView * vec4(aPos, 1.0);
    gl_Position = pos.xyww;
}
