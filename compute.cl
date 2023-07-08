

__kernel void compute(
    __global float2* pos,
    __global float2* speed,
    int count,
    float screenRatio,
    float clickRadius,
    float2 mouse
) {
    int id = get_global_id(0);

    if(id < 0 || id >= count) return;

    if(pos[id].x <= -1 || pos[id].x >= 1) {
        speed[id].x *= -1.f;
    }

    if(pos[id].y <= -1 || pos[id].y >= 1) {
        speed[id].y *= -1.f;
    }

    pos[id] += speed[id];

    if(mouse.x >= -1) {
        float2 screenPos = pos[id];

        screenPos.y /= screenRatio;
        mouse.y /= screenRatio;

        float distance = length(screenPos - mouse);

        if(distance < clickRadius * 2) {
            speed[id] = ((screenPos - mouse) * 0.005f / distance) * (1.3f - distance / clickRadius / 2);
            speed[id].y *= screenRatio;
        }
    }
}