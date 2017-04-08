struct bm_queue{
    struct bm_queue_vertex *begin;
    struct bm_queue_vertex *end;
    size_t size;
};

struct bm_queue_vertex {
    struct bm_queue_vertex *next;
    void* data;
};

void bm_queue_init(struct bm_queue *queue) {
    queue->begin = NULL;
    queue->end = NULL;
    queue->size = 0;
}

void bm_queue_push(struct bm_queue *queue, void *data) {
    struct bm_queue_vertex *new_vertex = malloc(sizeof(struct bm_queue));
    if (new_vertex == NULL) {
        printf("malloc failed\n");
        exit(9);
    }
    new_vertex->data = data;
    new_vertex->next = NULL;
    if (queue->size) {
        queue->end->next = new_vertex;
        queue->end = new_vertex;
    } else {
        queue->begin = queue->end = new_vertex;
    }
    ++queue->size;
}

void* bm_queue_pop(struct bm_queue *queue) {
    struct bm_queue_vertex *ret_vertex;
    void* ret_value;

    if (!queue->size) {
        return NULL;
    }

    ret_vertex = queue->begin;
    ret_value = queue->begin->data;
    --queue->size;
    queue->begin = queue->begin->next;
    free(ret_vertex);
    return ret_value;
}

void bm_queue_free(struct bm_queue *queue) {
    while (queue->begin != NULL) {
        bm_queue_pop(queue);
    }
}
