#!/usr/bin/env python3
"""ANSI input dashboard; optional PIN-protected RF bench control. Requires pyserial."""
import argparse
import shutil
import sys
import time
import select
import termios
import tty
import getpass
import os


class SnapshotParser:
    def __init__(self):
        self.pending = None

    def feed(self, line):
        fields = line.strip().split('\t')
        try:
            if fields[0] == '@INPUT_BEGIN':
                self.pending = None
                if len(fields) == 12 and fields[1] == '1':
                    self.pending = {'header': fields[2:], 'rows': {}, 'mcps': {}}
            elif self.pending is not None and fields[0] == '@INPUT_MCP':
                chip, online, levels, errors = map(int, fields[1:])
                self.pending['mcps'][chip] = (online, levels, errors)
            elif self.pending is not None and fields[0] == '@INPUT_RF':
                if len(fields) != 8:
                    raise ValueError('incomplete RF status')
                self.pending['rf'] = tuple(map(int, fields[1:]))
            elif self.pending is not None and fields[0] == '@INPUT_ROW':
                if len(fields) != 13:
                    raise ValueError('incomplete row')
                row = {'id': int(fields[1]), 'name': fields[2], 'pin': fields[3],
                       'kind': int(fields[4]), 'mapped': int(fields[5]),
                       'fault': int(fields[6]), 'raw': tuple(map(int, fields[7:9])),
                       'stable': tuple(map(int, fields[9:11])),
                       'event': fields[11], 'event_ms': int(fields[12])}
                self.pending['rows'][row['id']] = row
            elif self.pending is not None and fields[0] == '@INPUT_END':
                frame, self.pending = self.pending, None
                count = int(fields[1])
                if set(frame['rows']) == set(range(count)) and count == 19 and set(frame['mcps']) == {1, 2}:
                    # Validate numeric header before handing it to the renderer.
                    for i, value in enumerate(frame['header']):
                        if i != 1:
                            int(value)
                    return frame
        except (ValueError, IndexError):
            self.pending = None
        return None


def safe(text):
    return ''.join(c if ' ' <= c <= '~' else '?' for c in str(text))


def bar(value, width=19):
    pos = round((max(-1000, min(1000, value)) + 1000) * (width - 1) / 2000)
    chars = ['-'] * width
    chars[width // 2] = '|'
    chars[pos] = '#'
    return '[' + ''.join(chars) + ']'


def render(frame, stale=False, rf_test=False, reply=''):
    if frame is None:
        return ['JR R-1 INPUT MONITOR — waiting for INPUTMON firmware',
                'No snapshots yet. Startup/PIN does not require unlocking.',
                'If this persists, check firmware version and serial port.',
                'Ctrl+C exits. Opening USB serial may restart the XIAO.']
    h = frame['header']
    uptime, status = int(h[0]), h[1]
    raw, filtered, throttle, steering_adc, steering = map(int, h[2:7])
    lines = ['JR R-1 INPUT MONITOR | READ ONLY | Ctrl+C exits',
             f"{'STALE - do not trust values' if stale else 'LIVE'}  Uptime {uptime/1000:.1f}s  Outputs: {status}  Input/mix valid: {h[7]}"]
    for chip, (online, levels, errors) in sorted(frame['mcps'].items()):
        lines.append(f"K{chip} {'ONLINE ' if online else 'OFFLINE'}  GPIO=0x{levels:04X}  I2C read errors={errors}")
    lines += [f'Throttle raw {raw:4d} filtered {filtered:4d} {throttle/10:+6.1f}% {bar(throttle)}',
              f'Steering filtered {steering_adc:4d}          {steering/10:+6.1f}% {bar(steering)}',
              f"Battery ADC: {h[8]} mV" if int(h[9]) else 'Battery ADC: sensing disabled in TX battery settings',
              'Input positions above are before CUT; they are not transmitted outputs.',
              '', 'CONTROL              PCB CONTACTS         RAW   STATE       LAST EVENT',
              '-' * 78]
    if status == 'HALTED_SAFE':
        lines[4] = f'Throttle raw ADC {raw:4d}  |  Control task not started'
        lines[5] = f'Steering raw ADC {steering_adc:4d}  |  Calibration not applied'
        lines[7] = 'Startup failed: sampling only. No control actions; outputs remain safe.'
    if rf_test:
        lines[0] = 'JR R-1 RF BENCH | A: arm request | Space/C: CUT | Q/Ctrl+C: exit'
        rf = frame.get('rf')
        if rf:
            enabled, requested, thr_sent, steer_sent, link, lq, frames = rf
            lines[7] = f"RF bench={'ON' if enabled else 'OFF'} arm={requested} link={link} LQ={lq} sent THR={thr_sent} STR={steer_sent}"
            lines[8] = f'CRSF frames: {frames} | {reply}'
        else:
            lines[8] = 'RF bench needs updated firmware; arming unavailable.'
    for row in frame['rows'].values():
        if row['kind'] == 0:  # proportional controls already shown above
            continue
        if not row['mapped']:
            state = 'UNMAPPED'
        elif row['fault']:
            state = 'FAULT'
        elif row['kind'] == 3:
            state = {(0, 0): 'CENTRE', (1, 0): 'NEGATIVE', (0, 1): 'POSITIVE'}.get(row['stable'], 'BOTH!')
        else:
            state = ('ON' if row['kind'] == 2 else 'PRESSED') if row['stable'][0] else ('OFF' if row['kind'] == 2 else 'RELEASED')
        contacts = '/'.join(map(str, row['raw'])) if row['kind'] == 3 else str(row['raw'][0])
        event = row['event'] if row['event_ms'] else '-'
        lines.append(f"{row['name'][:20]:20} {row['pin'][:20]:20} {contacts:5} {state:11} {event}")
    lines += ['', 'RAW 1=active (contact LOW). Trim pair = negative/positive.',
              'Unconnected ADCs may float. DISPLAY_SW is not wired.',
              'Recovery: control actions disabled; reset after hardware repairs.' if status == 'HALTED_SAFE'
              else 'Controls retain their normal firmware actions; this monitor changes none.']
    return lines


def main():
    import serial
    from serial.tools import list_ports
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--port', help='USB serial port; auto-detect the XIAO if omitted')
    ap.add_argument('--rf-test', action='store_true', help='Temporary live RF bench mode; asks for PIN, starts CUT')
    args = ap.parse_args()
    if not sys.stdout.isatty():
        ap.error('Run this in an interactive ANSI terminal.')
    if args.rf_test and not sys.stdin.isatty():
        ap.error('RF bench control requires an interactive keyboard.')
    pin = getpass.getpass('RF bench PIN (outputs start CUT): ') if args.rf_test else ''
    if args.rf_test and (len(pin) != 4 or not pin.isascii() or not pin.isdigit()):
        ap.error('PIN must be four digits.')
    port = args.port
    if not port:
        ports = [p.device for p in list_ports.comports() if p.vid == 0x303A and p.pid == 0x1001]
        if len(ports) != 1:
            ap.error('Specify --port; expected exactly one Espressif USB device.')
        port = ports[0]
    s = serial.Serial()
    s.port, s.baudrate, s.timeout, s.write_timeout = port, 115200, .05, .5
    s.dtr = s.rts = False
    parser, frame = SnapshotParser(), None
    received, request, draw = 0., 0., 0.
    buffer = b''
    keyboard = None
    authenticated = False
    pin_accepted = False
    attempted = False
    keepalive = 0.
    reply = 'Waiting for recovery firmware'
    try:
        s.open()
        if args.rf_test:
            keyboard = termios.tcgetattr(sys.stdin)
            tty.setcbreak(sys.stdin.fileno())
        sys.stdout.write('\x1b[?1049h\x1b[?25l\x1b[2J')
        while True:
            now = time.monotonic()
            if args.rf_test:
                if authenticated and now >= keepalive and now - received < 1:
                    s.write(b'RFKEEP\n')
                    keepalive = now + .25
                if select.select([sys.stdin], [], [], 0)[0]:
                    key = os.read(sys.stdin.fileno(), 1).decode('ascii', errors='ignore').lower()
                    if key == 'q':
                        break
                    if key in ('c', ' '):
                        s.write(b'RFCUT\n')
                        reply = 'CUT requested'
                    elif key == 'a' and authenticated and now - received < 1:
                        s.write(b'RFARM\n')
                        reply = 'Arm request sent'
            if now >= request:
                s.write(b'INPUTMON\n')
                request = now + 1.5  # at most one outstanding request; timeout handles old firmware
            buffer += s.read(max(1, min(s.in_waiting, 4096)))
            while b'\n' in buffer:
                line, buffer = buffer.split(b'\n', 1)
                decoded = line.decode('utf-8', errors='replace')
                if decoded.startswith('@RF_REPLY '):
                    reply = safe(decoded[len('@RF_REPLY '):].strip())
                    if reply.startswith('Bench mode enabled;'):
                        pin_accepted = True
                    elif reply.startswith('Bench mode rejected:'):
                        pin_accepted = authenticated = False
                result = parser.feed(decoded)
                if result is not None:
                    frame, received, request = result, now, now + .2
                    if args.rf_test and 'rf' in frame:
                        # Never retry credentials automatically after a reboot.
                        if not attempted:
                            s.write(('RFTEST ' + pin + '\n').encode('ascii'))
                            pin = ''
                            attempted = True
                        authenticated = pin_accepted and bool(frame['rf'][0])
            if len(buffer) > 8192:
                buffer = b''
            if now >= draw:
                draw = now + .2
                size = shutil.get_terminal_size()
                lines = render(frame, now - received > 2, args.rf_test, reply)
                if size.lines < len(lines):
                    lines = lines[:max(0, size.lines - 1)] + ['Enlarge terminal vertically to see all controls.']
                output = '\x1b[H' + '\r\n'.join(safe(x)[:max(1, size.columns-1)] + '\x1b[K' for x in lines) + '\x1b[J'
                sys.stdout.write(output)
                sys.stdout.flush()
    except KeyboardInterrupt:
        pass
    except serial.SerialException as e:
        print(f'\r\nSerial connection ended: {safe(e)}', file=sys.stderr)
        return 1
    finally:
        if args.rf_test and s.is_open:
            try:
                s.write(b'RFCUT\n')
            except serial.SerialException:
                pass  # Firmware's independent heartbeat expiry also forces CUT.
        s.close()
        if keyboard is not None:
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, keyboard)
        sys.stdout.write('\x1b[0m\x1b[?25h\x1b[?1049l')
        sys.stdout.flush()
    return 0


if __name__ == '__main__':
    sys.exit(main())
