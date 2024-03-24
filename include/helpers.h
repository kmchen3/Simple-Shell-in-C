// A header file for helpers.c
// Declare any additional functions in this file

#include "icssh.h"
#include <stdlib.h>
#include "linkedlist.h"

int Comparator(const void *second1, const void *second2);
void Printer(void *data, void *temp);
void Deleter(void *temp);
int findIndex(pid_t wait_result, list_t *list);