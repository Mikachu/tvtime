
#ifndef CHANNELS_H_INCLUDED
#define CHANNELS_H_INCLUDED

typedef struct frequency_table_s frequency_table_t;

frequency_table_t *frequency_table_new( const char *name );
void frequency_table_delete( frequency_table_t *table );
const char *frequency_table_get_name( frequency_table_t *table );
void frequency_table_set_min_channel_number( frequency_table_t *table, int min );
void frequency_table_set_max_channel_number( frequency_table_t *table, int max );
void frequency_table_set_valid_norm( frequency_table_t *table, const char *norm );
void frequency_table_add_channel( frequency_table_t *table, int channelnum, int freq );
int frequency_table_get_min_channel_number( frequency_table_t *table );
int frequency_table_get_max_channel_number( frequency_table_t *table );
int frequency_table_get_channel_frequency( frequency_table_t *table, int channelnum );

typedef struct channels_s channels_t;

channels_t *channels_new( void );
void channels_delete( channels_t *channels );
void channels_add_frequency_table( channels_t *channels, frequency_table_t *list );
void channels_read_channel_file( channels_t *channels, const char *filename );

#endif /* CHANNELS_H_INCLUDED */
