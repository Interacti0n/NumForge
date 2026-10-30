from django.urls import path
from identity import views

urlpatterns = [
    path("login", views.login_page),
    path("api/account/ui.js", views.ui_asset, {"name": "ui.js"}),
    path("api/account/ui.css", views.ui_asset, {"name": "ui.css"}),
    path("api/account/session", views.session),
    path("api/account/login", views.login),
    path("api/account/logout", views.logout),
]
