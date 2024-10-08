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

int main(int argc, const char **argv)
{
    BITMAP *buffer;
    int timer;
    float gravity = -20.0f;
    float ballX;
    float ballY;
    float ballVelX = 8.0f;
    float ballVelY = 12.0f;

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

    ballX = simWidth / 2;
    ballY = simHeight / 2;

    buffer = create_bitmap(SCREEN_W, SCREEN_H);
    
    while (!keypressed()) {

        new_time = (double)retrace_count / CLOCKS_PER_SEC;
        dt = new_time - old_time;
        old_time = new_time;

        clear_bitmap(buffer);
        circlefill(screen, sX(ballX), sY(ballY), 7, makecol(0,0,0));

        ballVelY += gravity * dt;
        ballX += ballVelX * dt;
        ballY += ballVelY * dt;

        if (ballX < 0) {
            ballX = 0;
            ballVelX *= -1;
        }
        if (ballX > simWidth) {
            ballX = simWidth;
            ballVelX *= -1;
        }
        if (ballY < 0) {
            ballY = 0;
            ballVelY *= -1;
        }

        vsync();
        blit(buffer, screen, 0, 0, 0, 0, SCREEN_W, SCREEN_H);
    }

    return 0;
}

END_OF_MAIN()
