#include <include/settings.h>

namespace btorrent {

settings_t global_settings;
settings_t *gsettings() { return &global_settings; }

}

