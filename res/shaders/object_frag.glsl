#ifdef GL_ES
precision mediump float;
#endif

out vec4 color;
uniform vec3 objectColor;
uniform vec3 lightColor;

uniform vec3 lightPos;

in vec3 fragNormal;
in vec3 fragPos;

void main()
{
    float ambientStrength = 0.2f;
    vec3 ambient = ambientStrength*lightColor;

    vec3 norm = normalize(fragNormal);
    vec3 lightDir = normalize(lightPos - fragPos);

    float diff = max(dot(fragNormal, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    vec3 result = (lightColor*0.8f + diffuse*0.55f)*objectColor;

    color = vec4(result, 1.0f);
}

