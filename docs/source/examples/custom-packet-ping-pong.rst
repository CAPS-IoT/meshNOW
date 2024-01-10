Custom Packet Ping Pong
=======================

This example demonstrates the custom packet functionality of MeshNOW.

We set everything up, and then broadcast a ``PING`` message from the root to all nodes in the mesh.
Each node will respond with a ``PONG`` message, and the root will print the responses.

.. literalinclude:: custom-packet-ping-pong.c
    :language: c
    :linenos:
