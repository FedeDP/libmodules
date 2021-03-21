.. |br| raw:: html

   <br />

Queue API
=========

Libmodule offers an easy to use queue implementation, provided by <module/queue.h> header. |br|

Structures
----------

.. code::

    typedef enum {
        QUEUE_WRONG_PARAM = -4,
        QUEUE_MISSING,
        QUEUE_ERR,
        QUEUE_OMEM,
        QUEUE_OK
    } queue_ret_code;

    /* Callback for queue_iterate */
    typedef queue_ret_code (*queue_cb)(void *, void *);

    /* Fn for queue_set_dtor */
    typedef void (*queue_dtor)(void *);

    /* Incomplete struct declaration for queue */
    typedef struct _queue queue_t;

    /* Incomplete struct declaration for queue iterator */
    typedef struct _queue_itr queue_itr_t;

API
---

Where not specified, these functions return a queue_ret_code.

.. c:function:: queue_new(fn)

  Create a new queue_t object.
  
  :param fn: callback called on value destroy. If NULL, values won't be automatically destroyed.
  :type fn: :c:type:`const queue_dtor`
    
  :returns: pointer to newly allocated queue_t.
  
.. c:function:: queue_itr_new(q)

  Create a new queue_itr_t object. Note that it can be freed with free().
  
  :param q: pointer to queue_t
  :type q: :c:type:`const queue_t *`
    
  :returns: pointer to newly allocated queue_itr_t.
  
.. c:function:: queue_itr_next(itr)

  Get next iterator. If next iterator is past last element, iterator will be automatically freed.
  
  :param itr: pointer to queue_itr_t
  :type itr: :c:type:`queue_itr_t *`
    
  :returns: pointer to next iterator.
  
.. c:function:: queue_itr_get_data(itr)

  Get iterator's data.
  
  :param itr: pointer to queue_itr_t
  :type itr: :c:type:`const queue_itr_t *`
    
  :returns: pointer to current iterator's data.
  
.. c:function:: queue_itr_set_data(itr)

  Set iterator's data.
  
  :param itr: pointer to queue_itr_t
  :type itr: :c:type:`const queue_itr_t *`

.. c:function:: queue_iterate(q, fn, userptr)

  Iterate a queue calling cb on each element until QUEUE_OK is returned (or end of queue is reached). Returns QUEUE_MISSING if queue is NULL or empty. |br|
  If fn() returns a value != QUEUE_OK, iteration will stop and: if value < QUEUE_OK, value will be returned, else QUEUE_OK will be returned.

  :param q: pointer to queue_t
  :param fn: callback to be called
  :param userptr: userdata to be passed to callback as first parameter
  :type q: :c:type:`queue_t *`
  :type fn: :c:type:`const queue_cb`
  :type userptr: :c:type:`void *`
  
.. c:function:: queue_enqueue(q, val)

  Push a value on top of queue.

  :param q: pointer to queue_t
  :param val: value to be put inside queue
  :type q: :c:type:`queue_t *`
  :type val: :c:type:`void *`

.. c:function:: queue_dequeue(q)

  Pop a value from top of queue, removing it from queue.

  :param q: pointer to queue_t
  :type q: :c:type:`queue_t *`
  :returns: void pointer to value, on NULL on error.
  
.. c:function:: queue_peek(q)

  Return queue's head element, without removing it.

  :param q: pointer to queue_t
  :type q: :c:type:`queue_t *`
  :returns: void pointer to value, on NULL on error.

.. c:function:: queue_clear(q)

  Clears a queue object by deleting any object inside queue.

  :param q: pointer to queue_t
  :type q: :c:type:`queue_t *`
  
.. c:function:: queue_free(q)

  Free a queue object (it internally calls queue_clear too).

  :param q: pointer to queue_t
  :type q: :c:type:`queue_t *`
  
.. c:function:: queue_length(q)

  Get queue length.

  :param q: pointer to queue_t
  :type q: :c:type:`queue_t *`
  :returns: queue length or a queue_ret_code if any error happens (queue_t is null).

