

#ifndef DEINTERLACE_H_INCLUDED
#define DEINTERLACE_H_INCLUDED

/**
 * Our deinterlacer plugin API is modeled after DScaler's.
 */
typedef struct deinterlace_setting_s deinterlace_setting_t;
typedef struct deinterlace_method_s deinterlace_method_t;

typedef void (*setting_onchange_t)(deinterlace_setting_t *);

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
    int overlay_half_height;
    int fields_required;
    int numsettings;
    deinterlace_setting_t *settings;
};

#endif /* DEINTERLACE_H_INCLUDED */
