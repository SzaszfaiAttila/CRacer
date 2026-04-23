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
uniform float uSpecularStr;   /* 0.0 = matte, 0.85+ = plastic-glass         */
uniform float uShininess;     /* higher = tighter highlight (32-256)         */

/* ── Fog ─────────────────────────────────────────────────────────────────── */
uniform vec3  fogColor;
uniform float fogDensity;

/* ── Transparency ────────────────────────────────────────────────────────── */
uniform float uAlphaTop;      /* alpha for upward-facing faces  (default 1)  */
uniform float uAlphaSide;     /* alpha for side-facing faces    (default 1)  */
uniform float uAlphaMult;     /* global alpha multiplier        (default 1)  */

/* ── Normal flipping for Y-mirrored geometry ────────────────────────────── */
uniform float uNormalYFlip;   /* +1.0 normal, -1.0 reflected  (default +1)  */

/* ── Two-sided lighting ──────────────────────────────────────────────────── */
/* Set to 1.0 to use abs(diff) so both polygon faces receive diffuse light.
   This corrects OBJ models whose normals are exported pointing inward.       */
uniform float uTwoSided;      /* 0.0 = one-sided, 1.0 = two-sided (default 0) */

void main() {
    vec3  raw    = vNormal;
    raw.y       *= uNormalYFlip;
    vec3  norm   = normalize(raw);

    vec3  lightDir = normalize(lightPos - vFragPos);
    vec3  viewDir  = normalize(viewPos  - vFragPos);

    /* Ambient */
    vec3 ambient  = ambientStr * lightColor * objectColor;

    /* Diffuse — two-sided mode uses abs() so inverted normals still light up */
    float rawDiff = dot(norm, lightDir);
    float diff    = (uTwoSided > 0.5) ? abs(rawDiff) : max(rawDiff, 0.0);
    vec3  diffuse = diff * lightColor * objectColor;

    /* Specular — Blinn-Phong half-vector
       For two-sided: negate norm when polygon faces away so highlight is
       on the correct side.                                                    */
    vec3 effNorm  = (uTwoSided > 0.5 && rawDiff < 0.0) ? -norm : norm;
    vec3  halfVec = normalize(lightDir + viewDir);
    float spec    = (uSpecularStr > 0.001)
                  ? pow(max(dot(effNorm, halfVec), 0.0), uShininess)
                  : 0.0;
    vec3  specular = uSpecularStr * spec * lightColor;

    vec3 result = ambient + diffuse + specular + emissive;

    /* Exponential-squared fog */
    float d         = length(viewPos - vFragPos);
    float fogFactor = clamp(1.0 - exp(-(fogDensity * d) * (fogDensity * d)), 0.0, 1.0);
    result          = mix(result, fogColor, fogFactor);

    /* Normal-driven alpha × global multiplier */
    float yFactor = clamp(abs(norm.y), 0.0, 1.0);
    float alpha   = mix(uAlphaSide, uAlphaTop, yFactor) * uAlphaMult;

    FragColor = vec4(result, alpha);
}
