target_sources(numforge_web PRIVATE
    web_main.c
)
target_sources(numforge_web_backend PRIVATE
    web_server.c
    web_workers.c
)
