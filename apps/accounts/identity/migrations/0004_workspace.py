import uuid
from django.conf import settings
from django.db import migrations, models
import django.db.models.deletion


class Migration(migrations.Migration):
    dependencies = [("identity", "0003_preferences")]
    operations = [migrations.CreateModel(
        name="Workspace",
        fields=[
            ("id", models.UUIDField(default=uuid.uuid4, editable=False, primary_key=True, serialize=False)),
            ("name", models.CharField(max_length=80)),
            ("data", models.JSONField()),
            ("size", models.PositiveIntegerField()),
            ("revision", models.PositiveBigIntegerField(default=1)),
            ("updated_at", models.DateTimeField(auto_now=True)),
            ("user", models.ForeignKey(on_delete=django.db.models.deletion.CASCADE, to=settings.AUTH_USER_MODEL)),
        ],
        options={"indexes": [models.Index(fields=["user", "-updated_at"], name="workspace_owner_updated")]},
    )]
