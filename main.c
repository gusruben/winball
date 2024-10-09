#include <allegro.h>
#include <stdbool.h>
#include <math.h>

// macros
#define BORDER_POINTS 8
#define BOUNCER_AMOUNT 4


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
    float restitution;
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
Bouncer bouncers[BOUNCER_AMOUNT];
Flipper flippers[2];
float margin = 0.02;
Vector border[BORDER_POINTS];


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
float vectorLength(Vector v) {
    return sqrt((double)(v.x*v.x + v.y*v.y));
}
// alternatively, there's an existing built in function for 3d
Vector normalizeVector(Vector v) {
    return scaleVector(v, 1 / vectorLength(v));
}
Vector perpendicularVector(Vector v) {
    return (Vector){-v.y, v.x};
}
// get line segment from position, length, and angle
LineSegment getLineSegment(Vector position, float length, float angle) {
    Vector directionVector = {cos(angle), sin(angle)};
    Vector endpoint = addVectors(position, scaleVector(directionVector, length));
    return (LineSegment){.a = position, .b = endpoint};
}
LineSegment lineSegmentFromFlipper(Flipper f) {
    float realAngle = f.restAngle + (f.sign * f.currentAngle);
    return getLineSegment(f.position, f.length, realAngle);
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
    b->velocity.y += gravity * dt;
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
// collision handlers
void handleBouncerCollision(Ball* ball, Bouncer* bouncer) {
    Vector directionVector = subtractVectors(ball->position, bouncer->position); // vector pointing from the ball center to the bouncer center
    float distance = vectorLength(directionVector);
    // if the distance is greater than the sum of the radii, they aren't touching
    if (distance > ball->radius + bouncer->radius || distance == 0) { return; }

    directionVector = normalizeVector(directionVector);

    // how far into the bouncer the ball is
    float inset = ball->radius + bouncer->radius - distance;
    // move the ball outside the boucner
    ball->position = addVectors(ball->position, scaleVector(directionVector, inset));

    // add the new velocity to the ball (away from the bouncer)
    float velocityTowardsBouncer = dotProduct(ball->velocity, directionVector); // the component of the ball's velocity in the bouncer's direction
    ball->velocity = addVectors(ball->velocity, scaleVector(directionVector, bouncer->pushStrength - velocityTowardsBouncer));
}
void handleFlipperCollision(Ball* ball, Flipper* flipper) {
    Vector closestPoint = closestPointOnLineSegment(ball->position, lineSegmentFromFlipper(*flipper));
    // after finding the closest point, the rest is basically just the same as bouncer physics
    Vector directionVector = subtractVectors(ball->position, closestPoint);
    float distance = vectorLength(directionVector);
    if (distance > ball->radius + flipper->radius || distance == 0) { return; }

    directionVector = normalizeVector(directionVector);

    float inset = ball->radius + flipper->radius - distance;
    ball->position = addVectors(ball->position, scaleVector(directionVector, inset));

    // finding the velocity of the flipper at the contact point
    Vector radiusVector = addVectors(closestPoint, scaleVector(directionVector, flipper->radius));
    radiusVector = subtractVectors(radiusVector, flipper->position);
    // to get the velocity, just rotate it by 90 degrees and scale it with the angular velocity of the flipper
    Vector surfaceVelocityVector = scaleVector(perpendicularVector(radiusVector), flipper->currentAngularVelocity);
    
    // same code as the bouncers for updating the ball's velocity
    float oldVelocityTowardsFlipper = dotProduct(ball->velocity, directionVector);
    float newVelocityTowardsFlipper = dotProduct(surfaceVelocityVector, directionVector);
    ball->velocity = addVectors(ball->velocity, scaleVector(directionVector, newVelocityTowardsFlipper - oldVelocityTowardsFlipper));
}
void handleBorderCollision(Ball* ball, Vector border[]) {
    // first, find the closest border point to c
    Vector directionVector;
    Vector closest;
    float shortestDistance = 0; // will always be set on the first iteration
    Vector inwardNormal;

    for (int i = 0; i < BORDER_POINTS; i++) {
        Vector p1 = border[i];
        Vector p2 = border[(i + 1) % (BORDER_POINTS - 1)];
        Vector closestPoint = closestPointOnLineSegment(ball->position, (LineSegment){p1, p2});
        directionVector = subtractVectors(ball->position, closestPoint);
        float distance = vectorLength(directionVector);
        if (i == 0 || distance < shortestDistance) {
            shortestDistance = distance;
            closest = closestPoint;

            // inward-facing normal
            inwardNormal = perpendicularVector(subtractVectors(p2, p1));
        }
    }

    // if the ball is inside the border, move it out
    float distance = shortestDistance;
    if (distance == 0) { // edge case where the ball is exactly on the border
        directionVector = inwardNormal;
        distance = vectorLength(inwardNormal);
    };
    directionVector = scaleVector(directionVector, 1 / distance);

    if (dotProduct(directionVector, inwardNormal) >= 0) {
        if (distance > ball->radius) return;
        ball->position = addVectors(ball->position, scaleVector(directionVector, ball->radius - distance));
    } else {
        ball->position = addVectors(ball->position, scaleVector(directionVector, -(distance + ball->radius)));
    }
    
    // update velocity
    float oldVelocityTowardsBorder = dotProduct(ball->velocity, directionVector);
    float newVelocityTowardsBorder = ABS(oldVelocityTowardsBorder) * ball->restitution;
    
    ball->velocity = addVectors(ball->velocity, scaleVector(directionVector, newVelocityTowardsBorder - oldVelocityTowardsBorder));
}

void rotate_translate(float x, float y, float cx, float cy, float angle, float *rx, float *ry) {
    float cos_angle = cos(angle);
    float sin_angle = sin(angle);
    float tx = x * cos_angle - y * sin_angle;
    float ty = x * sin_angle + y * cos_angle;
    *rx = sX(cx + tx);
    *ry = sY(cy + ty);
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
        .color = makecol(0, 0, 0),
        .restitution = 0
    };
    Vector border[BORDER_POINTS] = {
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
            .restAngle = -2.094395, // 120 degrees
            .activeAngle = -0.5235988, // 30 degrees
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
            .restAngle = 2.094395, // -120 degrees
            .activeAngle = 0.5235988, // -30 degrees
            .sign = 1,
            .angularVelocity = 10,
            .touchIdentifier = 1,
            .currentAngularVelocity = 0,
            .currentAngle = 0,
        },
    };
    Bouncer bouncers[BOUNCER_AMOUNT]  = {
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

        // draw ball
        circlefill(buffer, sX(ball.position.x), sY(ball.position.y), ball.radius, ball.color);

        // draw borders
        for (int i = 0; i < 7; i++) {
            line(buffer, 
                sX(border[i].x), sY(border[i].y),
                sX(border[i+1].x), sY(border[i+1].y),
                makecol(0, 0, 0));
        }
        // the last line connecting the end to the start
        line(buffer,
            sX(border[BORDER_POINTS-1].x), sY(border[BORDER_POINTS-1].y),
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
        for (int i = 0; i < BOUNCER_AMOUNT; i++) {
            Bouncer bouncer = bouncers[i];
            circlefill(buffer, sX(bouncer.position.x), sY(bouncer.position.y), sX(bouncer.radius), makecol(0,0,255));
        }

        // physics simulations
        // flippers
        updateFlipper(&flippers[0], dt, key[KEY_LEFT]);
        updateFlipper(&flippers[1], dt, key[KEY_RIGHT]);
        // ball
        updateBall(&ball, dt);
        // ball interactions
        for (int i = 0; i < BOUNCER_AMOUNT; i++) {
            handleBouncerCollision(&ball,  &bouncers[i]);
        }
        for (int i = 0; i < 2; i++) {
            handleFlipperCollision(&ball, &flippers[i]);
        }
        handleBorderCollision(&ball, border);

        vsync();
        blit(buffer, screen, 0, 0, 0, 0, SCREEN_W, SCREEN_H);

        if (keyboard_needs_poll()) {
            poll_keyboard();
        }

        if (key[KEY_ESC] || (key[KEY_LCONTROL] && key[KEY_C])) {
            allegro_exit();
            return 0;
        }
    }

    return 0;
}

END_OF_MAIN()
