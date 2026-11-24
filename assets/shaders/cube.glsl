@vs vs
layout(binding=0) uniform vs_params {
    mat4 mvp;
    vec4 tint;   // rgb = overlay colour, a = blend strength (0 = no change)
};

in vec3 position;
in vec4 color0;

out vec4 color;

void main() {
    color = mix(color0, vec4(tint.rgb, 1.0), tint.a);
    gl_Position = mvp * vec4(position, 1.0);
}
@end

@fs fs
in vec4 color;
out vec4 frag_color;

void main() {
    frag_color = color;
}
@end

@program cube vs fs