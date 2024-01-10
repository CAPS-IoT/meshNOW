MQTT Hello World
================

This is a simple example of using the MQTT protocol to send data to a remote broker on the Internet.

We set everything up, wait for the ``GOT_IP`` event, and then start MQTT and send the string "Hello World" followed by the MAC address of the respective node every 5 seconds.

.. literalinclude:: mqtt-hello-world.c
    :language: c
    :linenos:
