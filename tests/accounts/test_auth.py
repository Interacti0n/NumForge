"""Integration checks loaded by the disposable PostgreSQL harness."""
from concurrent.futures import ThreadPoolExecutor
from datetime import timedelta
import unittest
from unittest.mock import patch

from django.contrib.auth import get_user_model
from django.contrib.sessions.models import Session
from django.core.management import call_command
from django.db import connections
from django.test import Client, RequestFactory, override_settings
from django.utils import timezone

from identity.models import LoginThrottle
from identity.views import admit_login, LOGIN_LIMIT


class AuthChecks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.user = get_user_model().objects.create_user(username="auth-alice", password="pilot-password-for-tests")
        cls.other = get_user_model().objects.create_user(username="auth-bob", password="other-pilot-password")

    def setUp(self):
        Session.objects.all().delete()
        LoginThrottle.objects.all().delete()
        self.client = Client(enforce_csrf_checks=True, HTTP_HOST="localhost")

    def post(self, path="login", data=None, client=None, **headers):
        client = client or self.client
        token = client.get("/api/account/session", secure=True).json()["csrf_token"]
        return client.post("/api/account/" + path, data=data or {
            "username": self.user.username, "password": "pilot-password-for-tests"},
            content_type="application/json", secure=True, HTTP_X_CSRFTOKEN=token,
            HTTP_ORIGIN="https://localhost", **headers)

    def test_csrf_required_for_login_and_logout(self):
        for path in ("login", "logout"):
            response = self.client.post("/api/account/" + path, data="{}", content_type="application/json", secure=True)
            self.assertEqual(response.status_code, 403)
            self.assertEqual(response.json()["error"], "csrf_failed")
        token = self.client.get("/api/account/session", secure=True).json()["csrf_token"]
        response = self.client.post("/api/account/login", data="{}", content_type="application/json", secure=True,
                                    HTTP_X_CSRFTOKEN=token, HTTP_ORIGIN="https://foreign.example")
        self.assertEqual(response.status_code, 403)

    def test_login_rotation_cookie_flags_and_logout_revocation(self):
        self.assertTrue(self.user.password.startswith("argon2$argon2id$"))
        response = self.post()
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.json()["user"]["id"], self.user.pk)
        self.assertIn("no-store", response["Cache-Control"])
        cookie = response.cookies["numforge_account"]
        csrf_cookie = response.cookies["numforge_csrf"]
        self.assertTrue(csrf_cookie["secure"] and csrf_cookie["httponly"])
        self.assertEqual(csrf_cookie["samesite"], "Lax")
        self.assertTrue(cookie["secure"] and cookie["httponly"])
        self.assertEqual(cookie["samesite"], "Lax")
        self.assertFalse(cookie["expires"])
        key = cookie.value
        self.assertEqual(self.post().status_code, 200)
        self.assertNotEqual(key, self.client.cookies["numforge_account"].value)
        self.assertFalse(Session.objects.filter(session_key=key).exists())
        replay = Client(HTTP_HOST="localhost")
        replay.cookies["numforge_account"] = self.client.cookies["numforge_account"].value
        self.assertEqual(self.post("logout").status_code, 200)
        self.assertIsNone(replay.get("/api/account/session", secure=True).json()["user"])

    def test_two_clients_expiry_inactive_user_and_password_revocation(self):
        self.assertEqual(self.post().status_code, 200)
        other = Client(enforce_csrf_checks=True, HTTP_HOST="localhost")
        self.assertEqual(self.post(client=other, data={"username": self.other.username,
            "password": "other-pilot-password", "remember": True}).json()["user"]["id"], self.other.pk)
        self.assertTrue(other.cookies["numforge_account"]["expires"])
        self.assertEqual(self.client.get("/api/account/session", secure=True).json()["user"]["id"], self.user.pk)
        Session.objects.filter(session_key=self.client.cookies["numforge_account"].value).update(expire_date=timezone.now()-timedelta(seconds=1))
        self.assertIsNone(self.client.get("/api/account/session", secure=True).json()["user"])
        self.other.set_password("changed-password-for-tests")
        self.other.save()
        self.assertIsNone(other.get("/api/account/session", secure=True).json()["user"])
        self.other.is_active = False
        self.other.save()
        self.assertEqual(self.post(data={"username": self.other.username, "password": "changed-password-for-tests"}).status_code, 401)

    def test_cleanup_removes_only_expired_sessions(self):
        self.assertEqual(self.post().status_code, 200)
        active_key = self.client.cookies["numforge_account"].value
        Session.objects.create(session_key="expired-cleanup-fixture", session_data="",
            expire_date=timezone.now()-timedelta(days=1))
        call_command("clearsessions", verbosity=0)
        self.assertFalse(Session.objects.filter(session_key="expired-cleanup-fixture").exists())
        self.assertTrue(Session.objects.filter(session_key=active_key).exists())
        self.assertEqual(self.client.get("/api/account/session", secure=True).json()["user"]["id"], self.user.pk)

    def test_generic_errors_and_bounded_input(self):
        errors = [self.post(data={"username": username, "password": "wrong"}) for username in (self.user.username, "missing")]
        self.assertEqual([r.status_code for r in errors], [401, 401])
        self.assertEqual(errors[0].json(), errors[1].json())
        for data in ({"username": "a\0", "password": "x"}, {"username": "a", "password": "\ud800"},
                     {"username": "a", "password": "x", "remember": "yes"}, [1], {"username": "a"}):
            self.assertEqual(self.post(data=data).status_code, 400)
        token = self.client.get("/api/account/session", secure=True).json()["csrf_token"]
        response = self.client.post("/api/account/login", data='{"x":"'+"x"*5000+'"}', content_type="application/json",
                                    secure=True, HTTP_X_CSRFTOKEN=token, HTTP_ORIGIN="https://localhost")
        self.assertEqual(response.status_code, 413)

    def test_pilot_provisioning_creates_no_staff_privileges(self):
        from django.core.management import call_command, CommandError
        from io import StringIO
        with patch("identity.management.commands.createpilotuser.getpass", return_value="short"):
            with self.assertRaises(CommandError):
                call_command("createpilotuser", "auth-new", stdout=StringIO())
        self.assertFalse(get_user_model().objects.filter(username="auth-new").exists())
        with patch("identity.management.commands.createpilotuser.getpass", return_value="separate-strong-pilot-password"):
            call_command("createpilotuser", "auth-new", stdout=StringIO())
        user = get_user_model().objects.get(username="auth-new")
        self.assertFalse(user.is_staff or user.is_superuser)

    def test_throttling_shared_by_clients_ignores_forwarded_ip_and_expires(self):
        for _ in range(LOGIN_LIMIT):
            self.assertEqual(self.post(data={"username": "missing", "password": "wrong"}).status_code, 401)
        response = self.post(HTTP_X_FORWARDED_FOR="203.0.113.4")
        self.assertEqual(response.status_code, 429)
        self.assertGreater(int(response["Retry-After"]), 0)
        fresh = Client(enforce_csrf_checks=True, HTTP_HOST="localhost")
        self.assertEqual(self.post(client=fresh).status_code, 429)
        LoginThrottle.objects.update(expires_at=timezone.now()-timedelta(seconds=1))
        self.assertEqual(self.post().status_code, 200)

    def test_concurrent_throttle_admission_is_bounded(self):
        def attempt(_):
            try:
                return admit_login(RequestFactory().post("/", REMOTE_ADDR="192.0.2.5"))
            finally:
                connections.close_all()
        with ThreadPoolExecutor(max_workers=4) as pool:
            attempts = list(pool.map(attempt, range(LOGIN_LIMIT+4)))
        self.assertEqual(attempts.count(0), LOGIN_LIMIT)

    def test_unsafe_methods_and_http_are_rejected(self):
        self.assertEqual(self.client.get("/api/account/login", secure=True).status_code, 405)
        self.assertEqual(self.client.get("/api/account/logout", secure=True).status_code, 405)
        self.assertEqual(self.client.get("/api/account/session").status_code, 301)

    @override_settings(GATEWAY_TOKEN="x"*48, SECURE_PROXY_SSL_HEADER=("HTTP_X_FORWARDED_PROTO", "https"))
    def test_gateway_authenticates_forwarded_scheme_and_client(self):
        self.assertEqual(self.client.get("/api/account/session", HTTP_X_FORWARDED_PROTO="https").status_code, 403)
        headers = dict(HTTP_X_NUMFORGE_GATEWAY="x"*48, HTTP_X_FORWARDED_PROTO="https", HTTP_X_NUMFORGE_CLIENT_IP="192.0.2.4")
        self.assertEqual(self.client.get("/api/account/session", **headers).status_code, 200)
        headers["HTTP_X_NUMFORGE_CLIENT_IP"] = "invalid"
        self.assertEqual(self.client.get("/api/account/session", **headers).status_code, 403)
        headers["HTTP_X_NUMFORGE_GATEWAY"] = "é"
        self.assertEqual(self.client.get("/api/account/session", **headers).status_code, 403)
