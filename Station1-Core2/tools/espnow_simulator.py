# Standalone ESP-NOW message simulator
# Generates binary payloads compatible with ShowcaseMessage struct used in firmware
# Usage: python3 espnow_simulator.py [--type TYPE] [--username NAME] [--amount N] [--desc DESC] [--out FILE]

import struct
import time
import argparse
import binascii

# ShowcaseMessage layout (packed) - matches the C struct in ShowcaseProtocol.h
# uint8_t type;                 -> B
# char username[32];            -> 32s
# int32_t amount;               -> i
# char description[16];         -> 16s
# uint8_t status;               -> B
# uint16_t checksum;            -> H
# uint32_t timestamp;           -> I
# uint8_t padding[2];           -> 2s

STRUCT_FMT = '<B32si16sBHI2s'  # little-endian
STRUCT_SIZE = struct.calcsize(STRUCT_FMT)

POLY = 0xA001

def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ POLY
            else:
                crc >>= 1
    return crc & 0xFFFF

def create_message(msg_type: int, username: str = '', amount: int = 0, description: str = '', status: int = 0) -> bytes:
    username_b = username.encode('utf-8')[:31]
    username_b = username_b.ljust(32, b'\x00')
    desc_b = description.encode('utf-8')[:15]
    desc_b = desc_b.ljust(16, b'\x00')
    timestamp = int(time.time()) & 0xFFFFFFFF

    # checksum placeholder 0
    checksum_placeholder = 0
    padding = b'\x00\x00'

    packed = struct.pack(STRUCT_FMT, msg_type, username_b, amount, desc_b, status, checksum_placeholder, timestamp, padding)
    # calculate checksum over whole struct except the checksum field (bytes offset)
    # find offset of checksum field: B(1)+32+4+16+1 = 54 bytes -> checksum at offset 54
    # but easier: rebuild with checksum=0 then compute
    chk = crc16(packed)
    # repack with checksum
    packed = struct.pack(STRUCT_FMT, msg_type, username_b, amount, desc_b, status, chk, timestamp, padding)
    return packed

def hexdump(data: bytes) -> str:
    return binascii.hexlify(data).decode('ascii')

if __name__ == '__main__':
    p = argparse.ArgumentParser(description='Create test ShowcaseMessage binary payloads')
    p.add_argument('--type', type=int, default=2, help='Message type (number)')
    p.add_argument('--username', type=str, default='TestUser', help='Username')
    p.add_argument('--amount', type=int, default=0, help='Amount (int)')
    p.add_argument('--desc', type=str, default='', help='Description')
    p.add_argument('--status', type=int, default=0, help='Status byte')
    p.add_argument('--out', type=str, default='', help='Write binary to file')

    args = p.parse_args()

    data = create_message(args.type, args.username, args.amount, args.desc, args.status)
    print(f'Created payload ({len(data)} bytes)')
    print('Hex:')
    print(hexdump(data))

    if args.out:
        with open(args.out, 'wb') as f:
            f.write(data)
        print(f'Wrote binary payload to {args.out}')

    # Example: print field breakdown
    unpacked = struct.unpack(STRUCT_FMT, data)
    print('\nFields:')
    print(' type:', unpacked[0])
    print(' username:', unpacked[1].split(b"\x00",1)[0].decode('utf-8', errors='ignore'))
    print(' amount:', unpacked[2])
    print(' desc:', unpacked[3].split(b"\x00",1)[0].decode('utf-8', errors='ignore'))
    print(' status:', unpacked[4])
    print(' checksum:', hex(unpacked[5]))
    print(' timestamp:', unpacked[6])
