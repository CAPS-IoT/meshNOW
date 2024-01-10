SDKConfig Options
=================
The following options can be changed in the project's ``sdkconfig`` file:


Connecting
^^^^^^^^^^
Config values related to the connection process of MeshNOW.

CONFIG_MAX_CHILDREN
""""""""""""
Determines the maximum number of children that a node can have.
If a node has reached the maximum number of children, it will not accept new children.
Lowering this value can save memory, but complicate node deployment.

**Default value:** ``5``

CONFIG_SEARCH_PROBE_INTERVAL
"""""""""""""""""""""
Time in milliseconds until the next search probe is sent during the search phase.
A smaller value will result in a faster connection, but will also increase network congestion and power consumption.

**Default value:** ``50``

CONFIG_PROBES_PER_CHANNEL
""""""""""""""""""
During the search phase, the node performs an all-channel scan as the home channel of any potential parent node is unknown.
This value determines the number of search probes that are sent on each channel.
A smaller value may result in failing to accept a connection because the channel is switched before a reply is received.
A larger value may increase the time it takes to connect as the node stays longer on dead channels.

**Default value:** ``3``

CONFIG_FIRST_PARENT_WAIT
"""""""""""""""""
During the search phase, after a first potential parent was found, the node keeps searching for more parents in case an even better parent is found.
This value determines the time in milliseconds that the node keeps searching for more parents after the first one was found.
Set to ``0`` to disable this feature.

**Default value:** ``3000``

CONFIG_MAX_PARENTS_TO_CONSIDER
"""""""""""""""""""""""
During the search phase, the node keeps track of only a few parents at a time to save memory and speed up the connection process.
This value determines the maximum number of parents that the node keeps track of.
If a better parent is found, the node will replace the worst parent it is tracking.

**Default value:** ``5``

CONFIG_CONNECT_TIMEOUT
"""""""""""""""
During the connect phase, the node sends a connect request to the best parent and waits for a reply.
This value determines the time in milliseconds that the node waits for a reply before trying to connect to the next best parent.

**Default value:** ``3000``


Keep Alive
^^^^^^^^^^
Config values related to handling lost connections.

CONFIG_STATUS_SEND_INTERVAL
""""""""""""""""""""
A node sends a special status beacon to each of its neighbors at regular intervals.
This value determines the time in milliseconds between two status beacons.
The value should best be smaller than `CONFIG_KEEP_ALIVE_TIMEOUT`_ to prevent false disconnects.

**Default value:** ``500``

CONFIG_KEEP_ALIVE_TIMEOUT
""""""""""""""""""
A node considers a neighbor to be disconnected if it has not received a status beacon from it for a certain time.
This value determines the time in milliseconds after which a neighbor is considered to be disconnected.
The value should best be larger than `CONFIG_STATUS_SEND_INTERVAL`_ to prevent false disconnects.

**Default value:** ``3000``

CONFIG_ROOT_UNREACHABLE_TIMEOUT
""""""""""""""""""""""""
If a node disconnects from its parent, all its (indirect) children will stay connected.
After this timeout value in milliseconds, the nodes will disconnect and search for new parents as they cannot reach the root node anymore.

**Default value:** ``10000``


TCP/IP
^^^^^^
Config values related to the TCP/IP support of MeshNOW.

CONFIG_FRAGMENT_TIMEOUT
""""""""""""""""
TCP/IP packets need to be fragmented by MeshNOW to fit into the ESP-NOW payload size limit.
This value determines the time in milliseconds that MeshNOW waits for another fragment of the same TCP/IP packet to be received before completely discarding it.
A smaller value will lead to less memory usage, but may result in higher retransmission counts and therefore higher network congestion.

**Default value:** ``3000``


CONFIG_STATIC_DNS_ADDR
"""""""""""""""
The IP address of the DNS server that is used for DNS lookups.
This value is encoded as a 4-byte hex value.

**Default value:** ``0x01010101`` (1.1.1.1, Cloudflare DNS)
