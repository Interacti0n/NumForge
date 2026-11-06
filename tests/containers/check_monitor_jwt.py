"""Security regressions for the supported PyJWT import; disposable containers only."""
import base64
import hashlib
import hmac
import json
import os
from pathlib import Path
import unittest
from unittest.mock import patch

import jwt
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import ec, rsa

if os.environ.get('NUMFORGE_MONITOR_JWT_CHECK') != '1' or not Path('/.dockerenv').is_file():
    raise RuntimeError('Run only in a disposable monitor qualification container.')


def forged(key):
    enc = lambda value: base64.urlsafe_b64encode(value).rstrip(b'=')
    message = enc(b'{"alg":"HS256","typ":"JWT"}') + b'.' + enc(b'{"sub":"forged"}')
    return (message + b'.' + enc(hmac.new(key, message, hashlib.sha256).digest())).decode()


class SecurityChecks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rsa = rsa.generate_private_key(public_exponent=65537, key_size=2048)
        cls.ec = ec.generate_private_key(ec.SECP256R1())

    def guard_mutated_pem(self, private, algorithm):
        public = private.public_key().public_bytes(serialization.Encoding.PEM, serialization.PublicFormat.SubjectPublicKeyInfo)
        candidates = [public.replace(b'-----BEGIN PUBLIC KEY-----', b'-----BEGIN PUBLIC KEY----- '),
                      public.replace(b'\n', b'\r'), public.replace(b'\n', b' '),
                      b'  ' + public.replace(b'\n', b'\n  ')]
        accepted = 0
        for pem in candidates:
            try:
                serialization.load_pem_public_key(pem)
            except ValueError:
                continue
            accepted += 1
            with self.subTest(pem_sha256=hashlib.sha256(pem).hexdigest()):
                with self.assertRaises(jwt.InvalidKeyError):
                    jwt.decode(forged(pem), pem, algorithms=[algorithm, 'HS256'])
                with self.assertRaises(jwt.InvalidAlgorithmError):
                    jwt.decode(forged(pem), pem, algorithms=[algorithm])
        self.assertGreater(accepted, 0, 'Fixture must exercise loader-accepted PEM mutations')

    def test_rsa_pem_confusion_is_rejected(self):
        self.guard_mutated_pem(self.rsa, 'RS256')

    def test_ec_pem_confusion_is_rejected(self):
        self.guard_mutated_pem(self.ec, 'ES256')

    def test_der_public_key_confusion_is_rejected(self):
        for private, algorithm in ((self.rsa, 'RS256'), (self.ec, 'ES256')):
            public = private.public_key().public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo)
            with self.subTest(algorithm=algorithm), self.assertRaises(jwt.InvalidKeyError):
                jwt.decode(forged(public), public, algorithms=[algorithm, 'HS256'])

    def test_untrusted_non_http_jwks_schemes_are_rejected_without_io(self):
        with patch('urllib.request.build_opener') as opener:
            for url in ('file:///etc/passwd', 'data:application/json,{}', 'ftp://localhost/keys'):
                with self.subTest(url=url), self.assertRaises(jwt.PyJWKClientError):
                    jwt.PyJWKClient(url)
            opener.assert_not_called()

    def test_rsa_ec_and_hmac_roundtrips_with_explicit_algorithms(self):
        for private, algorithm in ((self.rsa, 'RS256'), (self.ec, 'ES256')):
            token = jwt.encode({'sub': 'fixture'}, private, algorithm=algorithm)
            self.assertEqual(jwt.decode(token, private.public_key(), algorithms=[algorithm]), {'sub': 'fixture'})
        secret = b'fixture-only-hmac-key-' * 4
        token = jwt.encode({'sub': 'fixture'}, secret, algorithm='HS256')
        self.assertEqual(jwt.decode(token, secret, algorithms=['HS256']), {'sub': 'fixture'})

    def test_oauthlib_service_application_assertion(self):
        from oauthlib.oauth2 import ServiceApplicationClient
        from urllib.parse import parse_qs
        private = self.rsa.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption())
        client = ServiceApplicationClient('fixture-client')
        body = client.prepare_request_body(private_key=private, issuer='fixture-client', subject='fixture-user', audience='https://fixture.invalid/token')
        token = parse_qs(body)['assertion'][0]
        claims = jwt.decode(token, self.rsa.public_key(), algorithms=['RS256'], audience='https://fixture.invalid/token')
        self.assertEqual(claims['sub'], 'fixture-user')
        self.assertEqual(claims['iss'], 'fixture-client')


unittest.main(verbosity=2)
