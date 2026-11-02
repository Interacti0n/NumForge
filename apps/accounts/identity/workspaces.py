"""Versioned input-only workspaces. The owner's lock serializes quotas and revisions."""
import json
import math
import re

from django.db import transaction
from django.http import JsonResponse
from django.views.decorators.cache import never_cache
from django.views.decorators.http import require_http_methods

from .json_api import read_object
from .models import User, Workspace
from .preferences import valid_values

MAX_ITEMS, MAX_BYTES, MAX_TOTAL_BYTES = 100, 100000, 1000000


def text(value, limit):
    try:
        return (isinstance(value, str) and len(value.encode("utf-8")) <= limit
                and not re.search(r"[\x00-\x1f\x7f]", value))
    except UnicodeError:
        return False


def keys(value, expected):
    return isinstance(value, dict) and value.keys() == set(expected.split())


def valid_data(data):
    if not keys(data, "version tool inputs") or type(data["version"]) is not int or data["version"] != 1:
        return False
    value = data["inputs"]
    if data["tool"] == "calculator":
        return (keys(value, "expression settings") and text(value["expression"], 4096)
                and valid_values(value["settings"]))
    if data["tool"] == "solve":
        return (keys(value, "mode inputs expression bounds precision")
                and value["mode"] in ("expression", "coefficients", "numeric", "numeric_scan")
                and isinstance(value["inputs"], list) and len(value["inputs"]) == 3
                and all(text(v, 4096) for v in value["inputs"]) and text(value["expression"], 4096)
                and isinstance(value["bounds"], list) and len(value["bounds"]) == 2
                and all(text(v, 47) for v in value["bounds"])
                and isinstance(value["precision"], str)
                and re.fullmatch(r"[0-9]{1,3}", value["precision"]) is not None
                and 16 <= int(value["precision"]) <= 100)
    if data["tool"] == "matrices":
        def matrix(m):
            return (isinstance(m, list) and 1 <= len(m) <= 8 and isinstance(m[0], list)
                    and 1 <= len(m[0]) <= 8 and all(isinstance(row, list) and len(row) == len(m[0])
                    and all(text(cell, 64) for cell in row) for row in m))
        return (keys(value, "operation a b") and value["operation"] in
                ("determinant", "rank", "multiply", "solve", "solve_general")
                and matrix(value["a"]) and matrix(value["b"]))
    if data["tool"] == "graph":
        def axis(a):
            if (not isinstance(a, list) or len(a) != 2 or not all(text(v, 32)
                    and re.fullmatch(r"[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?", v) for v in a)):
                return False
            try:
                lo, hi = map(float, a)
            except ValueError:
                return False
            return (math.isfinite(lo) and math.isfinite(hi) and math.isfinite(hi-lo)
                    and hi-lo >= max(1, abs(lo), abs(hi))*1e-8)
        return (keys(value, "view curves") and keys(value["view"], "x y")
                and axis(value["view"]["x"]) and axis(value["view"]["y"])
                and isinstance(value["curves"], list) and 1 <= len(value["curves"]) <= 4
                and all(keys(c, "expression color visible") and text(c["expression"], 4096)
                        and isinstance(c["color"], str) and re.fullmatch(r"#[0-9a-fA-F]{6}", c["color"])
                        and type(c["visible"]) is bool for c in value["curves"]))
    return False


def representation(row, detail=True):
    result = {"id": str(row.pk), "name": row.name, "tool": row.data["tool"],
              "revision": row.revision, "updated_at": row.updated_at.isoformat()}
    return result | {"data": row.data} if detail else result


def error(code, status=400):
    return JsonResponse({"error": code}, status=status)


@never_cache
@require_http_methods(["GET", "POST"])
def workspaces(request, workspace_id=None):
    if not request.user.is_authenticated:
        return error("authentication_required", 401)
    owned = Workspace.objects.filter(user=request.user)
    if request.method == "GET":
        if workspace_id is None:
            return JsonResponse({"user_id": request.user.pk, "items": [representation(r, False)
                for r in owned.order_by("-updated_at", "id")],
                "limits": {"count": MAX_ITEMS, "bytes": MAX_BYTES, "total_bytes": MAX_TOTAL_BYTES}})
        row = owned.filter(pk=workspace_id).first()
        return JsonResponse({"user_id": request.user.pk, **representation(row)}) if row else error("not_found", 404)
    data, failure = read_object(request, 128000)
    if failure is not None:
        return failure
    action = data.get("action") if workspace_id else "create"
    expected = {"create": "user_id name data", "update": "user_id action expected_revision data",
                "rename": "user_id action expected_revision name", "duplicate": "user_id action expected_revision name",
                "delete": "user_id action expected_revision"}
    if (not isinstance(action, str) or action not in expected or (workspace_id and action == "create")
            or not keys(data, expected[action])
            or type(data["user_id"]) is not int
            or (workspace_id and (type(data["expected_revision"]) is not int
                or not 1 <= data["expected_revision"] < 2**53-1))):
        return error("invalid_request")
    if data["user_id"] != request.user.pk:
        return error("account_changed", 409)
    name = data.get("name")
    if "name" in data and (not text(name, 320) or not 1 <= len(name.strip()) <= 80):
        return error("invalid_name")
    if "data" in data and not valid_data(data["data"]):
        return error("invalid_workspace")
    with transaction.atomic():
        if not User.objects.select_for_update().filter(pk=request.user.pk, is_active=True).exists():
            return error("authentication_required", 401)
        row = owned.filter(pk=workspace_id).first() if workspace_id else None
        if workspace_id and not row:
            return error("not_found", 404)
        if row and row.revision != data["expected_revision"]:
            return error("revision_conflict", 409)
        if action == "delete":
            row.delete()
            return JsonResponse({"user_id": request.user.pk, "deleted": str(workspace_id)})
        value = data.get("data", row.data if row else None)
        # Reject escaped lone surrogates and non-finite JSON, even inside strings/metadata.
        try:
            size = len(json.dumps(value, ensure_ascii=False, separators=(",", ":"), allow_nan=False).encode("utf-8"))
        except (ValueError, UnicodeError):
            return error("invalid_workspace")
        if size > MAX_BYTES:
            return error("workspace_too_large", 413)
        rows = list(owned.values_list("id", "size"))
        creating = action in ("create", "duplicate")
        if ((creating and len(rows) >= MAX_ITEMS) or
                sum(s for key, s in rows if creating or key != row.pk) + size > MAX_TOTAL_BYTES):
            return error("quota_exceeded", 409)
        if row and not creating and value["tool"] != row.data["tool"]:
            return error("invalid_workspace")
        if creating:
            row = Workspace(user=request.user)
        row.data, row.size = value, size
        if name is not None:
            row.name = name.strip()
        if not creating:
            row.revision += 1
        row.save()
    return JsonResponse({"user_id": request.user.pk, **representation(row)}, status=201 if creating else 200)
