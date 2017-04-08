
struct bm_map{
    struct bm_vector vec;
};

struct bm_map_item{
    char* str;
    int value;
};

void bm_map_init(struct bm_map *map) {
    bm_vector_init(&map->vec);
}

void bm_map_free(struct bm_map *map) {
    size_t i;
    for (i = 0; i < map->vec.pushed; ++i) {
        struct bm_map_item *item = map->vec.ptr[i];
        free(item->str);
        free(item);
    }
    bm_vector_free(&map->vec);
}

/** insert or set(is exists) value */
void bm_map_set(struct bm_map *map, char *str, int val) {
    struct bm_map_item *item;
    size_t i;
    for (i = 0; i < map->vec.pushed; ++i) {
        item = map->vec.ptr[i];
        if (strcmp(item->str, str) == 0) {
            item->value = val;
            return;
        }
    }

    /* doesn`t exist */
    item = malloc(sizeof(struct bm_map_item));
    if (item == NULL) {
        printf("malloc failed\n");
        exit(9);
    }
    item->str = malloc(strlen(str) + 1);
    if (item->str == NULL) {
        printf("malloc failed\n");
        exit(9);
    }
    strcpy(item->str, str);
    item->value = val;
    bm_vector_push(&map->vec, item);
}

int bm_map_get(struct bm_map *map, char *str) {
    struct bm_map_item *item;
    size_t i;
    for (i = 0; i < map->vec.pushed; ++i) {
        item = map->vec.ptr[i];
        if (strcmp(item->str, str) == 0) {
            return item->value;
        }
    }
    return ~0;
}
