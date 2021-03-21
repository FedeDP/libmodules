.. |br| raw:: html

   <br />

List API
========

Libmodule offers an easy to use linked list implementation, provided by <module/list.h> header. |br|
It is used internally to store module's list of pollable fds. |br|

Structures
----------

.. code::

    typedef enum {
        LIST_WRONG_PARAM = -4,
        LIST_MISSING,
        LIST_ERR,
        LIST_OMEM,
        LIST_OK
    } list_ret_code;

    /* Callback for list_iterate */
    typedef list_ret_code (*list_cb)(void *, void *);

    /* Fn for list_set_dtor */
    typedef void (*list_dtor)(void *);

    /* Callback for list comapre */
    typedef int (*list_comp)(void *, void *);

    /* Incomplete struct declaration for list */
    typedef struct _list list_t;

    /* Incomplete struct declaration for list iterator */
    typedef struct _list_itr list_itr_t;

API
---

Where not specified, these functions return a list_ret_code.

.. c:function:: list_new(fn)

  Create a new list_t object.
  
  :param fn: callback called on value destroy. If NULL, values won't be automatically destroyed.
  :type fn: :c:type:`const list_dtor`
    
  :returns: pointer to newly allocated list_t.
  
.. c:function:: list_itr_new(l)

  Create a new list_itr_t object. Note that it can be freed with free().
  
  :param l: pointer to list_t
  :type l: :c:type:`const list_t *`
    
  :returns: pointer to newly allocated list_itr_t.
  
.. c:function:: list_itr_next(itr)

  Get next iterator. If next iterator is past last element, iterator will be automatically freed.
  
  :param itr: pointer to list_itr_t
  :type itr: :c:type:`list_itr_t *`
    
  :returns: pointer to next iterator.
  
.. c:function:: list_itr_get_data(itr)

  Get iterator's data.
  
  :param itr: pointer to list_itr_t
  :type itr: :c:type:`const list_itr_t *`
    
  :returns: pointer to current iterator's data.
  
.. c:function:: list_itr_set_data(itr)

  Set iterator's data.
  
  :param itr: pointer to list_itr_t
  :type itr: :c:type:`list_itr_t *`
  
.. c:function:: list_itr_insert(itr, val)

  Insert new node between previous node and current node.
  
  :param itr: pointer to list_itr_t
  :param val: value to be added
  :type itr: :c:type:`list_itr_t *`
  :type val: :c:type:`void *`

.. c:function:: list_itr_remove(itr)

  Removes current node from list.
  
  :param itr: pointer to list_itr_t
  :type itr: :c:type:`list_itr_t *`

.. c:function:: list_iterate(l, fn, userptr)

  Iterate a list calling cb on each element until LIST_OK is returned (or end of list is reached). Returns LIST_MISSING if list is NULL or empty. |br|
  If fn() returns a value != LIST_OK, iteration will stop and: if value < LIST_OK, value will be returned, else LIST_OK will be returned.

  :param l: pointer to list_t
  :param fn: callback to be called
  :param userptr: userdata to be passed to callback as first parameter
  :type l: :c:type:`list_t *`
  :type fn: :c:type:`const list_cb`
  :type userptr: :c:type:`void *`
  
.. c:function:: list_insert(l, val, comp)

  Insert a value inside list.

  :param l: pointer to list_t
  :param val: value to be put inside list
  :param comp: comparator's callback. Its return value behaves like strcmp. If NULL, append to end of list.
  :type l: :c:type:`list_t *`
  :type val: :c:type:`void *`
  :type comp: :c:type:`const list_comp`

.. c:function:: list_remove(l, val, comp)

  Remove a value from list.

  :param l: pointer to list_t
  :param val: value to be removed from list
  :param comp: comparator's callback. Its return value behaves like strcmp. If NULL, check val pointer equality.
  :type l: :c:type:`list_t *`
  :type val: :c:type:`void *`
  :type comp: :c:type:`const list_comp`
  
.. c:function:: list_find(l, val, comp)

  Remove a value from list.

  :param l: pointer to list_t
  :param val: value to be searched inside list
  :param comp: comparator's callback. Its return value behaves like strcmp. If NULL, check val pointer equality.
  :type l: :c:type:`list_t *`
  :type val: :c:type:`void *`
  :type comp: :c:type:`const list_comp`

  :returns: pointer to found userdata or NULL
  
.. c:function:: list_clear(l)

  Clears a list object by deleting any object inside list.

  :param l: pointer to list_t
  :type l: :c:type:`list_t *`
  
.. c:function:: list_free(l)

  Free a list object (it internally calls list_clear too).

  :param l: pointer to list_t
  :type l: :c:type:`list_t *`
  
.. c:function:: list_length(l)

  Get list length.

  :param l: pointer to list_t
  :type l: :c:type:`list_t *`
  :returns: list length or a list_ret_code if any error happens (list_t is null).

