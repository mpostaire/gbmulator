#version 330 core

// Inputs the texture coordinates from the Vertex Shader
in vec2 texCoord;

// Outputs colors in RGBA
out vec4 FragColor;

// Gets the Texture Unit from the main function
uniform sampler2D tex0;

const uint COLOR_CORRECTION_NONE = 0u;
const uint COLOR_CORRECTION_CGB = 1u;
const uint COLOR_CORRECTION_GBA = 2u;

uniform uint color_correction = COLOR_CORRECTION_NONE;

void main() {
    vec4 color = texture(tex0, texCoord);

    switch (color_correction) {
    case COLOR_CORRECTION_NONE:
        break;
    case COLOR_CORRECTION_CGB:
        color.rgb *= 255.0;

        int r = int(color.r);
        int g = int(color.g);
        int b = int(color.b);

        color.rgb = vec3(
            float((r * 13 + g * 2 + b) >> 4),
            float((g *  3 + b) >> 2),
            float((r *  3 + g * 2 + b * 11) >> 4)
        );

        color.rgb /= 255.0;
        break;
    case COLOR_CORRECTION_GBA:
        // TODO
        break;
    default:
        break;
    }

    FragColor = color;
}
