.. |br| raw:: html

   <br />

Lifecycle
=========

Easy API
--------

We previously saw that every module exposes a set of callbacks. |br|
We will now go deeper, better understanding a module's lifecycle. |br|
First of all, module lifecycle is automatically managed by libmodule; moreover, when using :ref:`easy` API,
module registration/deregistration is completely automated and transparent to developer. |br|
This means that when using it, you will only have to declare a source file as a module and define needed functions. |br|

First function to be called is m_ctx_pre_start(); it is called before any module is registered, and it is the global version of m_mod_pre_start(). |br|
Then, each module's m_mod_pre_start() function is called. At this stage, no module is registered yet. |br|

After every m_mod_pre_start() function is called, libmodule will register every module. |br|
Once a module is registered, it will be initially set to an M_MOD_IDLE state. Idle means that the module is not started yet, thus it cannot receive any event. |br|

As soon as its context starts looping, m_mod_on_eval() function will be called on every M_MOD_IDLE module, trying to start it right away. |br|
m_mod_on_eval() will be then called at each state machine change, for each idle module, until it returns true. |br|

As soon as m_mod_on_eval() returns true, a module is started. It means its m_mod_on_start() function is finally called and its state is set to M_MOD_RUNNING. |br|
When a module reaches RUNNING state, its context loop will finally receive events for its registered sources. |br|

Whenever an event triggers a module's wakeup, its m_mod_on_evt() callback is called. |br|

Finally, when stopping a module, each module's m_mod_on_stop() function is called. |br|
Note that a module is automatically stopped during the process of module's deregistration. |br|

Thus, for Easy API, you should only implement m_mod_on_eval() to return true when it is ready to be started, |br|
then eventually register event sources in m_mod_on_start(), and manage events in m_mod_on_evt(). |br|
If you need to cleanup any data, manage it in m_mod_on_stop(). |br|

Normal API
-----------

When dealing with libmodule's normal API, no context nor module is automatically registered for you, ie: you must manually call m_{ctx,mod}_(de)register() API. |br|
In normal API, you are responsible to register/deregister modules and contexts. |br|
Ideally, a context is registered and some modules are registered in it. Then, m_ctx_loop() gets called on the context, eventually starting any registered module. |br|
Note that a context will automatically stop looping when there are no M_MOD_RUNNING modules in it anymore. |br|

Module States
-------------

As previously mentioned, a registered module, before being started, is in M_MOD_IDLE state. |br|
M_MOD_IDLE state means that it has never been started before; it won't receive any event thus its m_evt_cb will never be called. |br|
When module is started, thus reaching M_MOD_RUNNING state, all its registered sources will finally start to receive events. Sources registered while in M_MOD_RUNNING state are automatically polled. |br|
If a module is paused, reaching M_MOD_PAUSED state, it will stop polling on its event sources, but event sources will still be alive, just not polled. Thus, as soon as module is resumed, all events received during PAUSED state will trigger m_evt_cb. |br|
If a module gets stopped, reaching M_MOD_STOPPED state, it will destroy any registered source and it will be resetted to its initial state. |br|
If you instead wish to stop a module letting it manage any already-enqueued event, you need to send a POISONPILL message to it, through m_mod_poisonpill() function. |br|
The difference between M_MOD_IDLE and M_MOD_STOPPED states is that M_MOD_IDLE modules will be evaluated to be started during context loop, while M_MOD_STOPPED modules won't. |br|
When a module is deregistered, it will reach a final M_MOD_ZOMBIE state. It means that the module is no more reachable neither usable, but it can still be referenced by any previously sent message (or any m_mod_ref()). |br|
After all module's sent messages are received by respective recipients, module will finally be destroyed and its memory freed, unless there still is any m_mod_unref() to be called on it. |br|
You can only call m_mod_is(), m_mod_name() and m_mod_ctx() on a ZOMBIE module. |br|

m_mod_start() needs to be called on an M_MOD_IDLE or M_MOD_STOPPED module. |br|
m_mod_pause() needs to be called on a M_MOD_RUNNING module. |br|
m_mod_resume() needs to  be called on a M_MOD_PAUSED module. |br|
m_mod_stop() needs to be called on a M_MOD_RUNNING or M_MOD_PAUSED module. |br|
