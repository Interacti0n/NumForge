"""Real PostgreSQL ownership, schema limits, quota serialization and stale writes."""
from concurrent.futures import ThreadPoolExecutor
import unittest
from unittest.mock import patch

from django.contrib.auth import get_user_model
from django.db import connections
from django.test import Client

from identity.models import Workspace, preference_defaults


def task(tool="calculator"):
    return {"version": 1, "tool": tool, "inputs": {
        "calculator": {"expression": "123456789012345678901234567890+1", "settings": preference_defaults()},
        "solve": {"mode": "numeric", "inputs": ["1", "0", "-2"], "expression": "cos(x)=x", "bounds": ["0", "1"], "precision": "30"},
        "graph": {"view": {"x": ["-10", "10"], "y": ["-10", "10"]}, "curves": [{"expression": "sin(x)", "color": "#bbaa00", "visible": True}]},
        "matrices": {"operation": "determinant", "a": [["1/3", ""], ["0", "12345678901234567890"]], "b": [["0"]]},
    }[tool]}


class WorkspaceChecks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.alice = get_user_model().objects.create_user(username="workspace-alice")
        cls.bob = get_user_model().objects.create_user(username="workspace-bob")

    def client_for(self, user=None):
        client = Client(enforce_csrf_checks=True, HTTP_HOST="localhost")
        if user:
            client.force_login(user)
        token = client.get("/api/account/session", secure=True).json()["csrf_token"]
        return client, token

    def setUp(self):
        Workspace.objects.all().delete()
        self.client, self.token = self.client_for(self.alice)

    def post(self, data=None, row=None, client=None, token=None):
        return (client or self.client).post("/api/account/workspaces" + ("/"+row if row else ""),
            data=data if data is not None else {"user_id": self.alice.pk, "name": "My task", "data": task()},
            content_type="application/json", secure=True, HTTP_ORIGIN="https://localhost",
            HTTP_X_CSRFTOKEN=token or self.token)

    def change(self, row, action, revision=1, **extra):
        return self.post({"user_id": self.alice.pk, "action": action, "expected_revision": revision, **extra}, row)

    def test_auth_csrf_cache_and_cookies(self):
        guest, token = self.client_for()
        self.assertEqual(guest.get("/api/account/workspaces", secure=True).status_code, 401)
        self.assertEqual(self.post(client=guest, token=token).status_code, 401)
        self.assertEqual(self.post(token="bad").status_code, 403)
        for response in (self.client.get("/api/account/workspaces", secure=True), self.post()):
            self.assertIn("no-store", response["Cache-Control"])
            self.assertNotIn("numforge_csrf", response.cookies)
            self.assertNotIn("numforge_account", response.cookies)

    def test_four_tools_roundtrip_and_crud(self):
        for tool in ("calculator", "graph", "solve", "matrices"):
            original = task(tool)
            saved = self.post({"user_id": self.alice.pk, "name": "  <b>Task</b>  ", "data": original})
            self.assertEqual(saved.status_code, 201)
            row = saved.json()["id"]
            self.assertEqual(saved.json()["name"], "<b>Task</b>")
            loaded = self.client.get("/api/account/workspaces/"+row, secure=True).json()
            self.assertEqual(loaded["data"], original)
            renamed = self.change(row, "rename", name="New name")
            self.assertEqual(renamed.json()["revision"], 2)
            copied = self.change(row, "duplicate", 2, name="Copy")
            self.assertEqual(copied.status_code, 201)
            self.assertNotEqual(copied.json()["id"], row)
            self.assertEqual(copied.json()["data"], original)
            self.assertEqual(self.change(row, "update", 2, data=original).json()["revision"], 3)
            self.assertEqual(self.change(row, "delete", 3).status_code, 200)
            self.assertEqual(self.client.get("/api/account/workspaces/"+row, secure=True).status_code, 404)
        listing = self.client.get("/api/account/workspaces", secure=True).json()
        self.assertEqual(len(listing["items"]), 4)
        self.assertTrue(all("data" not in item for item in listing["items"]))

    def test_all_operations_are_owner_scoped(self):
        row = self.post().json()["id"]
        bob, token = self.client_for(self.bob)
        self.assertEqual(bob.get("/api/account/workspaces", secure=True).json()["items"], [])
        self.assertEqual(bob.get("/api/account/workspaces/"+row, secure=True).status_code, 404)
        for action, extra in (("update", {"data":task()}), ("rename", {"name":"stolen"}),
                              ("duplicate", {"name":"stolen"}), ("delete", {})):
            response = self.post({"user_id": self.bob.pk, "action": action, "expected_revision":1, **extra}, row, bob, token)
            self.assertEqual(response.status_code, 404)
        self.assertEqual(self.post({"user_id":self.bob.pk,"name":"wrong owner","data":task()}).status_code, 409)
        self.assertEqual(Workspace.objects.get(pk=row).revision, 1)

    def test_stale_mutations_and_tool_changes_preserve_data(self):
        row = self.post().json()["id"]
        self.assertEqual(self.change(row, "rename", name="new").status_code, 200)
        for action, extra in (("update", {"data":task()}),("rename", {"name":"old"}),("duplicate", {"name":"old"}),("delete", {})):
            self.assertEqual(self.change(row, action, **extra).status_code, 409)
        self.assertEqual(self.change(row, "update", 2, data=task("graph")).status_code, 400)
        self.assertEqual(Workspace.objects.get(pk=row).name, "new")

    def test_schema_names_and_body_limits(self):
        bad = [None, [], {}, task() | {"result":"must not persist"}, task() | {"version":True}, task() | {"tool":"unknown"}]
        for path, value in [(("calculator","expression"), 123),(("calculator","expression"),"x"*4097),
                            (("calculator","expression"),"\ud800"),(("calculator","expression"),"a\nb"),
                            (("graph","view"),{"x":["nan","1"],"y":["0","1"]}),
                            (("graph","view"),{"x":["0","1_0"],"y":["0","1"]}),
                            (("graph","curves"),[]),(("matrices","a"),[[1]]),(("matrices","b"),[["x"*65]]),
                            (("solve","precision"),"1000"),(("solve","bounds"),["0"] )]:
            data=task(path[0]);data["inputs"][path[1]]=value;bad.append(data)
        for data in bad:
            with self.subTest(data=repr(data)[:80]):
                self.assertEqual(self.post({"user_id":self.alice.pk,"name":"test","data":data}).status_code, 400)
        for name in ("", "  ", "x"*81, "a\nb", 1, "\ud800"):
            self.assertEqual(self.post({"user_id":self.alice.pk,"name":name,"data":task()}).status_code, 400)
        row=self.post().json()["id"]
        self.assertEqual(self.post({"user_id":self.alice.pk,"action":"create","name":"bad","data":task()},row).status_code,400)
        for revision in (True, 0, -1, 2**53, "1"):
            self.assertEqual(self.change(row,"delete",revision).status_code,400)
        for body, content, status in [("x"*128001,"application/json",413),("[]","application/json",400),("{","application/json",400),("{}","text/plain",415)]:
            response=self.client.post("/api/account/workspaces",data=body,content_type=content,secure=True,
                HTTP_ORIGIN="https://localhost",HTTP_X_CSRFTOKEN=self.token)
            self.assertEqual(response.status_code,status)

    def test_quota_create_duplicate_update_and_owner_cascade(self):
        with patch("identity.workspaces.MAX_ITEMS",1):
            row=self.post().json()["id"]
            self.assertEqual(self.post().json()["error"],"quota_exceeded")
            self.assertEqual(self.change(row,"duplicate",name="copy").json()["error"],"quota_exceeded")
            self.assertEqual(self.change(row,"update",data=task()).status_code,200)
        with patch("identity.workspaces.MAX_BYTES",10):
            self.assertEqual(self.post().status_code,413)
        with patch("identity.workspaces.MAX_TOTAL_BYTES",Workspace.objects.get(pk=row).size):
            self.assertEqual(self.post().json()["error"],"quota_exceeded")
            self.assertEqual(self.change(row,"rename",2,name="renamed").status_code,200)
        temporary=get_user_model().objects.create_user(username="workspace-temporary")
        Workspace.objects.create(user=temporary,name="temp",data=task(),size=1)
        temporary.delete()
        self.assertFalse(Workspace.objects.filter(name="temp").exists())

    def test_concurrent_updates_and_final_quota_slot(self):
        clients=[self.client_for(self.alice) for _ in range(2)]
        def attempt(index, row=None):
            client,token=clients[index]
            data={"user_id":self.alice.pk,"name":"new","data":task()} if row is None else {
                "user_id":self.alice.pk,"action":"rename","expected_revision":1,"name":str(index)}
            try:
                return self.post(data,row,client,token).status_code
            finally:
                connections.close_all()
        with patch("identity.workspaces.MAX_ITEMS",1), ThreadPoolExecutor(max_workers=2) as pool:
            self.assertEqual(sorted(pool.map(attempt,range(2))),[201,409])
        row=str(Workspace.objects.get().pk)
        with ThreadPoolExecutor(max_workers=2) as pool:
            self.assertEqual(sorted(pool.map(lambda i:attempt(i,row),range(2))),[200,409])
        self.assertEqual(Workspace.objects.get().revision,2)
