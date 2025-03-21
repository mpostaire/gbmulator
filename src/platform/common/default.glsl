#version 300 es

precision mediump float;

// Inputs the texture coordinates from the Vertex Shader
in vec2 tex_coord;

// Outputs colors in RGBA
out vec4 color;

// Gets the Texture Unit from the main function
uniform sampler2D tex0;

void main() {
    color = texture(tex0, tex_coord);
}
