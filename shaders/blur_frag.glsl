#version 460 core
in  vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTex;
uniform vec2      uDir;        /* (1/w, 0) horizontal pass,  (0, 1/h) vertical */
uniform float     uBlurRadius; /* pixel offset per tap                          */
uniform float     uAlpha;      /* output alpha multiplier — 1.0 for blur passes,
                                  <1.0 for the final composite attenuation       */

void main() {
    float weights[9] = float[](
        0.0625, 0.0938, 0.1563, 0.1875, 0.2188,
        0.1875, 0.1563, 0.0938, 0.0625
    );
    float total = 0.9375; /* pre-summed exact value */

    vec4 colour = vec4(0.0);
    for (int i = 0; i < 9; i++) {
        float offset = (float(i) - 4.0) * uBlurRadius;
        colour += texture(uTex, vUV + uDir * offset) * (weights[i] / total);
    }
    /* Apply alpha attenuation — used by the composite pass to dim the
       reflection without affecting the two-pass blur quality.               */
    FragColor = vec4(colour.rgb, colour.a * uAlpha);
}
