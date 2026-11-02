import uuid

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


class Workspace(models.Model):
    id = models.UUIDField(primary_key=True, default=uuid.uuid4, editable=False)
    user = models.ForeignKey(User, on_delete=models.CASCADE)
    name = models.CharField(max_length=80)
    data = models.JSONField()
    size = models.PositiveIntegerField()
    revision = models.PositiveBigIntegerField(default=1)
    updated_at = models.DateTimeField(auto_now=True)

    class Meta:
        indexes = [models.Index(fields=["user", "-updated_at"], name="workspace_owner_updated")]
