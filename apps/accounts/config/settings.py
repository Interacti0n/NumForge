"""Private account service; publishing requires an explicit HTTPS gateway."""
import json
import os
from pathlib import Path
from urllib.parse import urlsplit

from django.core.exceptions import ImproperlyConfigured

try:
    config = json.loads(Path(os.environ["NF_ACCOUNTS_CONFIG"]).read_text(encoding="utf-8"))
    SECRET_KEY = config["secret_key"]
    DATABASES = {"default": {
        "ENGINE": "django.db.backends.postgresql",
        "NAME": config["name"],
        "USER": config["user"],
        "PASSWORD": config["password"],
        "HOST": config["host"],
        "PORT": config["port"],
        "CONN_MAX_AGE": 0,
        "OPTIONS": {"connect_timeout": 5},
    }}
except (KeyError, ValueError, OSError) as error:
    raise ImproperlyConfigured("Set NF_ACCOUNTS_CONFIG to a private account configuration file.") from error

if len(SECRET_KEY) < 50:
    raise ImproperlyConfigured("The account secret key must contain at least 50 characters.")

DEBUG = False
ALLOWED_HOSTS = ["localhost", "127.0.0.1"]
ROOT_URLCONF = "config.urls"
TEMPLATES = [{"BACKEND": "django.template.backends.django.DjangoTemplates", "APP_DIRS": True}]
INSTALLED_APPS = ["django.contrib.auth", "django.contrib.contenttypes", "django.contrib.sessions", "identity"]
AUTH_USER_MODEL = "identity.User"
DEFAULT_AUTO_FIELD = "django.db.models.BigAutoField"
USE_TZ = True
TIME_ZONE = "UTC"
MIDDLEWARE = [
    "identity.gateway.GatewayMiddleware",
    "django.middleware.security.SecurityMiddleware",
    "django.contrib.sessions.middleware.SessionMiddleware",
    "django.middleware.csrf.CsrfViewMiddleware",
    "django.contrib.auth.middleware.AuthenticationMiddleware",
]
SESSION_COOKIE_NAME = "numforge_account"
SESSION_COOKIE_SECURE = True
SESSION_COOKIE_HTTPONLY = True
SESSION_COOKIE_SAMESITE = "Lax"
CSRF_COOKIE_SECURE = True
CSRF_COOKIE_HTTPONLY = True
CSRF_COOKIE_SAMESITE = "Lax"
CSRF_COOKIE_NAME = "numforge_csrf"
CSRF_FAILURE_VIEW = "identity.views.csrf_failure"
SESSION_COOKIE_AGE = 12 * 60 * 60
DATA_UPLOAD_MAX_MEMORY_SIZE = 4096
PASSWORD_HASHERS = [
    "django.contrib.auth.hashers.Argon2PasswordHasher",
    "django.contrib.auth.hashers.PBKDF2PasswordHasher",
    "django.contrib.auth.hashers.PBKDF2SHA1PasswordHasher",
]
AUTH_PASSWORD_VALIDATORS = [
    {"NAME": "django.contrib.auth.password_validation.UserAttributeSimilarityValidator"},
    {"NAME": "django.contrib.auth.password_validation.MinimumLengthValidator", "OPTIONS": {"min_length": 12}},
    {"NAME": "django.contrib.auth.password_validation.CommonPasswordValidator"},
    {"NAME": "django.contrib.auth.password_validation.NumericPasswordValidator"},
]
SECURE_CONTENT_TYPE_NOSNIFF = True
SECURE_SSL_REDIRECT = True
SECURE_HSTS_SECONDS = 0  # Set only at the eventual HTTPS gateway deployment.
GATEWAY_TOKEN = Path(os.environ["NF_GATEWAY_TOKEN_FILE"]).read_text().strip() if os.environ.get("NF_GATEWAY_TOKEN_FILE") else None
if GATEWAY_TOKEN is not None:
    if len(GATEWAY_TOKEN) < 40:
        raise ImproperlyConfigured("Gateway token is too short.")
    SECURE_PROXY_SSL_HEADER = ("HTTP_X_FORWARDED_PROTO", "https")
CSRF_TRUSTED_ORIGINS = []
for public_origin in (os.environ.get("NF_ACCOUNT_PUBLIC_ORIGIN", ""), os.environ.get("NF_LOCAL_ORIGIN", "")):
    if not public_origin:
        continue
    origin = urlsplit(public_origin)
    if origin.scheme != "https" or not origin.hostname or origin.username or origin.password or origin.path or origin.query or origin.fragment or "*" in public_origin:
        raise ImproperlyConfigured("Use one exact HTTPS account origin without a path.")
    CSRF_TRUSTED_ORIGINS.append(public_origin)
