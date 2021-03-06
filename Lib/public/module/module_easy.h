#pragma once

#include "module.h"

/* Interface Macros */
#define self() _self

#define MODULE_CTX(name, ctx) \
static void init(void); \
static bool check(void); \
static bool evaluate(void); \
static void receive(const msg_t *const msg, const void *userdata); \
static void destroy(void); \
static const self_t *_self = NULL; \
static void _ctor3_ constructor(void) { \
    if (check()) { \
        userhook_t hook = { init, evaluate, receive, destroy }; \
        module_register(name, ctx, (self_t **)&self(), &hook); \
    } \
} \
static void _dtor1_ destructor(void) { module_deregister((self_t **)&self()); } \
static void _ctor2_ module_pre_start(void)

#define MODULE(name) MODULE_CTX(name, MODULES_DEFAULT_CTX)

/* Defines for easy API (with no need bothering with both _self and ctx) */
#define m_load(path)                            module_load(path, MODULES_DEFAULT_CTX)
#define m_unload(path)                          module_unload(path) // just an alias

#define m_is(state)                             module_is(self(), state)
#define m_dump()                                module_dump(self())

#define m_start()                               module_start(self())
#define m_pause()                               module_pause(self())
#define m_resume()                              module_resume(self())
#define m_stop()                                module_stop(self())

#define m_set_userdata(userdata)                module_set_userdata(self(), userdata)

#define m_register_fd(fd, autoclose, data)      module_register_fd(self(), fd, autoclose, data)
#define m_deregister_fd(fd)                     module_deregister_fd(self(), fd)

#define m_log(...)                              module_log(self(), ##__VA_ARGS__)

#define m_ref(name, modref)                     module_ref(self(), name, modref)

#define m_become(x)                             module_become(self(), receive_##x)
#define m_unbecome()                            module_unbecome(self())

#define m_subscribe(topic)                      module_subscribe(self(), topic)
#define m_unsubscribe(topic)                    module_unsubscribe(self(), topic)
#define m_tell(recipient, msg, size, free)      module_tell(self(), recipient, msg, size, free)
#define m_publish(topic, msg, size, free)       module_publish(self(), topic, msg, size, free)
#define m_broadcast(msg, size, free, global)    module_broadcast(self(), msg, size, free, global)
#define m_poisonpill(recipient)                 module_poisonpill(self(), recipient)
#define m_tell_str(recipient, msg)              module_tell(self(), recipient, (const void *)msg, strlen(msg), false)
#define m_publish_str(topic, msg)               module_publish(self(), topic, (const void *)msg, strlen(msg), false)
#define m_broadcast_str(msg, global)            module_broadcast(self(), (const void *)msg, strlen(msg), false, global)
