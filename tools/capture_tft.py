#!/usr/bin/env python3
"""Capture the TFT35 framebuffer over the ESP32-S3 native USB serial port."""

from __future__ import annotations

import argparse
import binascii
import struct
import sys
import time
import zlib
from pathlib import Path

import serial
from serial.tools import list_ports


MAGIC = b"TFTSHOT1 "
ERROR_MAGIC = b"TFTSHOT-ERROR "


def default_port() -> str:
    candidates = [
        port.device
        for port in list_ports.comports()
        if "usbmodem" in port.device or port.vid == 0x303A
    ]
    if len(candidates) == 1:
        return candidates[0]
    if not candidates:
        raise RuntimeError("no ESP32 USB serial port found; supply --port")
    raise RuntimeError("multiple USB serial ports found; supply --port: " + ", ".join(candidates))


def read_header(port: serial.Serial, timeout: float) -> tuple[int, int, int, bytes]:
    deadline = time.monotonic() + timeout
    received = bytearray()
    while time.monotonic() < deadline:
        chunk = port.read(256)
        if not chunk:
            continue
        received.extend(chunk)
        error_marker = received.find(ERROR_MAGIC)
        if error_marker >= 0:
            line_end = received.find(b"\n", error_marker)
            if line_end >= 0:
                message = bytes(received[error_marker:line_end]).decode(
                    "utf-8", errors="replace"
                )
                raise RuntimeError(message)
        marker = received.find(MAGIC)
        if marker < 0:
            if len(received) > 4096:
                del received[:-len(MAGIC)]
            continue
        line_end = received.find(b"\n", marker)
        while line_end < 0 and time.monotonic() < deadline:
            chunk = port.read(256)
            if chunk:
                received.extend(chunk)
                line_end = received.find(b"\n", marker)
        if line_end < 0:
            break
        header = bytes(received[marker:line_end]).decode("ascii")
        fields = header.split()
        if len(fields) != 5 or fields[3] != "RGB888":
            raise RuntimeError(f"unexpected screenshot header: {header!r}")
        width, height, length = int(fields[1]), int(fields[2]), int(fields[4])
        payload_prefix = bytes(received[line_end + 1 :])
        return width, height, length, payload_prefix
    diagnostic = bytes(received[-500:]).decode("utf-8", errors="replace")
    raise TimeoutError(f"no screenshot header received; recent serial data:\n{diagnostic}")


def read_payload(port: serial.Serial, length: int, timeout: float,
                 prefix: bytes = b"") -> bytes:
    data = bytearray(prefix)
    deadline = time.monotonic() + timeout
    next_report = 10
    while len(data) < length and time.monotonic() < deadline:
        chunk = port.read(min(8192, length - len(data)))
        if chunk:
            data.extend(chunk)
            percent = len(data) * 100 // length
            if percent >= next_report:
                print(f"  {percent:3d}%", file=sys.stderr)
                next_report = (percent // 10 + 1) * 10
    if len(data) < length:
        raise TimeoutError(f"pixel stream stopped at {len(data)}/{length} bytes")
    return bytes(data[:length])


def transform_rgb(data: bytes, width: int, height: int, swap_rb: bool,
                  flip_x: bool, flip_y: bool) -> bytes:
    rows = [bytearray(data[y * width * 3 : (y + 1) * width * 3]) for y in range(height)]
    if swap_rb:
        for row in rows:
            for offset in range(0, len(row), 3):
                row[offset], row[offset + 2] = row[offset + 2], row[offset]
    if flip_x:
        for index, row in enumerate(rows):
            pixels = [row[offset : offset + 3] for offset in range(0, len(row), 3)]
            rows[index] = bytearray().join(reversed(pixels))
    if flip_y:
        rows.reverse()
    return b"".join(rows)


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    body = kind + payload
    return struct.pack(">I", len(payload)) + body + struct.pack(">I", binascii.crc32(body) & 0xFFFFFFFF)


def write_png(path: Path, width: int, height: int, rgb: bytes) -> None:
    scanlines = b"".join(
        b"\x00" + rgb[y * width * 3 : (y + 1) * width * 3]
        for y in range(height)
    )
    png = (
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
        + png_chunk(b"IDAT", zlib.compress(scanlines, 9))
        + png_chunk(b"IEND", b"")
    )
    path.write_bytes(png)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", nargs="?", default="tft-screenshot.png")
    parser.add_argument("--port")
    parser.add_argument("--timeout", type=float, default=60.0)
    parser.add_argument("--swap-rb", action="store_true", help="swap red and blue components")
    parser.add_argument("--flip-x", action="store_true")
    parser.add_argument("--flip-y", action="store_true")
    args = parser.parse_args()

    device = args.port or default_port()
    output = Path(args.output)
    print(f"Capturing {device} -> {output}", file=sys.stderr)
    with serial.Serial(device, 115200, timeout=0.25, write_timeout=2) as port:
        time.sleep(0.3)
        port.reset_input_buffer()
        port.write(b"SCREENSHOT\n")
        port.flush()
        width, height, length, prefix = read_header(port, 8.0)
        if length != width * height * 3:
            raise RuntimeError(f"invalid RGB888 payload size {length} for {width}x{height}")
        print(f"Receiving {width}x{height} RGB888 ({length} bytes)", file=sys.stderr)
        rgb = read_payload(port, length, args.timeout, prefix)

    rgb = transform_rgb(rgb, width, height, args.swap_rb, args.flip_x, args.flip_y)
    write_png(output, width, height, rgb)
    unique_sample = len(set(rgb[:: max(1, len(rgb) // 4096)]))
    print(f"Saved {output} ({output.stat().st_size} bytes; sampled levels={unique_sample})")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, TimeoutError, serial.SerialException) as error:
        print(f"capture_tft: {error}", file=sys.stderr)
        raise SystemExit(1)
