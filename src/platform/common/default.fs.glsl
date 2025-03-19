#version 300 es

precision mediump float;

// Inputs the texture coordinates from the Vertex Shader
in vec2 tex_coord;

// Outputs colors in RGBA
out vec4 color;

// Gets the Texture Unit from the main function
uniform sampler2D tex0;

const uint COLOR_CORRECTION_NONE = 0u;
const uint COLOR_CORRECTION_CGB = 1u;

uniform uint color_correction;

void main() {
    vec4 temp_color = texture(tex0, tex_coord);

    switch (color_correction) {
    case COLOR_CORRECTION_NONE:
        break;
    case COLOR_CORRECTION_CGB:
        temp_color.rgb *= 255.0;

        int r = int(temp_color.r);
        int g = int(temp_color.g);
        int b = int(temp_color.b);

        temp_color.rgb = vec3(
            float((r * 13 + g * 2 + b) >> 4),
            float((g *  3 + b) >> 2),
            float((r *  3 + g * 2 + b * 11) >> 4)
        );

        temp_color.rgb /= 255.0;
        break;
    default:
        break;
    }

    color = temp_color;
}
