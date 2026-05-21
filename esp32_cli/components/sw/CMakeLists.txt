idf_component_register(SRCS "src/cli_console.c" "src/telnet_server.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES hw console lwip vfs)
