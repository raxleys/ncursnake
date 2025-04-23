#ifndef NCURSNAKE_H
#define NCURSNAKE_H

// Constants
#define FPS 5
#define FRAME_TIME (1000 * 1000 / FPS) // us
#define SNAKE_INIT_SIZE 5

// Structs
enum Dir {
    DIR_NONE, DIR_LEFT, DIR_RIGHT, DIR_UP, DIR_DOWN
};

typedef struct {
    int x;
    int y;
    enum Dir direction;
} Tile;

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

// Functions
void ncurses_init();
void ncurses_deinit();
void draw_border();
void draw_debug(TileArena *tilemem, TileQueue *movepoints);
bool ta_init(TileArena *ta);
void ta_deinit(TileArena *ta);
void ta_clear(TileArena *ta);
Tile *ta_next(TileArena *ta);
void tq_printarr(FILE *file, TileQueue *queue);
TileQueueIter tq_iter(TileQueue *tq);
TileQueueIter tq_iter_clone(const TileQueueIter *it);
bool tq_next(TileQueueIter *it);
bool tq_grow(TileQueue *queue);
bool tq_pushback(TileQueue *queue, Tile *tile);
Tile *tq_popfront(TileQueue *queue);
Tile *tq_peekfront(TileQueue *queue);
Tile *tq_peekback(TileQueue *queue);
void tq_deinit(TileQueue *queue);
void tq_clear(TileQueue *queue);
bool tile_eq(const Tile *t1, const Tile *t2);
bool check_collisions(TileQueueIter *it);
void snake_init(TileArena *mem, TileQueue *snake);
void grow_snake(TileArena *mem, TileQueue *snake);
bool move_snake(TileQueue *snake, TileQueue *movepoints);
void add_movepoint(TileArena *mem, TileQueue *snake, TileQueue *movepoints, enum Dir direction);
void new_apple();
bool check_eat_apple(const Tile *tile);
bool prompt_play_again();

#endif // NCURSNAKE_H
