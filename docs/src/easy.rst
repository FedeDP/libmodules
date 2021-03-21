.. |br| raw:: html

   <br />

Mod Easy API
============

MOD macro
=========

Libmodule easy API allows simple single-context applications developing. It can be found in <module/mod_easy.h> header. |br|
It offers a

.. code::

    M_MOD(name)

macro that automatically manages a module's lifecycle for you. |br|
Moreover, it declares all needed callbacks as seen in :ref:`callbacks`. |br|
Finally, a m_mod_pre_start function is declared too, but it is not needed by libmodule interface, ie: it can be left undefined. Your compiler may warn you about that though. |br|

.. code::

    static void m_mod_pre_start(void);
    static bool m_mod_on_start(m_mod_t *mod);
    static bool m_mod_on_eval(m_mod_t *mod);
    static void m_mod_on_evt(m_mod_t *mod, const m_evt_t *const msg);
    static void m_mod_on_stop(m_mod_t *mod);

.. c:function:: m_mod_pre_start(void)

  This function will be called before any module is registered. |br|
  It is the per-module version of :ref:`m_ctx_pre_start <m_ctx_pre_start>` function.

Provided Main
=============

Libmodule offers a weak, thus overridable, default main symbol that just loops on default ctx. |br|
To further customize it, a weak m_ctx_pre_loop() function is also available, called by default main. |br|
You can define this function to eg: parse argv parameters or read some config values. |br|

.. c:function:: m_ctx_pre_loop(c, argc, argv)

  Called by default main. |br|
  Useful to initialize your program before libmodule starts looping on default context. |br|

  :param: :c:type:`m_ctx_t *` c: pointer to context.
  :param: :c:type:`int` argc: number of commandline arguments.
  :param: :c:type:`char *[]` argv: array of commandline arguments strings.

