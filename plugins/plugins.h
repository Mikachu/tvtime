
#ifndef TVTIME_PLUGINS_H_INCLUDED
#define TVTIME_PLUGINS_H_INCLUDED

/**
 * tvtime has a plugin system for deinterlacer plugins.
 * However, at this point it's a bit silly to bother using
 * them as 'dynamic' plugins.  So, for the standard plugins,
 * we allow them to be built into the executable, and their
 * initializer methods go here.
 */

void greedy2frame_plugin_init( void );
void twoframe_plugin_init( void );
void linear_plugin_init( void );

#endif /* TVTIME_PLUGINS_H_INCLUDED */
