#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in ivec4 aBoneIDs;
layout (location = 4) in vec4 aWeights;

out vec3 Normal;
out vec2 TexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

const int MAX_BONES = 100;
uniform mat4 bones[MAX_BONES];
uniform bool skinned;

void main()
{
    vec4 position = vec4(aPos, 1.0);
    vec3 normal = aNormal;

    float total = aWeights.x + aWeights.y + aWeights.z + aWeights.w;
    if (skinned && total > 0.0)
    {
        mat4 skin = aWeights.x * bones[aBoneIDs.x]
                  + aWeights.y * bones[aBoneIDs.y]
                  + aWeights.z * bones[aBoneIDs.z]
                  + aWeights.w * bones[aBoneIDs.w];
        position = skin * position;
        normal = mat3(skin) * normal;
    }

    Normal = mat3(transpose(inverse(model))) * normal;
    TexCoords = aTexCoords;

    gl_Position = projection * view * model * position;
}
