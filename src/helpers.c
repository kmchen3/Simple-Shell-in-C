// Your helper functions need to be here.

#include "helpers.h"

int Comparator(const void *second1temp, const void *second2temp) {
    bgentry_t *second1 = (bgentry_t *)second1temp;
    bgentry_t *second2 = (bgentry_t *)second2temp;

    if (second1->seconds < second2->seconds) {
        return -1;
    }
    else if (second1->seconds > second2->seconds) {
        return 1;
    }
    return 0;
}

void Printer(void *data, void *temp) {
    print_bgentry((bgentry_t *)data);
}

void Deleter(void *temp) {
    free_job(((bgentry_t *)temp)->job);
    free(temp);
}

int findIndex(pid_t wait_result, list_t *list) {
    int index = 0;
    if (list == NULL) {
        return -1;
    }
    node_t *current = list->head;
    while (current != NULL) {
        if (((bgentry_t *)current->data)->pid == wait_result) {
            return index;
        }
        current = current->next;
        index++;
    }
    return -1;
}