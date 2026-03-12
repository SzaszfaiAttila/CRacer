#version 460 core

in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 objectColor;
uniform vec3 viewPos;
uniform vec3 emissive;      // additive glow — set to vec3(0) for normal objects
uniform float ambientStr;   // per-object ambient override (default 0.15)

void main() {
    // Ambient
    vec3 ambient = ambientStr * lightColor;

    // Diffuse
    vec3 norm     = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff    = max(dot(norm, lightDir), 0.0);
    vec3 diffuse  = diff * lightColor;

    // Specular
    vec3 viewDir    = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec      = pow(max(dot(viewDir, reflectDir), 0.0), 64);
    vec3 specular   = 0.6 * spec * lightColor;

    vec3 result = (ambient + diffuse + specular) * objectColor + emissive;
    FragColor   = vec4(result, 1.0);
}
