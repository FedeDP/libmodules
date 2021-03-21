.. |br| raw:: html

   <br />

Concepts
========

Module
------

A module is core entity of libmodule: it is a single and indipendent logical unit that reacts to events. |br|
It can be seen as an Actor with the ability of managing non-PubSub events. |br|
It requires some callbacks that are used by libmodule to manage its life. |br|

Context
-------

A context can be seen as a collector for modules. You can loop on events from each context, and each context behaves independently from others. |br|
This can be particularly useful when dealing with 2+ threads; ideally, each thread has its own module's context and thus its own events to be polled. |br|
Modules can only see and reach (through PubSub messaging) other modules from same context. |br|

Loop
----

Libmodule offers an internal loop, started with m_ctx_loop(); note that each context has its own loop. |br|
Moreover, you can even easily integrate it into your own loop: m_ctx_get_fd() will retrieve a pollable fd and POLLIN events will be raised whenever a new message is available. |br|
Remember that before starting your loop, m_ctx_dispatch() should be called, to dispatch initial "LoopStarted" messages to each module. |br|
Then, whenever POLLIN data is available on libmodule's fd, you only need to call m_ctx_dispatch() again. |br|
Finally, remember to close() libmodule's fd retrieved through m_ctx_get_fd(). |br|

Source
------

Each module's listens to events by registering sources. |br|
A source can be a timer, a topic (for PubSub communications between modules), a signal, a socket, etc etc. |br|