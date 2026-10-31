"""Private JSON authentication. Mathematical sessions are intentionally separate."""
from datetime import timedelta
import ipaddress
import math
from pathlib import Path

from django.contrib.auth import authenticate, login as auth_login, logout as auth_logout
from django.contrib.auth import get_user_model
from django.db import transaction
from django.http import JsonResponse, FileResponse
from django.shortcuts import render
from django.middleware.csrf import get_token, rotate_token
from django.utils import timezone
from django.utils.crypto import salted_hmac
from django.views.decorators.cache import never_cache
from django.views.decorators.http import require_GET, require_POST
from django.views.decorators.debug import sensitive_variables

from .models import LoginThrottle
from .json_api import read_object

LOGIN_LIMIT = 10
LOGIN_WINDOW = timedelta(minutes=5)

COPY = {
    "sk": dict(title="Tvoj účet", lead="Prihlás sa do súkromného pilotu NumForge.", username="Používateľské meno",
        password="Heslo", show="Zobraziť heslo", remember="Zapamätať na 14 dní", signin="Prihlásiť sa", signout="Odhlásiť sa",
        checking="Overujem prihlásenie…", pending="Pracujem…", signed="Prihlásený účet", retry="Skúsiť znova",
        invalid="Meno alebo heslo nie je správne.", csrf="Platnosť formulára sa zmenila. Skús to znova.",
        limited="Príliš veľa pokusov. Skús to znova o chvíľu.", offline="Spojenie sa nepodarilo. Over stav účtu znova.",
        secure="Prihlasovanie vyžaduje HTTPS.", guest="Kalkulačku môžeš používať aj bez účtu.",
        pilot="Účty v tomto pilote zakladá správca. Verejná registrácia ešte nie je dostupná.",
        scope="Predvolené nastavenia sa načítajú aj na inom zariadení. Výpočty a história zatiaľ zostávajú v prehliadači.",
        defaults="Predvolené nastavenia", language="Jazyk", precision="Presnosť", places="Desatinné miesta",
        auto="Auto · 10 miest", full="Plný výpis", custom="Vlastná", notation="Zápis", plain="Bežný",
        scientific="Vedecký", mathematical="Matematický", fraction="Zlomok", angle="Uhol",
        complex="Komplexný zápis", cartesian="Bežný · a + bi", trig="Trigonometrický", exponential="Exponenciálny",
        rounding="Zaokrúhľovanie: half-even", save="Uložiť nastavenia", import_guest="Prevziať nastavenia hosťa",
        saved="Nastavenia sú uložené.", unsaved="Neuložené zmeny", saving="Ukladám…",
        conflict="Uložená verzia sa zmenila. Tvoje úpravy zostali vo formulári.", reload="Načítať uložené",
        import_note="Prevzaté nastavenia hosťa. Potvrď ich uložením.", invalid_settings="Skontroluj nastavenia a počet miest (0–10 000).",
        calc="Kalkulačka", graphs="Grafy", equations="Rovnice", matrices="Matice", units="Jednotky", guide="Návod / API", account="Účet"),
    "en": dict(title="Your account", lead="Sign in to the private NumForge pilot.", username="Username",
        password="Password", show="Show password", remember="Remember for 14 days", signin="Sign in", signout="Sign out",
        checking="Checking your session…", pending="Working…", signed="Signed-in account", retry="Try again",
        invalid="The username or password is incorrect.", csrf="The form has changed. Please try again.",
        limited="Too many attempts. Please try again later.", offline="Could not connect. Check your account status again.",
        secure="Sign-in requires HTTPS.", guest="You can use the calculator without an account.",
        pilot="An administrator creates pilot accounts. Public registration is not available yet.",
        scope="Defaults load on your other devices. Calculations and history still stay in the browser.",
        defaults="Default settings", language="Language", precision="Precision", places="Decimal places",
        auto="Auto · 10 places", full="Full output", custom="Custom", notation="Notation", plain="Plain",
        scientific="Scientific", mathematical="Mathematical", fraction="Fraction", angle="Angle",
        complex="Complex form", cartesian="Cartesian · a + bi", trig="Trigonometric", exponential="Exponential",
        rounding="Rounding: half-even", save="Save settings", import_guest="Use guest settings",
        saved="Settings saved.", unsaved="Unsaved changes", saving="Saving…",
        conflict="The saved version changed. Your edits are still in the form.", reload="Load saved settings",
        import_note="Guest settings copied. Save to confirm.", invalid_settings="Check settings and decimal places (0–10,000).",
        calc="Calculator", graphs="Graphs", equations="Equations", matrices="Matrices", units="Units", guide="Guide / API", account="Account"),
}


@never_cache
@require_GET
def login_page(request):
    language = "en" if request.GET.get("lang") == "en" else "sk"
    return render(request, "identity/account.html", {"lang": language, "copy": COPY[language]})


@require_GET
def ui_asset(request, name):
    return FileResponse((Path(__file__).parent / "ui" / name).open("rb"),
                        content_type="text/javascript" if name.endswith(".js") else "text/css")


def current_user(request):
    return {"id": request.user.pk, "username": request.user.username} if request.user.is_authenticated else None


@never_cache
def csrf_failure(request, reason=""):
    return JsonResponse({"error": "csrf_failed"}, status=403)


@never_cache
@require_GET
def session(request):
    return JsonResponse({"user": current_user(request), "csrf_token": get_token(request)})


def admit_login(request):
    # Forwarded IP headers are untrusted until the HTTPS gateway is implemented.
    try:
        address = str(ipaddress.ip_address(request.META.get("REMOTE_ADDR", "")))
    except ValueError:
        address = "unknown"
    key = salted_hmac("numforge.login", address, algorithm="sha256").hexdigest()
    now = timezone.now()
    LoginThrottle.objects.filter(expires_at__lte=now).delete()
    with transaction.atomic():
        bucket, _ = LoginThrottle.objects.select_for_update().get_or_create(
            key=key, defaults={"expires_at": now + LOGIN_WINDOW})
        if bucket.expires_at <= now:
            bucket.attempts, bucket.expires_at = 0, now + LOGIN_WINDOW
        if bucket.attempts >= LOGIN_LIMIT:
            return max(1, math.ceil((bucket.expires_at - now).total_seconds()))
        bucket.attempts += 1
        bucket.save(update_fields=["attempts", "expires_at"])
    return 0


@never_cache
@require_POST
@sensitive_variables("data")
def login(request):
    data, error = read_object(request)
    if error is not None:
        return error
    if (set(data) - {"username", "password", "remember"}
            or not isinstance(data.get("username"), str) or not 1 <= len(data["username"]) <= 150
            or not isinstance(data.get("password"), str) or not 1 <= len(data["password"]) <= 1024
            or not isinstance(data.get("remember", False), bool)):
        return JsonResponse({"error": "invalid_request"}, status=400)
    try:
        data["username"].encode("utf-8")
        data["password"].encode("utf-8")
        if "\x00" in data["username"]:
            raise ValueError()
    except (UnicodeError, ValueError):
        return JsonResponse({"error": "invalid_request"}, status=400)
    retry = admit_login(request)
    if retry:
        response = JsonResponse({"error": "too_many_attempts"}, status=429)
        response["Retry-After"] = str(retry)
        return response
    username = get_user_model().normalize_username(data["username"])
    user = authenticate(request, username=username, password=data["password"])
    if user is None:
        return JsonResponse({"error": "invalid_credentials"}, status=401)
    auth_login(request, user)
    request.session.cycle_key()
    request.session.set_expiry(14 * 86400 if data.get("remember", False) else 0)
    return JsonResponse({"user": current_user(request), "csrf_token": get_token(request)})


@never_cache
@require_POST
def logout(request):
    auth_logout(request)
    rotate_token(request)
    return JsonResponse({"user": None, "csrf_token": get_token(request)})
