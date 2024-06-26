#if defined(ARLIB_GUI_GTK3) || defined(ARLIB_GUI_GTK4)
#include "global.h"
#include "string.h"
#include "file.h"

void arlib_init();
bool arlib_try_init();

#include <gtk/gtk.h>

static void init_gui_shared_early()
{
	// gtk does not want to be an optional library; it wants to be the foundation that everything else is built on
	// but I want to use it as a library, and my want is stronger; this piece makes it slightly less intrusive
	gtk_disable_setlocale();
	// C locale is terribly designed anyways, it uglifies debug printfs and breaks everything that didn't reinvent strtod/sprintf
	// for more details, see https://github.com/mpv-player/mpv/commit/1e70e82baa9193f6f027338b0fab0f5078971fbe
}

static void init_gui_shared_late()
{
}

#ifdef ARLIB_GUI_GTK3
void arlib_init()
{
	init_gui_shared_early();
	gtk_init(nullptr, nullptr);
	init_gui_shared_late();
}
bool arlib_try_init()
{
	init_gui_shared_early();
	bool ret = gtk_init_check(nullptr, nullptr);
	init_gui_shared_late();
	return ret;
}
#endif

#ifdef ARLIB_GUI_GTK4
void arlib_init()
{
	init_gui_shared_early();
	gtk_init();
	init_gui_shared_late();
}

bool arlib_try_init()
{
	init_gui_shared_early();
	bool ret = gtk_init_check();
	init_gui_shared_late();
	return ret;
}
#endif
#endif
