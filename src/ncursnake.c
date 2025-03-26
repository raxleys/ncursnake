#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <ncurses.h>

#define FPS 30
#define FRAME_TIME (1000 * 1000 / FPS) // us

void ncurses_init();
void ncurses_deinit();
void draw_border();

// Global variables
int SCR_HEIGHT = 0;
int SCR_WIDTH = 0;

int main()
{
    ncurses_init();

    /* int width, height; */
    /* getmaxyx(stdscr, height, width); */
    getmaxyx(stdscr, SCR_HEIGHT, SCR_WIDTH);

    int ch;
    struct timespec tlast, tcur;
    do {
        clock_gettime(CLOCK_MONOTONIC, &tlast);

        // Input
        ch = getch();
        if (ch == 'q')
            break;

        // Render
        /* clear(); */
        draw_border();
        refresh();

        // Control FPS
        clock_gettime(CLOCK_MONOTONIC, &tcur);
        long elapsedus = (tcur.tv_sec - tlast.tv_sec) * (1000 * 1000) + (tcur.tv_nsec - tlast.tv_nsec) / 1000;
        if (elapsedus < FRAME_TIME)
            usleep(FRAME_TIME - elapsedus);
    } while (true);

    ncurses_deinit();
    return 0;
}

void ncurses_init()
{
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, true);
    nodelay(stdscr, true);
}

void ncurses_deinit()
{
    endwin();
}

void draw_border()
{
    // Corners
    mvaddch(0, 0, '+');
    mvaddch(0, SCR_WIDTH - 1, '+');
    mvaddch(SCR_HEIGHT - 1, 0, '+');
    mvaddch(SCR_HEIGHT - 1, SCR_WIDTH - 1, '+');

    // Edges
    mvhline(0, 1, '-', SCR_WIDTH - 2);
    mvhline(SCR_HEIGHT - 1, 1, '-', SCR_WIDTH - 2);
    mvvline(1, 0, '|', SCR_HEIGHT - 2);
    mvvline(1, SCR_WIDTH - 1, '|', SCR_HEIGHT - 2);
}
