struct bm_vector {
    void **ptr;
    size_t pushed;
    size_t allocated;
};

void bm_vector_init(struct bm_vector *vec) {
    vec->ptr = NULL;
    vec->pushed = 0;
    vec->allocated = 0;
}

void bm_vector_free(struct bm_vector *vec) {
    free(vec->ptr);
    vec->pushed = 0;
    vec->allocated = 0;
}

void bm_vector_grow(struct bm_vector *vec) {
    if (vec->allocated != 0) {
        void **newptr = malloc(sizeof(void*) * vec->allocated * 2);
        if (newptr == NULL) {
            printf("malloc failed\n");
            exit(9);
        }
        memcpy(newptr, vec->ptr, sizeof(void*)  * vec->allocated);
        free(vec->ptr);
        vec->ptr = newptr;
        vec->allocated *= 2;
    } else {
        vec->ptr = malloc(sizeof(void*));
        if (vec->ptr == NULL) {
            printf("malloc failed\n");
            exit(9);
        }
        vec->pushed = 0;
        vec->allocated = 1;
    }
}

void bm_vector_delete(struct bm_vector *vec, size_t i) {
    void* ptr;
    if (vec->pushed <= i) {
        printf("vector delete failed\n");
        exit(8);
    }
    ptr = vec->ptr[i];
    vec->ptr[i] = vec->ptr[vec->pushed - 1];
    vec->ptr[vec->pushed - 1] = ptr;
    --vec->pushed;
}

void bm_vector_push(struct bm_vector *vec, void *data) {
    if (vec->allocated == vec->pushed) {
        bm_vector_grow(vec);
    }
    vec->ptr[vec->pushed] = data;
    ++vec->pushed;
}

