#version 460 core
out vec4 FragColor;

in vec3 Normal;
in vec2 TexCoords;

uniform sampler2D texture_diffuse1;
uniform vec4 baseColor;

void main()
{
    vec3 lightDir = normalize(vec3(-0.3, 0.6, 1.0));
    float diff = max(dot(normalize(Normal), lightDir), 0.0);

    vec4 texColor = texture(texture_diffuse1, TexCoords) * baseColor;
    vec3 result = texColor.rgb * (0.65 + 0.35 * diff);

    FragColor = vec4(result, 1.0);
}
