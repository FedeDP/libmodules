.. |br| raw:: html

   <br />
   
Mod API
========

Mod API denotes libmodule interface functions to manage each module lifecycle. It can be found in <module/mod.h> header. |br|
Sometime you may avoid using easy API; eg: if you wish to use same source file for different modules, or if you are developing a multi context application. |br|
Where not specified, the API returns a negative errno style error code; 0 indicates success. |br|

.. c:function:: m_mod_register(name, c, mod, hook, flags, userdata)

  Registers a new module in context
    
  :param name: module's name.
  :param c: context where the module will be registered.
  :param mod: handler for this module that will be created by this call.
  :param hook: struct that holds the module's user callbacks.
  :param flags: flags for the module.
  :param userdata: module's userdata.
  :type name: :c:type:`const char *`
  :type c: :c:type:`m_ctx_t *`
  :type mod: :c:type:`m_mod_t **`
  :type hook: :c:type:`const m_mod_hook_t *`
  :type flags: :c:type:`m_mod_flags`
  :type userdata: :c:type:`const void *`
  
.. c:function:: m_mod_deregister(mod)

  Deregisters a module
    
  :param mod: pointer to module's handler. It is set to NULL after this call.
  :type mod: :c:type:`m_mod_t **`

.. c:function:: m_mod_load(mod, module_path, flags, ref)

  Runtime links a module from a shared object file

  :param mod: pointer to module's handler.
  :param module_path: filesystem path of shared object file.
  :param flags: flags to forcefully set on newly loaded module.
  :param ref: if not NULL; a loaded module's reference will be stored here. The ref must later be unref. See m_mem_unref().
  :type mod: :c:type:`const m_mod_t *`
  :type mod: :c:type:`const char *`
  :type mod: :c:type:`m_mod_flags`
  :type mod: :c:type:`m_mod_t **`

.. c:function:: m_mod_unload(mod, module_path)

  Runtime unlinks a module from a shared object file

  :param mod: pointer to module's handler.
  :param module_path: filesystem path of shared object file.
  :type mod: :c:type:`const m_mod_t *`
  :type mod: :c:type:`const char *`

.. c:function:: m_mod_ctx(mod)

  Retrieves ctx handler from mod handler.

  :param mod: pointer to module's handler.
  :type mod: :c:type:`const m_mod_t *`

  returns: pointer to module's ctx.

.. c:function:: m_mod_name(mod)

  Retrieves module's name from mod handler.

  :param mod: pointer to module's handler.
  :type mod: :c:type:`const m_mod_t *`

  returns: pointer to module's name.
  
.. c:function:: m_mod_is(mod, state)

  Check current module's state.
    
  :param mod: pointer to module's handler.
  :param state: state we are interested in; note that it can be an OR of states (eg: M_MOD_IDLE | M_MOD_RUNNING)
  :type mod: :c:type:`const m_mod_t *`
  :type state: :c:type:`m_mod_states`
  :returns: false if module state is not 'state', true if it is.
  
.. c:function:: m_mod_start(mod)

  Starts module.
    
  :param mod: pointer to module's handler
  :type mod: :c:type:`m_mod_t *`
  
.. c:function:: m_mod_pause(mod)

  Pauses module.
    
  :param mod: pointer to module's handler
  :type mod: :c:type:`m_mod_t *`
  
.. c:function:: m_mod_resume(mod)

  Resumes module.
    
  :param mod: pointer to module's handler
  :type mod: :c:type:`m_mod_t *`
  
.. c:function:: m_mod_stop(mod)

  Stops module and reset its state (eventually destroying any registered source).
  Moreover, its enqueued pubsub messages are destroyed.
  Note that module itself is not destroyed: you can register new sources and call m_mod_start again.

  :param mod: pointer to module's handler
  :type mod: :c:type:`m_mod_t *`

.. c:function:: m_mod_log(mod, fmt, ...)

  Logs a string from a module using context specific logger.

  :param mod: pointer to module's handler
  :param fmt: string's format
  :type mod: :c:type:`const m_mod_t *`
  :type fmt: :c:type:`const char *`

.. c:function:: m_mod_dump(mod)

  Dumps all module's info using default context logger. This is just a debugging helper.

  :param mod: pointer to module's handler
  :type mod: :c:type:`const m_mod_t *`

.. c:function:: m_mod_stats(mod, stats)

  Stores in stats pointer some module's statistics.

  :param mod: pointer to module's handler
  :param stats: storage for stats
  :type mod: :c:type:`const m_mod_t *`
  :type fmt: :c:type:`m_mod_stats_t *`

.. c:function:: m_mod_userdata(mod)

  Retrieves a module's userdata.

  :param mod: pointer to module's handler
  :type mod: :c:type:`const m_mod_t *`

  :returns: module's userdata pointer.

.. c:function:: m_mod_ref(mod, name)

  Takes a reference on another module.

  :param mod: pointer to module's handler
  :param name: name of desired module
  :type mod: :c:type:`const m_mod_t *`
  :type name: :c:type:`const char *`

  :returns: reference to module's named 'name' if exists, or NULL.

.. c:function:: m_mod_become(mod, new_on_evt)

  Push new_on_evt callback on module's stack, updating its behaviour.
    
  :param mod: pointer to module's handler
  :param new_on_evt: new module's on_evt callback.
  :type mod: :c:type:`m_mod_t *`
  :type new_on_evt: :c:type:`m_evt_cb`

.. c:function:: m_mod_unbecome(mod)

  Pop a callback from module's stack, updating its behaviour.

  :param mod: pointer to module's handler
  :type mod: :c:type:`m_mod_t *`

.. c:function:: m_mod_ps_tell(mod, recipient, msg, flags)

  Tell a message to recipient module.
    
  :param mod: pointer to module's handler.
  :param recipient: module to whom deliver the message.
  :param msg: actual data to be sent.
  :param flags: message's flags.
  :type mod: :c:type:`m_mod_t *`
  :type recipient: :c:type:`const m_mod_t *`
  :type msg: :c:type:`const void *`
  :type autofree: :c:type:`m_ps_flags`
  
.. c:function:: m_mod_ps_publish(mod, topic, msg, flags)

  Publish a message on a topic.

  :param mod: pointer to module's handler.
  :param topic: topic on which publish the message.
  :param msg: actual data to be sent.
  :param flags: message's flags.
  :type mod: :c:type:`m_mod_t *`
  :type topic: :c:type:`const char *`
  :type msg: :c:type:`const void *`
  :type autofree: :c:type:`m_ps_flags`
  
.. c:function:: m_mod_ps_broadcast(mod, msg, flags)

  Broadcast a message to all modules in mod's context.

  :param mod: pointer to module's handler.
  :param msg: actual data to be sent.
  :param flags: message's flags.
  :type mod: :c:type:`m_mod_t *`
  :type msg: :c:type:`const void *`
  :type autofree: :c:type:`m_ps_flags`
  
.. c:function:: m_mod_ps_poisonpill(mod, recipient)

  Send a M_PS_MOD_POISONPILL message to recipient. This allows to stop a module after it flushes its pubsub messages.
  
  :param mod: pointer to module's handler
  :param recipient: M_MOD_RUNNING module to be stopped.
  :type mod: :c:type:`m_mod_t *`
  :type recipient: :c:type:`const m_mod_t *`

.. c:function:: m_mod_stash(mod, evt)

  Stash a message to avoid dealing with it now, but store it for later usage.
  Note that for obvious reasons, M_SRC_TYPE_FD events cannot be stashed (they must be dealt with to avoid a close loop).

  :param mod: pointer to module's handler.
  :param evt: evt as received in on_evt() callback.
  :type mod: :c:type:`m_mod_t *`
  :type evt: :c:type:`const m_evt_t *`

.. c:function:: m_mod_unstash(mod)

  Calls on_evt() callback with oldest stashed event.

  :param mod: pointer to module's handler
  :type mod: :c:type:`m_mod_t *`

.. c:function:: m_mod_unstashall(mod)

  Calls on_evt() callback for all stashed events, in a FIFO manner.

  :param mod: pointer to module's handler
  :type mod: :c:type:`m_mod_t *`

