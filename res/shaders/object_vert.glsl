#ifdef GL_ES
precision mediump float;
#endif

#if __VERSION__ >= 140
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 inNormal;
#endif

uniform mat4 rotate;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 fragNormal;
out vec3 fragPos;

void main()
{
    gl_Position = projection*view*model*vec4(position, 1.0f);

    fragPos = vec3 (model*vec4(position, 1.0f));
    fragNormal = normalize(vec3 (rotate*vec4(inNormal, 1.0f)));
}
