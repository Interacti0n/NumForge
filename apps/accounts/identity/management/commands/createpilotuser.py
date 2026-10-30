from getpass import getpass

from django.contrib.auth import get_user_model
from django.contrib.auth.password_validation import validate_password
from django.core.exceptions import ValidationError
from django.core.management.base import BaseCommand, CommandError
from django.db import IntegrityError


class Command(BaseCommand):
    help = "Create a regular private-pilot user; password is prompted, never passed in arguments."

    def add_arguments(self, parser):
        parser.add_argument("username")

    def handle(self, username, **options):
        user = get_user_model()(username=get_user_model().normalize_username(username))
        password = getpass("Password: ")
        if password != getpass("Repeat password: "):
            raise CommandError("Passwords differ.")
        try:
            user.full_clean(exclude=["password"])
            validate_password(password, user)
            user.set_password(password)
            user.save()
        except (ValidationError, IntegrityError) as error:
            raise CommandError("Invalid or existing username, or password rejected by validators.") from error
        self.stdout.write("Private pilot user created.")
