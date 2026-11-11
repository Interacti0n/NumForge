"""Rehearse additive workspace migration and app rollback using a restored daily dump."""
import argparse
import json
import os
from pathlib import Path
import secrets
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
import compose
import ops_backup as backup
from check_recovery import profile_for_kit

SNAPSHOT = '''import hashlib,json,sys
sys.path.insert(0,'/support');import accounts_db as db
from psycopg import sql
with db.connect(db.read_config('/run/secrets/migrate')) as c:
 tables=[r[0] for r in c.execute("SELECT tablename FROM pg_tables WHERE schemaname='public' ORDER BY tablename")]
 result={'tables':{},'permissions':{}}
 for table in tables:
  rows=c.execute(sql.SQL('SELECT row_to_json(t)::text FROM {} t ORDER BY row_to_json(t)::text').format(sql.Identifier(table))).fetchall()
  result['tables'][table]={'rows':len(rows),'sha256':hashlib.sha256(json.dumps(rows).encode()).hexdigest()}
 result['migrations']=c.execute('SELECT app,name FROM django_migrations ORDER BY app,name').fetchall()
 result['roles']=c.execute("SELECT rolname,rolsuper,rolcreatedb,rolcreaterole,rolreplication,rolbypassrls FROM pg_roles WHERE rolname LIKE 'nf_%' ORDER BY rolname").fetchall()
 result['permissions']=dict(c.execute("SELECT c.relname,json_build_array(c.relowner::regrole::text,c.relacl::text)::text FROM pg_class c JOIN pg_namespace n ON n.oid=c.relnamespace WHERE n.nspname='public' AND c.relkind IN ('r','S') ORDER BY c.relname").fetchall())
 result['metadata']={'types':c.execute('SELECT id,app_label,model FROM django_content_type ORDER BY id').fetchall(),
                     'permissions':c.execute('SELECT id,name,content_type_id,codename FROM auth_permission ORDER BY id').fetchall()}
 print(json.dumps(result))
'''

API = '''import django,json
django.setup()
from django.conf import settings
from django.contrib.auth import get_user_model
from django.test import Client
from identity.models import preference_defaults
username=USERNAME
phase=PHASE
password='workspace-rollout-fixture-only'
client=Client(enforce_csrf_checks=True,HTTP_HOST='localhost',HTTP_X_NUMFORGE_GATEWAY=settings.GATEWAY_TOKEN or '',HTTP_X_FORWARDED_PROTO='https',HTTP_X_NUMFORGE_CLIENT_IP='192.0.2.201')
def post(path,data):
 token=client.get('/api/account/session',secure=True).json()['csrf_token']
 r=client.post('/api/account/'+path,data=data,content_type='application/json',secure=True,HTTP_ORIGIN='https://localhost',HTTP_X_CSRFTOKEN=token)
 assert r.status_code in (200,201),(phase,path,r.status_code)
 return r.json()
if phase=='baseline':get_user_model().objects.create_user(username=username,password=password)
post('login',{'username':username,'password':password})
user=client.get('/api/account/session',secure=True).json()['user']['id']
pref=client.get('/api/account/preferences',secure=True).json()
expected={'baseline':0,'save':26,'rollback':27,'verify':28}[phase]
if phase!='baseline':assert pref['values']['precision']==expected
if phase!='verify':
 values=preference_defaults()|{'precision_mode':'custom','precision':{'baseline':26,'save':27,'rollback':28}[phase]}
 post('preferences',{'user_id':user,'expected_revision':pref['revision'],'values':values})
data=[{'version':1,'tool':'calculator','inputs':{'expression':'1/3 + ','settings':preference_defaults()}},
      {'version':1,'tool':'matrices','inputs':{'operation':'determinant','a':[['1/3',''],['0','123456789012345678901234567890']],'b':[['0']]}}]
if phase=='save':
 for i,value in enumerate(data):post('workspaces',{'user_id':user,'name':'Obnova – skúška '+str(i),'data':value})
if phase in ('save','verify'):
 listing=client.get('/api/account/workspaces',secure=True)
 assert listing.status_code==200
 items=listing.json()['items'];assert len(items)==2
 actual=[]
 for item in items:
  response=client.get('/api/account/workspaces/'+str(item['id']),secure=True)
  assert response.status_code==200
  actual.append(response.json()['data'])
 assert sorted(actual,key=lambda x:x['tool'])==sorted(data,key=lambda x:x['tool'])
if phase=='rollback':assert client.get('/api/account/workspaces',secure=True).status_code==404
print(json.dumps({'phase':phase,'ok':True}))
'''


def migration_preserved(before, after):
    if set(after['tables']) != set(before['tables']) | {'identity_workspace'}:
        raise ValueError('Workspace migration changed an unexpected table set')
    for name, value in before['tables'].items():
        if name not in ('django_migrations', 'django_content_type', 'auth_permission') and after['tables'][name] != value:
            raise ValueError('Migration changed original rows in ' + name)
    old_types = {row[0]: row for row in before['metadata']['types']}
    new_types = {row[0]: row for row in after['metadata']['types']}
    old_permissions = {row[0]: row for row in before['metadata']['permissions']}
    new_permissions = {row[0]: row for row in after['metadata']['permissions']}
    if any(new_types.get(key) != value for key, value in old_types.items()) \
            or any(new_permissions.get(key) != value for key, value in old_permissions.items()):
        raise ValueError('Migration changed original Django metadata rows')
    added_types = [row for key, row in new_types.items() if key not in old_types]
    if len(added_types) != 1 or added_types[0][1:] != ['identity', 'workspace']:
        raise ValueError('Unexpected Django content-type additions')
    workspace_type = added_types[0][0]
    added_permissions = [row[1:] for key, row in new_permissions.items() if key not in old_permissions]
    expected_permissions = [['Can ' + action + ' workspace', workspace_type, action + '_workspace']
                            for action in ('add', 'change', 'delete', 'view')]
    if sorted(added_permissions) != sorted(expected_permissions):
        raise ValueError('Unexpected Django permission additions')
    for name, expected_rows in (('django_content_type', 1), ('auth_permission', 4), ('django_migrations', 1)):
        if after['tables'][name]['rows'] != before['tables'][name]['rows'] + expected_rows:
            raise ValueError('Unexpected metadata row count in ' + name)
    if before['roles'] != after['roles'] or any(after['permissions'].get(n) != v for n, v in before['permissions'].items()):
        raise ValueError('Migration changed original roles or grants')
    if sorted(after['migrations']) != sorted(before['migrations'] + [['identity', '0004_workspace']]):
        raise ValueError('Unexpected migration ledger changes')


def run(candidate, output):
    backup.load_environment()
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    compose.private_directory(output)
    candidate = json.loads(backup.execute(['docker', 'image', 'inspect', candidate]).stdout)[0]['Id']
    environment = {k: v for k, v in os.environ.items() if not k.startswith('COMPOSE_')}
    with backup.lock(), tempfile.TemporaryDirectory(prefix='workspace-rollout-', dir=backup.STATE) as temporary:
        temporary = Path(temporary)
        status = json.loads((backup.STATE / 'backup-status.json').read_text())
        if not status['ok'] or not status.get('restore_verified'):
            raise ValueError('A verified daily backup is required')
        backup.restic(backup.config(), 'restore', status['snapshot'], '--target', temporary)
        dumps = list(temporary.rglob('accounts.dump'))
        if len(dumps) != 1:
            raise ValueError('Unexpected restored daily layout')
        kit = temporary / 'kit'
        shutil.copytree(dumps[0].parent, kit / 'daily')
        original = json.loads((kit / 'daily/pilot-runtime.json').read_text())
        original['services'] = {n: original['services'][n] for n in ('db', 'setup', 'accounts')}
        original['volumes'] = {'accounts-data': original['volumes']['accounts-data']}
        (kit / 'daily/pilot-runtime.json').write_text(json.dumps(original))
        images = {n: json.loads(backup.execute(['docker', 'image', 'inspect', original['services'][n]['image']]).stdout)[0]['Id'] for n in ('db', 'accounts')}
        project = 'numforge-test-recovery-workspace-' + secrets.token_hex(6)
        profile = profile_for_kit(kit, {'images': images, 'bindings': {}}, project)
        config = output / 'compose.json'
        def select(image):
            for name in ('setup', 'accounts'):
                profile['services'][name]['image'] = image
            config.write_text(json.dumps(profile, indent=2))
        select(images['accounts'])
        command = ['docker', 'compose', '--project-name', project, '--file', str(config)]
        def call(*args, **kwargs):
            try:
                return backup.execute([*command, *args], env=environment, **kwargs).stdout
            except subprocess.CalledProcessError as error:
                print(error.stderr.decode('utf-8', errors='replace'), file=sys.stderr)
                raise
        def wait(service):
            for _ in range(120):
                cid = call('ps', '-q', service).decode().strip()
                if cid:
                    state = json.loads(backup.execute(['docker', 'inspect', cid]).stdout)[0]['State']
                    if state.get('Health', {}).get('Status') == 'healthy':
                        return
                    if not state['Running']:
                        raise RuntimeError('Fixture service stopped: ' + service)
                time.sleep(1)
            raise TimeoutError('Fixture service not healthy: ' + service)
        def snapshot():
            return json.loads(call('run', '--rm', '--no-deps', '-T', '--entrypoint', 'python', 'setup', '-c', SNAPSHOT))
        username = 'rollout-' + secrets.token_hex(8)
        def api(phase):
            program = API.replace('USERNAME', repr(username)).replace('PHASE', repr(phase))
            json.loads(call('exec', '-T', 'accounts', 'python', '-', input=program.encode()))
        result = {'ok': False, 'project': project, 'daily_snapshot': status['snapshot'], 'images': images | {'candidate': candidate}, 'resources_absent': False}
        try:
            call('up', '--detach', '--no-build', '--pull', 'never', 'db')
            wait('db')
            call('run', '--rm', '--no-deps', '-T', '--entrypoint', 'python', 'setup', '-c',
                 "import sys;sys.path.insert(0,'/support');import accounts_db as d;d.provision({n:d.read_config('/run/secrets/'+n) for n in ('admin','migrate','app')})")
            call('exec', '-T', 'db', 'psql', '-U', 'nf_local_admin', '-d', 'postgres', '-v', 'ON_ERROR_STOP=1', '-c', 'CREATE ROLE nf_inspect NOLOGIN;')
            with (kit / 'daily/accounts.dump').open('rb') as archive:
                call('exec', '-T', 'db', 'pg_restore', '-U', 'nf_local_admin', '-d', 'numforge_accounts', '--single-transaction', '--exit-on-error', stdin=archive)
            call('up', '--detach', '--no-build', '--pull', 'never', 'accounts')
            wait('accounts')
            api('baseline')
            before = snapshot()
            if len(before['tables']) != 11:
                raise ValueError('Expected the unmigrated eleven-table pilot')
            select(candidate)
            call('run', '--rm', '--no-deps', '-T', 'setup')
            migrated = snapshot()
            migration_preserved(before, migrated)
            call('up', '--detach', '--no-build', '--no-deps', '--force-recreate', 'accounts')
            wait('accounts')
            api('save')
            written = snapshot()
            select(images['accounts'])
            call('up', '--detach', '--no-build', '--no-deps', '--force-recreate', 'accounts')
            wait('accounts')
            api('rollback')
            rolled = snapshot()
            assert rolled['tables']['identity_workspace'] == written['tables']['identity_workspace']
            assert rolled['migrations'] == written['migrations']
            archive = temporary / 'migrated.dump'
            with archive.open('xb') as stream:
                call('exec', '-T', 'db', 'pg_dump', '-U', 'nf_local_admin', '-d', 'numforge_accounts', '-Fc', stdout=stream)
            restored_name = 'nf_restore_workspace_' + secrets.token_hex(6)
            call('exec', '-T', 'db', 'createdb', '-U', 'nf_local_admin', '-O', 'nf_migrate', restored_name)
            with archive.open('rb') as stream:
                call('exec', '-T', 'db', 'pg_restore', '-U', 'nf_local_admin', '-d', restored_name, '--single-transaction', '--exit-on-error', stdin=stream)
            call('run', '--rm', '--no-deps', '-T', 'setup', 'python', '/support/accounts_setup.py', 'verify', restored_name)
            select(candidate)
            call('run', '--rm', '--no-deps', '-T', 'setup')
            call('up', '--detach', '--no-build', '--no-deps', '--force-recreate', 'accounts')
            wait('accounts')
            api('verify')
            final = snapshot()
            assert final['tables']['identity_workspace'] == written['tables']['identity_workspace']
            assert final['migrations'] == written['migrations']
            call('exec', '-T', '-e', 'NUMFORGE_SQLITE_BACKPORT_CHECK=1', 'accounts', 'python', '-', input=(ROOT / 'tests/containers/check_accounts_sqlite.py').read_bytes())
            result.update(ok=True, original_tables=len(before['tables']), migrated_tables=len(final['tables']), original_rows_roles_grants_preserved=True,
                          old_application_read_write_after_migration=True, candidate_workspaces_preserved_on_rollback=True,
                          logical_restore_all_tables_rows=True, reapply_retains_old_app_writes=True, live_pilot_untouched=True)
        finally:
            call('down', '--volumes', '--remove-orphans', '--timeout', '10')
            for args in (['ps', '-a', '-q'], ['volume', 'ls', '-q'], ['network', 'ls', '-q']):
                assert not backup.execute(['docker', *args, '--filter', 'label=com.docker.compose.project=' + project]).stdout.strip()
            result['resources_absent'] = True
            (output / 'result.json').write_text(json.dumps(result, indent=2))
        print('Workspace migration, old-application rollback, new writes and twelve-table restore verified; fixture removed.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--candidate', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    run(args.candidate, args.output)
