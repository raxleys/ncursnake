#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <ncurses.h>
#include <string.h>
#include "ncursnake.h"

// Global variables
int SCR_HEIGHT = 0;
int SCR_WIDTH = 0;

// Only 1 apple exists at a time, so just track it with a global
// variable.
Tile APPLE = {0};

int main()
{
    srand(time(NULL));

    ncurses_init();
    getmaxyx(stdscr, SCR_HEIGHT, SCR_WIDTH);

    TileArena tilemem = {0};
    ta_init(&tilemem);

    // Create snake
    TileQueue snake = {0};
    snake_init(&tilemem, &snake);
    while (snake.size < SNAKE_INIT_SIZE)
        grow_snake(&tilemem, &snake);

    // A "move point" is a tile on the board at which a snake tile has
    // to change directions. It is the tile at which the head was
    // located at a time when the player changed directions
    TileQueue movepoints = {0};

    int ch;
    struct timespec tlast, tcur;
    size_t score = 0;
    new_apple();
    do {
        clock_gettime(CLOCK_MONOTONIC, &tlast);

        // Input
        ch = getch();
        if (ch == 'q') {
            break;
        } else if (ch == KEY_UP) {
            const Tile *head = tq_peekfront(&snake);
            if (head->direction != DIR_UP && head->direction != DIR_DOWN)
                add_movepoint(&tilemem, &snake, &movepoints, DIR_UP);
        } else if (ch == KEY_DOWN) {
            const Tile *head = tq_peekfront(&snake);
            if (head->direction != DIR_UP && head->direction != DIR_DOWN)
                add_movepoint(&tilemem, &snake, &movepoints, DIR_DOWN);
        } else if (ch == KEY_RIGHT) {
            const Tile *head = tq_peekfront(&snake);
            if (head->direction != DIR_LEFT && head->direction != DIR_RIGHT)
                add_movepoint(&tilemem, &snake, &movepoints, DIR_RIGHT);
        } else if (ch == KEY_LEFT) {
            const Tile *head = tq_peekfront(&snake);
            if (head->direction != DIR_LEFT && head->direction != DIR_RIGHT)
                add_movepoint(&tilemem, &snake, &movepoints, DIR_LEFT);
        }

        // Update
        if (!move_snake(&snake, &movepoints)) {
            // Collision
            // TODO
            break;
        }

        if (check_eat_apple(tq_peekfront(&snake))) {
            score += 10;
            grow_snake(&tilemem, &snake);
            new_apple();
        }

        // Render
        clear();
        draw_border();

        // Draw snake
        TileQueueIter it = tq_iter(&snake);
        while (tq_next(&it)) {
            mvaddch(it.val->y, it.val->x, 'O');
        }

        // Draw apple
        mvaddch(APPLE.y, APPLE.x, 'a');

        // Print score
        mvprintw(0, 0, "Score: %zu", score);

#ifdef DEBUG
        draw_debug(&tilemem, &movepoints);
#endif

        refresh();

        // Control FPS
        clock_gettime(CLOCK_MONOTONIC, &tcur);
        long elapsedus = (tcur.tv_sec - tlast.tv_sec) * (1000 * 1000) + (tcur.tv_nsec - tlast.tv_nsec) / 1000;
        if (elapsedus < FRAME_TIME)
            usleep(FRAME_TIME - elapsedus);
    } while (true);

    // TODO
    // Make getch blocking again.
    nodelay(stdscr, false);
    getch();

    tq_deinit(&snake);
    tq_deinit(&movepoints);
    ta_deinit(&tilemem);
    ncurses_deinit();
    printf("????\n");
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
    mvaddch(1, 0, '#');
    mvaddch(1, SCR_WIDTH - 1, '#');
    mvaddch(SCR_HEIGHT - 1, 0, '#');
    mvaddch(SCR_HEIGHT - 1, SCR_WIDTH - 1, '#');

    // Edges
    mvhline(1, 1, '#', SCR_WIDTH - 2);
    mvhline(SCR_HEIGHT - 1, 1, '#', SCR_WIDTH - 2);
    mvvline(2, 0, '#', SCR_HEIGHT - 3);
    mvvline(2, SCR_WIDTH - 1, '#', SCR_HEIGHT - 3);
}

void draw_debug(TileArena *tilemem, TileQueue *movepoints)
{
    // Display movepoints
    TileQueueIter it = tq_iter(movepoints);
    while (tq_next(&it)) {
        mvaddch(it.val->y, it.val->x, 'X');
    }

    // Print arena allocation info
    mvprintw(0, 25, "Tile mem: %zu/%zu (%.2f%%)", tilemem->size, tilemem->capacity, ((double)tilemem->size)/((double)tilemem->capacity));
}

bool ta_init(TileArena *ta)
{
    // The main thing eating memory is the "movepoint queue". For
    // simplicity, the only allocations are in the queue's dynamic
    // array of pointers, and this arena, which is used to generate
    // Tile structs. A player could opt to just ignore eating apples
    // and just move around the snake. This would result in a
    // theoretically infinite number of Tile allocations from our
    // arena, and since we cannot free individual allocations,
    // eventually we'll run out of space.
    // It would be more robust to find a way to reuse memory for the
    // movepoint queue, but I don't care enough to bother, and prefer
    // to keep the implementation simple. So, we'll just use an
    // arbitrary multiple of the size of the board, and hopefully the
    // player wins/loses before we run out of memory, otherwise, we'll
    // just crash the program.
    size_t mem = SCR_HEIGHT * SCR_WIDTH * 1000;
    Tile *tiles = calloc(mem, sizeof(*tiles));
    if (!tiles)
        return false;

    ta->size = 0;
    ta->capacity = mem;
    ta->tiles = tiles;
    return true;
}

void ta_deinit(TileArena *ta)
{
    free(ta->tiles);
    ta->tiles = NULL;
    ta->size = 0;
    ta->capacity = 0;
}

Tile *ta_next(TileArena *ta)
{
    if (ta->size + 1 > ta->capacity) {
        ncurses_deinit();
        fprintf(stderr, "ERROR: Tile arena is out of memory!");
        ta_deinit(ta);
        exit(1);
    }

    return &ta->tiles[ta->size++];
}

// DEBUG
void tq_printarr(FILE *file, TileQueue *queue)
{
    fprintf(file, "[");
    for (size_t i = 0; i < queue->capacity; i++) {
        if (i == queue->head && i == queue->tail)
            fprintf(file, "F");
        else if (i == queue->head)
            fprintf(file, "H");
        else if (i == queue->tail)
            fprintf(file, "T");
        else if (queue->tiles[i] != NULL)
            fprintf(file, "X");
        else
            fprintf(file, "_");

        if (i != queue->capacity - 1)
            fprintf(file, "|");
    }

    fprintf(file, "]\n");
}

TileQueueIter tq_iter(TileQueue *tq)
{
    return (TileQueueIter) {
        .i = 0,
        .tq = tq,
        .val = NULL,
    };
}

TileQueueIter tq_iter_clone(const TileQueueIter *it)
{
    return (TileQueueIter) {
        .i = it->i,
        .tq = it->tq,
        .val = it->val,
    };
}

bool tq_next(TileQueueIter *it)
{
    TileQueue *tq = it->tq;
    if (it->i >= tq->size)
        return false;

    int i = (tq->head + it->i) % tq->capacity;
    it->i++;
    it->val = tq->tiles[i];
    return true;
}

bool tq_grow(TileQueue *queue)
{
    size_t newsize = queue->capacity > 0 ? queue->capacity * 2 : 64;
    Tile **tmp = calloc(newsize, sizeof(*tmp));
    if (!tmp) {
        perror("OOM");
        return false;
    }

    // Copy over values
    for (size_t i = 0; i < queue->size; i++) {
        size_t index = (queue->head + i) % queue->capacity;
        tmp[i] = queue->tiles[index];
    }

    free(queue->tiles);
    queue->tiles = tmp;
    queue->capacity = newsize;
    queue->head = 0;
    queue->tail = queue->size;
    return true;
}

bool tq_pushback(TileQueue *queue, Tile *tile)
{
    if (queue->size + 1 > queue->capacity && !tq_grow(queue))
        return false;

    queue->tiles[queue->tail] = tile;
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->size++;
    return true;
}

Tile *tq_popfront(TileQueue *queue)
{
    if (queue->size <= 0)
        return NULL;

    Tile *ret = queue->tiles[queue->head];
    queue->tiles[queue->head] = NULL;
    queue->head = (queue->head + 1) % queue->capacity;
    queue->size--;
    return ret;
}

Tile *tq_peekfront(TileQueue *queue)
{
    if (queue->size <= 0)
        return NULL;

    return queue->tiles[queue->head];
}

Tile *tq_peekback(TileQueue *queue)
{
    if (queue->size <= 0)
        return NULL;

    size_t i = (queue->tail - 1) % queue->capacity;
    return queue->tiles[i];
}

void tq_deinit(TileQueue *queue)
{
    free(queue->tiles);
    memset(queue, 0, sizeof(*queue));
}

bool tile_eq(const Tile *t1, const Tile *t2)
{
    return t1->x == t2->x && t1->y == t2->y;
}

bool check_collisions(TileQueueIter *it)
{
    Tile *t = it->val;
    // Arena bounds
    if (t->x >= SCR_WIDTH - 1 || t->x < 1 || t->y >= SCR_HEIGHT - 1 || t->y <= 1)
        return false;

    // Iterate from the next tile
    TileQueueIter it2 = tq_iter_clone(it);
    tq_next(&it2);
    while (tq_next(&it2)) {
        if (t->x == it2.val->x && t->y == it2.val->y)
            return false;
    }

    return true;
}

void snake_init(TileArena *mem, TileQueue *snake)
{
    // Start off the snake in the centre
    Tile *t = ta_next(mem);
    t->x = SNAKE_INIT_SIZE / 2 + SCR_WIDTH / 2;
    t->y = SCR_HEIGHT / 2;
    t->direction = DIR_RIGHT;
    tq_pushback(snake, t);
}

void grow_snake(TileArena *mem, TileQueue *snake)
{
    Tile *t = ta_next(mem);
    const Tile *tail = tq_peekback(snake);
    if (tail->direction == DIR_UP) {
        t->x = tail->x;
        t->y = tail->y + 1;
    } else if (tail->direction == DIR_DOWN) {
        t->x = tail->x;
        t->y = tail->y - 1;
    } else if (tail->direction == DIR_LEFT) {
        t->x = tail->x + 1;
        t->y = tail->y;
    } else if (tail->direction == DIR_RIGHT) {
        t->x = tail->x - 1;
        t->y = tail->y;
    }
    t->direction = tail->direction;
    tq_pushback(snake, t);
}

bool move_snake(TileQueue *snake, TileQueue *movepoints)
{
    TileQueueIter it = tq_iter(snake);
    while (tq_next(&it)) {
        Tile *t = it.val;

        // Check if tile is on a move point
        TileQueueIter mps = tq_iter(movepoints);
        while (tq_next(&mps)) {
            Tile *mp = mps.val;
            if (t->x == mp->x && t->y == mp->y) {
                t->direction = mp->direction;

                // If it is the tail, remove the movepoint
                if (tile_eq(t, tq_peekback(snake))) {
                    // All data structures are queues, so if the tail
                    // is passing through a movepoint, it HAS to be
                    // the oldest one.
                    tq_popfront(movepoints);
                    break;
                }
            }
        }

        switch(t->direction) {
        case DIR_LEFT:
            t->x--;
            break;
        case DIR_RIGHT:
            t->x++;
            break;
        case DIR_UP:
            t->y--;
            break;
        case DIR_DOWN:
            t->y++;
            break;
        default:
            assert(false && "Unreachable!");
        }

        if (!check_collisions(&it))
            return false;
    }

    return true;
}

void add_movepoint(TileArena *mem, TileQueue *snake,
                   TileQueue *movepoints, enum Dir direction)
{
    Tile *t = tq_peekfront(snake);
    Tile *mp = ta_next(mem);
    memcpy(mp, t, sizeof(*t));
    mp->direction = direction;
    tq_pushback(movepoints, mp);
}

void new_apple()
{
    // 1 to SCR_WIDTH - 1
    size_t x = (rand() % (SCR_WIDTH - 2)) + 1;

    // 2 to SCR_HEIGHT - 1
    size_t y = (rand() % (SCR_HEIGHT - 3)) + 2;

    APPLE.x = x;
    APPLE.y = y;
}

bool check_eat_apple(const Tile *tile)
{
    return tile->x == APPLE.x && tile->y == APPLE.y;
}
