#!/usr/bin/env python3
import os
import re
import json
import argparse
import base64
from collections import OrderedDict

def parse_configs(folder):
    """
    Walk `folder` (and subfolders) for files matching:
      <RithmicSystem>_<Gateway>_connection_params[.<version>].txt
    Extract all MML_* parameters, but always set
      MML_SSL_CLNT_AUTH_FILE=rithmic_ssl_cert_auth_params
    Return a nested dict { system: { gateway: { MML_xxx: val, … } } }.
    """
    fname_re = re.compile(
        r'^(?P<system>[^_]+)_' +
        r'(?P<gateway>.+?)_connection_params' +
        r'(?:\.(?P<version>[^.]+))?\.txt$'
    )
    param_re = re.compile(r'(?P<key>MML_[\w_]+)\s*=\s*(?P<val>.+)')

    data = {}
    for root, _, files in os.walk(folder):
        for fname in files:
            m = fname_re.match(fname)
            if not m:
                continue

            system  = m.group('system')
            gateway = m.group('gateway')
            params  = {}

            with open(os.path.join(root, fname), 'r', encoding='utf-8') as f:
                for line in f:
                    pm = param_re.search(line)
                    if not pm:
                        continue
                    key = pm.group('key')
                    val = pm.group('val').strip()
                    params[key] = val

            # Force the SSL client-auth file param
            params['MML_SSL_CLNT_AUTH_FILE'] = 'rithmic_ssl_cert_auth_params'

            if params:
                data.setdefault(system, {}) \
                    .setdefault(gateway, {}) \
                    .update(params)

    return data

def reorder_systems(data):
    """
    Return an OrderedDict where keys starting with "Rithmic" come first (sorted),
    then all other keys (also sorted).
    """
    ordered = OrderedDict()
    # Rithmic* first
    for system in sorted(data):
        if system.startswith("Rithmic"):
            ordered[system] = data[system]
    # Others next
    for system in sorted(data):
        if not system.startswith("Rithmic"):
            ordered[system] = data[system]
    return ordered

def main():
    parser = argparse.ArgumentParser(
        description="Parse Rithmic connection-params .txt files, encode as Base64, and emit to a .bin file."
    )
    parser.add_argument('folder', help="Root folder containing the .txt config files")
    parser.add_argument('-o','--output', default='rithmic.bin',
                        help="Path to write the Base64-encoded config (default: rithmic.bin)")
    args = parser.parse_args()

    # Parse and order
    result  = parse_configs(args.folder)
    ordered = reorder_systems(result)

    # Dump to JSON string
    json_str = json.dumps(ordered, indent=2)

    # Encode as Base64
    b64_bytes = base64.b64encode(json_str.encode('utf-8'))

    # Write binary file
    with open(args.output, 'wb') as out:
        out.write(b64_bytes)

    print(f"✅ Parsed {len(ordered)} system(s), Base64-encoded JSON written to {args.output}")

if __name__ == '__main__':
    main()
