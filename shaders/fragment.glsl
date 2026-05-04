#version 460 core

in vec3 vFragPos;
in vec3 vNormal;

out vec4 FragColor;

/* ── Material / scene ───────────────────────────────────────────────────── */
uniform vec3  objectColor;
uniform vec3  emissive;
uniform float ambientStr;

/* ── Lighting ────────────────────────────────────────────────────────────── */
uniform vec3  lightPos;
uniform vec3  lightColor;
uniform vec3  viewPos;

/* ── Specular (Blinn-Phong) ─────────────────────────────────────────────── */
uniform float uSpecularStr;
uniform float uShininess;

/* ── Fog ─────────────────────────────────────────────────────────────────── */
uniform vec3  fogColor;
uniform float fogDensity;

/* ── Transparency ────────────────────────────────────────────────────────── */
uniform float uAlphaTop;
uniform float uAlphaSide;
uniform float uAlphaMult;

/* ── Normal flipping ────────────────────────────────────────────────────── */
uniform float uNormalYFlip;

/* ── Two-sided lighting ──────────────────────────────────────────────────── */
uniform float uTwoSided;

/* ── Hemisphere (GI-style) ambient ─────────────────────────────────────────
   Simulates bounced skylight so surfaces in shadow are not pitch black.
   Sky and ground colours are intentionally dark to fit the Tron night scene.
   uHemiStr = 0 disables (default), 1 = full hemisphere wrap.               */
uniform float uHemiStr;
uniform vec3  uSkyColor;      /* colour of light from above */
uniform vec3  uGroundColor;   /* colour of light from below */

void main() {
    vec3  raw    = vNormal;
    raw.y       *= uNormalYFlip;
    vec3  norm   = normalize(raw);

    vec3  lightDir = normalize(lightPos - vFragPos);
    vec3  viewDir  = normalize(viewPos  - vFragPos);

    /* ── Hemisphere ambient (GI wrap) ──────────────────────────────────── */
    /* Blend from groundColor (norm.y=-1) to skyColor (norm.y=+1) */
    float hemi      = norm.y * 0.5 + 0.5;
    vec3  hemiLight = mix(uGroundColor, uSkyColor, hemi) * uHemiStr;

    /* ── Moon / directional ambient ──────────────────────────────────────── */
    vec3 ambient  = (ambientStr * lightColor + hemiLight) * objectColor;

    /* ── Diffuse ─────────────────────────────────────────────────────────── */
    float rawDiff = dot(norm, lightDir);
    float diff    = (uTwoSided > 0.5) ? abs(rawDiff) : max(rawDiff, 0.0);
    vec3  diffuse = diff * lightColor * objectColor;

    /* ── Specular ────────────────────────────────────────────────────────── */
    vec3  effNorm = (uTwoSided > 0.5 && rawDiff < 0.0) ? -norm : norm;
    vec3  halfVec = normalize(lightDir + viewDir);
    float spec    = (uSpecularStr > 0.001)
                  ? pow(max(dot(effNorm, halfVec), 0.0), uShininess)
                  : 0.0;
    vec3  specular = uSpecularStr * spec * lightColor;

    vec3 result = ambient + diffuse + specular + emissive;

    /* ── Fog ─────────────────────────────────────────────────────────────── */
    float d         = length(viewPos - vFragPos);
    float fogFactor = clamp(1.0 - exp(-(fogDensity*d)*(fogDensity*d)), 0.0, 1.0);
    result          = mix(result, fogColor, fogFactor);

    /* ── Alpha ───────────────────────────────────────────────────────────── */
    float yFactor = clamp(abs(norm.y), 0.0, 1.0);
    float alpha   = mix(uAlphaSide, uAlphaTop, yFactor) * uAlphaMult;

    FragColor = vec4(result, alpha);
}
