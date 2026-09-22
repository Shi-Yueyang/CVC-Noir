import argparse
import socket
import struct
import time


DMI_MAGIC_FIRST = 0x55
DMI_MAGIC_SECOND = 0xAA
DMI_HEADER_SIZE = 6


def build_length_prefix(length: int, endian: str) -> bytes:
    if endian == "big":
        return struct.pack(">H", length)
    if endian == "little":
        return struct.pack("<H", length)
    raise ValueError(f"Unsupported endian: {endian}")


def build_dmi_frame(payload: bytes) -> bytes:
    """Build a DMI-framed message: [0x55, 0xAA, 0x00, total_len, 0x00, 0x00] + payload."""
    total_length = DMI_HEADER_SIZE + len(payload)
    if total_length > 255:
        raise ValueError(f"Payload too large for DMI framing: {len(payload)} bytes (max 249)")
    header = bytes([
        DMI_MAGIC_FIRST,
        DMI_MAGIC_SECOND,
        0x00,
        total_length,
        0x00,
        0x00,
    ])
    return header + payload


def send_frames(host: str, port: int, payload: bytes, endian: str, count: int, delay: float) -> None:
    prefix = build_length_prefix(len(payload), endian)
    with socket.create_connection((host, port), timeout=3) as sock:
        for _ in range(count):
            sock.sendall(prefix + payload)
            time.sleep(delay)


def send_dmi_frames(host: str, port: int, payload: bytes, count: int, delay: float) -> None:
    frame = build_dmi_frame(payload)
    with socket.create_connection((host, port), timeout=3) as sock:
        for _ in range(count):
            sock.sendall(frame)
            time.sleep(delay)


def main() -> None:
    parser = argparse.ArgumentParser(description="TCP framing test helper")
    parser.add_argument("--host", default="127.0.0.1", help="TCP server host")
    parser.add_argument("--port", type=int, default=8080, help="TCP server port")
    parser.add_argument("--payload", default="HELLO", help="Payload string")
    parser.add_argument("--framing", default="length",
                        choices=["length", "dmi"],
                        help="Framing method: length (2-byte prefix) or dmi")
    parser.add_argument("--endian", choices=["big", "little"], default="big",
                        help="Length field endianness (only for --framing length)")
    parser.add_argument("--count", type=int, default=2, help="Number of frames to send")
    parser.add_argument("--delay", type=float, default=0.2, help="Delay between frames (seconds)")
    args = parser.parse_args()

    payload = args.payload.encode("ascii")

    if args.framing == "dmi":
        send_dmi_frames(
            host=args.host,
            port=args.port,
            payload=payload,
            count=args.count,
            delay=args.delay,
        )
    else:
        send_frames(
            host=args.host,
            port=args.port,
            payload=payload,
            endian=args.endian,
            count=args.count,
            delay=args.delay,
        )


if __name__ == "__main__":
    main()
