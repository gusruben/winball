#include <allegro.h>


// scaling
float minSimWidth = 20.0f;
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
    Vector position;
    Vector velocity;
    float radius;
    int color;
} Ball;


int main(int argc, const char **argv)
{
    BITMAP *buffer;
    int timer;
    float gravity = -20.0f;

    // delta time
    double old_time = 0;
    double new_time = 0;
    double dt = 0;

    // Initializes the Allegro library.
    if (allegro_init() != 0) {
        return 1;
    }

    // Installs the Allegro keyboard interrupt handler.
    install_keyboard();
    install_timer();

    // Switch to graphics mode, 320x200.
    if (set_gfx_mode(GFX_AUTODETECT, 320, 200, 0, 0) != 0) {
        set_gfx_mode(GFX_TEXT, 0, 0, 0, 0);
        allegro_message("Cannot set graphics mode:\r\n%s\r\n", allegro_error);
        return 1;
    }

    // Print a single line of "hello world" on a white screen.
    set_palette(desktop_palette);
    clear_to_color(screen, makecol(255, 255, 255));

    // set up scaling
    scale = MIN(SCREEN_W, SCREEN_H) / minSimWidth;
    simWidth = SCREEN_W / scale;
    simHeight = SCREEN_H / scale;

    Ball ball = {
        .position = {simWidth / 2, simHeight / 2},
        .velocity = {8, 12},
        .radius = 7,
        .color = makecol(0, 0, 0)
    };

    buffer = create_bitmap(SCREEN_W, SCREEN_H);
    
    while (!keypressed()) {

        new_time = (double)retrace_count / CLOCKS_PER_SEC;
        dt = new_time - old_time;
        old_time = new_time;

        clear_bitmap(buffer);
        circlefill(screen, sX(ball.position.x), sY(ball.position.y), ball.radius, ball.color);

        ball.velocity.y += gravity * dt;
        ball.position.x += ball.velocity.x * dt;
        ball.position.y += ball.velocity.y * dt;

        if (ball.position.x < 0) {
            ball.position.x = 0;
            ball.velocity.x *= -1;
        }
        if (ball.position.x > simWidth) {
            ball.position.x = simWidth;
            ball.velocity.x *= -1;
        }
        if (ball.position.y < 0) {
            ball.position.y = 0;
            ball.velocity.y *= -1;
        }

        vsync();
        blit(buffer, screen, 0, 0, 0, 0, SCREEN_W, SCREEN_H);
    }

    return 0;
}

END_OF_MAIN()
