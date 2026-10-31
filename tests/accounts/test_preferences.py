"""Real PostgreSQL preference ownership, validation and concurrent CAS checks."""
from concurrent.futures import ThreadPoolExecutor
import unittest

from django.contrib.auth import get_user_model
from django.db import connections
from django.test import Client

from identity.models import Preferences, preference_defaults


class PreferencesChecks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.alice = get_user_model().objects.create_user(username="preferences-alice")
        cls.bob = get_user_model().objects.create_user(username="preferences-bob")

    def setUp(self):
        Preferences.objects.all().delete()
        self.client, self.token = self.client_for(self.alice)

    def client_for(self, user=None):
        client = Client(enforce_csrf_checks=True, HTTP_HOST="localhost")
        if user:
            client.force_login(user)
        token = client.get("/api/account/session", secure=True).json()["csrf_token"]
        return client, token

    def save(self, values=None, revision=0, client=None, token=None, **changes):
        data = {"user_id": self.alice.pk, "expected_revision": revision,
                "values": values if values is not None else preference_defaults()} | changes
        return (client or self.client).post("/api/account/preferences", data=data,
            content_type="application/json", secure=True, HTTP_ORIGIN="https://localhost",
            HTTP_X_CSRFTOKEN=token or self.token)

    def test_authentication_csrf_and_non_caching(self):
        guest, token = self.client_for()
        self.assertEqual(guest.get("/api/account/preferences", secure=True).status_code, 401)
        self.assertEqual(self.save(client=guest, token=token).status_code, 401)
        self.assertEqual(self.save(token="invalid").status_code, 403)
        self.assertIn("no-store", self.client.get("/api/account/preferences", secure=True)["Cache-Control"])

    def test_defaults_read_without_write_and_owner_isolation(self):
        data = self.client.get("/api/account/preferences", secure=True).json()
        self.assertEqual(data["values"], preference_defaults())
        self.assertEqual(data["revision"], 0)
        self.assertEqual(Preferences.objects.count(), 0)
        chosen = preference_defaults() | {"precision_mode":"custom", "precision":10000, "angle":"deg"}
        saved = self.save(chosen)
        self.assertEqual(saved.status_code, 200)
        self.assertNotIn("numforge_csrf", saved.cookies)
        self.assertNotIn("numforge_account", saved.cookies)
        self.assertNotIn("numforge_csrf", self.client.get("/api/account/preferences", secure=True).cookies)
        self.assertEqual(saved.json()["revision"], 1)
        bob, _ = self.client_for(self.bob)
        self.assertEqual(bob.get("/api/account/preferences", secure=True).json()["values"], preference_defaults())
        self.assertEqual(self.save(user_id=self.bob.pk).status_code, 409)
        self.assertEqual(self.save(owner=self.bob.pk).status_code, 400)
        self.assertEqual(Preferences.objects.get(user=self.alice).values, chosen)
        self.assertFalse(Preferences.objects.filter(user=self.bob).exists())

    def test_stale_revision_preserves_saved_values(self):
        first = preference_defaults() | {"precision":0}
        self.assertEqual(self.save(first).status_code, 200)
        stale = self.save(preference_defaults() | {"precision":50})
        self.assertEqual(stale.status_code, 409)
        self.assertEqual(stale.json()["error"], "revision_conflict")
        self.assertEqual(stale.json()["values"], first)
        self.assertEqual(self.save(revision=1).json()["revision"], 2)

    def test_validation_and_body_limits(self):
        for name, value in [("precision",True),("precision",-1),("precision",10001),("precision","10"),
                            ("language","de"),("rounding","floor"),("notation",[]),("complex_form","bad")]:
            with self.subTest(name=name, value=value):
                self.assertEqual(self.save(preference_defaults() | {name:value}).status_code, 400)
        for revision in (True, -1, 2**53, "0"):
            self.assertEqual(self.save(revision=revision).status_code, 400)
        self.assertEqual(self.save({"precision":10}).status_code, 400)
        for body, content, expected in [("{", "application/json",400), ("[]", "application/json",400),
                                        ("x"*4097,"application/json",413),("{}","text/plain",415)]:
            response = self.client.post("/api/account/preferences", data=body, content_type=content,
                secure=True, HTTP_ORIGIN="https://localhost", HTTP_X_CSRFTOKEN=self.token)
            self.assertEqual(response.status_code, expected)
        self.assertFalse(Preferences.objects.exists())

    def test_simultaneous_first_save_has_one_winner(self):
        clients = [self.client_for(self.alice) for _ in range(2)]
        def attempt(index):
            client, token = clients[index]
            try:
                return self.save(preference_defaults() | {"precision":index+20}, client=client, token=token).status_code
            finally:
                connections.close_all()
        with ThreadPoolExecutor(max_workers=2) as pool:
            self.assertEqual(sorted(pool.map(attempt, range(2))), [200,409])
        row = Preferences.objects.get(user=self.alice)
        self.assertEqual(row.revision, 1)
        self.assertIn(row.values["precision"], (20,21))
