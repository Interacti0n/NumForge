"""Bounded JSON objects shared by the private account endpoints."""
import json

from django.core.exceptions import RequestDataTooBig
from django.http import JsonResponse


def read_object(request, maximum=4096):
    if request.content_type != "application/json":
        return None, JsonResponse({"error": "json_required"}, status=415)
    try:
        if len(request.body) > maximum:
            raise RequestDataTooBig()
        data = json.loads(request.body)
    except RequestDataTooBig:
        return None, JsonResponse({"error": "request_too_large"}, status=413)
    except (ValueError, UnicodeDecodeError, RecursionError):
        data = None
    if not isinstance(data, dict):
        return None, JsonResponse({"error": "invalid_request"}, status=400)
    return data, None
