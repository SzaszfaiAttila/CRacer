#version 460 core

in  vec3 FragPos;
in  vec3 Normal;
out vec4 FragColor;

uniform vec3  viewPos;
uniform vec3  moonPos;     // world position of moon (used as distant light)
uniform vec3  fogColor;
uniform float fogDensity;
uniform float time;

void main() {
    vec3 norm    = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    // Fresnel blend: looking straight down = deep color, grazing = surface color
    float fresnel     = pow(1.0 - max(dot(norm, viewDir), 0.0), 3.0);

    vec3 deepColor    = vec3(0.005, 0.018, 0.055);  // very deep dark blue
    vec3 surfaceColor = vec3(0.02,  0.055, 0.12 );  // slightly lighter teal-navy
    vec3 waterColor   = mix(deepColor, surfaceColor, fresnel * 0.6);

    // Moon specular — tight Blinn-Phong highlight
    vec3  moonDir  = normalize(moonPos - FragPos);
    vec3  halfVec  = normalize(moonDir + viewDir);
    float spec     = pow(max(dot(norm, halfVec), 0.0), 220.0);
    vec3  moonCol  = vec3(0.88, 0.92, 0.78); // warm moon white
    vec3  specular = spec * moonCol * 1.4;

    // Subtle moon diffuse (moonlight is very dim)
    float diff    = max(dot(norm, moonDir), 0.0) * 0.07;
    vec3  diffuse = diff * moonCol;

    // Wave crest shimmer — slightly lighter where waves peak
    float crest  = smoothstep(-0.3, 1.2, FragPos.y);
    waterColor  += vec3(0.015, 0.03, 0.04) * crest;

    // Tiny animated sparkle on crests
    float sparkle = pow(max(dot(norm, normalize(viewDir + moonDir * 0.5)), 0.0), 400.0);
    waterColor += sparkle * moonCol * 0.6;

    vec3 color = waterColor + diffuse + specular;

    // Fog
    float dist      = length(viewPos - FragPos);
    float fogFactor = clamp(exp(-fogDensity * dist), 0.0, 1.0);
    color           = mix(fogColor, color, fogFactor);

    FragColor = vec4(color, 1.0);
}
