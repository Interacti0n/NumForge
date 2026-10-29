from django.contrib.auth.models import AbstractUser


class User(AbstractUser):
    """Project-owned identity, selected before the first database migration."""
