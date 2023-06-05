#pragma once

char *get_xdg_path(const char *xdg_variable, const char *fallback);

char *get_config_path(void);

char *get_save_path(const char *rom_filepath);

char *get_savestate_path(const char *rom_filepath, int slot);
