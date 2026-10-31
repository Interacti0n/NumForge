from django.contrib.auth.models import AbstractUser
from django.db import models


class User(AbstractUser):
    """Project-owned identity, selected before the first database migration."""


class LoginThrottle(models.Model):
    key = models.CharField(max_length=64, primary_key=True)
    attempts = models.PositiveIntegerField(default=0)
    expires_at = models.DateTimeField(db_index=True)


def preference_defaults():
    return {"language": "sk", "precision_mode": "auto", "precision": 10,
            "notation": "auto", "angle": "rad", "rounding": "half_even",
            "complex_form": "cartesian"}


class Preferences(models.Model):
    user = models.OneToOneField(User, on_delete=models.CASCADE, primary_key=True)
    values = models.JSONField(default=preference_defaults)
    revision = models.PositiveBigIntegerField(default=0)
