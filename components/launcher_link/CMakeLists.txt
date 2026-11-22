# launcher_link -- what a game needs to live under the Knob Launcher.
#
# A game pulls it in through its main/idf_component.yml:
#
#   dependencies:
#     launcher_link:
#       git: https://github.com/EvgeniChak/knob-launcher.git
#       path: components/launcher_link
#
# and calls launcher_link_init() in app_main(). See include/launcher_link.h.
idf_component_register(
    SRCS "src/launcher_link.c"
    INCLUDE_DIRS "include"
    REQUIRES
        lvgl__lvgl
    PRIV_REQUIRES
        app_update
        esp_partition
        nvs_flash
)
