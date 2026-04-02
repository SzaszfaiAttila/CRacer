#version 460 core

layout(location = 0) in vec3 aPos; // flat grid position (y=0)
// location 1 (normal from mesh) is intentionally unused — we derive analytically

uniform mat4  view;
uniform mat4  projection;
uniform float time;

out vec3 FragPos;
out vec3 Normal;

// ── Gerstner wave parameters: (dirX, dirZ, steepness, wavelength) ─────────────
// Steepness Q: 0 = pure sine, 1 = full Gerstner. Amplitude a = steepness/k.
const int   N_WAVES = 4;
const vec4  waves[4] = vec4[](
    vec4( 0.60,  1.00,  0.25, 30.0),  // main swell
    vec4( 1.00,  0.35,  0.20, 20.0),  // cross-swell
    vec4(-0.50,  1.00,  0.15, 12.0),  // secondary
    vec4( 0.80, -0.60,  0.10,  7.0)   // high-freq chop
);

// Returns positional offset for one Gerstner wave,
// and accumulates tangent/binormal for normal derivation.
vec3 gerstner(vec4 w, vec3 p, float t, inout vec3 tang, inout vec3 binorm) {
    float Q   = w.z;
    float L   = w.w;
    float k   = 6.28318 / L;
    float c   = sqrt(9.81 / k);
    float a   = Q / k;
    vec2  d   = normalize(w.xy);
    float f   = k * (dot(d, p.xz) - c * t);
    float sf  = sin(f), cf = cos(f);

    tang   += vec3(-d.x*d.x*(Q*sf),  d.x*(Q*cf), -d.x*d.y*(Q*sf));
    binorm += vec3(-d.x*d.y*(Q*sf),  d.y*(Q*cf), -d.y*d.y*(Q*sf));

    return vec3(d.x*(a*cf), a*sf, d.y*(a*cf));
}

void main() {
    vec3 tang   = vec3(1.0, 0.0, 0.0);
    vec3 binorm = vec3(0.0, 0.0, 1.0);
    vec3 pos    = aPos;

    for (int i = 0; i < N_WAVES; i++)
        pos += gerstner(waves[i], aPos, time, tang, binorm);

    Normal  = normalize(cross(binorm, tang));
    FragPos = pos;

    gl_Position = projection * view * vec4(pos, 1.0);
}
