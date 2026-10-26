"""Export an offline Aurora pipeline-cache snapshot for the Android APK.

Copy pipeline_cache.db and its WAL only after the game process has exited.
The exported seed contains portable pipeline descriptions, never Dawn binaries.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sqlite3

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=Path, required=True)
    p.add_argument('--destination', type=Path, required=True)
    a = p.parse_args()
    if a.source.resolve() == a.destination.resolve():
        p.error('source and destination must differ')
    source = sqlite3.connect(a.source.resolve().as_uri()+'?mode=ro', uri=True)
    if source.execute('PRAGMA integrity_check').fetchone()[0] != 'ok':
        raise RuntimeError('Invalid source SQLite database')
    if source.execute('SELECT value FROM aurora_schema').fetchall() != [(1,)]:
        raise RuntimeError('Unsupported Aurora pipeline schema')
    names = {r[0] for r in source.execute("SELECT name FROM sqlite_master WHERE type='table'")}
    if names != {'aurora_schema', 'pipeline_cache'}:
        raise RuntimeError('Unexpected tables; this is not a portable pipeline cache')
    if source.execute('SELECT count(*) FROM pipeline_cache WHERE config_size != length(config)').fetchone()[0]:
        raise RuntimeError('Invalid pipeline configuration sizes')
    a.destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = a.destination.with_suffix('.tmp')
    if temporary.exists():
        raise RuntimeError('Temporary export already exists: '+str(temporary))
    destination = sqlite3.connect(temporary)
    source.backup(destination)
    destination.execute('PRAGMA journal_mode=DELETE')
    destination.execute('VACUUM')
    groups = destination.execute('SELECT type,config_version,count(*) FROM pipeline_cache GROUP BY type,config_version').fetchall()
    destination.close(); source.close()
    temporary.replace(a.destination)
    manifest = dict(source='Android ARM64 Aurora pipeline descriptions; no game assets or driver binaries',
                    coverage='Observed boot, PartyBoard menus and Bianco Hills 2,0 only; other scenes compile when first seen',
                    schema=1, groups=groups,
                    sha256=hashlib.sha256(a.destination.read_bytes()).hexdigest())
    a.destination.with_suffix('.json').write_text(json.dumps(manifest, indent=2)+'\n')
    print(json.dumps(manifest, indent=2))

if __name__ == '__main__':
    main()
