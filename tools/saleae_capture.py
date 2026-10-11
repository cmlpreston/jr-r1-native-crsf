#!/usr/bin/env python3
"""Local Logic 2 capture/export interface; never resets or flashes the transmitter."""
from __future__ import annotations

import argparse
import csv
import dataclasses
import enum
import json
import math
from datetime import datetime, timezone
from pathlib import Path
import sys


def json_value(value):
    if dataclasses.is_dataclass(value):
        return {k: json_value(v) for k, v in dataclasses.asdict(value).items()}
    if isinstance(value, enum.Enum):
        return value.name
    if isinstance(value, dict):
        return {k: json_value(v) for k, v in value.items()}
    if isinstance(value, (list, tuple)):
        return [json_value(v) for v in value]
    return value


def channels(value):
    try:
        result = [int(v) for v in value.split(',')]
    except ValueError as exc:
        raise argparse.ArgumentTypeError('channels must be comma-separated integers') from exc
    if not result or len(set(result)) != len(result) or any(v < 0 or v > 15 for v in result):
        raise argparse.ArgumentTypeError('choose unique channels between 0 and 15')
    return result


def positive(value):
    result = float(value)
    if not math.isfinite(result) or result <= 0:
        raise argparse.ArgumentTypeError('value must be finite and positive')
    return result


def select_device(devices, wanted=None):
    real = [d for d in devices if not d.is_simulation]
    matches = [d for d in real if wanted is None or d.device_id == wanted]
    if len(matches) != 1:
        raise ValueError('Select exactly one real device with --device; available: ' +
                         ', '.join(d.device_id for d in real))
    return matches[0]


def summarize_csv(path):
    """Count captured transitions without loading a large raw export into RAM."""
    rows = 0
    previous = None
    changes = {}
    first = last = None
    with Path(path).open(newline='') as f:
        reader = csv.DictReader(f)
        names = reader.fieldnames or []
        if not names or not names[0].startswith('Time'):
            raise ValueError('Expected a Saleae raw digital CSV with Time as first column')
        changes = {k: 0 for k in names[1:]}
        for row in reader:
            stamp = float(row[names[0]])
            if first is None:
                first = stamp
            last = stamp
            if previous is not None:
                for key in changes:
                    changes[key] += row[key] != previous[key]
            previous = row
            rows += 1
    return {'rows': rows, 'first_sample_seconds': first,
            'last_sample_seconds': last, 'transitions': changes}


def read_analyzers(path):
    specs = json.loads(Path(path).read_text()) if path else []
    if not isinstance(specs, list):
        raise ValueError('Analyzer configuration must be a JSON list')
    for spec in specs:
        if not isinstance(spec, dict) or not isinstance(spec.get('name'), str):
            raise ValueError('Each analyzer requires a name and optional settings/label')
        if not isinstance(spec.get('settings', {}), dict):
            raise ValueError('Analyzer settings must be an object')
    return specs


def export_capture(capture, output, raw, specs):
    # Save the original first, so a failed analyzer/export never loses the capture.
    capture.save_capture(str(output / 'capture.sal'))
    if raw in ('csv', 'both'):
        capture.export_raw_data_csv(str(output))
    if raw in ('binary', 'both'):
        capture.export_raw_data_binary(str(output))
    handles = [capture.add_analyzer(s['name'], label=s.get('label'),
                                   settings=s.get('settings', {})) for s in specs]
    if handles:
        capture.export_data_table(str(output / 'decoded.csv'), handles)
    if (output / 'digital.csv').exists():
        (output / 'summary.json').write_text(json.dumps(
            summarize_csv(output / 'digital.csv'), indent=2) + '\n')


def parser():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port', type=int, default=10430, help='Local Logic 2 automation port')
    sub = p.add_subparsers(dest='command', required=True)
    sub.add_parser('devices', help='Report Logic version and attached physical devices')
    c = sub.add_parser('capture', help='Capture a fixed duration from a physical device')
    c.add_argument('--device', help='Required if more than one analyser is connected')
    c.add_argument('--channels', type=channels, default=[0, 1, 2, 3, 4])
    c.add_argument('--rate', type=int, default=24_000_000, help='Samples per second')
    c.add_argument('--seconds', type=positive, default=2.0)
    c.add_argument('--signal-hz', type=positive, help='Reject clearly undersampled bus clocks')
    e = sub.add_parser('export', help='Export an existing .sal without acquiring new data')
    e.add_argument('session', type=Path)
    for cmd in (c, e):
        cmd.add_argument('--out', type=Path, help='New directory (must not already exist)')
        cmd.add_argument('--raw', choices=['csv', 'binary', 'both', 'none'], default='csv')
        cmd.add_argument('--analyzers', type=Path, help='JSON list of Saleae analyzer settings')
    s = sub.add_parser('summary', help='Summarize an existing raw digital CSV offline')
    s.add_argument('csv', type=Path)
    return p


def main(argv=None):
    p = parser()
    args = p.parse_args(argv)
    if args.command == 'summary':
        print(json.dumps(summarize_csv(args.csv), indent=2))
        return 0
    if args.command == 'capture':
        if args.rate <= 0:
            p.error('--rate must be positive')
        if args.signal_hz and args.rate < 4 * args.signal_hz:
            p.error('Use at least 4 samples per clock for this diagnostic; lower the bus speed or use a faster analyser')
    try:
        from saleae import automation
    except ImportError:
        p.error('Install: .venv/bin/python -m pip install -r tools/requirements-saleae.txt')
    try:
        specs = read_analyzers(args.analyzers) if args.command != 'devices' else []
        with automation.Manager.connect(port=args.port, connect_timeout_seconds=5) as manager:
            if args.command == 'devices':
                print(json.dumps(json_value({'app': manager.get_app_info(),
                                             'devices': manager.get_devices()}), indent=2))
                return 0
            device = None
            if args.command == 'capture':
                device = select_device(manager.get_devices(), args.device)
            elif not args.session.is_file():
                raise ValueError(f'Session not found: {args.session}')
            output = (args.out or Path('/tmp') / datetime.now().strftime('r1-saleae-%Y%m%d-%H%M%S-%f')).resolve()
            output.mkdir(parents=True, exist_ok=False)
            metadata = {'created_utc': datetime.now(timezone.utc).isoformat(),
                        'app': json_value(manager.get_app_info()), 'analyzers': specs,
                        'status': 'started'}
            if device:
                metadata.update(device=json_value(device), channels=args.channels,
                                sample_rate=args.rate, duration_seconds=args.seconds)
            else:
                metadata['source'] = str(args.session.resolve())
            manifest = output / 'manifest.json'
            manifest.write_text(json.dumps(metadata, indent=2) + '\n')
            try:
                if device:
                    config = automation.LogicDeviceConfiguration(
                        enabled_digital_channels=args.channels, digital_sample_rate=args.rate)
                    mode = automation.CaptureConfiguration(
                        capture_mode=automation.TimedCaptureMode(duration_seconds=args.seconds))
                    capture = manager.start_capture(device_id=device.device_id,
                                                    device_configuration=config,
                                                    capture_configuration=mode)
                else:
                    capture = manager.load_capture(str(args.session.resolve()))
                with capture:
                    if device:
                        print(f'Capturing {args.seconds:g}s to {output}', flush=True)
                        capture.wait()
                    export_capture(capture, output, args.raw, specs)
                metadata['status'] = 'complete'
            except BaseException as exc:
                metadata.update(status='failed', error=str(exc))
                raise
            finally:
                manifest.write_text(json.dumps(metadata, indent=2) + '\n')
            print(output)
            return 0
    except Exception as exc:
        print(f'Saleae: {exc}\nEnsure Logic 2 is running with Settings → Enable automation server (port {args.port}).', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
