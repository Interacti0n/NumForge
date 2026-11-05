"""Bounded checks for the monitor's Debian Python helpers; no external services."""
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import subprocess
import threading
import unittest
import urllib.request

import jwt
from jwt.exceptions import PyJWKClientError

if os.environ.get('NUMFORGE_MONITOR_SYSTEM_CHECK') != '1' or not Path('/.dockerenv').is_file():
    raise RuntimeError('Run only in the disposable monitor qualification container.')


class Checks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.requests = []

        class Handler(BaseHTTPRequestHandler):
            def do_GET(self):
                cls.requests.append((self.path, self.headers.get('Authorization')))
                if self.path.startswith('/redirect/'):
                    self.send_response(int(self.path.rsplit('/', 1)[1]))
                    self.send_header('Location', f'http://localhost:{self.server.server_port}/destination')
                    self.end_headers()
                    return
                if self.path == '/error':
                    self.send_response(503)
                    self.end_headers()
                    return
                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(b'broken' if self.path == '/malformed' else json.dumps(cls.jwks).encode())

            def do_POST(self):
                body = self.rfile.read(int(self.headers.get('Content-Length', 0)))
                cls.requests.append((self.path, json.loads(body)))
                self.send_response(200)
                self.end_headers()
                self.wfile.write(b'{}')

            def log_message(self, *args):
                pass

        from cryptography.hazmat.primitives.asymmetric import rsa
        cls.private = rsa.generate_private_key(public_exponent=65537, key_size=2048)
        cls.jwks = {'keys': [dict(json.loads(jwt.algorithms.RSAAlgorithm.to_jwk(cls.private.public_key())),
                                  kid='fixture-key', use='sig', alg='RS256')]}
        cls.server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        cls.server.daemon_threads = True
        cls.server_thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.server_thread.start()
        cls.base = f'http://127.0.0.1:{cls.server.server_port}'

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.server_thread.join(timeout=5)

    def setUp(self):
        self.requests.clear()

    def test_redirects_do_not_contact_destination_or_poison_cache(self):
        for status in (301, 302, 303, 307, 308):
            with self.subTest(status=status):
                self.requests.clear()
                request = urllib.request.Request(self.base + f'/redirect/{status}',
                                                 headers={'Authorization': 'fixture-secret'})
                client = jwt.PyJWKClient(request)
                client.jwk_set_cache.put(self.jwks)
                with self.assertRaises(PyJWKClientError):
                    client.fetch_data()
                self.assertEqual(self.requests, [(f'/redirect/{status}', 'fixture-secret')])
                self.assertIsNone(client.jwk_set_cache.get())

    def test_fetch_signing_key_token_verification_and_cache(self):
        client = jwt.PyJWKClient(self.base + '/keys', cache_keys=True)
        token = jwt.encode({'sub': 'fixture'}, self.private, algorithm='RS256', headers={'kid': 'fixture-key'})
        key = client.get_signing_key_from_jwt(token)
        self.assertEqual(jwt.decode(token, key.key, algorithms=['RS256']), {'sub': 'fixture'})
        self.assertEqual(client.get_signing_key('fixture-key').key_id, 'fixture-key')
        self.assertEqual(len(self.requests), 1)
        with self.assertRaises(PyJWKClientError):
            client.get_signing_key('missing')
        self.assertEqual(len(self.requests), 2)

    def test_fetch_errors_clear_cached_material(self):
        for path, error in (('/error', PyJWKClientError), ('/malformed', json.JSONDecodeError)):
            with self.subTest(path=path):
                client = jwt.PyJWKClient(self.base + path)
                client.jwk_set_cache.put(self.jwks)
                with self.assertRaises(error):
                    client.fetch_data()
                self.assertIsNone(client.jwk_set_cache.get())

    def test_redirect_policy_does_not_change_other_urlopen_users(self):
        with urllib.request.urlopen(self.base + '/redirect/302', timeout=5) as response:
            self.assertEqual(json.load(response), self.jwks)
        self.assertEqual([p for p, _ in self.requests], ['/redirect/302', '/destination'])

    def test_oauthlib_signing_and_verification(self):
        from cryptography.hazmat.primitives import serialization
        from oauthlib.common import generate_signed_token, verify_signed_token
        from types import SimpleNamespace
        private = self.private.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8,
                                             serialization.NoEncryption())
        public = self.private.public_key().public_bytes(serialization.Encoding.PEM,
                                                        serialization.PublicFormat.SubjectPublicKeyInfo)
        token = generate_signed_token(private, SimpleNamespace(scope='read', expires_in=60,
                                                               claims={'sub': 'fixture'}))
        self.assertEqual(verify_signed_token(public, token)['sub'], 'fixture')
        with self.assertRaises(jwt.InvalidSignatureError):
            jwt.decode(jwt.encode({'sub': 'other'}, 'wrong-key', algorithm='HS256'),
                       'fixture-key', algorithms=['HS256'])

    def test_apprise_notification_through_actual_node_provider(self):
        program = '''const Provider=require('/app/server/notification-providers/apprise');
new Provider().send({appriseURL:process.argv[1],title:'Fixture title'},'Fixture body')
.then(result=>{if(result!=='Sent Successfully.')throw Error(result);})
.catch(e=>{console.error(e);process.exitCode=1;});'''
        result = subprocess.run(['node', '-e', program,
                                 f'json://127.0.0.1:{self.server.server_port}/notify'],
                                capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(len(self.requests), 1)
        path, payload = self.requests[0]
        self.assertEqual(path, '/notify')
        self.assertEqual(payload['message'], 'Fixture body')
        self.assertEqual(payload['title'], 'Fixture title')

    def test_pinned_receipts_and_loaded_module(self):
        import jwt.jwks_client
        source = Path(jwt.jwks_client.__file__)
        receipt = json.loads(Path('/opt/numforge/system-jwt-backport.json').read_text())
        self.assertEqual(hashlib.sha256(source.read_bytes()).hexdigest(), receipt['afterSha256'])
        self.assertEqual(receipt['upstream'], '0a795b8e1f6ef08f634aa7086fc41cc6d5ce3e56')
        self.assertEqual(jwt.__version__, '2.6.0')
        library = Path('/usr/lib/x86_64-linux-gnu/libsqlite3.so.0.8.6')
        self.assertEqual(hashlib.sha256(library.read_bytes()).hexdigest(),
                         'f4b32d83fadc4787cabff8a115e4aca3ac808efe9d9521b9630f1d8815f129b5')
        query = 'SELECT DISTINCT ' + ','.join(['1'] * 66) + ' ORDER BY ' + ','.join(["\u0027x\u0027"] * 64)
        result = subprocess.run(['sqlite3', ':memory:', query], capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), '|'.join(['1'] * 66))


unittest.main(verbosity=2)
