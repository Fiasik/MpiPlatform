#!/usr/bin/env python3
"""Strict reader for the particular legacy PV-2223 Pressure file, not a general Qt reader."""
import csv
import hashlib
import json
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RAW = Path('/media/vladislav-b/Tree/Library/рез/ЛУКОЙЛ-УНП/ВИСБРЕКИНГ/-/PV-2223/03_10_2024/Pressure_1.data')
CSV = Path('/home/vladislav-b/Documents/Математика_основы/_данные/PV-2223')


def main():
    data = RAW.read_bytes()
    pos = 0

    def take(fmt):
        nonlocal pos
        value = struct.unpack_from('>'+fmt, data, pos)
        pos += struct.calcsize('>'+fmt)
        return value[0]

    def string():
        nonlocal pos
        size = take('I')
        if size > len(data)-pos or size % 2:
            raise ValueError('Unsupported string encoding')
        value = data[pos:pos+size].decode('utf-16-be')
        pos += size
        return value

    title, numeric_x, x_format = string(), take('?'), string()
    if take('q') != 1:
        raise ValueError('Expected one y axis in this particular file')
    y_format = string()
    if take('q') != 2:
        raise ValueError('Expected two series in this particular file')
    metadata = []
    for _ in range(2):
        axis, name = take('B'), string()
        color = data[pos:pos+11]
        if len(color) != 11 or color[0] != 1:
            raise ValueError('Unsupported QColor layout')
        pos += 11
        metadata.append({'axis': axis, 'name': name, 'color_hex': color.hex()})
    comparisons = []
    for duplicate in [False, True]:
        for series in range(2):
            count = take('I')
            if count > (len(data)-pos)//16:
                raise ValueError('Invalid point count')
            points = [struct.unpack_from('>dd', data, pos+16*i) for i in range(count)]
            pos += 16*count
            name = f"Pressure_1__{'dubl' if duplicate else ''}{series}.csv"
            path = CSV/name
            with path.open() as stream:
                exported = [(float(r['x']), float(r['y'])) for r in csv.DictReader(stream)]
            if points != exported:
                raise ValueError(f'CSV differs from binary source: {name}')
            comparisons.append({'csv': str(path), 'count': count, 'exact_numeric_match': True,
                                'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                                'duplicate_layer': duplicate, 'series': series})
    # This old file has only one final boolean, no appended axis titles.
    if len(data)-pos != 1:
        raise ValueError('Unexpected tail; do not interpret as the checked legacy layout')
    visible = take('?')
    result = {'raw': str(RAW), 'sha256': hashlib.sha256(data).hexdigest(),
              'bytes': len(data), 'consumed_bytes':pos, 'chart':title, 'numeric_x':numeric_x,
              'x_format':x_format, 'y_format':y_format, 'series':metadata,
              'points_visible':visible, 'comparisons':comparisons,
              'unit_status':'UNRESOLVED: serialized y format says deg; teaching materials say mm',
              'direction_status':'duplicate layer interpreted as forward per application documentation; physical run log not independently checked'}
    out = ROOT/'results/stage01'
    out.mkdir(parents=True, exist_ok=True)
    (out/'source_verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(result,ensure_ascii=False,indent=2))


if __name__ == '__main__':
    main()
