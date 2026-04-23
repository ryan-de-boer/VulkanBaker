#version 450

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 fragColor;

layout(push_constant) uniform PushConstants {
    int shapeType; // 0 for quad, 1 for triangle
    int flipV;
} pc;

void main() {
    // Full screen quad
    vec2 positions[6] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 1.0, -1.0),
        vec2( 1.0,  1.0),
        vec2(-1.0, -1.0),
        vec2( 1.0,  1.0),
        vec2(-1.0,  1.0)
    );
    
    vec2 texCoords[6] = vec2[](
        vec2(0.0, 1.0),
        vec2(1.0, 1.0),
        vec2(1.0, 0.0),
        vec2(0.0, 1.0),
        vec2(1.0, 0.0),
        vec2(0.0, 0.0)
    );

    // triangle
vec2 positionsT[3] = vec2[](
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5)
);

vec2 uvsT[3] = vec2[](
    vec2(0.8, 0.0),
    vec2(0.8, 1.0),
    vec2(0.0, 0.6)
);

vec3 colorsT[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);        

    if (pc.shapeType == 0) {
        // ... logic for quad 6 vertices ...
        gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
        fragColor = vec3(1.0, 1.0, 1.0);
        fragTexCoord = texCoords[gl_VertexIndex];
    } else {
        // ... logic for tri 3 vertices ...

        gl_Position = vec4(positionsT[gl_VertexIndex], 0.0, 1.0);
        fragColor = colorsT[gl_VertexIndex];
        fragTexCoord = uvsT[gl_VertexIndex];
        if (pc.flipV==1) {
            fragTexCoord.y = 1.0-fragTexCoord.y;
        }

    }

}