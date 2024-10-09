#include <allegro.h>
#include <stdbool.h>
#include <math.h>


// scaling
float scale;
float simWidth;
float simHeight;
float sX(float x) {
    return x * scale;
}
float sY(float y) {
    return SCREEN_H - (y * scale);
}


typedef struct {
    float x;
    float y;
} Vector;

typedef struct {
    Vector a;
    Vector b;
} LineSegment;

// in-game objects
typedef struct {
    Vector position;
    Vector velocity;
    float radius;
    int color;
} Ball;

typedef struct {
    Vector position;
    float radius;
    float pushStrength;
} Bouncer;
typedef struct {
    Vector position;
    float radius;
    float length;
    float restAngle;
    float activeAngle;
    bool sign;
    float angularVelocity;
    // non-constants
    float currentAngle;
    float currentAngularVelocity;
    float touchIdentifier;
} Flipper;


// physics scene
bool paused = false;
float gravity = -20.0f;
float flipperHeight = 1.7f;

Ball ball;
Bouncer bouncers[4];
Flipper flippers[2];
float margin = 0.02;
Vector border[8];


// general util functions
float clamp(float n, float start, float end) {
    return MAX(start, MIN(end, n));
}

// vector & point functions
Vector subtractVectors(Vector a, Vector b) {
    a.x -= b.x;
    a.y -= b.y;
    return a;
}
Vector addVectors(Vector a, Vector b) {
    a.x += b.x;
    a.y += b.y;
    return a;
}
Vector scaleVector(Vector v, float scale) {
    v.x *= scale;
    v.y *= scale;
    return v;
}

// builtin allegro function is for 3d
float dotProduct(Vector a, Vector b) {
    return (a.x * b.x) + (a.y * b.y);
}

Vector closestPointOnLineSegment(Vector point, LineSegment line) {
    Vector segmentVector = subtractVectors(line.b, line.a);
    float segmentLengthSquared = dotProduct(segmentVector, segmentVector);

    // if it's just a point, return the point
    if (segmentLengthSquared == 0) {
        return line.a;
    }

    float distAlongLine = clamp((dotProduct(point, segmentVector) - dotProduct(line.a, segmentVector)) / segmentLengthSquared, 0, 1);
    return addVectors(line.a, scaleVector(segmentVector, distAlongLine));
}

// feature-specific functions
void updateBall(Ball* b, float dt) {
    b->velocity.x += gravity * dt;
    b->position = addVectors(b->position, scaleVector(b->velocity, dt));
}

void updateFlipper(Flipper* flipper, float dt, bool pressed) {
    float oldAngle = flipper->currentAngle;
    if (pressed) {
        flipper->currentAngle = MIN(flipper->currentAngle + (flipper->currentAngularVelocity * dt), flipper->activeAngle);
    } else {
        flipper->currentAngle = MAX(flipper->currentAngle - (flipper->currentAngularVelocity * dt), flipper->restAngle);
    }
    flipper->currentAngularVelocity = flipper->sign * (flipper->currentAngle - oldAngle) / dt;
}

void rotate_translate(float x, float y, float cx, float cy, float angle, float *rx, float *ry) {
    float cos_angle = cos(angle);
    float sin_angle = sin(angle);
    float tx = x * cos_angle - y * sin_angle;
    float ty = x * sin_angle + y * cos_angle;
    *rx = sX(cx + tx);
    *ry = sY(cy + ty);
}

void render(BITMAP *buffer) {
    // draw ball
    circlefill(buffer, sX(ball.position.x), sY(ball.position.y), ball.radius, ball.color);

    // draw border
    // for (int i = 0; i < 7; i++) {
    //     line(buffer, 
    //         sX(border[i].x), sY(border[i].y),
    //         sX(border[i+1].x), sY(border[i+1].y),
    //         makecol(0, 0, 0));
    // }
    // // the last line connecting the end to the start
    // line(buffer,
    //     sX(border[7].x), sY(border[7].y),
    //     sX(border[0].x), sY(border[0].y),
    //     makecol(0, 0, 0));
}

int main(int argc, const char **argv)
{
    BITMAP *buffer;
    int timer;

    // delta time
    double old_time = 0;
    double new_time = 0;
    double dt = 0;

    // initialize allegro.
    if (allegro_init() != 0) {
        return 1;
    }

    install_keyboard();
    install_timer();

    // 320x200 graphics mode
    if (set_gfx_mode(GFX_AUTODETECT, 320, 200, 0, 0) != 0) {
        set_gfx_mode(GFX_TEXT, 0, 0, 0, 0);
        allegro_message("Cannot set graphics mode:\r\n%s\r\n", allegro_error);
        return 1;
    }

    set_palette(desktop_palette);
    clear_to_color(screen, makecol(255, 255, 255));

    // set up scaling
    scale = MIN(SCREEN_W, SCREEN_H) / flipperHeight;
    simWidth = SCREEN_W / scale;
    simHeight = SCREEN_H / scale;

    // initialize physics scene
    ball = (Ball){
        .position = {simWidth / 2, simHeight / 2},
        .velocity = {5, 5},
        .radius = 4,
        .color = makecol(0, 0, 0)
    };
    Vector border[8] = {
        {0.74, 0.25},
        {1 - margin, 0.4},
        {1 - margin, flipperHeight - margin},
        {margin, flipperHeight - margin},
        {margin, 0.4},
        {.26, .25},
        {.26, margin},
        {.74, margin}
    };
    Flipper flippers[2] = {
        {
            .position = {0.26, 0.22},
            .radius = 0.03,
            .length = 0.2,
            .restAngle = 2.094395, // 120 degrees
            .activeAngle = 1,
            .sign = 1,
            .angularVelocity = 10,
            .touchIdentifier = 1,
            .currentAngularVelocity = 0,
            .currentAngle = 0,
        },
        {
            .position = {0.74, 0.22},
            .radius = 0.03,
            .length = 0.2,
            .restAngle = -2.094395, // -120 degrees
            .activeAngle = -1,
            .sign = 1,
            .angularVelocity = 10,
            .touchIdentifier = 1,
            .currentAngularVelocity = 0,
            .currentAngle = 0,
        },
    };
    Bouncer bouncers[4]  = {
        { .position = {0.25, 0.6}, .radius = 0.1, .pushStrength = 2 },
        { .position = {0.75, 0.5}, .radius = 0.12, .pushStrength = 2 },
        { .position = {0.7, 1.0}, .radius = 0.1, .pushStrength = 2 },
        { .position = {0.2, 1.2}, .radius = 0.1, .pushStrength = 2 }
    };
    
    buffer = create_bitmap(SCREEN_W, SCREEN_H);
    
    while (1) {

        new_time = (double)retrace_count / CLOCKS_PER_SEC;
        dt = new_time - old_time;
        old_time = new_time;

        clear_bitmap(buffer);
        render(buffer);

        ball.velocity.y += gravity * dt;
        ball.position.x += ball.velocity.x * dt;
        ball.position.y += ball.velocity.y * dt;

        if (ball.position.x < 0) {
            ball.position.x = 0;
            ball.velocity.x *= -1;
        }
        if (ball.position.x > simWidth) {
            ball.position.x = simWidth;
            ball.velocity.x *= -0.9;
        }
        if (ball.position.y < 0) {
            ball.position.y = 0;
            ball.velocity.y *= -0.9;
            ball.velocity.x *= 0.9;
        }
        // if (abs(ball.velocity.x) < 0.01) { ball.velocity.x = 0; }
        // if (abs(ball.velocity.y) < 0.01) { ball.velocity.y = 0; }

        // for some reason this doesn't work in another function
        for (int i = 0; i < 7; i++) {
            line(buffer, 
                sX(border[i].x), sY(border[i].y),
                sX(border[i+1].x), sY(border[i+1].y),
                makecol(0, 0, 0));
        }
        // the last line connecting the end to the start
        line(buffer,
            sX(border[7].x), sY(border[7].y),
            sX(border[0].x), sY(border[0].y),
            makecol(0, 0, 0));

        // draw flippers
        MATRIX_f transform;
        MATRIX_f temp;
        
        for (int i = 0; i < 2; i++) {  // Assuming you have 2 flippers
            Flipper flipper = flippers[i];

            float angle = -(flipper.restAngle + flipper.sign * flipper.currentAngle);

            // Draw the flipper body
            float x1, y1, x2, y2, x3, y3, x4, y4;
            rotate_translate(-flipper.radius, 0, flipper.position.x, flipper.position.y, angle, &x1, &y1);
            rotate_translate(-flipper.radius, flipper.length, flipper.position.x, flipper.position.y, angle, &x2, &y2);
            rotate_translate(flipper.radius, flipper.length, flipper.position.x, flipper.position.y, angle, &x3, &y3);
            rotate_translate(flipper.radius, 0, flipper.position.x, flipper.position.y, angle, &x4, &y4);

            int points[8] = {x1, y1, x2, y2, x3, y3, x4, y4};
            polygon(buffer, 4, points, makecol(255, 0, 0));

            // Draw the pivot circle
            float cx, cy;
            rotate_translate(0, 0, flipper.position.x, flipper.position.y, angle, &cx, &cy);
            circlefill(buffer, cx, cy, sX(flipper.radius), makecol(255, 0, 0));

            // Draw the end circle
            rotate_translate(0, flipper.length, flipper.position.x, flipper.position.y, angle, &cx, &cy);
            circlefill(buffer, cx, cy, sX(flipper.radius), makecol(255, 0, 0));
        }
        
        // draw bouncers
        for (int i = 0; i < 4; i++) {
            Bouncer bouncer = bouncers[i];
            circlefill(buffer, sX(bouncer.position.x), sY(bouncer.position.y), sX(bouncer.radius), makecol(0,0,255));
        }

        vsync();
        blit(buffer, screen, 0, 0, 0, 0, SCREEN_W, SCREEN_H);

        if (keyboard_needs_poll()) {
            poll_keyboard();
        }
    }

    return 0;
}

END_OF_MAIN()
