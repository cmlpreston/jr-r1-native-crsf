# Highfield Park offline basemap

Location: Highfield Park, Camberwell, Victoria, Australia.
The user-selected Google Maps link resolves to -37.8361917, 145.0852289.
The basemap geometry is **OpenStreetMap**, not Google imagery.

Map data © OpenStreetMap contributors, available under the Open Database
License (ODbL) 1.0: https://www.openstreetmap.org/copyright
License: https://opendatacommons.org/licenses/odbl/1-0/

`highfield.osm` is the unmodified public OSM API extract downloaded 2026-09-16:
https://api.openstreetmap.org/api/0.6/map?bbox=145.081,-37.840,145.090,-37.832

Run `python3 tools/generate_highfield_map.py` from the repository to regenerate
`src/highfield_map_data.inc`. The generator projects geometry into integer
metres, selects parks, pitches, buildings, playgrounds, parking, roads and
paths, and triangulates filled areas. The extract and generated vector data
are distributed under ODbL 1.0. The rendering code uses the repository license.

The renderer uses a local equirectangular projection, north up, centred on the
most recent GPS fix. It shares the trace's metres-to-pixels scale. Outside this
extract it falls back to a grid and states “No local map”; it does not fetch maps
or require internet at runtime. This is a local map preview, not a navigation
or survey system. Map selection/upload is future work.

Simulator telemetry follows a synthetic oval inside the northern cricket oval
centred at -37.8354286, 145.0857802. It is not a recorded vehicle trace.
