#version 330 core
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in vec3 LightPos;

uniform vec4 objectColor;
uniform vec3 lightColor;
uniform vec3 viewPos;
uniform bool selected;

out vec4 FragColor;

void main() {
    // Ambient lighting
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * lightColor;
    
    // Diffuse lighting
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(LightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // Specular lighting
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;
    
    // Combine lighting components
    vec3 result = (ambient + diffuse + specular) * objectColor.rgb;
    
    // Selection highlight
    if (selected) {
        result = mix(result, vec3(1.0, 1.0, 0.0), 0.3);
    }
    
    FragColor = vec4(result, objectColor.a);
}