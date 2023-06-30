

__kernel void compute(
    __global float* x,
    __global float* y,
    __global float* dx,
    __global float* dy,
    int count,
    float minSpeed,
    float slowBy,
    float width,
    float height,
    float clickRadius,
    float mouseX, 
    float mouseY
) {
    int id = get_global_id(0);

    if(id < 0 || id >= count) return;
    
    if(x[id] <= 0 || x[id] >= width) {
        dx[id] *= -1;
    }

    if(y[id] <= 0 || y[id] >= height) {
        dy[id] *= -1;
    }

    x[id] += dx[id];
    y[id] += dy[id];

    if(dx[id] > minSpeed) {
        float diff = dx[id] - minSpeed;
        if(diff < slowBy) {
            dx[id] -= diff;
        } else {
            dx[id] -= slowBy;
        }
    }

    if(dy[id] > minSpeed) {
        float diff = dy[id] - minSpeed;
        if(diff < slowBy) {
            dy[id] -= diff;
        } else {
            dy[id] -= slowBy;
        }
    }

    if(mouseX >= 0) {
        float vx = x[id] - mouseX;
        float vy = y[id] - mouseY;

        if(
            vx * vx
            + vy * vy
            <= clickRadius * clickRadius
        ) {
            float len = sqrt(vx * vx + vy * vy);
            dx[id] = ((x[id] - mouseX) * 5 / len) * (1.3 - len / clickRadius);
            dy[id] = ((y[id] - mouseY) * 5 / len) * (1.3 - len / clickRadius);
        }
    }
}