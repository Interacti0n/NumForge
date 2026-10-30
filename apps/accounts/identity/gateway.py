import ipaddress
from secrets import compare_digest

from django.conf import settings
from django.http import JsonResponse


class GatewayMiddleware:
    def __init__(self, get_response):
        self.get_response = get_response

    def __call__(self, request):
        if settings.GATEWAY_TOKEN:
            if not compare_digest(request.META.get("HTTP_X_NUMFORGE_GATEWAY", "").encode("utf-8", errors="replace"), settings.GATEWAY_TOKEN.encode()):
                return JsonResponse({"error": "gateway_required"}, status=403)
            if request.META.get("HTTP_X_FORWARDED_PROTO") != "https":
                return JsonResponse({"error": "https_required"}, status=403)
            try:
                address = ipaddress.ip_address(request.META.get("HTTP_X_NUMFORGE_CLIENT_IP", ""))
            except ValueError:
                return JsonResponse({"error": "invalid_client_address"}, status=403)
            request.META["REMOTE_ADDR"] = str(address)
        request.get_host()
        return self.get_response(request)
