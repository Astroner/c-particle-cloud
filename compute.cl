

__kernel void compute(
    __global float2* pos,
    __global float2* speed,
    int count,
    float2 window,
    float clickRadius,
    float2 mouse
) {
    int id = get_global_id(0);

    if(id < 0 || id >= count) return;

    if(pos[id].x <= 0 || pos[id].x >= window.x) {
        speed[id].x *= -1.f;
    }

    if(pos[id].y <= 0 || pos[id].y >= window.y) {
        speed[id].y *= -1.f;
    }

    pos[id] += speed[id];

    if(mouse.x >= 0) {
        float distance = length(pos[id] - mouse);

        if(distance < clickRadius) {
            speed[id] = ((pos[id] - mouse) * 5.0f / distance) * (1.3f - distance / clickRadius);
        }
    }
}