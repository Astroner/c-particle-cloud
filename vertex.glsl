attribute vec2 pos;

uniform vec2 window;

void main() {
    vec2 normalizedPosition = ((pos * 2.0 / window) - 1.0) * vec2(1.0, -1.0);

    gl_Position = vec4(normalizedPosition, 1.0, 1.0);
}
