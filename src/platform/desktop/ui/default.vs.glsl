#version 330 core

// Positions/Coordinates
layout (location = 0) in vec3 aPos;
// Texture Coordinates
layout (location = 1) in vec2 aTex;

// Outputs the texture coordinates to the fragment shader
out vec2 texCoord;

void main() {
    // Outputs the positions/coordinates of all vertices
    gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);
    // Assigns the texture coordinates from the Vertex Data to "texCoord"
    texCoord = aTex;
}
