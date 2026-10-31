"""Owner-scoped defaults; saves compare revisions under the owner's row lock."""
from django.db import transaction
from django.http import JsonResponse
from django.views.decorators.cache import never_cache
from django.views.decorators.http import require_http_methods

from .json_api import read_object
from .models import Preferences, User, preference_defaults
from .views import current_user


def valid_values(values):
    return (isinstance(values, dict) and values.keys() == preference_defaults().keys()
            and type(values["precision"]) is int and 0 <= values["precision"] <= 10000
            and values["language"] in ("sk", "en")
            and values["precision_mode"] in ("auto", "custom", "full")
            and values["notation"] in ("auto", "plain", "scientific", "math", "fraction")
            and values["angle"] in ("rad", "deg") and values["rounding"] == "half_even"
            and values["complex_form"] in ("cartesian", "trig", "exp"))


def representation(request, row):
    return {"user": current_user(request), "revision": row.revision if row else 0,
            "values": row.values if row else preference_defaults()}


@never_cache
@require_http_methods(["GET", "POST"])
def preferences(request):
    if not request.user.is_authenticated:
        return JsonResponse({"error": "authentication_required"}, status=401)
    if request.method == "GET":
        return JsonResponse(representation(request, Preferences.objects.filter(user=request.user).first()))
    data, error = read_object(request)
    if error is not None:
        return error
    if (data.keys() != {"user_id", "expected_revision", "values"}
            or type(data["user_id"]) is not int
            or type(data["expected_revision"]) is not int
            or not 0 <= data["expected_revision"] < 2**53-1
            or not valid_values(data["values"])):
        return JsonResponse({"error": "invalid_request"}, status=400)
    if data["user_id"] != request.user.pk:
        return JsonResponse({"error": "account_changed"}, status=409)
    with transaction.atomic():
        # Serialize first saves too; there may not yet be a preferences row.
        if not User.objects.select_for_update().filter(pk=request.user.pk, is_active=True).first():
            return JsonResponse({"error": "authentication_required"}, status=401)
        row = Preferences.objects.filter(user=request.user).first()
        current = representation(request, row)
        if data["expected_revision"] != current["revision"]:
            return JsonResponse(current | {"error": "revision_conflict"}, status=409)
        row = row or Preferences(user=request.user)
        row.values, row.revision = data["values"], current["revision"] + 1
        row.save()
    return JsonResponse(representation(request, row))
