#version 460 core

in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform vec3  lightPos;
uniform vec3  lightColor;
uniform vec3  objectColor;
uniform vec3  viewPos;
uniform vec3  emissive;
uniform float ambientStr;
uniform vec3  fogColor;
uniform float fogDensity;

void main() {
    // Phong
    vec3 ambient    = ambientStr * lightColor;
    vec3 norm       = normalize(Normal);
    vec3 lightDir   = normalize(lightPos - FragPos);
    float diff      = max(dot(norm, lightDir), 0.0);
    vec3 diffuse    = diff * lightColor;
    vec3 viewDir    = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec      = pow(max(dot(viewDir, reflectDir), 0.0), 64.0);
    vec3 specular   = 0.6 * spec * lightColor;

    vec3 result = (ambient + diffuse + specular) * objectColor + emissive;

    // Exponential atmospheric fog
    float dist      = length(viewPos - FragPos);
    float fogFactor = clamp(exp(-fogDensity * dist), 0.0, 1.0);
    result          = mix(fogColor, result, fogFactor);

    FragColor = vec4(result, 1.0);
}
