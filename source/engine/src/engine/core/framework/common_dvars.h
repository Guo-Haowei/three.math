#include "core/dynamic_variable/dynamic_variable_begin.h"

DVAR_STRING(project, 0, "Open project at start", "");

// window
DVAR_IVEC2(window_resolution, DVAR_FLAG_CACHE, "Request window resolution", 800, 600);
DVAR_IVEC2(window_position, DVAR_FLAG_CACHE, "Request window position", 40, 40);

// IO
DVAR_BOOL(verbose, 0, "Print verbose log", true);

// cache
DVAR_STRING(content_browser_path, DVAR_FLAG_CACHE, "", "");

// gui
DVAR_BOOL(show_editor, DVAR_FLAG_CACHE, "Show editor", true);

#include "core/dynamic_variable/dynamic_variable_end.h"
