#include <stdio.h>

#ifdef HAVE_LIBXML2

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include "tvtimeconf.h"
#include "videotools.h"
#include "parser.h"
#include "input.h"

typedef struct
{
    char *name;
    char *value;
} ConfigDef;

typedef struct
{
    int key;
    int command;
} KeyBindDef;

typedef struct
{
    int button;
    int command;
} MouseBindDef;

typedef struct {
    char *name;
    int command;
} Cmd_Names;

/* Default configuration values  */
static const ConfigDef config_def[] = 
{
    {"Verbose", "0"},
    {"Debug", "0"},
    {"CaptureSource", "0"},
    {"V4LDevice", "/dev/video0"},
    {"VBIDevice", "/dev/vbi0"},
    {"UseVBI", "0"},
    {"Norm", "ntsc"},
    {"Frequencies", "us-cable"},
    {"NTSCCableMode", "Nominal"},
    {"FineTuneOffset", "0"},
    {"CheckForSignal", "1"},
    {"OutputWidth", "800"},
    {"InputWidth", "720"},
    {"WideString", "0"},
    {"ApplyLumaCorrection", "0"},
    {"LumaCorrection", "1.0"},
    {"ProcessPriority", "-19"},
    {"ChannelTextFG", "0xFFFFFF00"},
    {"OtherTextFG", "0xFFF5DEB3"},
    {"CommandPipe", "$HOME/.tvtime/tvtimefifo"},
    {"FullScreen", "0"},
    {"PreferredDeinterlaceMethod", "0"},
    {"Volume", "100"},
    {"VolumeMute", "0"},
    {"Hue", "50"},
    {"Brightnes", "50"},
    {"Contrast", "50"},
    {"Colour", "50"},
    {"HalfFramerate", "0"},
    {"LastChannel", "0"},
    {NULL, NULL}
};
    
static const KeyBindDef key_def[] =
{
    {I_ESCAPE,	TVTIME_QUIT},
    {'q',	TVTIME_QUIT},
    {I_UP,	TVTIME_CHANNEL_UP},
    {'k',	TVTIME_CHANNEL_UP},
    {I_DOWN,	TVTIME_CHANNEL_DOWN},
    {'j',	TVTIME_CHANNEL_DOWN},
    {I_RIGHT,	TVTIME_FINETUNE_UP},
    {'l',	TVTIME_FINETUNE_UP},
    {I_LEFT,	TVTIME_FINETUNE_DOWN},
    {'h',	TVTIME_FINETUNE_DOWN},
    {'p',	TVTIME_CHANNEL_PREV},
    {']',	TVTIME_FREQLIST_UP},
    {'[',	TVTIME_FREQLIST_DOWN},
    {'c',	TVTIME_LUMA_CORRECTION_TOGGLE},
    {'x',	TVTIME_LUMA_UP},
    {'z',	TVTIME_LUMA_DOWN},
    {'m',	TVTIME_MIXER_MUTE},
    {'+',	TVTIME_MIXER_UP},
    {'-',	TVTIME_MIXER_DOWN},
    {I_ENTER,	TVTIME_ENTER},
    {I_F1,	TVTIME_HUE_DOWN},
    {I_F2,	TVTIME_HUE_UP},
    {I_F3,	TVTIME_BRIGHT_DOWN},
    {I_F4,	TVTIME_BRIGHT_UP},
    {I_F5,	TVTIME_CONT_DOWN},
    {I_F6,	TVTIME_CONT_UP},
    {I_F7,	TVTIME_COLOUR_DOWN},
    {I_F8,	TVTIME_COLOUR_UP},
    {I_F11,	TVTIME_SHOW_BARS},
    {I_F12,	TVTIME_SHOW_CREDITS},
    {'d',	TVTIME_DEBUG},
    {'f',	TVTIME_FULLSCREEN},
    {'i',	TVTIME_TV_VIDEO},
    {'a',	TVTIME_ASPECT},
    {'s',	TVTIME_SCREENSHOT},
    {'t',	TVTIME_DEINTERLACINGMODE},
    {'n',	TVTIME_TOGGLE_NTSC_CABLE_MODE},
    {'`',	TVTIME_TOGGLE_CONSOLE},
    {I_PGUP,	TVTIME_SCROLL_CONSOLE_UP},
    {I_PGDN,	TVTIME_SCROLL_CONSOLE_DOWN},
    {'w',	TVTIME_TOGGLE_CC},
    {'r',	TVTIME_SKIP_CHANNEL},
    {' ',	TVTIME_AUTO_ADJUST_PICT},
    {'=',	TVTIME_TOGGLE_HALF_FRAMERATE},
    {0, 0}
};

static const MouseBindDef mouse_def[] = 
{
    {1, TVTIME_DISPLAY_INFO},
    {2, TVTIME_MIXER_MUTE},
    {3, TVTIME_TV_VIDEO},
    {4, TVTIME_CHANNEL_UP},
    {5, TVTIME_CHANNEL_DOWN},
    {0, 0}
};

static const Cmd_Names cmd_table[] = {
    { "QUIT", TVTIME_QUIT },
    { "CHANNEL_UP", TVTIME_CHANNEL_UP },
    { "CHANNEL_DOWN", TVTIME_CHANNEL_DOWN },
    { "CHANNEL_PREV", TVTIME_CHANNEL_PREV },
    { "LUMA_CORRECTION_TOGGLE", TVTIME_LUMA_CORRECTION_TOGGLE },
    { "LUMA_UP", TVTIME_LUMA_UP },
    { "LUMA_DOWN", TVTIME_LUMA_DOWN },
    { "MIXER_MUTE", TVTIME_MIXER_MUTE },
    { "MIXER_UP", TVTIME_MIXER_UP },
    { "MIXER_DOWN", TVTIME_MIXER_DOWN },
    { "TV_VIDEO", TVTIME_TV_VIDEO },
    { "HUE_DOWN", TVTIME_HUE_DOWN },
    { "HUE_UP", TVTIME_HUE_UP },
    { "BRIGHT_DOWN", TVTIME_BRIGHT_DOWN },
    { "BRIGHT_UP", TVTIME_BRIGHT_UP },
    { "CONT_DOWN", TVTIME_CONT_DOWN },
    { "CONT_UP", TVTIME_CONT_UP },
    { "COLOUR_DOWN", TVTIME_COLOUR_DOWN },
    { "COLOUR_UP", TVTIME_COLOUR_UP },

    { "FINETUNE_DOWN", TVTIME_FINETUNE_DOWN },
    { "FINETUNE_UP", TVTIME_FINETUNE_UP },

    { "FREQLIST_DOWN", TVTIME_FREQLIST_DOWN },
    { "FREQLIST_UP", TVTIME_FREQLIST_UP },

    { "SHOW_BARS", TVTIME_SHOW_BARS },
    { "DEBUG", TVTIME_DEBUG },

    { "FULLSCREEN", TVTIME_FULLSCREEN },
    { "ASPECT", TVTIME_ASPECT },
    { "SCREENSHOT", TVTIME_SCREENSHOT },
    { "DEINTERLACING_MODE", TVTIME_DEINTERLACINGMODE },

    { "MENUMODE", TVTIME_MENUMODE },
    { "DISPLAY_INFO", TVTIME_DISPLAY_INFO },
    { "SHOW_CREDITS", TVTIME_SHOW_CREDITS },

    { "TOGGLE_NTSC_CABLE_MODE", TVTIME_TOGGLE_NTSC_CABLE_MODE },
    { "AUTO_ADJUST_PICT", TVTIME_AUTO_ADJUST_PICT },
    { "TOGGLE_CONSOLE", TVTIME_TOGGLE_CONSOLE },
    { "SCROLL_CONSOLE_UP", TVTIME_SCROLL_CONSOLE_UP },
    { "SCROLL_CONSOLE_DOWN", TVTIME_SCROLL_CONSOLE_DOWN },
    { "SKIP_CHANNEL", TVTIME_SKIP_CHANNEL },
    { "TOGGLE_CC", TVTIME_TOGGLE_CC },
    { "TOGGLE_HALF_FRAMERATE", TVTIME_TOGGLE_HALF_FRAMERATE },
    { NULL, NULL }
};


xmlDocPtr xml_config_init(char *docname)
{
    xmlDocPtr doc;
    xmlNodePtr cur;
    
    if((doc = xmlParseFile(docname)) == NULL)
    {
	fprintf(stderr, "config_init(): Configuration file %s not parsed successfully.\n", docname);
	return NULL;
    }
    
    if((cur = xmlDocGetRootElement(doc)) == NULL)
    {
	fprintf(stderr, "config_init(): Configuration file %s empty.\n", docname);
	xmlFreeDoc(doc);
	return NULL;
    }
    
    if (xmlStrcmp(cur->name, (const xmlChar *) "Conf"))
    {
	fprintf(stderr,"Incorrect root node in configuration file %s (must be 'Conf')");
	xmlFreeDoc(doc);
	return NULL;
    }

    return doc;
}

#endif /* #ifdef HAVE_LIBXML2 */
