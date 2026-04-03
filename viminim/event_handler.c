#pragma once
#include <stddef.h>
#include "../vcommons.h"

#define GROWTH_FACTOR 1.5

typedef Value* EventCallback;

typedef struct {
    int event_id;
    EventCallback callback;
} Subscription;

typedef struct {
    Subscription* subscriptions;
    size_t count;
    size_t capacity;
} EventHandler;
EventHandler* event_handler_init(size_t initial_capacity);
int subscribe(EventHandler* handler, int id, EventCallback func);
int unsubscribe(EventHandler* handler, int id, EventCallback func);
void event_call(EventHandler* handler, int id, void* data);
size_t event_subscription_count(EventHandler* handler, int id);
void event_clear(EventHandler* handler, int id);
void event_clear_all(EventHandler* handler);
void event_handler_destroy(EventHandler* handler);


EventHandler* event_handler_init(size_t initial_capacity) {
    EventHandler* handler = (EventHandler*)malloc(sizeof(EventHandler));;
    if (!handler || initial_capacity == 0) {
        return NULL;
    }

    handler->subscriptions = (Subscription*)malloc(initial_capacity * sizeof(Subscription));
    if (!handler->subscriptions) {
        return NULL;
    }

    handler->count = 0;
    handler->capacity = initial_capacity;
    return handler;
}

int subscribe(EventHandler* handler, int id, EventCallback func) {
    if (!handler || !func) {
        return -1;
    }

    // Resize if needed
    if (handler->count >= handler->capacity) {
        size_t new_capacity = (size_t)(handler->capacity * GROWTH_FACTOR);
        Subscription* new_subscriptions = (Subscription*)realloc(
            handler->subscriptions,
            new_capacity * sizeof(Subscription)
        );
        
        if (!new_subscriptions) {
            return -1;
        }

        handler->subscriptions = new_subscriptions;
        handler->capacity = new_capacity;
    }

    handler->subscriptions[handler->count].event_id = id;
    handler->subscriptions[handler->count].callback = val_retain(func);
    handler->count++;

    return 0;
}

int unsubscribe(EventHandler* handler, int id, EventCallback func) {
    if (!handler || !func) {
        return -1;
    }
    for (size_t i = 0; i < handler->count; i++) {
        if (handler->subscriptions[i].event_id == id &&
            handler->subscriptions[i].callback == func) {
            val_release(handler->subscriptions[i].callback);
            for (size_t j = i; j < handler->count - 1; j++) {
                handler->subscriptions[j] = handler->subscriptions[j + 1];
            }
            handler->count--;
            return 0;
        }
    }

    return -1;
}

void event_call(EventHandler* handler, int id, void* data) {
    if (!handler) {
        return;
    }

    for (size_t i = 0; i < handler->count; i++) {
        if (handler->subscriptions[i].event_id == id) {
            Value* vid = vint(id);
            Value* vdata = vopaque(data);
            Value* res = call_function_with(mila_globals, handler->subscriptions[i].callback, vid, vdata, NULL);
            val_release(res);
        }
    }
}

size_t event_subscription_count(EventHandler* handler, int id) {
    if (!handler) {
        return 0;
    }

    size_t count = 0;
    for (size_t i = 0; i < handler->count; i++) {
        if (handler->subscriptions[i].event_id == id) {
            count++;
        }
    }

    return count;
}

void event_clear(EventHandler* handler, int id) {
    if (!handler) {
        return;
    }

    for (size_t i = 0; i < handler->count; ) {
        if (handler->subscriptions[i].event_id == id) {
            // Shift remaining elements
            for (size_t j = i; j < handler->count - 1; j++) {
                handler->subscriptions[j] = handler->subscriptions[j + 1];
            }
            handler->count--;
        } else {
            i++;
        }
    }
}

void event_clear_all(EventHandler* handler) {
    if (!handler) {
        return;
    }

    handler->count = 0;
}

void event_handler_destroy(EventHandler* handler) {
    if (!handler) {
        return;
    }

    free(handler->subscriptions);
    handler->subscriptions = NULL;
    handler->count = 0;
    handler->capacity = 0;
}
