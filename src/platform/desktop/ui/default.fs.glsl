#version 330 core

// Inputs the texture coordinates from the Vertex Shader
in vec2 texCoord;

// Outputs colors in RGBA
out vec4 FragColor;

// Gets the Texture Unit from the main function
uniform sampler2D tex0;

void main() {
    FragColor = texture(tex0, texCoord);
}
