

#ifndef DEINTERLACE_H_INCLUDED
#define DEINTERLACE_H_INCLUDED

/**
 * Our deinterlacer plugin API is modeled after DScaler's.
 */
typedef struct deinterlace_setting_s deinterlace_setting_t;
typedef struct deinterlace_method_s deinterlace_method_t;

typedef void (*setting_onchange_t)(deinterlace_setting_t *);

typedef void (*deinterlace_scanline_t)( unsigned char *output,
                                        unsigned char *t1,
                                        unsigned char *m1,
                                        unsigned char *b1,
                                        unsigned char *t0,
                                        unsigned char *m0,
                                        unsigned char *b0,
                                        int width );

typedef deinterlace_method_t *(*deinterlace_plugin_init_t)( int accel );

typedef enum
{
    SETTING_NOT_PRESENT,
    SETTING_ONOFF,
    SETTING_YESNO,
    SETTING_ITEMFROMLIST,
    SETTING_SLIDER
} setting_type_t;

struct deinterlace_setting_s
{
    const char *name;
    setting_type_t type;
    int *value;
    int defvalue;
    int minvalue;
    int maxvalue;
    int stepvalue;
    setting_onchange_t onchange;
};

struct deinterlace_method_s
{
    const char *name;
    const char *short_name;
    int fields_required;
    int numsettings;
    deinterlace_setting_t *settings;
    deinterlace_scanline_t function;
};

/**
 * Registers a new deinterlace method.
 */
void register_deinterlace_method( deinterlace_method_t *method );

/**
 * Returns how many deinterlacing methods are available.
 */
int get_num_deinterlace_methods( void );

/**
 * Returns the specified method in the list.
 */
deinterlace_method_t *get_deinterlace_method( int i );

/**
 * Loads a deinterlace plugin from the given file.
 */
void register_deinterlace_plugin( const char *filename );

#endif /* DEINTERLACE_H_INCLUDED */
