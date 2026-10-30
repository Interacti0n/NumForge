FROM python:3.12-slim-bookworm
WORKDIR /service
COPY apps/accounts/requirements.txt deploy/accounts-requirements.txt ./
RUN pip install --no-cache-dir -r accounts-requirements.txt
COPY apps/accounts/ ./
COPY scripts/accounts_db.py deploy/accounts_setup.py /support/
USER 10001:10001
ENV PYTHONDONTWRITEBYTECODE=1 PYTHONUNBUFFERED=1 DJANGO_SETTINGS_MODULE=config.settings
CMD ["gunicorn", "config.wsgi:application", "--bind", "0.0.0.0:8000", "--workers", "1", "--threads", "2", "--worker-tmp-dir", "/tmp", "--no-control-socket"]
