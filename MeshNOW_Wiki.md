# MeshNOW Architecture Wiki

## 1. Library Overview & Intent

MeshNOW is a standalone, lightweight ESP-IDF mesh library built on ESP-NOW. It is not a TCP/UDP/WebSocket protocol stack; instead it uses ESP-NOW packet transport and a custom ESP-NETIF driver for internal IP forwarding.

Key facts:
- Transport: ESP-NOW wireless frames
- Mesh shape: hierarchical tree with parent/children relationships, not a full peer-to-peer mesh
- Root nodes: optionally connect to an external Wi-Fi router if configured
- IP layer: custom `esp_netif` implementation provides a virtual 10.0.0.0/16 subnet and NAT for root nodes
- Serialization: `bitsery` used to encode/decode packets on the wire

Core source areas:
- `meshnow/src/include/meshnow.h` — public API, event types and their config structs, callback definitions,
- `meshnow/src/meshnow.cpp` — init/start/stop/send lifecycle
- `meshnow/src/packets.cpp` / `meshnow/src/packets.hpp` — protocol wire format
- `meshnow/src/job` — connection, keepalive, fragment GC, packet handling
- `meshnow/src/send` — send queue, worker, routing behavior
- `meshnow/src/netif.cpp` — custom network interface and fragment transport

---

## 2. Public API Reference

### API entrypoints

`esp_err_t meshnow_init(meshnow_config_t* config)`
- Initializes MeshNOW.
- Requires NVS, Wi-Fi, and `esp_netif` already initialized.
- If `config->root` is true, the node becomes root and optionally connects to a router.
- If `config->router_config.should_connect` is true, `router_config.sta_config` must be set.

`esp_err_t meshnow_start()`
- Starts Wi-Fi and all MeshNOW network tasks.
- For non-root nodes, begins parent search and connection.
- For root nodes, starts router connectivity and mesh services.

`esp_err_t meshnow_stop()`
- Stops networking and Wi-Fi.
- Blocks until tasks finish to ensure a clean shutdown.

`esp_err_t meshnow_deinit()`
- Deinitializes MeshNOW after stop.

### Data APIs

`esp_err_t meshnow_send(meshnow_addr_t dest, uint8_t* buffer, size_t len)`
- Sends a custom data packet through the mesh.
- Accepts `dest` values:
  - `MESHNOW_BROADCAST_ADDRESS` — broadcast to the whole mesh
  - `MESHNOW_ROOT_ADDRESS` — send to the root node
  - specific MAC address for destination routing
- Maximum payload size: `MESHNOW_MAX_CUSTOM_MESSAGE_SIZE` (230 bytes).

`esp_err_t meshnow_register_data_cb(meshnow_data_cb_t cb, meshnow_data_cb_handle_t* handle)
- Register a custom data callback.
- Incoming `CustomData` payloads are delivered via this callback.

`esp_err_t meshnow_unregister_data_cb(meshnow_data_cb_handle_t handle)`
- Unregister the callback.

### Layout queries

`meshnow_visible_mesh_size(size_t* size)`
- Returns the number of visible nodes in the mesh. For root, includes all nodes; otherwise includes the subtree.

`meshnow_get_parent(meshnow_addr_t parent_mac, bool* has_parent)`
- Returns the MAC of the parent node if connected.

`meshnow_get_children_num(size_t* num)`
- Returns count of direct children.

`meshnow_get_children(meshnow_addr_t* children, size_t* num)`
- Returns direct children MACs.

`meshnow_get_child_children_num(meshnow_addr_t child, size_t* num)`
- Returns count of indirect children beneath a direct child.

`meshnow_get_child_children(meshnow_addr_t child, meshnow_addr_t* children, size_t* num)`
- Returns MACs of indirect children under a direct child.

---

## 3. Internal State Machine

### Defined states

From `meshnow/src/state.hpp` / `meshnow/src/state.cpp`:
- `DISCONNECTED_FROM_PARENT`
- `CONNECTED_TO_PARENT`
- `REACHES_ROOT`

### Root node
- Root starts in `REACHES_ROOT`.
- Root never performs the parent search/connect flow.
- Root accepts children and propagates root reachability downstream.

### Non-root node transitions

#### Start
- After `meshnow_start()`, non-root nodes enter the connection process.

#### Search phase
- `meshnow/src/job/connect.cpp` `ConnectJob::SearchPhase`
- Broadcasts `SearchProbe` packets on all channels.
- Collects parent candidates until at least one reply is found.
- Saves candidate MAC/RSSI and optionally saves the discovered channel to NVS.
- After `CONFIG_FIRST_PARENT_WAIT`, transitions to connect phase.

#### Connect phase
- `ConnectJob::ConnectPhase`
- Selects the best parent by RSSI.
- Sends `ConnectRequest` directly to that parent.
- Awaits `ConnectOk` response.

#### Connected
- On receiving `ConnectOk`, the node:
  - sets parent in `Layout`
  - sets root MAC
  - transitions to `REACHES_ROOT`
  - fires `MESHNOW_EVENT_PARENT_CONNECTED`

#### Disconnected / degraded
- `StatusSendJob` sends keepalive `Status` packets every `CONFIG_STATUS_SEND_INTERVAL`.
- `NeighborCheckJob` removes a neighbor after `CONFIG_KEEP_ALIVE_TIMEOUT`.
- If parent times out, the node transitions back to `DISCONNECTED_FROM_PARENT`.
- `UnreachableTimeoutJob` can also force disconnect if root becomes unreachable for `CONFIG_ROOT_UNREACHABLE_TIMEOUT`.

### State-driven behavior
- `state::setState()` fires `STATE_CHANGED` events internally.
- On `REACHES_ROOT`, status is propagated downstream via `RootReachable`.
- On leaving `REACHES_ROOT`, children receive `RootUnreachable`.
- The netif is signaled connected/disconnected only when root reachability changes.

### Observed lifecycle mapping
- `Disconnected` → search/connect phases
- `Handshaking` → `SearchPhase` / `ConnectPhase`
- `Active` → `CONNECTED_TO_PARENT` or `REACHES_ROOT`
- `Degraded` → parent connected but root unreachable, or parent timeouts pending recovery

---

## 4. Protocol Wire Format

### Packet structure

Every packet is serialized with `bitsery` and begins with a 3-byte magic prefix:
- `MAGIC = {0x55, 0x77, 0x55}`

`meshnow::packets::Packet` fields:
- `uint32_t id`
- `util::MacAddr from` (6 bytes)
- `util::MacAddr to` (6 bytes)
- `Payload payload`

### Payload message types

Defined in `meshnow/src/packets.hpp`:
- `Status`
- `SearchProbe`
- `SearchReply`
- `ConnectRequest`
- `ConnectOk`
- `RoutingTableAdd`
- `RoutingTableRemove`
- `RootUnreachable`
- `RootReachable`
- `DataFragment`
- `CustomData`

### Important payload details

#### `Status`
- Contains current node state.
- If state is `REACHES_ROOT`, carries an optional root MAC.

#### `SearchProbe` / `SearchReply`
- `SearchProbe` is broadcast by a disconnected node.
- `SearchReply` is returned by reachable parents.

#### `ConnectRequest` / `ConnectOk`
- `ConnectRequest` is sent to attempt parent attachment.
- `ConnectOk` carries the root MAC and completes connection.

#### `RoutingTableAdd` / `RoutingTableRemove`
- Used to share indirect child reachability information.
- Nodes update their children routing tables on receipt.

#### `RootUnreachable` / `RootReachable`
- Sent downstream to inform children about root reachability changes.

#### `DataFragment`
- 4-byte `frag_id`
- 2-byte packed `options`
  - `frag_num` (3 bits)
  - `total_size` (11 bits)
- `data` buffer
- Fragment payload size is limited by `MAX_FRAG_PAYLOAD_SIZE`.

#### `CustomData`
- Holds application payload bytes.
- Serialized with 1-byte length prefix and limited by `MESHNOW_MAX_CUSTOM_MESSAGE_SIZE`.

### Size constraints

From `meshnow/src/constants.hpp`:
- `HEADER_SIZE = 20`
- `MAX_CUSTOM_PAYLOAD_SIZE = ESP_NOW_MAX_DATA_LEN - HEADER_SIZE`
- `MAX_FRAG_PAYLOAD_SIZE = ESP_NOW_MAX_DATA_LEN - HEADER_SIZE - 6`

This means a `CustomData` packet can carry at most 230 bytes.

### Routing and forwarding rules

Implemented in `meshnow/src/send/def.cpp`.

Core behaviors:
- `DirectOnce`: direct send to a specific neighbor.
- `NeighborsOnce`: send to direct parent and direct children.
- `UpstreamRetry`: send upstream to parent, retry on failure.
- `DownstreamRetry`: send to all children, retry failed sends.
- `FullyResolve`: route packets using tree logic:
  - Broadcast to all neighbors except previous hop
  - If destination is root, send upstream
  - If destination is parent, send upstream
  - If destination matches a child or a child routing table entry, send downstream to that child
  - Else, send upstream

Forwarding path:
- Non-local packets are re-enqueued with `FullyResolve`.
- Broadcast packets are re-sent even if the current node is a recipient.
- Routing tables are dynamically learned from packet source addresses.

---

## 5. Known Configuration Options

### MeshNOW specific Kconfig knobs

Source: `meshnow/Kconfig.projbuild`

| CONFIG | Default | Purpose |
|---|---|---|
| `CONFIG_MAX_CHILDREN` | `5` | Maximum direct children per node |
| `CONFIG_SEARCH_PROBE_INTERVAL` | `50` | Interval between search probes in ms |
| `CONFIG_PROBES_PER_CHANNEL` | `3` | Probes per channel during discovery |
| `CONFIG_FIRST_PARENT_WAIT` | `3000` | Wait after first parent found before connecting |
| `CONFIG_MAX_PARENTS_TO_CONSIDER` | `5` | Max parent candidates to remember |
| `CONFIG_CONNECT_TIMEOUT` | `3000` | Connect response timeout in ms |
| `CONFIG_STATUS_SEND_INTERVAL` | `500` | Keepalive beacon interval in ms |
| `CONFIG_KEEP_ALIVE_TIMEOUT` | `3000` | Neighbor timeout in ms |
| `CONFIG_ROOT_UNREACHABLE_TIMEOUT` | `10000` | Timeout before root-unreach cleanup |
| `CONFIG_FRAGMENT_TIMEOUT` | `3000` | Fragment reassembly timeout in ms |
| `CONFIG_STATIC_DNS_ADDR` | `0x01010101` | Static DNS IP for custom netif |

### Derived internal constants

| Constant | Purpose |
|---|---|
| `MESHNOW_MAX_CUSTOM_MESSAGE_SIZE` | Maximum custom payload size (230 bytes) |
| `HEADER_SIZE` | Fixed mesh packet header size (20 bytes) |
| `MAX_FRAG_PAYLOAD_SIZE` | Max fragment payload for IP forwarding |
| `QUEUE_SIZE` | Send queue capacity (128 items) |
| `TASK_PRIORITY` | FreeRTOS task priority (5) |
| `DNS_IP_ADDR` | DNS address from `CONFIG_STATIC_DNS_ADDR` |
| `QUEUE_TIMEOUT` | 100 ms enqueue timeout |
| `MIN_TIMEOUT` | 500 ms send worker poll timeout |

### Runtime config and env

- MeshNOW itself does not use shell environment variables.
- `meshnow/src/netif.cpp` uses `CONFIG_STATIC_DNS_ADDR` to configure DNS.
- `meshnow::checkNetif()` warns if `esp_netif_init()` has not been called, but does not validate it safely.

---

## 6. Architectural Technical Debt

### Known code debt

- `PacketHandler` TODOs:
  - duplicate packet detection
  - routing cycle detection
  - safety checks for routing table packets
- `meshnow_send()` does not verify upstream connectivity before enqueueing data.
- `send::queue.cpp` uses fixed `QUEUE_SIZE = 128` and warns that it may need to be larger.
- `send::enqueuePayload()` blocks for only 100 ms before timing out.
- `driver_free_rx_buffer()` has a comment warning about possible double-free.

### Threading and task risks

- MeshNOW uses multiple FreeRTOS tasks:
  - `job_runner_task` — packet handling and periodic jobs
  - `send_worker_task` — ESP-NOW transmission
  - `io_receive_task` — IP receive injection for reassembled fragments
- Locks are used, but overall shared-state access is coarse-grained and not fully documented.
- The root’s `esp_netif` connected/disconnected signaling depends on state events and may be fragile if state changes are missed.

### Protocol/operational bottlenecks

- Root is a single choke point for non-root IP traffic.
- Join discovery can be disrupted while the root is changing Wi-Fi channel or connecting to a router.
- Broadcasts have no TTL/deduplication and could create loops or unnecessary retransmission in larger trees.
- Fragment reassembly has no packet-level retransmit; lost fragments are dropped after `CONFIG_FRAGMENT_TIMEOUT`.
- `total_size` in `DataFragment` is stored in 11 bits, limiting fragmented payloads to 2047 bytes.

### Maintenance recommendations

- Add duplicate packet filtering and loop detection for broadcast/forwarded traffic.
- Harden `PacketHandler::handle()` with explicit sequence/routing validation.
- Increase or dynamically size the send queue and make enqueue backpressure deterministic.
- Verify `driver_free_rx_buffer()` memory ownership carefully.
- Add stronger netif initialization checks and explicit `esp_netif_init()` usage documentation.

---

## 7. Quick navigation for maintainers

- `meshnow/src/meshnow.cpp` — library entry point and public APIs
- `meshnow/src/include/meshnow.h` — exported API and config definitions
- `meshnow/src/job/connect.cpp` — parent search/connect state flow
- `meshnow/src/job/keep_alive.cpp` — neighbor keepalive and timeout logic
- `meshnow/src/job/packet_handler.cpp` — packet handling and forwarding
- `meshnow/src/send/def.cpp` — routing decisions and forwarding behaviors
- `meshnow/src/packets.cpp` — serialization and wire format
- `meshnow/src/netif.cpp` — custom netif, IP receive path, fragment transmit
- `meshnow/Kconfig.projbuild` — MeshNOW build-time knobs
