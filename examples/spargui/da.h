/* A dynamic array of pointers. */

#ifndef __DYN_ARR_H
#define __DYN_ARR_H

#include <stdlib.h>
#include <stdio.h>

#define __DA_MIN_CAPACITY 8
#define __DA_DEFAULT_CAPACITY 32UL

const size_t POINTER_SIZE = sizeof(void*);


typedef struct {
    size_t size, capacity;
    void **data;
} DynamicArray;


// Resize a dynamic array.
int __da_resize(DynamicArray *arr, size_t new_capacity) {
    if (new_capacity < __DA_MIN_CAPACITY) {
        return -1;
    }

    void **new_data = realloc(arr->data, new_capacity * POINTER_SIZE);
    if (!new_data) {
        return -2;
    }

    arr->data = new_data;
    arr->capacity = new_capacity;
    return 0;
}


// Create a new dynamic array with space for `capacity` elements.
int da_create_capacity(DynamicArray *arr, size_t capacity) {
    arr->size = 0;
    arr->capacity = capacity;
    arr->data = calloc(arr->capacity, POINTER_SIZE);
    if (!arr->data) {
        return -1;
    }

    return 0;
}


// Create a new dynamic array.
int da_create(DynamicArray *arr) {
    return da_create_capacity(arr, __DA_DEFAULT_CAPACITY);
}


// Free `arr`.
void da_free(const DynamicArray *arr) {
    free(arr->data);
}


// Call a free function on each element in `arr`.
void da_free_contents(const DynamicArray *arr, void (*free_func)(void *)) {
    for (size_t i = 0; i < arr->size; i++) {
        free_func(arr->data[i]);
    }
}


// Frees all elements of `arr` and `arr` itself.
void da_free_all(const DynamicArray *arr, void (*free_func)(void *)) {
    da_free_contents(arr, free_func);
    da_free(arr);
}


// Add `value` to the end of `arr`.
int da_append(DynamicArray *arr, void *value) {
    if (arr->size >= arr->capacity) {
        return -1;
    }

    arr->data[arr->size] = value;
    arr->size++;

    if (arr->size >= arr->capacity) {
        int resize_result = __da_resize(arr, arr->capacity * 2);
        if (resize_result) {
            return -2;
        }
    }

    return 0;
}


// Get a value from `arr` at `index` and store it in `dest`.
void* da_get(const DynamicArray *arr, size_t index) {
    if (index < 0 || index >= arr->size) {
        return NULL;
    }

    return arr->data[index];
}


// Set a value in `arr` at `index`.
void da_set(DynamicArray *arr, size_t index, void *value) {
    if (index < 0 || index >= arr->size) {
        return;
    }

    arr->data[index] = value;
}


// Remove a value from `arr` at `index`.
int da_pop(DynamicArray *arr, size_t index) {
    if (index < 0 || index >= arr->size) {
        return -1;
    }

    for (int i = index; i < arr->size-1; i++) {
        arr->data[i] = arr->data[i+1];
    }

    arr->size--;
    if (arr->size > __DA_MIN_CAPACITY && arr->size < arr->capacity / 2) {
        int resize_result = __da_resize(arr, arr->capacity / 2);
        if (resize_result) {
            return -2;
        }
    }

    return 0;
}

#endif
