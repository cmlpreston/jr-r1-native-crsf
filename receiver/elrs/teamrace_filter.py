"""Apply upstream PR3819's exact RX-slot guard to a disposable pinned tree."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def apply(source):
    source=Path(source)
    patch=(ROOT/'receiver/elrs/teamrace-telemetry-slot.patch').read_text()
    added='\n'.join(line[1:] for line in patch.splitlines() if line.startswith('+') and not line.startswith('+++'))+'\n'
    assert 'ExpressLRS_currTlmDenom != 1' in added and 'OtaNonce % ExpressLRS_currTlmDenom' in added
    text=source.read_text();anchor='    if (ProcessRFPacket(status))\n'
    if text.count(anchor)!=1 or added in text:raise ValueError('Unexpected RX ISR; review patch anchor')
    source.write_text(text.replace(anchor,added+anchor,1))
