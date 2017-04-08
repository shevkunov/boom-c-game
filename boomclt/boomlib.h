#define _POSIX_SOURCE
#define _POSIX_C_SOURCE 199309L /* for nanosleep */
#include <termios.h>
#include <unistd.h>

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <pthread.h>
#include <signal.h>
#include <netdb.h>
#include <time.h>
#include <poll.h>
#include <fcntl.h>

#include <arpa/inet.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/ip.h>
#include <netinet/in.h>

#include "boomque.h"
#include "boomvec.h"
#include "boommap.h"

#define BM_TRUE 1
#define BM_FALSE 0

/** user screen will have height and length =  BM_BOOK * 2 + 1 */  
#define BM_LOOK 10
#define BM_LOOK_FULL (BM_LOOK * 2 + 1)
#define BM_ADD_LINES 3
/** in nanoseconds */
#define BM_SENDER_DELAY 5*1E7
/** this size should be more than 1 + sizeof(struct bm_slient_data)*/
#define BM_MSG_LEN 512
#define BM_NAME_LEN 512
#define BM_FGETS_BUFSIZE 256

/** size of explosion */
#define BM_BOOM_DRAW_SIZE 1
/** time of explosion */
#define BM_BOOM_DRAW_STEPS 5

/** distance of mines */
#define BM_MINES_LOOK 10
#define BM_LISTEN_MAX 5

#define BM_DEFAULT_PORT 1666

struct bm_client_data {
    /** this value contains user screen
     * without any divisors. only char array */
    char screen[BM_LOOK_FULL][BM_LOOK_FULL];
    /** player health may be below zero */
    int health;
    /** count of available mines */
    int mines;
    /** server-setted value of combat-bomb status */
    int boom;
};

#define BM_NET_MSG_DIE 0
#define BM_NET_MSG_CDATA 1
#define BM_NET_MSG_CONNECT 2
#define BM_NET_MSG_GAMEACT 3
#define BM_NET_MSG_NEWGAME 4
#define BM_NET_MSG_STARTGAME 5

struct bm_net_msg {
    /** first byte contains type of meesage, the others - the data */
    char data[BM_MSG_LEN];
};

void bm_net_msg_to_client_data(struct bm_net_msg* msg,
                               struct bm_client_data *data) {
    memcpy(data, &(msg->data[1]), sizeof(struct bm_client_data));
}

void bm_client_data_to_net_msg(struct bm_client_data *data,
                               struct bm_net_msg* msg) {
    msg->data[0] = BM_NET_MSG_CDATA;
    memcpy((void*)(&msg->data[1]), data, sizeof(struct bm_client_data));
}

void bm_print_new_lines(size_t count)
{
    while (count--) {
        putchar('\n');        
    }
}

void bm_print_spaces(size_t count)
{
    while (count--) {
        putchar(' ');
    }
}

void bm_draw_logo(int val)
{
    printf("  ____     _____   _____               \n");
    printf(" /\\  _`\\  /\\  __`\\/\\  __`\\  /'\\_/`\\    \n");
    printf(" \\ \\ \\_\\ \\\\ \\ \\/\\ \\ \\ \\/\\ \\/\\      \\   \n");
    printf("  \\ \\  _ < \\ \\ \\ \\ \\ \\ \\ \\ \\ \\ \\__\\ \\  \n");
    printf("   \\ \\ \\_\\ \\\\ \\ \\_\\ \\ \\ \\_\\ \\ \\ \\_/\\ \\ \n");
    printf("    \\ \\____/ \\ \\_____\\ \\_____\\ \\_\\\\ \\_\\\n");
    printf("     \\/___/   \\/_____/\\/_____/\\/_/ \\/_/\n");
    switch (val) {
        case 0:
            printf("          SERVER        [D|B]OOM v0.17\n\n");
            break;
        case 1:
            printf("          CLIENT        [D|B]OOM v0.17\n\n");
            break;
        case 2:
            printf("          TEAM KEEPER   [D|B]OOM v0.17\n\n");
            break;
    }
}

void bm_draw_raw_net(struct bm_net_msg *net) {
    int i;
    for (i = 1; i < BM_NAME_LEN; ++i) {
        if (!(net->data[i])) break;
        putchar(net->data[i]);
    }
}

void bm_draw(struct bm_client_data* data, size_t height, size_t width)
{
    size_t i, j;

    if ((height < BM_ADD_LINES + BM_LOOK_FULL) || (width < BM_LOOK_FULL)) {
        printf("so small term\n");
        bm_print_new_lines(height - 2);
        return;
    }
    /* this will end last line, if needed */
    putchar('\n');
    for (i = 0; i < BM_LOOK_FULL; ++i) {
        bm_print_spaces((width - BM_LOOK_FULL) / 2);
        for (j = 0; j < BM_LOOK_FULL; ++j) {
            putchar(data->screen[i][j]);
        } 
        putchar('\n');
    }

    printf("HP[%d]\nMN[%d]\nBM[%d]\n", data->health,
            data->mines, data->boom);
    
    bm_print_new_lines(height - BM_ADD_LINES - BM_LOOK_FULL);
}

struct bm_pair {
    int a;
    int b;
};

struct bm_engine_msg {
    struct bm_net_msg *net_msg;
    size_t client_id;
};

struct bm_engine_map {
    char** map;
    unsigned int width;
    unsigned int height;
    unsigned int initial_health;
    unsigned int initial_mines;
    unsigned int hit_value;
    unsigned int recharge_duration;
    unsigned int mining_time;
    unsigned int stay_health_drop;
    unsigned int movement_health_drop;
    unsigned int step_standard_delay;
    unsigned int moratory_duration;
    unsigned int freecells;
    struct bm_vector engine_items;
};

struct bm_engine_player {
    unsigned int x;
    unsigned int y;
    size_t last_action_step;
    struct bm_client_data cdata;
};

#define BM_ITEM_MED 0
#define BM_ITEM_MINE 1
#define BM_ITEM_BOOM 2
#define BM_ITEM_MINE_BOOM 3
struct bm_engine_item {
    unsigned int x;
    unsigned int y;
    int value;
    int father;
    int type;
};

struct bm_engine_data{
    struct bm_engine_map map;
    struct bm_vector plr;
    char** render;
    /** current step-base time on server */
    size_t global_steps;
};

int bm_load_map_from_file(char* fname, struct bm_engine_map *map) {
    FILE* file = fopen(fname, "r");
    char file_header[4];
    char read_buffer[BM_FGETS_BUFSIZE];
    struct bm_engine_item *item;
    struct bm_map settings;
    unsigned int i, j;

    if (file == NULL) {
        return -1;
    }

    fread(file_header, 4, 1, file);
    if (strncmp(file_header, "Map", 3)) {
        fclose(file);
        return -1;
    }
    fscanf(file, "%ux%u\n", &map->height, &map->width);
    map->height += 2;
    map->width += 2;
    map->map = malloc(sizeof(char*) * map->height);
    for (i = 0; i < map->height; ++i) {
        map->map[i] = malloc(sizeof(char) * map->width);
    }

    map->freecells = 0;
    for (i = 0; i < map->height; ++i) {
        for (j = 0; j < map->width; ++j) {
            map->map[i][j] = fgetc(file);
            if (map->map[i][j] == ' ') {
                ++map->freecells;
            }
        }
        fgetc(file);
    }

    /* default */
    map->initial_health = 5000;
    map->hit_value = 500;
    map->recharge_duration = 3;
    map->mining_time = 3;
    map->stay_health_drop = 1;
    map->movement_health_drop = 4;
    map->step_standard_delay = 200;
    map->moratory_duration = 5;
    map->initial_mines = 10;

    bm_map_init(&settings);
    do {
        fgets(read_buffer, BM_FGETS_BUFSIZE, file);
        read_buffer[BM_FGETS_BUFSIZE - 1] = 0;
        if (strncmp(read_buffer, "items:", 6) != 0) {
            int space = -1;
            int eqv = -1;
            for (i = 0; i < strlen(read_buffer); ++i) {
                if ((space == -1) && (read_buffer[i] == ' ')) {
                    space = i;
                }
                if ((eqv == -1) && (read_buffer[i] == '=')) {
                    eqv = i;
                }
            }
            if ((space != -1) && (eqv != -1)) {
                int value = atoi(read_buffer + eqv + 1);
                read_buffer[space] = 0;
                read_buffer[eqv] = 0;
                bm_map_set(&settings, read_buffer, value);
            }
        }
    } while (strncmp(read_buffer, "items:", 6) != 0);

    if (bm_map_get(&settings, "initial_health") != ~(int)0) {
        map->initial_health = bm_map_get(&settings, "initial_health");
    }
    if (bm_map_get(&settings, "hit_value") != ~(int)0) {
        map->hit_value = bm_map_get(&settings, "hit_value");
    }
    if (bm_map_get(&settings, "recharge_duration") != ~(int)0) {
        map->recharge_duration = bm_map_get(&settings, "recharge_duration");
    }
    if (bm_map_get(&settings, "mining_time") != ~(int)0) {
        map->mining_time = bm_map_get(&settings, "mining_time");
    }
    if (bm_map_get(&settings, "stay_health_drop") != ~(int)0) {
        map->stay_health_drop = bm_map_get(&settings, "stay_health_drop");
    }
    if (bm_map_get(&settings, "movement_health_drop") != ~(int)0) {
        map->movement_health_drop =
                bm_map_get(&settings, "movement_health_drop");
    }
    if (bm_map_get(&settings, "step_standard_delay") != ~(int)0) {
        map->step_standard_delay =
                bm_map_get(&settings, "step_standard_delay");
    }
    if (bm_map_get(&settings, "moratory_duration") != ~(int)0) {
        map->moratory_duration =
                bm_map_get(&settings, "moratory_duration");
    }
    bm_map_free(&settings);

    bm_vector_init(&map->engine_items);
    while (!feof(file)) {
        item = malloc(sizeof(struct bm_engine_item));
        fscanf(file, "%u %u %d", &item->y, &item->x, &item->value);
        item->father = -1;
        if (!feof(file)) {
            item->type = BM_ITEM_MED;
            bm_vector_push(&map->engine_items, item);
        } else {
            free(item);
        }
    }
    fclose(file);
    return 0;
}

void bm_engine_map_free(struct bm_engine_map *map) {
    size_t i;
    for (i = 0; i < map->height; ++i) {
        free(map->map[i]);
    }
    free(map->map);
    for (i = 0; i < map->engine_items.pushed; ++i) {
        struct bm_engine_item *item = map->engine_items.ptr[i];
        free(item);
    }
    bm_vector_free(&map->engine_items);
}

void bm_set_random_player_position(struct bm_engine_data *data) {
    unsigned int y, x, p, freecells, fci, pos;
    freecells = data->map.freecells;
    for (p = 0; p < data->plr.pushed; ++p) {
        struct bm_engine_player *player = data->plr.ptr[p];
        fci = 0;
        pos = rand() % freecells;
        for (y = 0; y < data->map.height; ++y) {
            for (x = 0; x < data->map.width; ++x) {
                if (fci == pos) {
                    player->y = y;
                    player->x = x;
                }
                if (data->map.map[y][x] == ' ') {
                    ++fci;
                }
            }
        }
        --freecells;
    }
}

void bm_player_init(struct bm_engine_data *data) {
    unsigned int p;
    for (p = 0; p < data->plr.pushed; ++p) {
        struct bm_engine_player *player = data->plr.ptr[p];
        player->cdata.boom = 0;
        player->cdata.health = data->map.initial_health;
        player->cdata.mines = data->map.initial_mines;
        player->last_action_step = 0;
    }
}

void bm_draw_cdata(struct bm_engine_data *data, size_t player_id) {
    int xshift, yshift, x, y;
    struct bm_engine_player *player = data->plr.ptr[player_id];
    for (yshift = 0 - BM_LOOK; yshift <= BM_LOOK; ++yshift) {
        for (xshift = 0 - BM_LOOK; xshift <= BM_LOOK; ++xshift) {
            x = player->x + xshift;
            y = player->y + yshift;
            if ((y < 0) || (y >= (int)data->map.height) ||
                    (x < 0) || (x >= (int)data->map.width)) {
                player->cdata.screen[BM_LOOK + yshift][BM_LOOK + xshift] = ' ';
            } else {
                player->cdata.screen[BM_LOOK + yshift][BM_LOOK + xshift]
                        = data->render[y][x];
            }
        }
    }

    for (x = 0; x < (int)data->map.engine_items.pushed; ++x) {
        struct bm_engine_item *item = data->map.engine_items.ptr[x];
        if ((item->type == BM_ITEM_MINE) && (item->father == (int)player_id)) {
            xshift = item->x - player->x;
            yshift = item->y - player->y;
            if ((0 <= BM_LOOK + yshift) && (BM_LOOK + yshift <=  2 * BM_LOOK) &&
                (0 <= BM_LOOK + xshift) && (BM_LOOK + xshift <=  2 * BM_LOOK)) {
                player->cdata.screen[BM_LOOK + yshift][BM_LOOK + xshift]  = '*';
            }
        }
    }

    player->cdata.screen[BM_LOOK][BM_LOOK] = 'I';
}

void bm_engine_steps(struct bm_engine_data *data, size_t steps) {
    size_t i;
    for (i = 0; i < data->plr.pushed; ++i) {
        int delta_health = steps * data->map.stay_health_drop;
        struct bm_engine_player* player = data->plr.ptr[i];
        if (player->cdata.boom > 0) {
            --player->cdata.boom;
        }
        if (player->cdata.health >= delta_health) {
            player->cdata.health -= delta_health;
        } else {
            player->cdata.health = 0;
        }
    }

    for (i = 0; i < data->map.engine_items.pushed;) {
        struct bm_engine_item *item = data->map.engine_items.ptr[i];
        if ((item->type == BM_ITEM_BOOM) || (item->type == BM_ITEM_MINE_BOOM)) {
            ++item->value;
            if (item->value == BM_BOOM_DRAW_STEPS) {
                bm_vector_delete(&data->map.engine_items, i);
                free(item);
            } else {
                ++i;
            }
        } else {
            ++i;
        }
    }
}

void bm_engine_move_player(char dest, struct bm_engine_data *data, size_t id) {
    int i, newx, newy;
    struct bm_engine_player *player = data->plr.ptr[id];

    newx = player->x;
    newy = player->y;
    switch (dest) {
    case 'U':
        --newy;
        break;
    case 'R':
        ++newx;
        break;
    case 'D':
        ++newy;
        break;
    case 'L':
        --newx;
        break;
    }
    if ((newx < 0) || (newx >= (int)data->map.width) ||
            (newy < 0) || (newy >= (int)data->map.height)) {
        return;
    }
    if (data->map.map[newy][newx] == '#') {
        return;
    }
    for (i = 0; i < (int)data->plr.pushed; ++i) {
        struct bm_engine_player *player = data->plr.ptr[i];
        if (((int)player->x == newx)
                && ((int)player->y == newy)
                && (player->cdata.health != 0)) {
            return;
        }
    }

    player->x = newx;
    player->y = newy;
    player->last_action_step = data->global_steps;

    for (i = 0; i < (int)data->map.engine_items.pushed; ++i) {
        struct bm_engine_item *item = data->map.engine_items.ptr[i];
        if ((item->type == BM_ITEM_MINE) && ((int)item->x == newx)
                && ((int)item->y == newy)) {
            /* BOOOOM !!!! */
            if (player->cdata.health >= (int)data->map.hit_value) {
                player->cdata.health -= (int)data->map.hit_value;
            } else {
                player->cdata.health = 0;
            }

            item->type = BM_ITEM_MINE_BOOM;
        }
    }

}

void bm_engine_battle_boom(struct bm_engine_data *data, size_t client_id) {
    struct bm_engine_player *player = data->plr.ptr[client_id];
    if (player->cdata.boom == 0) {
        size_t i;
        struct bm_engine_item *item = malloc(sizeof(struct bm_engine_item));
        unsigned short was[BM_MINES_LOOK * 2 + 1][BM_MINES_LOOK * 2 + 1];
        struct bm_pair
                queue[(BM_MINES_LOOK * 2 + 1) * (BM_MINES_LOOK * 2 + 1) + 1];
        int ql = 0, qr = 0, stx, sty;
        player->cdata.boom = data->map.recharge_duration;
        player->last_action_step = data->global_steps;
        item->type = BM_ITEM_BOOM;
        item->value = 0;
        item->x = player->x;
        item->y = player->y;
        bm_vector_push(&data->map.engine_items, item);

        memset(was, 0, sizeof(unsigned short)
               * (BM_MINES_LOOK * 2 + 1) * (BM_MINES_LOOK * 2 + 1));
        item->type = BM_ITEM_BOOM;
        item->value = 0;
        queue[qr].a = item->x = stx = player->x;
        queue[qr].b = item->y = sty = player->y;
        stx -= BM_MINES_LOOK;
        sty -= BM_MINES_LOOK;
        was[queue[qr].b - sty][queue[qr].a - stx] = BM_MINES_LOOK;
        ++qr;
        while (ql != qr) {
            int shiftx, shifty;
            int x, y, waslast;
            for (shiftx = -1; shiftx <= 1; ++shiftx) {
                for (shifty = -1; shifty <= 1; ++shifty) {
                    /* shortest path BY CELLS */
                    if (abs(shiftx + shifty) == 1) {
                        x = shiftx + queue[ql].a;
                        y = shifty + queue[ql].b;
                        waslast = was[queue[ql].b - sty][queue[ql].a - stx];
                        if ((0 <= x) && (x < (int)data->map.width)
                                && (0 <= y) && (y < (int)data->map.height)
                                && (was[y - sty][x - stx] == 0)
                                && (data->map.map[y][x] != '#')
                                && (waslast - 1 > 0)) {
                            was[y - sty][x - stx] = waslast - 1;
                            queue[qr].a = x;
                            queue[qr].b = y;
                            ++qr;
                        }
                    }
                }
            }
            ++ql;
        }

        for (i = 0; i < data->plr.pushed; ++i) {
            if (i != client_id) {
                struct bm_engine_player *player = data->plr.ptr[i];
                if ((0 <= (int)player->x - stx)
                        && ((int)player->x - stx < 2 * BM_MINES_LOOK + 1)
                        && (0 <= (int)player->y - sty)
                        && ((int)player->y - sty < 2 * BM_MINES_LOOK + 1)) {
                    int delta = (was[player->y - sty][player->x - stx]
                            * data->map.hit_value) / BM_MINES_LOOK;
                    if (player->cdata.health >= delta) {
                        player->cdata.health -= delta;
                    } else {
                        player->cdata.health = 0;
                    }
                }
            }
        }
    }
}

void bm_engine_set_mine(struct bm_engine_data *data, size_t client_id) {
    struct bm_engine_item *item = malloc(sizeof(struct bm_engine_item));
    struct bm_engine_player *player = data->plr.ptr[client_id];

    if (player->cdata.mines > 0) {
        --player->cdata.mines;
        player->last_action_step = data->global_steps + data->map.mining_time;
        item->type = BM_ITEM_MINE;
        item->value = 0;
        item->x = player->x;
        item->y = player->y;
        item->father = client_id;
        bm_vector_push(&data->map.engine_items, item);
        /* player can be destroyed be his mine. it is nornal */
    }
}

void bm_engine_medicine(struct bm_engine_data *data, size_t client_id) {
    size_t i;
    struct bm_engine_player *player = data->plr.ptr[client_id];
    for(i = 0; i < data->map.engine_items.pushed; ++i) {
        struct bm_engine_item *item = data->map.engine_items.ptr[i];
        if ((item->type == BM_ITEM_MED)
                && (item->x == player->x) && (item->y == player->y)) {
            if (player->cdata.health + item->value >= 0) {
                player->cdata.health += item->value;
            } else {
                player->cdata.health = 0;
            }
            bm_vector_delete(&data->map.engine_items, i);
            free(item);
            return;
        }
    }
}

void bm_engine_data_render(struct bm_engine_data *data) {
    size_t y, x;
    int i, j;
    for (y = 0; y < data->map.height; ++y) {
        for (x = 0; x < data->map.width; ++x) {
            data->render[y][x] = data->map.map[y][x];
        }
    }

    for (x = 0; x < data->map.engine_items.pushed; ++x) {
        struct bm_engine_item *item = data->map.engine_items.ptr[x];
        switch (item->type) {
        case BM_ITEM_MED:
            data->render[item->y][item->x] = '+';
            break;
        case BM_ITEM_MINE:
            /*data->render[item->y][item->x] = '*';*/
            break;
        case BM_ITEM_MINE_BOOM:
            for (i = (int)item->y - BM_BOOM_DRAW_SIZE;
                 i <= (int)item->y + BM_BOOM_DRAW_SIZE; ++i) {
                for (j = (int)item->x - BM_BOOM_DRAW_SIZE;
                     j <= (int)item->x + BM_BOOM_DRAW_SIZE; ++j) {
                    if ((0 <= j) && (j < (int)data->map.width)
                            && (0 <= i) && (i < (int)data->map.height)) {
                        data->render[i][j] = '@';
                    }
                }
            }
            break;
        case BM_ITEM_BOOM: {
            short was[BM_MINES_LOOK * 2 + 1][BM_MINES_LOOK * 2 + 1];
            struct bm_pair
                    queue[(BM_MINES_LOOK*2 + 1) * (BM_MINES_LOOK*2 + 1) + 1];
            int ql = 0, qr = 0, stx, sty;
            memset(was, 0, sizeof(unsigned short)
                   * (BM_MINES_LOOK * 2 + 1) * (BM_MINES_LOOK * 2 + 1));
            queue[qr].a = stx = item->x;
            queue[qr].b = sty = item->y;
            stx -= BM_MINES_LOOK;
            sty -= BM_MINES_LOOK;

            was[queue[qr].b - sty][queue[qr].a - stx] = BM_MINES_LOOK - 1;
            ++qr;
            while (ql != qr) {
                int shiftx, shifty;
                int x, y, waslast;
                for (shiftx = -1; shiftx <= 1; ++shiftx) {
                    for (shifty = -1; shifty <= 1; ++shifty) {
                        /* shortest path BY CELLS */
                        if (abs(shiftx + shifty) == 1) {
                            x = shiftx + queue[ql].a;
                            y = shifty + queue[ql].b;

                            waslast = was[queue[ql].b - sty][queue[ql].a - stx];
                            if ((0 <= x) && (x < (int)data->map.width)
                                    && (0 <= y) && (y < (int)data->map.height)
                                    && (was[y - sty][x - stx] == 0)
                                    && (data->map.map[y][x] != '#')
                                    && (waslast - 1 > 0)) {
                                was[y - sty][x - stx] = waslast - 1;
                                data->render[y][x] = '@';
                                queue[qr].a = x;
                                queue[qr].b = y;
                                ++qr;
                            }
                        }
                    }
                }
                ++ql;
            }
            }
            break;
        }
    }

    for (x = 0; x < data->plr.pushed; ++x) {
        struct bm_engine_player *ptr = data->plr.ptr[x];
        if (ptr->cdata.health != 0) {
            data->render[ptr->y][ptr->x] = 'I';
        } else {
            data->render[ptr->y][ptr->x] = 'X';
        }
    }

    for (x = 0; x < data->plr.pushed; ++x) {
        bm_draw_cdata(data, x);
    }
}

int bm_engine_data_init(char* fname, struct bm_engine_data *data,
                        size_t players_count) {
    int i, err = bm_load_map_from_file(fname, &data->map);
    if (err) {
        return err;
    }

    data->global_steps = 0;
    data->render = malloc(sizeof(char*) * data->map.height);
    for (i = 0; i < (int)data->map.height; ++i) {
        data->render[i] = malloc(sizeof(char) * data->map.width);
    }
    bm_vector_init(&data->plr);
    for (i = 0; i < (int)players_count; ++i) {
        struct bm_engine_player *player
                = calloc(1, sizeof(struct bm_engine_player));
        if (player == NULL) {
            printf("malloc fail\n");
            exit(9);
        }
        bm_vector_push(&data->plr, player);
    }

    bm_set_random_player_position(data);
    bm_player_init(data);
    bm_engine_data_render(data);
    return 0;
}

void bm_engine_data_free(struct bm_engine_data *data) {
    int i;

    for (i = 0; i < (int)data->map.height; ++i) {
        free(data->render[i]);
    }
    free(data->render);

    bm_engine_map_free(&data->map);
    for (i = 0; i < (int)data->plr.pushed; ++i) {
        struct bm_engine_player *player = data->plr.ptr[i];
        free(player);
    }
    bm_vector_free(&data->plr);

}


