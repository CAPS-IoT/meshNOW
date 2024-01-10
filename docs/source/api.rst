API Reference
=============
Below you find the complete public API that is exposed through the single header file `meshnow.h`.

Functions
^^^^^^^^^
.. doxygenfunction:: meshnow_init
.. doxygenfunction:: meshnow_deinit
.. doxygenfunction:: meshnow_start
.. doxygenfunction:: meshnow_stop
.. doxygenfunction:: meshnow_send
.. doxygenfunction:: meshnow_register_data_cb
.. doxygenfunction:: meshnow_unregister_data_cb

Structures
^^^^^^^^^^
.. doxygenstruct:: meshnow_router_config_t
    :members:
.. doxygenstruct:: meshnow_config_t
    :members:
.. doxygenstruct:: meshnow_event_child_connected_t
    :members:
.. doxygenstruct:: meshnow_event_child_disconnected_t
    :members:
.. doxygenstruct:: meshnow_event_parent_connected_t
    :members:
.. doxygenstruct:: meshnow_event_parent_disconnected_t
    :members:

Macros
^^^^^^
.. doxygendefine:: MESHNOW_MAX_CUSTOM_MESSAGE_SIZE
.. doxygendefine:: MESHNOW_ADDRESS_LENGTH

Variables
^^^^^^^^^
.. doxygenvariable:: MESHNOW_EVENT
.. doxygenvariable:: MESHNOW_BROADCAST_ADDRESS
.. doxygenvariable:: MESHNOW_ROOT_ADDRESS

Type Definitions
^^^^^^^^^^^^^^^^
.. doxygentypedef:: meshnow_data_cb_t
.. doxygentypedef:: meshnow_data_cb_handle_t

Enumerations
^^^^^^^^^^^^
.. doxygenenum:: meshnow_event_t
