> This wiki was AI-generated and reviewed by the IoT Praktikum team.
> If you spot an error or outdated information, please open an issue or submit a PR.

# MeshNOW Codebase Architecture Wiki

Welcome to the **MeshNOW Architecture Wiki**! This document has been written specifically for **junior developers**, **new team members**, and **contributors** to help you understand the internal mechanics, file structure, and design principles of the MeshNOW library.

MeshNOW is a high-performance, lightweight sensor mesh network library designed for ESP32 microcontrollers using the ESP-IDF framework. It enables long-range, robust communication using **ESP-NOW** under the hood, and provides a transparent forwarding mechanism for standard TCP/IP networking (like MQTT, HTTP, and CoAP) over the mesh.

---

## Table of Contents
1. [High-Level Architecture & Concepts](#1-high-level-architecture--concepts)
2. [Network Topology & Tree Shape](#2-network-topology--tree-shape)
3. [Virtual Network Interface (netif) & NAT](#3-virtual-network-interface-netif--nat)
4. [File & Directory Structure Guided Tour](#4-file--directory-structure-guided-tour)
5. [The Lifecycle & Protocols (How It Works Under the Hood)](#5-the-lifecycle--protocols-how-it-works-under-the-hood)
    * [Discovery & Connection (The Join Flow)](#discovery--connection-the-join-flow)
    * [Routing & Packet Forwarding Strategy](#routing--packet-forwarding-strategy)
    * [Fragmentation & IP Forwarding Flow](#fragmentation--ip-forwarding-flow)
6. [Threading & Concurrency Model](#6-threading--concurrency-model)
7. [Build-Time Configurations (Kconfig Options)](#7-build-time-configurations-kconfig-options)
8. [Known Code Debt, Issues & How to Contribute](#8-known-code-debt-issues--how-to-contribute)
9. [Quick Code Cheat-Sheet for Junior Developers](#9-quick-code-cheat-sheet-for-junior-developers)

---

## 1. High-Level Architecture & Concepts

Standard Wi-Fi relies on an Access Point (AP) to route all station (STA) traffic. In an outdoor or sparse industrial environment, a single AP cannot cover the entire area. 

**MeshNOW** solves this by establishing a wireless mesh network over **ESP-NOW**—a low-power, direct-comms technology provided by Espressif that bypasses traditional Wi-Fi connection overhead.

```
       +--------------------+
       |  Wi-Fi Router /    |  <-- Standard Wi-Fi Station Mode
       |  External Server   |
       +--------------------+
                 ^
                 | 
       +--------------------+
       |     ROOT NODE      |  <-- Connects to Router, runs Virtual NAT Router
       +--------------------+
          /              \
         v                v    <-- ESP-NOW Tree Link
  +------------+    +------------+
  | PARENT_A   |    | PARENT_B   |  <-- Intermediary forwarders (Reaches Root)
  +------------+    +------------+
        |                 |
        v                 v    <-- ESP-NOW Tree Link
  +------------+    +------------+
  |  LEAF_A1   |    |  LEAF_B1   |  <-- Leaf nodes / End sensors
  +------------+    +------------+
```

### Key Concepts
* **ESP-NOW Transport:** MeshNOW utilizes ESP-NOW raw packets to establish its links.
* **No TCP/IP Overhead on Links:** Direct nodes communicate via custom serialized binary frames, avoiding the heavy memory footprint of IP stacks on local connections.
* **Virtual IP Forwarding:** For applications that require TCP/IP (like MQTT or HTTP), MeshNOW implements a custom Virtual Network Interface (`esp_netif`). It splits IP packets into smaller fragments, routes them over the tree, and reassembles them at the target.
* **Root Node as NAT Gateway:** The **Root Node** acts as the mesh gateway, performing Network Address Translation (NAT) to forward local virtual IP traffic to the external Wi-Fi router.

---

## 2. Network Topology & Tree Shape

Unlike a flat, peer-to-peer mesh where every node can talk directly to any neighbor, MeshNOW is organized as a **Hierarchical Tree**.

1. **The Root Node:** 
   * Configured via `meshnow_config_t::root = true`. (struct in src/include/meshnow.h)
   * Serves as the origin of the mesh tree.
   * Can optionally connect to a Wi-Fi router using standard Wi-Fi station mode (`router_config.should_connect = true`).
   * Manages the global IP subnet (provides standard DHCP-like IP allocations virtually to the rest of the mesh).
2. **Intermediate Parent Nodes:**
   * Nodes that have a direct parent link and some child links.
   * Maintain their own **Routing Tables** to know which child or grandchild is connected under which branch.
3. **Leaf Nodes:**
   * Endpoints that do not accept any children and only communicate through their parent.

### Parent-Child Relationships
* **Max Direct Children:** Capped by `CONFIG_MAX_CHILDREN` (default: 5) to prevent memory bloating and packet congestion.
* **Routing Tables:** Every node maintains a localized routing table that keeps track of its descendants. When a node gains a child, it announces it upstream via a `RoutingTableAdd` packet. When a child times out, it is pruned and a `RoutingTableRemove` packet is propagated.

---

## 3. Virtual Network Interface (netif) & NAT

How does an MQTT client on a leaf node communicate with a broker on the internet?

1. **Virtual Netif:** 
   MeshNOW registers a custom network interface (`esp_netif`) in the ESP-IDF stack. To the standard application, it looks like a regular network card.
2. **Custom Subnet (10.0.0.0/16):**
   * The MeshNOW network assigns each node a unique IP address within the `10.0.0.0/16` virtual subnet.
   * Node IPs are deterministically mapped using the last two octets of their MAC address (e.g., MAC `AA:BB:CC:DD:12:34` maps to IP `10.0.18.52`).
3. **NAT on Root Node:**
   When a leaf sends an IP packet to a public IP (e.g., `192.168.1.15` or `8.8.8.8`), the root node intercepting this traffic translates the source IP to its router station IP (Standard NAT) and routes it. On response, the root translates it back and forwards it down the mesh.
4. **Data Fragmentation:**
   ESP-NOW packets are limited to **250 bytes**. A typical TCP/IP packet can be up to **1500 bytes**. The Netif driver handles packet splitting on the sender's side (`DataFragment`) and reassembly on the receiver's side, managing timeouts and garbage collection for partial fragments.

---

## 4. File & Directory Structure Guided Tour

Here is a comprehensive directory breakdown of `meshnow/src/` to help you find your way around the codebase:

### Root Source Directory (`meshnow/src/`)
* **`meshnow.cpp` / `include/meshnow.h`**
  The user-facing public API. Contains standard initialization, setup, control lifecycle (`meshnow_init()`, `meshnow_start()`, `meshnow_stop()`, `meshnow_deinit()`), custom data sending (`meshnow_send()`), and API callbacks (`meshnow_register_data_cb()`).
* **`constants.hpp`**
  Declares core physical limits, priorities, and constants. This includes frame header sizes, payload caps, and default timing options.
* **`custom.cpp` / `custom.hpp`**
  Implements the custom application-layer data callback handlers.
* **`event.cpp` / `event.hpp`**
  Dispatches public system-level events (e.g., child connected, child disconnected, parent connected/disconnected) via the ESP-IDF system event loop.
* **`fragments.cpp` / `fragments.hpp`**
  Core logic for splitting outgoing IP frames into chunks and reassembling incoming chunks into a complete IP frame.
* **`layout.cpp` / `layout.hpp`**
  Manages the internal topology representation. Tracks the node's parent, direct children, and indirect children (via routing tables).
* **`lock.cpp` / `lock.hpp`**
  Defines critical synchronization locks (mutexes) used to make state-sharing thread-safe in FreeRTOS.
* **`netif.cpp` / `netif.hpp`**
  The Custom Netif integration driver. Connects MeshNOW to ESP-IDF's LwIP stack. It handles IP transmit hooks (`esp_netif_transmit`) and packet injection.
* **`networking.cpp` / `networking.hpp`**
  Direct hardware wrapper for ESP-NOW. Configures Wi-Fi physical channels, registers raw ESP-NOW receive and send-status callbacks, and handles physical broadcasts.
* **`packets.cpp` / `packets.hpp`**
  Defines the wire-format structures for all payload types. Implements binary serialization and deserialization using the `bitsery` library.
* **`state.cpp` / `state.hpp`**
  Maintains the node's current operating status (`DISCONNECTED_FROM_PARENT`, `CONNECTED_TO_PARENT`, `REACHES_ROOT`) and orchestrates state transition notifications.
* **`wifi.cpp` / `wifi.hpp`**
  Sets up and manages the Wi-Fi PHY layer (station and AP), implements channel scanning/switching, and handles external router connections on the root node.

### The Job Directory (`meshnow/src/job/`)
Jobs are asynchronous, periodic tasks orchestrated by a unified scheduler.
* **`job.hpp`**
  An abstract base class template for any periodic execution unit.
* **`runner.cpp` / `runner.hpp`**
  The master FreeRTOS task runner (`job_runner_task`) that executes pending jobs.
* **`connect.cpp` / `connect.hpp`**
  The crucial Mesh Join Job. Implements a multi-phase state machine: **Search Phase** (scanning channels and comparing parent candidates via RSSI), **Connect Phase** (handshake with the best parent), **Awaiting Connect Response Phase** (timeout guard), **Reconnect Phase** (retry the same parent before giving up), and **Done Phase** (idle, watches for disconnection).
* **`keep_alive.cpp` / `keep_alive.hpp`**
  Fires periodic status beacons (`Status` packets) and monitors neighbor timeouts. If a parent is silent for too long, it triggers a disconnect event.
* **`packet_handler.cpp` / `packet_handler.hpp`**
  Decides what to do with incoming packets. It parses, validates, executes local packet callbacks, or forwards packets onward using routing rules.
* **`fragment_gc.cpp` / `fragment_gc.hpp`**
  Garbage Collector for partial, timed-out IP packet fragments. If fragments of an IP packet do not arrive in time, it frees the allocated buffers to prevent memory leaks.

### The Receive Directory (`meshnow/src/receive/`)
* **`receiver.cpp` / `receiver.hpp`**
  Executes the high-priority `io_receive_task` which pulls raw packets from the ESP-NOW hardware callback, verifies the 3-byte magic header, and writes to the incoming queue.
* **`queue.cpp` / `queue.hpp`**
  A thread-safe FIFO buffer storing incoming packets before they are parsed by the `PacketHandler`.

### The Send Directory (`meshnow/src/send/`)
* **`worker.cpp` / `worker.hpp`**
  Runs `send_worker_task`. It pulls packets from the outgoing queue, resolves their destination addresses, and triggers the physical ESP-NOW transmissions.
* **`queue.cpp` / `queue.hpp`**
  A thread-safe FIFO buffer storing outgoing packets waiting to be physically sent.
* **`def.cpp` / `def.hpp`**
  Defines routing rules and transmission behaviors, such as `DirectOnce`, `UpstreamRetry`, `DownstreamRetry`, and the core `FullyResolve` logic.

### The Utility Directory (`meshnow/src/util/`)
* **`event.hpp`** - Lightweight compile-time C++ publish-subscribe system helper.
* **`mac.cpp` / `mac.hpp`** - Thread-safe, memory-efficient C++ wrapper around raw 6-byte MAC addresses.
* **`queue.hpp`** - FreeRTOS thread-safe queue template.
* **`task.hpp`** - Clean C++ wrapper for managing FreeRTOS task creation, execution, and termination.
* **`waitbits.hpp`** - FreeRTOS event-group flag wrapper.
* **`util.hpp`** - Miscellaneous formatting and tag generation helpers.

---

## 5. The Lifecycle & Protocols (How It Works Under the Hood)

To modify or debug the mesh, you need to understand the main data flows:

### Discovery & Connection (The Join Flow)

When a non-root node starts, it is in `DISCONNECTED_FROM_PARENT`. It runs `ConnectJob`, which is a multi-phase state machine implemented as a `std::variant` in `connect.cpp`:

```
+---------------------------+
|       Search Phase        |  Broadcasts SearchProbe on all channels, collects
|                           |  parent candidates ranked by RSSI. Saves the
|                           |  discovered channel to NVS for faster next boot.
+---------------------------+
             |
             | First parent found + FIRST_PARENT_WAIT elapsed
             v
+---------------------------+
|       Connect Phase       |  Picks the best candidate by RSSI,
|                           |  sends ConnectRequest to that parent.
+---------------------------+
             |
             | ConnectRequest sent
             v
+---------------------------+
| AwaitingConnectResponse   |  Waits up to CONNECT_TIMEOUT for a ConnectOk.
|        Phase              |  On timeout fires TIMEOUT_CONNECT_RESPONSE:
|                           |   - if initial attempt  → back to Connect Phase
|                           |   - if reconnect attempt → back to Reconnect Phase
+---------------------------+
             |
             | ConnectOk received (carries root MAC + RSSI)
             v
+---------------------------+
|        Done Phase         |  Sets parent in Layout, updates state to
|                           |  REACHES_ROOT, fires MESHNOW_EVENT_PARENT_CONNECTED.
|                           |  Idles here, listening for STATE_CHANGED.
+---------------------------+
             |
             | STATE_CHANGED → DISCONNECTED_FROM_PARENT
             v
+---------------------------+
|      Reconnect Phase      |  Retries the same parent up to RECONNECT_ATTEMPTS
|                           |  times before falling back to Search Phase.
+---------------------------+
             |
             | Attempts exhausted
             v
+---------------------------+
|       Search Phase        |  Full restart of parent discovery.
+---------------------------+
```

On the wire, the handshake looks like this:

```
Disconnected Node                        Potential Parent Nodes (Active)
       |                                              |
       | ----- [SearchProbe Broadcast] -------------> |  (Fires on all Wi-Fi channels)
       |                                              |
       | <---- [SearchReply (RSSI details)] --------- |  (Candidates respond with parent capacity)
       |                                              |
[Select Best Candidate by RSSI]                       |
       |                                              |
       | ----- [ConnectRequest] --------------------> |  (Handshake attempt to the best parent)
       |                                              |
       | <---- [ConnectOk (Root MAC & State Info)] -- |  (Handshake successful!)
       v                                              v
[Transition to REACHES_ROOT]
[Send RoutingTableAdd upstream]
```

### Routing & Packet Forwarding Strategy
When a packet needs to be sent, `send::def.cpp` checks its destination address against the local state:

* **Is the destination direct?**
  If the destination MAC is the direct parent or a direct child, the packet is sent directly via ESP-NOW.
* **`FullyResolve` Tree Search:**
  * **To Root (`00:00:00:00:00:00`):** Forward upstream to parent.
  * **To Broadcast (`FF:FF:FF:FF:FF:FF`):** Forward to direct parent and ALL children.
  * **Search Routing Table:** If the destination matches an entry inside our registered children tables, forward to the specific direct child managing that subtree.
  * **Default Fallback:** Forward upstream to parent (it might be in another branch of the tree).

### Fragmentation & IP Forwarding Flow
When an application (like MQTT) sends a large TCP/IP packet:

```
[LwIP / Socket API]
         |
         v
  [netif.cpp] ---> Intercepts frame, splits into fragments
         |
         v
  [fragments.cpp] ---> Generates multiple "DataFragment" packets
         |
         v
  [send_worker_task] ---> Dispatches fragments over ESP-NOW Tree
         |
       ~ ~ ~  (Transit through intermediate nodes)
         |
         v
  [io_receive_task] ---> Receives fragments
         |
         v
  [fragments.cpp] ---> Reassembles chunks into complete IP frame
         |
         v
  [netif.cpp] ---> Injects complete frame back into LwIP Stack
         |
         v
[LwIP / Socket API]
```

---

## 6. Threading & Concurrency Model

MeshNOW operates in a multi-threaded FreeRTOS environment to ensure low-latency packet forwarding and high responsive performance:

| Task Name | Source File | Priority | Responsibility |
|---|---|---|---|
| `io_receive_task` | `receive/receiver.cpp` | `5` | Highest priority receiver loop. Pulls bytes from raw ESP-NOW driver, validates MAGIC prefix, and pushes to raw input queue. |
| `send_worker_task` | `send/worker.cpp` | `5` | Core sender loop. Pulls packets from outgoing queue, resolves destination MAC, handles retries, and invokes ESP-NOW. |
| `job_runner_task` | `job/runner.cpp` | `5` | Orchestrates periodic routines. Schedules keep-alive beacons, manages timeouts, processes incoming queues via `PacketHandler`. |

### Synchronization & Safety
Because tasks interact with shared structures (like the `Layout` tree representation and `state` status flags), MeshNOW uses recursive and regular mutexes:
* **`lock::layout`** protects layout operations (adding/removing children, routing table entries).
* **`lock::state`** protects the core state transitions to prevent race conditions during handshakes or disconnects.

---

## 7. Build-Time Configurations (Kconfig Options)

You can customize the mesh performance in your ESP-IDF project configuration (`menuconfig`) under the **MeshNOW** section.

| Configuration Key | Default Value | Purpose / Description |
|---|---|---|
| `CONFIG_MAX_CHILDREN` | `5` | Limit on direct children per node. Increase for wider meshes, decrease for thin, deep chains. |
| `CONFIG_SEARCH_PROBE_INTERVAL` | `50` | Milliseconds between probes on a channel during parent search. |
| `CONFIG_PROBES_PER_CHANNEL` | `3` | Number of probe packets sent per channel before switching during discovery. |
| `CONFIG_FIRST_PARENT_WAIT` | `3000` | Wait time (ms) after finding the first candidate before completing selection (allows gathering multiple potential parents). |
| `CONFIG_MAX_PARENTS_TO_CONSIDER` | `5` | Size of parent candidate list to evaluate and select from. |
| `CONFIG_CONNECT_TIMEOUT` | `3000` | Handshake response timeout (ms) before aborting and retrying. |
| `CONFIG_RECONNECT_ATTEMPTS` | *(see Kconfig.projbuild)* | How many times to retry the same parent in Reconnect Phase before falling back to a full Search Phase. |
| `CONFIG_STATUS_SEND_INTERVAL` | `500` | Keepalive heartbeat beacon interval (ms) sent to parent and children. |
| `CONFIG_KEEP_ALIVE_TIMEOUT` | `3000` | Time (ms) without receiving status packets before declaring a neighbor dead. |
| `CONFIG_ROOT_UNREACHABLE_TIMEOUT`| `10000` | Time (ms) a node stays in degraded status (connected to parent but root unreachable) before forcing a full disconnect. |
| `CONFIG_FRAGMENT_TIMEOUT` | `3000` | Time (ms) allowed to collect all fragments of an IP packet before discarding them. |
| `CONFIG_STATIC_DNS_ADDR` | `0x01010101` | DNS address supplied virtually to clients (defaults to `1.1.1.1`). |

---

## 8. Known Code Debt, Issues & How to Contribute

If you are a junior developer tasked with extending or optimizing MeshNOW, here is where the current architectural pain points lie:

1. **Broadcast Storm Risks:**
   Broadcast frames have no Time-To-Live (TTL) or deduplication logic. In large networks, a loop or infinite forwarding storm could occur.
   * *Contribution Opportunity:* Add a sequence ID and a traversed-nodes list inside packet headers to filter out already-received broadcasts.
2. **Double-Free in RX Buffer:**
   There is a known warning in `driver_free_rx_buffer()` (inside `netif.cpp`) regarding ownership of the Wi-Fi receive buffers.
   * *Contribution Opportunity:* Audit memory leaks in the IP forwarding path.
3. **Queue Bloating:**
   `QUEUE_SIZE` is statically set to `128` inside `send::queue.cpp`. Under high traffic load, the send queue can saturate and reject packets.
   * *Contribution Opportunity:* Make the queue dynamically sizable or implement an intelligent backpressure/congestion mechanism.
4. **No Packet-Level Retransmission for Fragments:**
   IP packet reassembly depends on all fragments arriving. If a single fragment is lost, the entire IP packet fails and times out after `CONFIG_FRAGMENT_TIMEOUT`.
   * *Contribution Opportunity:* Implement simple sliding-window or ACK mechanisms for virtual fragments.

---

## 9. Quick Code Cheat-Sheet for Junior Developers

### How to Send a Custom Message
You can transmit up to **230 bytes** of custom data to any node (or broadcast to all nodes):

```c
#include "meshnow.h"

// 1. Broadcast message to all nodes
uint8_t payload[] = "Hello MeshNOW!";
meshnow_send(MESHNOW_BROADCAST_ADDRESS, payload, sizeof(payload));

// 2. Direct message to the Root node
meshnow_send(MESHNOW_ROOT_ADDRESS, payload, sizeof(payload));

// 3. Direct message to a specific MAC
meshnow_addr_t target_mac = {0x24, 0x0A, 0xC4, 0x12, 0x34, 0x56};
meshnow_send(target_mac, payload, sizeof(payload));
```

### How to Receive Custom Messages
Register a callback function to handle incoming application payloads:

```c
#include "meshnow.h"
#include <esp_log.h>

void my_data_handler(meshnow_addr_t src, uint8_t* buffer, size_t len) {
    ESP_LOGI("APP", "Received %d bytes from %02X:%02X:%02X:%02X:%02X:%02X",
             len, src[0], src[1], src[2], src[3], src[4], src[5]);
}

// Inside your initialization code:
meshnow_data_cb_handle_t cb_handle;
meshnow_register_data_cb(my_data_handler, &cb_handle);
```

### Writing a Custom Periodic Job
If you need to perform periodic background tasks (e.g., polling a sensor and saving a health log), inherit from the template class `Job`:

```cpp
#include "job/job.hpp"
#include <esp_log.h>

class MySensorJob : public meshnow::job::Job {
public:
    MySensorJob() : Job(1000) {} // Run every 1000ms

    void execute() override {
        ESP_LOGI("SENSOR_JOB", "Reading telemetry...");
        // Add your custom logic here!
    }
};

// Registered in your job runner setup.
```

---

We hope this wiki helps you get up to speed with the **MeshNOW** codebase! Happy hacking!
