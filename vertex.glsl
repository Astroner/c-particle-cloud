attribute float posX;
attribute float posY;

void main() {
    gl_Position = vec4(posX * 2.0 / 1280.0 - 1.0, (posY * 2.0 / 720.0 - 1.0) * -1.0, 1.0, 1.0);
}
