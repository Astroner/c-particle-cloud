
varying vec2 uv;

void main() {
    vec2 normalized = (uv * vec2(1., -1.) + 1.) / 2.;

    float light = (.5 - (normalized.x + normalized.y)) / 4.;

    float color = .4 + light;

    gl_FragColor = vec4(color, color, color, 1.0);
}
