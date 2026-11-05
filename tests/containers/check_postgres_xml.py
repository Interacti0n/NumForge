"""Disposable XML, logical restore and locale comparison; never mounts live data."""
import argparse
import json
from pathlib import Path
import secrets
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
LABEL = 'numforge.postgres-xml-fixture'


def docker(*args, **kwargs):
    result = subprocess.run(['docker', *args], capture_output=True, **kwargs)
    if result.returncode:
        raise RuntimeError('Disposable Docker operation failed: ' + result.stderr.decode('utf-8', errors='replace'))
    return result.stdout


def run(baseline, candidate, output):
    project = 'numforge-test-pgxml-' + secrets.token_hex(6)
    output.mkdir(parents=True, exist_ok=True)
    containers, volumes = [], []
    result = {'project': project, 'ok': False, 'images': {}, 'resources_absent': False}

    def sql(container, statement, database='fixture'):
        return docker('exec', '-i', container, 'psql', '-X', '-U', 'postgres', '-d', database,
                      '-Atq', '-v', 'ON_ERROR_STOP=1', input=statement.encode('utf-8')).decode('utf-8')

    def start(label, image):
        identity = json.loads(docker('image', 'inspect', image))[0]['Id']
        result['images'][label] = identity
        name = project + '-' + label
        volume = name + '-data'
        existing = subprocess.run(['docker', 'volume', 'inspect', volume], capture_output=True)
        if existing.returncode == 0:
            raise RuntimeError('Refusing to reuse an existing fixture volume: ' + volume)
        docker('volume', 'create', '--label', LABEL + '=' + project, volume)
        volumes.append(volume)
        docker('run', '--detach', '--name', name, '--label', LABEL + '=' + project,
               '--network', 'none', '--memory', '512m', '--cpus', '2', '--pids-limit', '128',
               '--security-opt', 'no-new-privileges:true', '--shm-size', '64m',
               '--mount', 'type=volume,source=' + volume + ',target=/var/lib/postgresql',
               '-e', 'POSTGRES_PASSWORD=disposable-fixture-only',
               '-e', 'POSTGRES_INITDB_ARGS=--locale=en_US.utf8 --encoding=UTF8', identity)
        containers.append(name)
        for _ in range(90):
            ready = subprocess.run(['docker', 'exec', name, 'pg_isready', '-h', '127.0.0.1',
                                    '-U', 'postgres'], capture_output=True)
            if ready.returncode == 0:
                return name
            state = json.loads(docker('inspect', name))[0]['State']
            if not state['Running']:
                raise RuntimeError('Fixture failed to start: ' + name)
            time.sleep(1)
        raise TimeoutError('Fixture database not ready: ' + name)

    def create(container, database):
        docker('exec', container, 'createdb', '-U', 'postgres', '--template=template0',
               '--locale=en_US.utf8', '--encoding=UTF8', database)

    def snapshot(container, database='fixture'):
        return json.loads(sql(container, """
SELECT json_build_object(
  'rows', (SELECT json_agg(row_to_json(t) ORDER BY id) FROM xml_fixture t),
  'ordering', (SELECT json_agg(value ORDER BY value) FROM xml_fixture),
  'entity_text', (xpath('string(/r)', XMLPARSE(DOCUMENT
      '<!DOCTYPE r [<!ENTITY x "small">]><r>&x;</r>')))[1]::text,
  'classification', (SELECT json_agg(json_build_array(value,lower(value),upper(value),
        value ~ '^[[:alpha:]]+$') ORDER BY id) FROM xml_fixture));
""", database))

    def dump(container, database='fixture'):
        return docker('exec', container, 'pg_dump', '-U', 'postgres', '-d', database,
                      '-Fc', '--no-owner', '--no-privileges')

    def restore(container, database, archive):
        docker('exec', '-i', container, 'pg_restore', '-U', 'postgres', '-d', database,
               '--exit-on-error', '--no-owner', '--no-privileges', input=archive)

    try:
        old = start('baseline', baseline)
        new = start('candidate', candidate)
        for container in (old, new):
            create(container, 'fixture')
            sql(container, (ROOT / 'tests/containers/check_postgres_xml.sql').read_text(encoding='utf-8'))
        sql(old, """
CREATE TABLE xml_fixture(id integer GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
                         value text UNIQUE NOT NULL, document xml NOT NULL);
INSERT INTO xml_fixture(value,document)
SELECT v, xmlelement(NAME r,v) FROM unnest(ARRAY[
 'a','A','á','ä','b','B','ch','Č','z','Z','ž','Ž','_','-','1','01','straße','İ','ı','é','é','😀'
]) AS v;
""")
        original = snapshot(old)
        archive = dump(old)
        restore(new, 'fixture', archive)
        migrated = snapshot(new)
        assert original['rows'] == migrated['rows'], 'Logical restore changed fixture rows'
        assert original['entity_text'] == migrated['entity_text'], 'Entity handling changed'
        sql(new, (ROOT / 'tests/containers/check_postgres_xml.sql').read_text(encoding='utf-8'))
        sql(new, "INSERT INTO xml_fixture(value,document) VALUES('candidate-canary','<r>new write</r>');")
        written = snapshot(new)
        create(old, 'fixture_rollback')
        restore(old, 'fixture_rollback', dump(new))
        assert snapshot(old, 'fixture_rollback')['rows'] == written['rows'], 'Logical rollback lost candidate writes'
        assert snapshot(old) == original, 'Original fixture database changed'
        metadata = """SELECT json_build_object('provider',datlocprovider,'collate',datcollate,
          'ctype',datctype,'version',datcollversion) FROM pg_database WHERE datname='fixture';"""
        result.update(ok=True, xml_checks=True, logical_restore=True, rollback_includes_new_writes=True,
                      original_database_unchanged=True,
                      ordering_preserved=original['ordering'] == migrated['ordering'],
                      classification_preserved=original['classification'] == migrated['classification'],
                      baseline_locale=json.loads(sql(old, metadata)), candidate_locale=json.loads(sql(new, metadata)),
                      baseline_behavior=original, candidate_behavior=migrated)
        result['locale_metadata_preserved'] = result['baseline_locale'] == result['candidate_locale']
        result['promotion_held'] = not (result['ordering_preserved'] and result['classification_preserved']
                                        and result['locale_metadata_preserved'])
        print('XML checks and logical restore/rollback passed; ordering preserved:',
              result['ordering_preserved'], '; classification preserved:', result['classification_preserved'], flush=True)
    finally:
        for name in reversed(containers):
            info = json.loads(docker('inspect', name))[0]
            assert name.startswith(project + '-') and info['Config']['Labels'][LABEL] == project
            logs = subprocess.run(['docker', 'logs', name], check=True, capture_output=True)
            (output / (name + '.log')).write_bytes(logs.stdout + logs.stderr)
            docker('rm', '--force', name)
        for volume in reversed(volumes):
            info = json.loads(docker('volume', 'inspect', volume))[0]
            assert volume.startswith(project + '-') and info['Labels'][LABEL] == project
            docker('volume', 'rm', volume)
        assert not docker('ps', '-a', '-q', '--filter', 'label=' + LABEL + '=' + project).strip()
        assert not docker('volume', 'ls', '-q', '--filter', 'label=' + LABEL + '=' + project).strip()
        result['resources_absent'] = True
        (output / 'xml-result.json').write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding='utf-8')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', required=True)
    parser.add_argument('--candidate', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--require-compatible', action='store_true',
                        help='Exit 2 if locale metadata, text ordering or character classification changes.')
    args = parser.parse_args()
    result = run(args.baseline, args.candidate, args.output)
    if args.require_compatible and result['promotion_held']:
        sys.exit(2)
