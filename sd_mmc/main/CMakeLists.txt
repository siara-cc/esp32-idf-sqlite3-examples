set(COMPONENT_SRCS "sd_mmc.c")
set(COMPONENT_ADD_INCLUDEDIRS "")

idf_build_component(esp32-idf-sqlite3)
idf_component_register(SRCS "${COMPONENT_SRCS}"
                       REQUIRES esp32-idf-sqlite3 sd_mmc
                      )

