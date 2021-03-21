## Poll plugins

In this folder you can find poll plugins to support various OSes.  
As of today, an *epoll* plugin (for Linux) and a *kqueue* plugin (for BSD and MacOS) are provided.  
Moreover, an experimental *uring* plugin is provided; it must be explicitly built with -DWITH_LIBURING=true cmake option.  
Each plugin must implement and provide [poll_priv](https://github.com/FedeDP/libmodule/blob/master/Lib/poll_priv.h) interface.  
In [CMakeLists.txt](https://github.com/FedeDP/libmodule/blob/master/CMakeLists.txt#L12) correct plugin for target OS is built.  

*Any pull request to expand libmodule's availability is warmly welcomed.*
