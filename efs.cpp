
#include "ttfont.h"
#include "efs.h"

struct efont_s
{
    TTFFont *font;
    int width;
    int height;
};

efont_t *efont_new( const char *fontfile, int fontsize, int video_width,
                    int video_height, double aspect_ratio )
{
    efont_t *font = new efont_t;

    if( !font ) {
        return 0;
    }

    font->font = new TTFFont( fontfile, fontsize, video_width, video_height, aspect_ratio );
    if( !font->font ) {
        delete font;
        return 0;
    }
    font->width = video_width;
    font->height = video_height;

    return font;
}

void efont_delete( efont_t *font )
{
    delete font->font;
    delete font;
}

struct efont_string_s
{
    efont_t *font;
    unsigned char *data;
    int width;
    int height;
    int stride;
};

efont_string_t *efs_new( efont_t *font )
{
    efont_string_t *efs = new efont_string_t;
    efs->data = new unsigned char[ font->width * font->height ];
    efs->font = font;
    return efs;
}

void efs_delete( efont_string_t *efs )
{
    delete efs->data;
    delete efs;
}

void efs_set_text( efont_string_t *efs, const char *text )
{
    efs->font->font->RenderString( efs->data, text, &efs->width, &efs->height,
                                   efs->font->width, efs->font->height );
    efs->stride = efs->width;
}

int efs_get_width( efont_string_t *efs )
{
    return efs->width;
}

int efs_get_height( efont_string_t *efs )
{
    return efs->height;
}

int efs_get_stride( efont_string_t *efs )
{
    return efs->stride;
}

unsigned char *efs_get_buffer( efont_string_t *efs )
{
    return efs->data;
}

