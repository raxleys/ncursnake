#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <ncurses.h>
#include <string.h>

#define FPS 5
#define FRAME_TIME (1000 * 1000 / FPS) // us

void ncurses_init();
void ncurses_deinit();
void draw_border();

// Global variables
int SCR_HEIGHT = 0;
int SCR_WIDTH = 0;

enum Dir {
    DIR_NONE, DIR_LEFT, DIR_RIGHT, DIR_UP, DIR_DOWN
};

typedef struct {
    int x;
    int y;
    enum Dir direction;
} Tile;

// Represents a location on the board where a Tile's direction changes
typedef struct {
    size_t head;
    size_t tail;
    size_t size;
    size_t capacity;
    Tile **tiles;
} TileQueue;

typedef struct {
    size_t i;
    TileQueue *tq;
    Tile *val;
} TileQueueIter;

typedef struct {
    size_t size;
    size_t capacity;
    Tile *tiles;
} TileArena;

bool ta_init(TileArena *ta)
{
    size_t mem = SCR_HEIGHT * SCR_WIDTH * 2;
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
        perror("OOM!");
        exit(1);
    }

    return &ta->tiles[ta->size++];
}

// DEBUG
void tq_printarr(TileQueue *queue)
{
    printf("[");
    for (size_t i = 0; i < queue->capacity; i++) {
        if (i == queue->head && i == queue->tail)
            printf("F");
        else if (i == queue->head)
            printf("H");
        else if (i == queue->tail)
            printf("T");
        else if (queue->tiles[i] != NULL)
            printf("X");
        else
            printf("_");

        if (i != queue->capacity - 1)
            printf("|");
    }

    printf("]\n");
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
    if (tq->head + it->i >= tq->size)
        return false;

    int i = (tq->head + it->i) % tq->capacity;
    it->i++;
    it->val = tq->tiles[i];
    return true;
}

bool tq_grow(TileQueue *queue)
{
    size_t newsize = queue->capacity > 0 ? queue->capacity * 2 : 4;
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
    memset(queue->tiles + queue->head, 0, sizeof(*ret));
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

bool tile_eq(const Tile *t1, const Tile *t2)
{
    return t1->x == t2->x && t1->y == t2->y;
}

bool check_collisions(TileQueueIter *it)
{
    Tile *t = it->val;
    // Arena bounds
    if (t->x >= SCR_WIDTH || t->x < 0 || t->y >= SCR_HEIGHT || t->y < 0)
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

        // TODO
        if (!check_collisions(&it)) {
            printf("COLLISION!\n");
            ncurses_deinit();
            exit(1);
        }
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

int main()
{
    ncurses_init();
    getmaxyx(stdscr, SCR_HEIGHT, SCR_WIDTH);

    TileArena tilemem = {0};
    ta_init(&tilemem);

    // Create snake
    TileQueue snake = {0};
#define SNAKE_INIT_SIZE 9
    for (int i = 0; i < SNAKE_INIT_SIZE; i++) {
        Tile *t = ta_next(&tilemem);
        if (!t) {
            // TODO: goto?
        }

        t->x = (SCR_WIDTH / 2) - i;
        t->y = (SCR_HEIGHT / 2);
        t->direction = DIR_RIGHT;

        tq_pushback(&snake, t);
        printf("Pushed %d!\n", i);
    }

    // A "move point" is a tile on the board at which a snake tile has
    // to change directions. It is the tile at which the head was
    // located at a time when the player changed directions
    TileQueue movepoints = {0};

    int ch;
    struct timespec tlast, tcur;
    do {
        clock_gettime(CLOCK_MONOTONIC, &tlast);

        // TODO: Need to ignore reverse head direction & multiple
        // move points in the direction the head is already heading
        // Input
        ch = getch();
        switch (ch) {
        case 'q':
            break;
        case KEY_UP:
            add_movepoint(&tilemem, &snake, &movepoints, DIR_UP);
            break;
        case KEY_DOWN:
            add_movepoint(&tilemem, &snake, &movepoints, DIR_DOWN);
            break;
        case KEY_LEFT:
            add_movepoint(&tilemem, &snake, &movepoints, DIR_LEFT);
            break;
        case KEY_RIGHT:
            add_movepoint(&tilemem, &snake, &movepoints, DIR_RIGHT);
            break;
        }

        // Update
        move_snake(&snake, &movepoints);

        // Render
        clear();
        draw_border();

        // Draw snake
        TileQueueIter it = tq_iter(&snake);
        while (tq_next(&it)) {
            mvaddch(it.val->y, it.val->x, '#');
        }

        refresh();

        // Control FPS
        clock_gettime(CLOCK_MONOTONIC, &tcur);
        long elapsedus = (tcur.tv_sec - tlast.tv_sec) * (1000 * 1000) + (tcur.tv_nsec - tlast.tv_nsec) / 1000;
        if (elapsedus < FRAME_TIME)
            usleep(FRAME_TIME - elapsedus);
    } while (true);

    ta_deinit(&tilemem);
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
