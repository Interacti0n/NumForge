import django.db.models.deletion
import identity.models
from django.conf import settings
from django.db import migrations, models


class Migration(migrations.Migration):
    dependencies = [("identity", "0002_loginthrottle")]
    operations = [migrations.CreateModel(name="Preferences", fields=[
        ("user", models.OneToOneField(on_delete=django.db.models.deletion.CASCADE, primary_key=True,
                                    serialize=False, to=settings.AUTH_USER_MODEL)),
        ("values", models.JSONField(default=identity.models.preference_defaults)),
        ("revision", models.PositiveBigIntegerField(default=0)),
    ])]
