#!/usr/bin/env python3
"""
UFSDxe.efi binary patcher for Xiaomi Pad 6 (pipa)
Patches:
  1. Sleep callback registration → skip (return 0)
  2. RPMB LUN init → skip (unconditional branch)
"""
import struct
import hashlib
import sys
import os

UFSDXE_PATH = os.path.join(os.path.dirname(__file__),
    "Binaries/pipa/QcomPkg/Drivers/UFSDxe/UFSDxe.efi")

PATCHES = [
    {
        "name": "Sleep callback skip",
        "offset": 0x695C,
        "original": bytes([0xA9, 0x00, 0x00, 0x90, 0x28, 0xB1, 0x44, 0xF9]),
        "patched":  bytes([0x00, 0x00, 0x80, 0x52, 0xC0, 0x03, 0x5F, 0xD6]),
        "ida_asm":  "MOV W0, #0; RET",
        "desc":     "sub_695C: EFI_KERNEL_PROTOCOL not available, skip RegisterPwrTransitionNotify",
    },
    {
        "name": "RPMB LUN init skip",
        "offset": 0x8FD0,
        "original": bytes([0x81, 0x00, 0x00, 0x54]),
        "patched":  bytes([0x04, 0x00, 0x00, 0x14]),
        "ida_asm":  "B.NE → B (unconditional)",
        "desc":     "sub_8EB8: Skip RPMB LUN (0xC4) init, SK Hynix RPMB hangs",
    },
]

def md5(data):
    return hashlib.md5(data).hexdigest()

def verify_patch(data, patch):
    """Verify original bytes match at offset."""
    actual = data[patch["offset"]:patch["offset"]+len(patch["original"])]
    if actual == patch["original"]:
        return "OK (original bytes match)"
    elif actual == patch["patched"]:
        return "ALREADY APPLIED"
    else:
        return f"MISMATCH: got {actual.hex(' ')} expected {patch['original'].hex(' ')}"

def apply_patch(data, patch):
    """Apply a single patch, return new data."""
    data = bytearray(data)
    data[patch["offset"]:patch["offset"]+len(patch["patched"])] = patch["patched"]
    return bytes(data)

def main():
    if not os.path.exists(UFSDXE_PATH):
        print(f"ERROR: {UFSDXE_PATH} not found")
        sys.exit(1)

    data = open(UFSDXE_PATH, "rb").read()
    original_md5 = md5(data)
    print(f"File: {UFSDXE_PATH}")
    print(f"Size: {len(data)} bytes")
    print(f"MD5:  {original_md5}")
    print()

    # Verify all patches
    all_ok = True
    for p in PATCHES:
        status = verify_patch(data, p)
        print(f"[{p['name']}] offset=0x{p['offset']:X}")
        print(f"  {p['desc']}")
        print(f"  Status: {status}")
        print(f"  Original: {p['original'].hex(' ')}")
        print(f"  Patched:  {p['patched'].hex(' ')}")
        print(f"  IDA asm:  {p['ida_asm']}")
        if "MISMATCH" in status:
            all_ok = False
        print()

    if not all_ok:
        print("ERROR: Some patches have mismatched original bytes!")
        sys.exit(1)

    # Apply patches
    patched_data = data
    applied = 0
    for p in PATCHES:
        if verify_patch(patched_data, p) == "ALREADY APPLIED":
            print(f"Skip (already applied): {p['name']}")
            continue
        patched_data = apply_patch(patched_data, p)
        applied += 1
        print(f"Applied: {p['name']}")

    if applied == 0:
        print("\nAll patches already applied. Nothing to do.")
        return

    # Write patched file
    with open(UFSDXE_PATH, "wb") as f:
        f.write(patched_data)

    new_md5 = md5(patched_data)
    print(f"\nPatched MD5: {new_md5}")
    print(f"Original MD5: {original_md5}")
    print(f"Bytes changed: {sum(1 for a, b in zip(data, patched_data) if a != b)}")

if __name__ == "__main__":
    main()
