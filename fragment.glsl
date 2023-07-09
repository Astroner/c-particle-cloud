
varying vec2 uv;

void main() {
    vec2 normalized = (uv * vec2(1., -1.) + 1.) / 2.;

    float ratio = (.5 - (normalized.x + normalized.y)) * 1.001 / 4.;

    gl_FragColor = vec4(.4 + ratio, .4 + ratio, .4 + ratio, 1.0);
}
