
varying vec2 uv;

void main() {
    gl_FragColor = vec4((uv + 1.) / 2., 1.0, 1.0);
}
