from django.urls import path
from identity import views

urlpatterns = [
    path("api/account/session", views.session),
    path("api/account/login", views.login),
    path("api/account/logout", views.logout),
]
