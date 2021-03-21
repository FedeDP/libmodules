.. |br| raw:: html

   <br />
   
Modules API
===========

Modules API denotes libmodule interface functions to manage context loop. |br|
Like Module API, it has an easy, single-context API. Moreover, it has an equivalent multi-context API. |br|
All these functions but modules_pre_start() return a :ref:`mod_ret <mod_ret>`. |br|

Moreover, there are a bunch of functions that are common to every context (thus not needing ctx_name param). |br|

.. _modules_pre_start:

.. c:function:: modules_pre_start(void)

  This function will be called by libmodule before doing anything else.
  It can be useful to set some global state/read config that are needed to decide whether to start a module.
  You only need to define this function and it will be automatically called by libmodule.
  
.. c:function:: main(argc, argv)

  Libmodule's provide its own main implementation. This means that, on simplest cases, you'll only have to write your modules and compile linking to libmodule.
  Libmodule's main() is quite simple: it just runs modules_ctx_loop() on every registered context (in different pthreads for multiple contexts).
  Note that this is a weak symbol, thus it is obviously overridable by users.
  
  :param argc: number of cmdline arguments.
  :param argv: cmdline arguments.
  :type argc: :c:type:`int`
  :type argv: :c:type:`char *[]`

.. c:function:: modules_set_memhook(hook)

  Set memory management functions. By default: malloc, realloc, calloc and free are used.
  
  :param hook: new memory management hook.
  :type hook: :c:type:`const memhook_t *`

Easy API
--------

Modules easy API should be used in conjunction with :ref:`module_easy`. |br|
It abstracts all of libmodule internals mechanisms to provide an easy-to-use and simple API. It can be found in <module/modules_easy.h> header.
  
.. c:macro:: modules_set_logger(logger)

  Set a logger. By default, module's log prints to stdout. 
  
  :param logger: logger function.
  :type logger: :c:type:`const log_cb`
  
.. c:macro:: modules_loop(void)

  Start looping on events from modules. Note that as soon as modules_loop is called, a message with type == LOOP_STARTED will be broadcasted to all context's modules.
  Moreover, loop is automatically stopped (with return code 0) if there are no more RUNNING modules in its context.
  
.. c:macro:: modules_quit(quit_code)

  Leave libmodule's events loop. Note that as soon as it is called, a message with type == LOOP_STOPPED will be broadcasted to all context's modules.
  
  :param quit_code: exit code that should be returned by modules_loop.
  :type quit_code: :c:type:`const uint8_t`
  
.. c:macro:: modules_get_fd(fd)

  Retrieve internal libmodule's events loop fd. Useful to integrate libmodule's loop inside client's own loop.
  
  :param fd: pointer in which to store libmodule's fd
  :type fd: :c:type:`int *`
  
.. c:macro:: modules_dispatch(ret)

  Dispatch libmodule's messages. Useful when libmodule's loop is integrated inside an external loop. This is a non-blocking function (ie: if no data is available to be dispatched, it will return).
  
  :param ret: ret >= 0 and MOD_OK returned -> number of dispatched messages. ret >= 0 and MOD_ERR returned -> loop has been quitted by a modules_quit() code, thus it returns quit_code. Ret < 0 and MOD_ERR returned: an error happened.
  :type ret: :c:type:`int *`
  
.. c:macro:: modules_dump()

  Dump default context info. Diagnostic API. Output explanation:  
  * Q -> "Quit", L -> "Looping", M -> "Max Events"
  
  :param ret: ret >= 0 and MOD_OK returned -> number of dispatched messages. ret >= 0 and MOD_ERR returned -> loop has been quitted by a modules_quit() code, thus it returns quit_code. Ret < 0 and MOD_ERR returned: an error happened.
  :type ret: :c:type:`int *`

.. c:macro:: m_load(path)
  
  Attaches a new module from a .so file to "default" context. If module has a different context, this will be an error.
  
  :param path: shared object path.
  :type path: :c:type:`const char *`
  
.. c:macro:: m_unload(path)
  
  Detaches a module loaded from a .so file from "default" context. If module was loaded in a different context, MOD_NO_MOD will be returned.
  
  :param path: shared object path.
  :type path: :c:type:`const char *`
  
Multi-context API
-----------------

Modules multi-context API let you manage your contexts in a very simple way. It is exposed by <module/modules.h> header. |br|
It exposes very similar functions to single-context API (again, single-context is only a particular case of multi-context), that now take a "context_name" parameter.
  
.. c:function:: modules_ctx_set_logger(ctx_name, logger)

  Set a logger for a context. By default, module's log prints to stdout.
  
  :param ctx_name: context name.
  :param logger: logger function.
  :type ctx_name: :c:type:`const char *`
  :type logger: :c:type:`const log_cb`
  
.. c:macro:: modules_ctx_loop(ctx_name)

  Start looping on events from modules. Note that this is just a macro that calls modules_ctx_loop_events with MODULE_MAX_EVENTS (64) events.
  Moreover, loop is automatically stopped (with return code 0) if there are no more RUNNING modules in its context.
  
  :param ctx_name: context name.
  :type ctx_name: :c:type:`const char *`
  
.. c:function:: modules_ctx_loop_events(ctx_name, maxevents)

  Start looping on events from modules, on at most maxevents events at the same time. Note that as soon as modules_loop is called, a message with type == LOOP_STARTED will be broadcasted to all context's modules.
  Moreover, loop is automatically stopped (with return code 0) if there are no more RUNNING modules in its context.
  
  :param ctx_name: context name.
  :param maxevents: max number of fds wakeup that will be managed at the same time.
  :type ctx_name: :c:type:`const char *`
  :type maxevents: :c:type:`const int`
  
.. c:function:: modules_ctx_quit(ctx_name, quit_code)

  Leave libmodule's events loop. Note that as soon as it is called, a message with type == LOOP_STOPPED will be broadcasted to all context's modules.
  
  :param ctx_name: context name.
  :param quit_code: exit code that should be returned by modules_loop.
  :type ctx_name: :c:type:`const char *`
  :type quit_code: :c:type:`const uint8_t`

.. c:function:: modules_ctx_get_fd(ctx_name, fd)

  Retrieve internal libmodule's events loop fd. Useful to integrate libmodule's loop inside client's own loop.
  
  :param ctx_name: context name.
  :param fd: pointer in which to store libmodule's fd
  :type ctx_name: :c:type:`const char *`
  :type fd: :c:type:`int *`
  
.. c:function:: modules_ctx_dispatch(ctx_name, ret)

  Dispatch libmodule's messages. Useful when libmodule's loop is integrated inside an external loop. This is a non-blocking function (ie: if no data is available to be dispatched, it will return).
  
  :param ctx_name: context name.
  :param ret: ret >= 0 and MOD_OK returned -> number of dispatched messages. ret >= 0 and MOD_ERR returned -> loop has been quitted by a modules_quit() code, thus it returns quit_code. Ret < 0 and MOD_ERR returned: an error happened.
  :type ctx_name: :c:type:`const char *`
  :type ret: :c:type:`int *`

.. c:function:: modules_ctx_dump()

  Dump context info. Diagnostic API. Output explanation:  
  * Q -> "Quit", L -> "Looping", M -> "Max Events"

  
.. c:function:: module_load(ctx_name, path)
  
  Attaches a new module from a .so file to ctx_name context. If module has a different context, this will be an error.
  
  :param path: shared object path.
  :param ctx_name: module's context name.
  :type path: :c:type:`const char *`
  :type ctx_name: :c:type:`const char *`
  
.. c:function:: module_unload(ctx_name, path)
  
  Detaches a module loaded from a .so file. If module was loaded in a different context, MOD_NO_MOD will be returned.
  
  :param path: shared object path.
  :type path: :c:type:`const char *`
