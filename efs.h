
#ifndef EFS_H_INCLUDED
#define EFS_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

typedef struct efont_s efont_t;
typedef struct efont_string_s efont_string_t;

efont_t *efont_new( const char *fontfile, int fontsize, int video_width,
                    int video_height, double pixel_aspect );
void efont_delete( efont_t *font );

efont_string_t *efs_new( efont_t *font, const char *text );
void efs_delete( efont_string_t *efs );
int efs_get_width( efont_string_t *efs );
int efs_get_height( efont_string_t *efs );
int efs_get_stride( efont_string_t *efs );
unsigned char *efs_get_buffer( efont_string_t *efs );


#ifdef __cplusplus
};
#endif

#endif /* EFS_H_INCLUDED */
