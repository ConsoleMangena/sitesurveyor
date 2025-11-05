#!/usr/bin/env python3
import argparse
import base64
import hmac
import hashlib
import os
import random
import re
import socket
import subprocess
import sys
from typing import Optional

# Discipline mapping (must match AppSettings::licensePrefixFor)
DISC_MAP = {
    'engineering surveying': 'ES',
    'cadastral surveying': 'CS',
    'remote sensing': 'RS',
    'gis & mapping': 'GM',
    'gis and mapping': 'GM',
}

CODES = {v: k for k, v in DISC_MAP.items()}


def normalize_disc(name_or_code: str) -> Optional[tuple[str, str]]:
    s = name_or_code.strip().lower()
    if s in DISC_MAP:
        return DISC_MAP[s], name_or_code
    # Try code
    s2 = s.replace('&', 'and').replace(' ', '')
    for long, code in DISC_MAP.items():
        if s2 == code.lower():
            return code, long.title()
    # Try match by prefix
    for long, code in DISC_MAP.items():
        if long.startswith(s):
            return code, long.title()
    return None


def read_machine_id() -> Optional[str]:
    # Linux
    for p in ['/etc/machine-id', '/var/lib/dbus/machine-id']:
        try:
            if os.path.isfile(p):
                with open(p, 'r') as f:
                    val = f.read().strip()
                    if val:
                        return val
        except Exception:
            pass
    # macOS
    try:
        out = subprocess.check_output(['ioreg', '-rd1', '-c', 'IOPlatformExpertDevice'], stderr=subprocess.DEVNULL, text=True)
        m = re.search(r'"IOPlatformUUID"\s*=\s*"([A-F0-9\-]+)"', out)
        if m:
            return m.group(1)
    except Exception:
        pass
    # Windows
    try:
        if os.name == 'nt':
            import winreg
            with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\\Microsoft\\Cryptography") as k:
                val, _ = winreg.QueryValueEx(k, 'MachineGuid')
                if val:
                    return str(val)
    except Exception:
        pass
    return None


def license_pepper(override: Optional[str] = None) -> bytes:
    base = 'SS-PEPPER-v1'
    if override:
        mid = override
    else:
        mid = read_machine_id() or socket.gethostname()
    return (base + mid).encode('utf-8', 'ignore')


def b32_no_pad(b: bytes) -> str:
    return base64.b32encode(b).decode('ascii').rstrip('=')


def gen_body(n: int = 16) -> str:
    alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'
    return ''.join(random.choice(alphabet) for _ in range(n))


def hmac_sha256(key: bytes, data: bytes) -> bytes:
    return hmac.new(key, data, hashlib.sha256).digest()


def make_key(secret: bytes, discipline: str, bind: bool = True, body_len: int = 16, device_override: Optional[str] = None) -> str:
    norm = normalize_disc(discipline)
    if not norm:
        raise SystemExit(f'Unknown discipline: {discipline}')
    code, long_name = norm
    body = gen_body(body_len)
    msg_parts = [code, long_name, body, license_pepper(device_override).decode('utf-8', 'ignore') if bind else '*']
    msg = '|'.join(msg_parts).encode('utf-8')
    sig = b32_no_pad(hmac_sha256(secret, msg))[:10]
    # Presentable: CODE-BODY-SIG (group body as 4-char chunks)
    grouped = '-'.join(body[i:i+4] for i in range(0, len(body), 4))
    return f'{code}-{grouped}-{sig}'


def make_dev_key(discipline: str) -> str:
    norm = normalize_disc(discipline)
    if not norm:
        raise SystemExit(f'Unknown discipline: {discipline}')
    code, _ = norm
    body = gen_body(12)
    return f'DEV-{code}-{body}'


def main():
    ap = argparse.ArgumentParser(description='Generate SiteSurveyor offline license keys')
    ap.add_argument('--disc', action='append', help='Discipline name or code (ES, CS, RS, GM). May be repeated.')
    ap.add_argument('--all', action='store_true', help='Generate for all disciplines')
    ap.add_argument('--no-bind', action='store_true', help='Do not bind to this machine (only works if app built to accept unbound keys)')
    ap.add_argument('--device', help='Override device binding token (advanced)')
    ap.add_argument('--count', type=int, default=1, help='How many keys per discipline')
    ap.add_argument('--dev', action='store_true', help='Generate DEV- keys (work only in dev builds)')
    args = ap.parse_args()

    discs = []
    if args.all:
        discs = list(DISC_MAP.keys())
    if args.disc:
        discs.extend(args.disc)
    if not discs:
        ap.error('Specify --all or at least one --disc')

    if args.dev:
        for d in discs:
            for _ in range(args.count):
                print(make_dev_key(d))
        return

    secret = os.environ.get('SS_LICENSE_SECRET')
    if not secret:
        print('error: SS_LICENSE_SECRET not set in environment', file=sys.stderr)
        sys.exit(2)
    secret_b = secret.encode('utf-8')

    for d in discs:
        for _ in range(args.count):
            print(make_key(secret_b, d, bind=(not args.no_bind), device_override=args.device))


if __name__ == '__main__':
    main()
