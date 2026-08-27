# udp_connection

`udp_connection` is a standalone C++ UDP module.

It provides a small object-oriented API with RAII-style ownership and does not depend on `udp_client_2`.

- automatic cleanup through the destructor
- move support
- no manual `create` or `destroy`
- direct methods for init, send, receive, and close

## Header

Include:

```cpp
#include "IConnection.h"
#include "udp_connection.h"
```

`udp_connection` implements `IConnection`.

The factory-created interface instance is expected to already know its endpoint configuration, so `open()` takes no arguments.

## Main API

### `open`

Open and initialize the UDP connection.

```cpp
connection_result open() noexcept;
```

Use this when the connection instance has already been configured by construction or by a future factory.

### `init`

Convenience wrapper over `open`.

```cpp
connection_result init(
    const char* local_ip,
    uint16_t local_port,
    const char* default_peer_ip = nullptr,
    uint16_t default_peer_port = 0U) noexcept;
```

### `set_default_peer`

Set or replace the default peer endpoint used by `send`.

```cpp
connection_result set_default_peer(const char* ip, uint16_t port) noexcept;
```

### `send`

Send data to the current default peer.

```cpp
connection_result send(const void* data, std::size_t size) noexcept;
```

This requires a default peer to already exist.

### `send_to`

Send data to a specific peer without changing the default peer.

```cpp
connection_result send_to(
    const char* ip,
    uint16_t port,
    const void* data,
    std::size_t size) noexcept;
```

### `receive`

Receive UDP data into a caller-provided buffer.

```cpp
connection_result receive(
    void* buffer,
    std::size_t capacity,
    std::size_t* received_size) noexcept;
```

### `last_socket_error`

Read the last socket error stored by the connection object.

```cpp
int last_socket_error() const noexcept;
```

### `close`

Close the UDP socket.

```cpp
connection_result close() noexcept;
```

Calling `close` manually is optional. The socket is released automatically when the object is destroyed.

## Tiny Example

```cpp
#include "udp_connection.h"
#include <cstring>
#include <iostream>

void example()
{
    udp_connection connection;
    IConnection& channel = connection;

    connection_result result = connection.init(
        "127.0.0.1",
        5000,
        "127.0.0.1",
        6000);
    if (result != connection_result::ok)
    {
        std::cerr << "init failed: "
                  << udp_connection::result_string(result)
                  << ", socket error="
                  << connection.last_socket_error()
                  << std::endl;
        return;
    }

    const char message[] = "hello udp";
    result = channel.send(message, std::strlen(message));
    if (result != connection_result::ok)
    {
        std::cerr << "send failed: "
                  << udp_connection::result_string(result)
                  << std::endl;
        return;
    }

    unsigned char buffer[512] = {0};
    std::size_t received_size = 0U;
    result = channel.receive(buffer, sizeof(buffer), &received_size);
    if (result == connection_result::ok)
    {
        std::cout << "received bytes: " << received_size << std::endl;
    }
}
```

## Notes

- `send` needs a default peer. If no default peer is configured, use `set_default_peer` first or call `send_to`.
- `receive` is non-blocking. Handle `connection_result::would_block` if no data is available yet.
- The class is move-only. Copy is intentionally disabled because the wrapped socket handle has unique ownership.

---

# tcp_connection_client

`tcp_connection_client` is a TCP client implementing `IConnection`. It initiates an outbound connection to a remote TCP server using non-blocking I/O.

- Non-blocking async connect — `open()` returns immediately, connection completes lazily on first `send()`/`receive()`.
- Supports the same framing modes as `tcp_connection_server`: `none`, `dmi`, `two_byte_len_big`, `two_byte_len_little`.
- Move-only (copy deleted). Socket handle has unique ownership.
- Connection loss (EOF, RST) is detected and surfaced as `would_block` / `send_failed`; reconnect by calling `close()` then `open()`.

## Header

Include:

```cpp
#include "tcp_connection_client.h"
```

## Config JSON

```json
{
    "type": "tcp_client",
    "remote_ip": "192.168.1.100",
    "remote_port": 5000,
    "local_ip": "0.0.0.0",
    "local_port": 0,
    "receive_framing": "dmi",
    "send_framing": "none"
}
```

- `remote_ip` (string, required) — server address.
- `remote_port` (integer, required) — server port.
- `local_ip` (string, optional) — local address to bind before connecting.
- `local_port` (integer, optional) — local port; `0` lets the OS choose.
- `receive_framing` (string, optional, default `"none"`) — receive-side framing: `"none"`, `"dmi"`, `"two_byte_len_big"`, `"two_byte_len_little"`.
- `send_framing` (string, optional, default `"none"`) — send-side framing: same values as `receive_framing`.
