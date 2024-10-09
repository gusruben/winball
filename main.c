#include <allegro.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>

// macros
#define BORDER_POINTS 8
#define BOUNCER_AMOUNT 4
#define TRAIL_LENGTH 10
#define TRAIL_CHECK_MS 50

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
    int color;
} Bouncer;
typedef struct {
    float radius;
    Vector position;
    float length;
    float restAngle;
    float maxRotation;
    float sign;
    float angularVelocity;
    // changing
    float rotation;
    float currentAngularVelocity;
    int touchIdentifier;
} Flipper;

typedef struct {
    Vector position;
    int color;
} TrailPoint;

typedef struct {
    double r;       // a fraction between 0 and 1
    double g;       // a fraction between 0 and 1
    double b;       // a fraction between 0 and 1
} rgb;

typedef struct {
    double h;       // angle in degrees
    double s;       // a fraction between 0 and 1
    double v;       // a fraction between 0 and 1
} hsv;


// physics scene
bool paused = false;
float gravity = -3.0f;
float flipperHeight = 1.7f;

// score
int score = 0;

Ball ball;
Bouncer bouncers[BOUNCER_AMOUNT];
Flipper flippers[2];
float margin = 0.02;
Vector border[BORDER_POINTS];

TrailPoint trail[TRAIL_LENGTH];
int trailIndex = 0;
unsigned long lastTrailUpdate = 0;
int latestColor;

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

Vector getFlipperTip(Flipper* flipper) {
    float angle = flipper->restAngle + flipper->sign * flipper->rotation;
    Vector dir = {cos(angle), sin(angle)};
    return addVectors(flipper->position, scaleVector(dir, flipper->length));
}

// https://stackoverflow.com/a/6930407
rgb hsv2rgb(hsv in)
{
    double      hh, p, q, t, ff;
    long        i;
    rgb         out;

    if(in.s <= 0.0) {       // < is bogus, just shuts up warnings
        out.r = in.v;
        out.g = in.v;
        out.b = in.v;
        return out;
    }
    hh = in.h;
    if(hh >= 360.0) hh = 0.0;
    hh /= 60.0;
    i = (long)hh;
    ff = hh - i;
    p = in.v * (1.0 - in.s);
    q = in.v * (1.0 - (in.s * ff));
    t = in.v * (1.0 - (in.s * (1.0 - ff)));

    switch(i) {
    case 0:
        out.r = in.v;
        out.g = t;
        out.b = p;
        break;
    case 1:
        out.r = q;
        out.g = in.v;
        out.b = p;
        break;
    case 2:
        out.r = p;
        out.g = in.v;
        out.b = t;
        break;

    case 3:
        out.r = p;
        out.g = q;
        out.b = in.v;
        break;
    case 4:
        out.r = t;
        out.g = p;
        out.b = in.v;
        break;
    case 5:
    default:
        out.r = in.v;
        out.g = p;
        out.b = q;
        break;
    }
    return out;     
}


// feature-specific functions
void updateBall(Ball* b, float dt) {
    b->velocity.y += gravity * dt;
    b->position.x += b->velocity.x * dt;
    b->position.y += b->velocity.y * dt;
}
void updateFlipper(Flipper* flipper, float dt, bool pressed) {
    float prevRotation = flipper->rotation;
    if (pressed) {
        flipper->rotation = MIN(flipper->rotation + dt * flipper->angularVelocity, flipper->maxRotation);
    } else {
        flipper->rotation = MAX(flipper->rotation - dt * flipper->angularVelocity, 0.0);
    }
    flipper->currentAngularVelocity = flipper->sign * (flipper->rotation - prevRotation) / dt;
}


// collision handlers
void handleBouncerCollision(Ball* ball, Bouncer* bouncer) {
    Vector directionVector = subtractVectors(ball->position, bouncer->position); // vector pointing from the ball center to the bouncer center
    float distance = vectorLength(directionVector);
    // if the distance is greater than the sum of the radii, they aren't touching
    if (distance > ball->radius + bouncer->radius || distance == 0) { return; }
    
    // add to score (smaller bouncers worth more)
    score += (int)(100 / (bouncer->radius * 100));

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
    Vector tip = getFlipperTip(flipper);
    Vector closest = closestPointOnLineSegment(ball->position, (LineSegment){flipper->position, tip});
    Vector dir = subtractVectors(ball->position, closest);
    float d = vectorLength(dir);
    
    if (d == 0.0 || d > ball->radius + flipper->radius)
        return;

    dir = scaleVector(dir, 1.0 / d);

    float corr = (ball->radius + flipper->radius - d);
    ball->position = addVectors(ball->position, scaleVector(dir, corr));

    // update velocity
    Vector radius = addVectors(closest, scaleVector(dir, flipper->radius));
    radius = subtractVectors(radius, flipper->position);
    Vector surfaceVel = perpendicularVector(radius);
    surfaceVel = scaleVector(surfaceVel, flipper->currentAngularVelocity);

    float v = dotProduct(ball->velocity, dir);
    float vnew = dotProduct(surfaceVel, dir);

    ball->velocity = addVectors(ball->velocity, scaleVector(dir, vnew - v));
}

void handleBorderCollision(Ball* ball, Vector border[], int borderCount) {
    if (borderCount < 3)
        return;

    Vector d, closest, ab, normal;
    float minDist = 0.0f;
    int closestIndex = 0;

    for (int i = 0; i < borderCount; i++) {
        Vector a = border[i];
        Vector b = border[(i + 1) % borderCount];
        Vector c = closestPointOnLineSegment(ball->position, (LineSegment){a, b});
        d = subtractVectors(ball->position, c);
        float dist = vectorLength(d);
        if (i == 0 || dist < minDist) {
            minDist = dist;
            closest = c;
            ab = subtractVectors(b, a);
            normal = normalizeVector(perpendicularVector(ab));
            closestIndex = i;
        }
    }

    d = subtractVectors(ball->position, closest);
    float dist = vectorLength(d);
    if (dist < 0.0001f) {
        d = normal;
        dist = vectorLength(normal);
    }
    d = normalizeVector(d);

    if (dotProduct(d, normal) >= 0.0f) {
        if (dist > ball->radius) 
            return;
        
        ball->position = addVectors(ball->position, scaleVector(normal, ball->radius - dist));
       
        float angle = acos(dotProduct(normalizeVector(ball->velocity), normal));
        
        if (fabs(angle - M_PI/2) < M_PI/6) {  
            float bounceStrength = 0.5f;
            Vector bounceVector = scaleVector(normal, bounceStrength);
            ball->velocity = addVectors(ball->velocity, bounceVector);
        }
    }
    else {
        ball->position = addVectors(ball->position, scaleVector(d, -(dist + ball->radius)));
    }

    float v = dotProduct(ball->velocity, normal);
    Vector reflectedVelocity = subtractVectors(ball->velocity, scaleVector(normal, 2 * v));
    
    float energyLoss = 0.8f; 
    ball->velocity = scaleVector(reflectedVelocity, energyLoss);
}


void draw_filled_polygon(BITMAP *bmp, int points[], int num_points, int color) {
    for (int i = 1; i < num_points - 1; i++) {
        triangle(bmp, 
                 points[0], points[1], 
                 points[i*2], points[i*2+1], 
                 points[(i+1)*2], points[(i+1)*2+1], 
                 color);
    }
}

void updateTrail(Ball* ball) {
    unsigned long currentTime = clock() * 1000 / CLOCKS_PER_SEC;
    if (currentTime - lastTrailUpdate >= TRAIL_CHECK_MS) {
        trail[trailIndex].position = ball->position;
        
        float hue = (float)trailIndex / TRAIL_LENGTH * 360.0f;
        int r, g, b;
        hsv_to_rgb(hue, 1.0f, 1.0f, &r, &g, &b);
        latestColor = makecol(r, g, b);
        trail[trailIndex].color = latestColor;

        trailIndex = (trailIndex + 1) % TRAIL_LENGTH;
        lastTrailUpdate = currentTime;
    }
}

void drawTrail(BITMAP* buffer) {
    for (int i = 0; i < TRAIL_LENGTH; i++) {
        int index = (trailIndex - i - 1 + TRAIL_LENGTH) % TRAIL_LENGTH;
        float radius = ball.radius * (TRAIL_LENGTH - i) / TRAIL_LENGTH;
        circlefill(buffer, sX(trail[index].position.x), sY(trail[index].position.y), sX(radius), trail[index].color);
    }
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
    
    // set up scaling
    scale = MIN(SCREEN_W, SCREEN_H) / flipperHeight;
    simWidth = SCREEN_W / scale;
    simHeight = SCREEN_H / scale;

    // initialize physics scene
    ball = (Ball){
        .position = {0.8, 0.7},
        .velocity = {0, 0},
        .radius = 0.05,
        .color = makecol(0, 0, 0),
        .restitution = 0
    };
    Vector ballPositions[10] = {}   ;
    Vector border[BORDER_POINTS] = {
        {0.74, 0.25},
        {1 - margin, 0.4},
        {1 - margin, flipperHeight - margin},
        {margin, flipperHeight - margin},
        {margin, 0.4},
        {.26, .25},
        {.26, -1},
        {.74, -1}
    };
    Flipper flippers[2] = {
        {
            .radius = 0.03,
            .position = {0.26, 0.22},
            .length = 0.15,
            .restAngle = -0.5,
            .maxRotation = 1.0,
            .sign = 1,
            .angularVelocity = 15.0,
            .rotation = 0.0,
            .currentAngularVelocity = 0.0,
            .touchIdentifier = -1
        },
        {
            .radius = 0.03,
            .position = {0.74, 0.22},
            .length = 0.15,
            .restAngle = M_PI + 0.5,
            .maxRotation = 1.0,
            .sign = -1,
            .angularVelocity = 15.0,
            .rotation = 0.0,
            .currentAngularVelocity = 0.0,
            .touchIdentifier = -1
        }
    };

    // initialize trail
    for (int i = 0; i < TRAIL_LENGTH; i++) {
        trail[i].position = ball.position;
        trail[i].color = makecol(255, 255, 255);
    }

    Bouncer bouncers[BOUNCER_AMOUNT] = {
        { .position = {0.35, 0.6},  .radius = 0.07, .pushStrength = 2.2, .color = makecol(240, 80, 130)},  // upper left
        { .position = {0.65, 0.7},  .radius = 0.09, .pushStrength = 2.0, .color = makecol(82, 247, 159) },  // upper right
        { .position = {0.25, 1.0},  .radius = 0.08, .pushStrength = 2.1, .color = makecol(82, 226, 247) },  // lower left
        { .position = {0.75, 1.1},  .radius = 0.06, .pushStrength = 2.3, .color = makecol(247, 235, 82) }   // lower right
    };
    
    buffer = create_bitmap(SCREEN_W, SCREEN_H);

    // for filling in over the dark background
    int white_area[BORDER_POINTS * 2];
    for (int i = 0; i < BORDER_POINTS; i++) {
        white_area[i*2] = sX(border[i].x);
        white_area[i*2+1] = sY(border[i].y);
    }
    
    while (1) {

        new_time = (double)retrace_count / CLOCKS_PER_SEC;
        dt = new_time - old_time;
        dt = MIN(dt, 1.0/60.0);
        old_time = new_time;

        // clear_bitmap(buffer);
        clear_to_color(buffer, makecol(0, 0, 0));

        draw_filled_polygon(buffer, white_area, BORDER_POINTS, makecol(255, 255, 255));

        // draw trail before ball
        drawTrail(buffer);

        // draw ball
        circlefill(buffer, sX(ball.position.x), sY(ball.position.y), sX(ball.radius), ball.color);

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
        for (int i = 0; i < 2; i++) {
            Flipper flipper = flippers[i];
            float angle = flipper.restAngle + flipper.sign * flipper.rotation;

            float cos_angle = cos(angle);
            float sin_angle = sin(angle);
            
            float x1 = flipper.position.x + (-flipper.radius * cos_angle);
            float y1 = flipper.position.y + (-flipper.radius * sin_angle);
            float x2 = flipper.position.x + (flipper.length * cos_angle - flipper.radius * sin_angle);
            float y2 = flipper.position.y + (flipper.length * sin_angle + flipper.radius * cos_angle);
            float x3 = flipper.position.x + (flipper.length * cos_angle + flipper.radius * sin_angle);
            float y3 = flipper.position.y + (flipper.length * sin_angle - flipper.radius * cos_angle);
            float x4 = flipper.position.x + (flipper.radius * cos_angle);
            float y4 = flipper.position.y + (flipper.radius * sin_angle);

            int points[8] = {
                sX(x1), sY(y1),
                sX(x2), sY(y2),
                sX(x3), sY(y3),
                sX(x4), sY(y4)
            };
            polygon(buffer, 4, points, makecol(0, 0, 0));

            circlefill(buffer, sX(flipper.position.x), sY(flipper.position.y), sX(flipper.radius), makecol(0, 0, 0));

            float end_x = flipper.position.x + flipper.length * cos_angle;
            float end_y = flipper.position.y + flipper.length * sin_angle;
            circlefill(buffer, sX(end_x), sY(end_y), sX(flipper.radius), makecol(0, 0, 0));
        }

        
        // draw bouncers
        for (int i = 0; i < BOUNCER_AMOUNT; i++) {
            Bouncer bouncer = bouncers[i];
            circlefill(buffer, sX(bouncer.position.x), sY(bouncer.position.y), sX(bouncer.radius), makecol(0,0,0));
            circle(buffer, sX(bouncer.position.x), sY(bouncer.position.y), sX(bouncer.radius) - 2, bouncer.color);
        }

        // draw score
        char scoreText[20];
        sprintf(scoreText, "Score: %d", score);
        textout_ex(buffer, font, scoreText, SCREEN_W - 100, 10, makecol(255, 255, 255), -1);

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
        handleBorderCollision(&ball, border, BORDER_POINTS);

        updateTrail(&ball);

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
