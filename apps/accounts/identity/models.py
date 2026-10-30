from django.contrib.auth.models import AbstractUser
from django.db import models


class User(AbstractUser):
    """Project-owned identity, selected before the first database migration."""


class LoginThrottle(models.Model):
    key = models.CharField(max_length=64, primary_key=True)
    attempts = models.PositiveIntegerField(default=0)
    expires_at = models.DateTimeField(db_index=True)
