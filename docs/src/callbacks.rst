.. |br| raw:: html

   <br />

Callbacks
=========

Every module requires following functions to be defined by developers. |br|
If using :ref:`easy` API, they are automatically declared by M_MOD() macro. |br|
Moreover, a m_mod_pre_start function is declared too, but it is not needed by libmodule interface, ie: it can be left undefined. Your compiler may warn you about that though. |br|
Note that libmodule only mandates m_evt_cb() callback. |br|
Leaving other callbacks as NULL means starting module right away with no further checks. |br|

.. code::

    void m_mod_pre_start(void);
    bool m_start_cb(m_mod_t *mod);
    bool m_eval_cb(m_mod_t *mod);
    void m_evt_cb(m_mod_t *mod, const m_evt_t *const evt);
    void m_stop_cb(m_mod_t *mod);

.. c:function:: m_mod_pre_start(void)

  This function will be called before any module is registered. |br|
  It is the per-module version of :ref:`m_ctx_pre_start <m_ctx_pre_start>` function.

.. c:function:: m_start_cb(mod)

  Initializes module state; useful to register any event source. |br|
  It is called whenever module is started. |br|
  If m_mod_on_start callback returns false, module is automatically stopped as its initialization failed.

  :param: :c:type:`m_mod_t *` mod: pointer to module.

  :returns: true if the module should be started, false otherwise.

.. c:function:: m_eval_cb(mod)

  This function is called for each IDLE module after evey state machine update, |br|
  and it should check whether a module is now ready to be started. |br|

  :param: :c:type:`m_mod_t *` mod: pointer to module.

  :returns: true if module is now ready to be started, false otherwise.
  
.. c:function:: m_evt_cb(mod, evt)

  Poll callback, called when any event is ready to be received by a module. |br|
  Use evt->type to establish which event source triggered the event. |br|
  Note that evt is memory-ref'd. Thus, if you want to keep a message alive, you are able to m_mem_ref() it. |br|
  Remember to unref it when done or you will cause a leak. |br|

  :param: :c:type:`m_mod_t *` mod: pointer to module.
  :param: :c:type:`const m_evt_t * const` evt: pointer to event.

.. c:function:: m_stop_cb(mod)

  Called whenever module gets stopped, either by deregistration or m_mod_stop(). |br|
  Useful to cleanup module state, eg: freeing some data or closing fds registered without M_SRC_FD_AUTOCLOSE flag. |br|

  :param: :c:type:`m_mod_t *` mod: pointer to module.