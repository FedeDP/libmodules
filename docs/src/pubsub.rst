.. |br| raw:: html

   <br />

PubSub
======

Concepts
--------

Those unfamiliar with actor like messaging, may wonder what a pubsub messaging is. |br|
PubSub (Publisher-Subscriber) messaging is much like a producer/consumer architecture: some entities publish messages on a topic; |br|
other entities can subscribe to the topic to receive those messages. |br|

Implementation
--------------

Since libmodule 2.1, pubsub implementation is async and makes use of unix pipes. |br|
When sending a message to other modules, a pubsub message is allocated and its address is written in recipient module's writable end of pipe. |br|
The message will then get caught by m_ctx_loop, the address read from readable end of pipe and callback called with the message. |br|

System messages
---------------

Beside M_PS_USER messages (ps_msg_t.type), there are some system messages, respectively: M_PS_CTX_STARTED, M_PS_CTX_STOPPED, M_PS_MOD_STARTED, M_PS_MOD_STOPPED. |br|
These pubsub messages are automatically sent by libmodule when matching functions are called, eg: |br|
* M_PS_CTX_STARTED(STOPPED) is sent whenever a context starts(stops) looping. It is useful to actually start(stop) your pubsub messagging (eg: one module waits on M_PS_CTX_STARTED to send a pubsub message to another module, and so on...). It won't have any valued fields, except for type. |br|
* M_PS_MOD_STARTED(STOPPED) is sent whenever a module starts/resumes(stops/pauses). Again this is useful to inform other modules that a new module is available. |br|
It will have valued type and sender fields; sender will be set to started(stopped) module; it will be NULL though when a module is stopped by deregistering it.

Finally, note that system messages with valued sender won't be sent to modules that actually generated the message |br|
(ie: if a module is being stopped, it won't receive the M_PS_MOD_STOPPED message itself). |br|

Topics
------

Topics are just strings. Thus, if subscribing to "foo" topic, a module will receive: broadcasted messages, messages told directly to it and messages published on "foo" topic. |br|
Note that you can subscribe to regex patterns too, eg: "f[o+]" would receive messages published on "fo", "foo", "fooo" etc etc. |br|
When you unsubscribe from a regex topic, you should pass same regex to which you subscribed.

Notes
-----

Note that a context must be looping to receive any pubsub message. |br|
Moreover, when a context stops looping, all pubsub messages will be flushed and thus synchronously delivered to each M_MOD_RUNNING module. |br|
Pubsub messages sent while context is not looping or module is M_MOD_PAUSED are buffered until context starts looping/module gets resumed. For more information, see `pipe capacity <https://linux.die.net/man/7/pipe>`_. |br|
Finally, please be aware that data pointer sent through pubsub messaging is trusted, ie: you should pay attention to its scope.
