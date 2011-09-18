/**
 * Copyright (c) 2003 Billy Biggs <vektor@dumbterm.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <libxml/tree.h>
#include "xmltv.h"

#define MAX_LANGUAGES 10

typedef struct language_s language_t;
typedef struct program_s program_t;

struct xmltv_s
{
    xmlDocPtr doc;
    xmlNodePtr root;
    int num_languages;
    language_t *languages;
    language_t *locale;
    char curchannel[ 256 ];
    int refresh;
    xmlChar *curchan;
    xmlChar *display_chan;
    program_t *pro;
    program_t *next_pro;
    int is_tv_grab_na;
};

/**
 * Struct to store language info.
 */
struct language_s {
    xmlChar code[3];
    const char *name;
    language_t *next;
};

/**
 * Struct to store program info.
 */
struct program_s {
    xmlChar *title;
    xmlChar *title_local;
    xmlChar *subtitle;
    xmlChar *subtitle_local;
    xmlChar *description;
    xmlChar *description_local;
    time_t start_time;
    time_t end_time;
};

/**
 * Support the Date::Manip timezone names.  This code will hopefully
 * go away when all XMLTV providers drop these names.  Using names
 * is a bad idea since there is no unified standard for them, and the
 * XMLTV DTD does not define a set of standard names to use.
 */
typedef struct tz_map_s {
    const char *name;
    int offset;
} tz_map_t;

tz_map_t date_manip_timezones[] = {
    { "IDLW",    -1200, }, { "NT",      -1100, }, { "HST",     -1000, },
    { "CAT",     -1000, }, { "AHST",    -1000, }, { "AKST",     -900, },
    { "YST",      -900, }, { "HDT",      -900, }, { "AKDT",     -800, },
    { "YDT",      -800, }, { "PST",      -800, }, { "PDT",      -700, },
    { "MST",      -700, }, { "MDT",      -600, }, { "CST",      -600, },
    { "CDT",      -500, }, { "EST",      -500, }, { "ACT",      -500, },
    { "SAT",      -400, }, { "BOT",      -400, }, { "EDT",      -400, },
    { "AST",      -400, }, { "AMT",      -400, }, { "ACST",     -400, },
    { "NFT",      -330, }, { "BRST",     -300, }, { "BRT",      -300, },
    { "AMST",     -300, }, { "ADT",      -300, }, { "ART",      -300, },
    { "NDT",      -230, }, { "AT",       -200, }, { "BRST",     -200, },
    { "FNT",      -200, }, { "WAT",      -100, }, { "FNST",     -100, },
    { "GMT",         0, }, { "UT",          0, }, { "UTC",         0, },
    { "WET",         0, }, { "CET",       100, }, { "FWT",       100, },
    { "MET",       100, }, { "MEZ",       100, }, { "MEWT",      100, },
    { "SWT",       100, }, { "BST",       100, }, { "GB",        100, },
    { "WEST",        0, }, { "CEST",      200, }, { "EET",       200, },
    { "FST",       200, }, { "MEST",      200, }, { "MESZ",      200, },
    { "METDST",    200, }, { "SAST",      200, }, { "SST",       200, },
    { "EEST",      300, }, { "BT",        300, }, { "MSK",       300, },
    { "EAT",       300, }, { "IT",        330, }, { "ZP4",       400, },
    { "MSD",       300, }, { "ZP5",       500, }, { "IST",       530, },
    { "ZP6",       600, }, { "NOVST",     600, }, { "NST",       630, },
    { "JAVT",      700, }, { "CCT",       800, }, { "AWST",      800, },
    { "WST",       800, }, { "PHT",       800, }, { "JST",       900, },
    { "ROK",       900, }, { "ACST",      930, }, { "CAST",      930, },
    { "AEST",     1000, }, { "EAST",     1000, }, { "GST",      1000, },
    { "ACDT",     1030, }, { "CADT",     1030, }, { "AEDT",     1100, },
    { "EADT",     1100, }, { "IDLE",     1200, }, { "NZST",     1200, },
    { "NZT",      1200, }, { "NZDT",     1300, } };

const int num_timezones = sizeof( date_manip_timezones ) / sizeof( tz_map_t );

/**
 * ISO 639 language codes
 */
typedef struct {
    const char *code;
    const char *name;
} xmltv_locale;

static xmltv_locale locale_table[] = {
    {"AA", "Afar"},           {"AB", "Abkhazian"},      {"AF", "Afrikaans"},
    {"AM", "Amharic"},        {"AR", "Arabic"},         {"AS", "Assamese"},
    {"AY", "Aymara"},         {"AZ", "Azerbaijani"},    {"BA", "Bashkir"},
    {"BE", "Byelorussian"},   {"BG", "Bulgarian"},      {"BH", "Bihari"},
    {"BI", "Bislama"},        {"BN", "Bengali"},        {"BO", "Tibetan"},
    {"BR", "Breton"},         {"CA", "Catalan"},        {"CO", "Corsican"},
    {"CS", "Czech"},          {"CY", "Welsh"},          {"DA", "Danish"},
    {"DE", "German"},         {"DZ", "Bhutani"},        {"EL", "Greek"},
    {"EN", "English"},        {"EO", "Esperanto"},      {"ES", "Spanish"},
    {"ET", "Estonian"},       {"EU", "Basque"},         {"FA", "Persian"},
    {"FI", "Finnish"},        {"FJ", "Fiji"},           {"FO", "Faeroese"},
    {"FR", "French"},         {"FY", "Frisian"},        {"GA", "Irish"},
    {"GD", "Gaelic"},         {"GL", "Galician"},       {"GN", "Guarani"},
    {"GU", "Gujarati"},       {"HA", "Hausa"},          {"HI", "Hindi"},
    {"HR", "Croatian"},       {"HU", "Hungarian"},      {"HY", "Armenian"},
    {"IA", "Interlingua"},    {"IE", "Interlingue"},    {"IK", "Inupiak"},
    {"IN", "Indonesian"},     {"IS", "Icelandic"},      {"IT", "Italian"},
    {"IW", "Hebrew"},         {"JA", "Japanese"},       {"JI", "Yiddish"},
    {"JW", "Javanese"},       {"KA", "Georgian"},       {"KK", "Kazakh"},
    {"KL", "Greenlandic"},    {"KM", "Cambodian"},      {"KN", "Kannada"},
    {"KO", "Korean"},         {"KS", "Kashmiri"},       {"KU", "Kurdish"},
    {"KY", "Kirghiz"},        {"LA", "Latin"},          {"LN", "Lingala"},
    {"LO", "Laothian"},       {"LT", "Lithuanian"},     {"LV", "Latvian"},
    {"MG", "Malagasy"},       {"MI", "Maori"},          {"MK", "Macedonian"},
    {"ML", "Malayalam"},      {"MN", "Mongolian"},      {"MO", "Moldavian"},
    {"MR", "Marathi"},        {"MS", "Malay"},          {"MT", "Maltese"},
    {"MY", "Burmese"},        {"NA", "Nauru"},          {"NE", "Nepali"},
    {"NL", "Dutch"},          {"NO", "Norwegian"},      {"OC", "Occitan"},
    {"OM", "Oromo"},          {"OR", "Oriya"},          {"PA", "Punjabi"},
    {"PL", "Polish"},         {"PS", "Pashto"},         {"PT", "Portuguese"},
    {"QU", "Quechua"},        {"RM", "Rhaeto-Romance"}, {"RN", "Kirundi"},
    {"RO", "Romanian"},       {"RU", "Russian"},        {"RW", "Kinyarwanda"},
    {"SA", "Sanskrit"},       {"SD", "Sindhi"},         {"SG", "Sangro"},
    {"SH", "Serbo-Croatian"}, {"SI", "Singhalese"},     {"SK", "Slovak"},
    {"SL", "Slovenian"},      {"SM", "Samoan"},         {"SN", "Shona"},
    {"SO", "Somali"},         {"SQ", "Albanian"},       {"SR", "Serbian"},
    {"SS", "Siswati"},        {"ST", "Sesotho"},        {"SU", "Sudanese"},
    {"SV", "Swedish"},        {"SW", "Swahili"},        {"TA", "Tamil"},
    {"TE", "Tegulu"},         {"TG", "Tajik"},          {"TH", "Thai"},
    {"TI", "Tigrinya"},       {"TK", "Turkmen"},        {"TL", "Tagalog"},
    {"TN", "Setswana"},       {"TO", "Tonga"},          {"TR", "Turkish"},
    {"TS", "Tsonga"},         {"TT", "Tatar"},          {"TW", "Twi"},
    {"UK", "Ukrainian"},      {"UR", "Urdu"},           {"UZ", "Uzbek"},
    {"VI", "Vietnamese"},     {"VO", "Volapuk"},        {"WO", "Wolof"},
    {"XH", "Xhosa"},          {"YO", "Yoruba"},         {"ZH", "Chinese"},
    {"ZU", "Zulu"} };

const int num_locales = sizeof( locale_table ) / sizeof( xmltv_locale );

/**
 * Timezone parsing code based loosely on the algorithm in
 * filldata.cpp of MythTV.
 */
static time_t parse_xmltv_timezone( const char *tzstr )
{
    if( strlen( tzstr ) == 5 && (tzstr[ 0 ] == '+' || tzstr[ 0 ] == '-') ) {
        char hour[ 3 ];
        int result;

        hour[ 0 ] = tzstr[ 1 ];
        hour[ 1 ] = tzstr[ 2 ];
        hour[ 2 ] = 0;

        result = (3600 * atoi( hour )) + (60 * atoi( tzstr + 3 ));

        return (tzstr[ 0 ] == '-') ? -result : result;
    } else {
        int i;

        for( i = 0; i < num_timezones; i++ ) {
            if( !strcasecmp( tzstr, date_manip_timezones[ i ].name ) ) {
                return 60 * ((date_manip_timezones[ i ].offset % 100) +
                             ((date_manip_timezones[ i ].offset / 100) * 60));
            }
        }
    }

    return 0;
}

static time_t parse_xmltv_date( const char *date )
{
    char syear[ 6 ];
    char smonth[ 3 ];
    char sday[ 3 ];
    char shour[ 3 ];
    char smin[ 3 ];
    char ssec[ 3 ];
    int len;
    struct tm tm_obj;
    time_t tz;
    const char *tzstring;
    /*
     * For some reason, mktime() accepts broken-time arguments as localtime,
     * and there is no corresponding UTC function. *Sigh*.
     * For this reason we have to calculate the offset from GMT to adjust the
     * argument given to mktime().
     */
    time_t now = time( 0 );
    long gmtoff = localtime( &now )->tm_gmtoff;

    memset( syear, 0, sizeof( syear ) );
    memset( smonth, 0, sizeof( smonth ) );
    memset( sday, 0, sizeof( sday ) );
    memset( shour, 0, sizeof( shour ) );
    memset( smin, 0, sizeof( smin ) );
    memset( ssec, 0, sizeof( ssec ) );

    /*
     * according to the xmltv dtd:
     *
     * All dates and times in this [the xmltv] DTD follow the same format,
     * loosely based on ISO 8601.  They can be 'YYYYMMDDhhmmss' or some
     * initial substring, for example if you only know the year and month you
     * can have 'YYYYMM'.  You can also append a timezone to the end; if no
     * explicit timezone is given, UT is assumed.  Examples:
     * '200007281733 BST', '200209', '19880523083000 +0300'.  (BST == +0100.)
     *
     * thus:
     * example *date = "20031022220000 +0200"
     * type:            YYYYMMDDhhmmss ZZzzz"
     * position:        0         1         2          
     *                  012345678901234567890
     *
     * note: since part of the time specification can be omitted, we cannot
     *       hard-code the offset to the timezone!
     */

    /* Find where the timezone starts */
    tzstring = strchr( date, ' ' );
    if( tzstring ) {
        /* Calculate the length of the date */
        len = tzstring - date;
        /* Parse the timezone, skipping the initial space */
        tz = parse_xmltv_timezone( tzstring + 1 );
    } else {
        /* No timezone present */
        len = strlen( date );
        /* Assume UT */
        tz = 0;
    }

    if( len >= 4 ) memcpy( syear, date, 4 );
    if( len >= 6 ) memcpy( smonth, date + 4, 2 );
    if( len >= 8 ) memcpy( sday, date + 6, 2 );
    if( len >= 10 ) memcpy( shour, date + 8, 2 );
    if( len >= 12 ) memcpy( smin, date + 10, 2 );
    if( len >= 14 ) memcpy( ssec, date + 12, 2 );

    tm_obj.tm_sec = atoi( ssec ) - tz + gmtoff;
    tm_obj.tm_min = atoi( smin );
    tm_obj.tm_hour = atoi( shour );
    tm_obj.tm_mday = atoi( sday );
    tm_obj.tm_mon = atoi( smonth ) - 1;
    tm_obj.tm_year = atoi( syear ) - 1900;
    tm_obj.tm_isdst = -1;

    return mktime( &tm_obj );
}

static void reinit_program( program_t *pro, xmlNodePtr cur, xmlChar *locale )
{
    if( cur ) {
        xmlChar *start = xmlGetProp( cur, BAD_CAST "start" );
        xmlChar *stop = xmlGetProp( cur, BAD_CAST "stop" );
        xmlChar *lang;
        time_t start_time = 0;
        time_t end_time = 0;

        if( start ) {
            start_time = parse_xmltv_date( (char *) start );
            xmlFree( start );
        }
        if( stop ) {
            end_time = parse_xmltv_date( (char *) stop );
            xmlFree( stop );
        } else {
            end_time = start_time + 1800;
        }
        pro->start_time = start_time;
        pro->end_time = end_time;

        cur = cur->xmlChildrenNode;
        while( cur ) {
            lang = xmlGetProp( cur, BAD_CAST "lang");
            if( !xmlStrcasecmp( cur->name, BAD_CAST "title" ) ) {
                if( !lang || !locale ) {
                    if ( pro->title ) xmlFree ( pro->title );
                    pro->title = xmlNodeGetContent( cur );
                } else if( !xmlStrncasecmp( lang, locale, 2 ) ) {
                    if ( pro->title_local ) xmlFree ( pro->title_local );
                    pro->title_local = xmlNodeGetContent( cur );
                } else if( !pro->title ) {
                    pro->title = xmlNodeGetContent( cur );
                }
            } else if( !xmlStrcasecmp( cur->name, BAD_CAST "sub-title" ) ) {
                if( !lang || !locale ) {
                    if ( pro->subtitle ) xmlFree ( pro->subtitle );
                    pro->subtitle = xmlNodeGetContent( cur );
                } else if( !xmlStrncasecmp( lang, locale, 2 ) ) {
                    if ( pro->subtitle_local )
                        xmlFree ( pro->subtitle_local );
                    pro->subtitle_local = xmlNodeGetContent( cur );
                } else if( !pro->subtitle ) {
                    pro->subtitle = xmlNodeGetContent( cur );
                }
            } else if( !xmlStrcasecmp( cur->name, BAD_CAST "desc" ) ) {
                if( !lang || !locale ) {
                    if ( pro->description ) xmlFree ( pro->description );
                    pro->description = xmlNodeGetContent( cur );
                } else if( !xmlStrncasecmp( lang, locale, 2 ) ) {
                    if ( pro->description_local )
                        xmlFree ( pro->description_local );
                    pro->description_local = xmlNodeGetContent( cur );
                } else if( !pro->description ) {
                    pro->description = xmlNodeGetContent( cur );
                }
            }
            if( lang ) xmlFree( lang );
            cur = cur->next;
        }
    } else {
         pro->title = pro->subtitle = pro->description = 0;
         pro->title_local = pro->subtitle_local = pro->description_local = 0;
         pro->start_time = 0;
         pro->end_time = 0;
    }
}

static program_t *program_new( void )
{
    program_t *pro = malloc( sizeof( program_t ) );
    reinit_program( pro, 0, 0 );
    return pro;
}

static xmlNodePtr get_program( xmlNodePtr root, const char *channelid,
                               time_t time )
{
    xmlNodePtr cur = root->xmlChildrenNode;
    while( cur ) {
        if( !xmlStrcasecmp( cur->name, BAD_CAST "programme" ) ) {
            xmlChar *channel = xmlGetProp( cur, BAD_CAST "channel" );
            if( channel && !xmlStrcasecmp( channel, BAD_CAST channelid ) ) {
                xmlChar *start = xmlGetProp( cur, BAD_CAST "start" );
                if( start ) {
                    time_t start_time;
                    time_t end_time = 0;
                    start_time = parse_xmltv_date( (char *) start );

                    if( start_time <= time ) {
                        xmlChar *stop = xmlGetProp( cur, BAD_CAST "stop" );
                        if( stop ) {
                            end_time = parse_xmltv_date( (char *) stop );
                            xmlFree( stop );
                        } else {
                            /* Set to half an hour later. */
                            end_time = start_time + 1800;
                        }
                        if( end_time > time ) {
                            xmlFree( start );
                            xmlFree( channel );
                            return cur;
                        }
                    }
                    xmlFree( start );
                }
            }
            if( channel ) xmlFree( channel );
        }
        cur = cur->next;
    }
    return 0;
}

static int xmltv_is_tv_grab_na( xmltv_t *xmltv )
{
    xmlNodePtr cur = xmltv->root->xmlChildrenNode;

    while( cur ) {
        if( !xmlStrcasecmp( cur->name, BAD_CAST "channel" ) ) {
            xmlChar *id = xmlGetProp( cur, BAD_CAST "id" );
            int is_na = 0;

            if( strstr( (char *) id, ".zap2it.com" ) ) {
                is_na = 1;
            }
            xmlFree( id );
            return is_na;
        }
        cur = cur->next;
    }

    return 0;
}

static language_t *xmltv_add_language( xmltv_t *xmltv, const xmlChar *lang )
{
    language_t *last = 0;
    language_t *l = xmltv->languages;
    int i;

    if( xmltv->num_languages >= MAX_LANGUAGES ) return 0;

    while( l ) {
        i = xmlStrncasecmp( l->code, lang, 2 );
        if( i == 0 ) {
            return l;
        } else if( i < 0 ) {
            last = l;
            l = l->next;
        } else {
            break;
        }
    }

    xmltv->num_languages++;
    l = malloc( sizeof( language_t ) );
    if( !l ) return 0;
    if( last ) {
        l->next = last->next;
        last->next = l;
    } else {
        l->next = xmltv->languages;
        xmltv->languages = l;
    }
    for( i=0; i<2 && isalpha(lang[i]); i++ ) {
        l->code[i] = tolower( lang[i] );
    }
    l->code[i] = '\0';

    for( i=0; i<num_locales; i++ ) {
        if( !xmlStrncasecmp( lang, BAD_CAST locale_table[i].code, 2 ) ) {
            l->name = locale_table[i].name;
            return l;
        }
    }
    l->name = 0;
    return l;
}

static int xmltv_find_languages( xmlNodePtr node, xmltv_t *xmltv )
{
    xmlNodePtr cur = node->xmlChildrenNode;
    while( cur ) {
        xmlChar *c = xmlGetProp( cur, BAD_CAST "lang" );
        if( c ) xmltv_add_language( xmltv, c );
        xmltv_find_languages( cur, xmltv );
        cur = cur->next;
    }
    return 0;
}

xmltv_t *xmltv_new( const char *filename, const char *locale )
{
    xmltv_t *xmltv = malloc( sizeof( xmltv_t ) );

    if( !xmltv ) {
        return 0;
    }

    xmltv->doc = xmlParseFile( filename );
    if( !xmltv->doc ) {
        fprintf( stderr, "xmltv: Can't open file %s.\n", filename );
        free( xmltv );
        return 0;
    }

    xmltv->root = xmlDocGetRootElement( xmltv->doc );
    if( !xmltv->root ) {
        fprintf( stderr, "xmltv: Empty XML document, "
                         "%s is not a valid xmltv file.\n",
                filename );
        xmlFreeDoc( xmltv->doc );
        free( xmltv );
        return 0;
    }

    if( xmlStrcmp( xmltv->root->name, BAD_CAST "tv" ) ) {
        fprintf( stderr, "xmltv: Root node is not <tv>, "
                         "%s is not a valid xmltv file.\n",
                 filename );
        xmlFreeDoc( xmltv->doc );
        free( xmltv );
        return 0;
    }

    xmltv->pro = program_new();
    xmltv->next_pro = program_new();
    xmltv->curchan = 0;
    xmltv->display_chan = 0;
    xmltv->refresh = 1;
    xmltv->is_tv_grab_na = xmltv_is_tv_grab_na( xmltv );

    xmltv->num_languages = 0;
    xmltv->languages = 0;
    if( locale ) {
        xmltv->locale = xmltv_add_language( xmltv, BAD_CAST locale );
    } else {
        xmltv->locale = 0;
    }
    xmltv_find_languages( xmltv->root, xmltv );

    return xmltv;
}

static void program_delete( program_t *pro )
{
    if( pro->title ) xmlFree( pro->title );
    if( pro->subtitle ) xmlFree( pro->subtitle );
    if( pro->description ) xmlFree( pro->description );
    reinit_program( pro, 0, 0 );
    free( pro );
}

static void languages_delete( language_t *languages )
{
    language_t *cur, *next;
    cur = languages;
    while( cur ) {
        next = cur->next;
        free( cur );
        cur = next;
    }
}

void xmltv_delete( xmltv_t *xmltv )
{
    program_delete( xmltv->pro );
    program_delete( xmltv->next_pro );
    if( xmltv->curchan ) xmlFree( xmltv->curchan );
    if( xmltv->display_chan ) xmlFree( xmltv->display_chan );
    xmlFreeDoc( xmltv->doc );
    languages_delete( xmltv->languages );
    free( xmltv );
}

void xmltv_set_channel( xmltv_t *xmltv, const char *channel )
{
    if( channel ) {
        snprintf( xmltv->curchannel, sizeof( xmltv->curchannel ),
                  "%s", channel );
    } else {
        *xmltv->curchannel = '\0';
    }
    xmltv->refresh = 1;
}

void xmltv_refresh( xmltv_t *xmltv )
{
    time_t curtime = time( 0 );

    if( xmltv->pro->title ) xmlFree( xmltv->pro->title );
    if( xmltv->pro->subtitle ) xmlFree( xmltv->pro->subtitle );
    if( xmltv->pro->description ) xmlFree( xmltv->pro->description );
    reinit_program( xmltv->pro, 0, 0 );

    if( xmltv->next_pro->title ) xmlFree( xmltv->next_pro->title );
    if( xmltv->next_pro->subtitle ) xmlFree( xmltv->next_pro->subtitle );
    if( xmltv->next_pro->description ) xmlFree( xmltv->next_pro->description );
    reinit_program( xmltv->next_pro, 0, 0 );

    if( *xmltv->curchannel ) {
        xmlNodePtr program_node = 0;
        program_node = get_program( xmltv->root, xmltv->curchannel, curtime );
        reinit_program( xmltv->pro, program_node, xmltv->locale->code );

        if( program_node ) {
            /* If we found a program, lookup what's on next. */
            program_node = get_program( xmltv->root, xmltv->curchannel,
                                        xmltv->pro->end_time );
            reinit_program( xmltv->next_pro, program_node, xmltv->locale->code );
        } else {
            /* We found no program, schedule a check in a half hour. */
            xmltv->pro->end_time = curtime + 1800;
        }
    }
    xmltv->refresh = 0;
}

const char *xmltv_get_title( xmltv_t *xmltv )
{
    if ( xmltv->pro->title_local ) {
        return (char *) xmltv->pro->title_local;
    }
    return (char *) xmltv->pro->title;
}

const char *xmltv_get_sub_title( xmltv_t *xmltv )
{
    if ( xmltv->pro->subtitle_local ) {
        return (char *) xmltv->pro->subtitle_local;
    }
    return (char *) xmltv->pro->subtitle;
}

const char *xmltv_get_description( xmltv_t *xmltv )
{
    if ( xmltv->pro->description_local ) {
        return (char *) xmltv->pro->description_local;
    }
    return (char *) xmltv->pro->description;
}

time_t xmltv_get_start_time( xmltv_t *xmltv )
{
    return xmltv->pro->start_time;
}

time_t xmltv_get_end_time( xmltv_t *xmltv )
{
    return xmltv->pro->end_time;
}

const char *xmltv_get_next_title( xmltv_t *xmltv )
{
    if ( xmltv->next_pro->title_local ) {
        return (char *) xmltv->next_pro->title_local;
    }
    return (char *) xmltv->next_pro->title;
}

const char *xmltv_get_channel( xmltv_t *xmltv )
{
    return (char *) xmltv->curchannel;
}

int xmltv_needs_refresh( xmltv_t *xmltv )
{
    time_t curtime = time( 0 );

    return xmltv->refresh || (curtime >= xmltv->pro->end_time);
}

const char *xmltv_lookup_channel( xmltv_t *xmltv, const char *name )
{
    xmlNodePtr cur = xmltv->root->xmlChildrenNode;

    if( xmltv->curchan ) xmlFree( xmltv->curchan );
    xmltv->curchan = 0;

    while( cur ) {
        if( !xmlStrcasecmp( cur->name, BAD_CAST "channel" ) ) {
            xmlNodePtr sub = cur->xmlChildrenNode;
            while( sub ) {
                if( !xmlStrcasecmp( sub->name, BAD_CAST "display-name" ) ) {
                    xmlChar *curname = xmlNodeGetContent( sub );
                    if( curname ) {
                        if( !xmlStrcasecmp( curname, BAD_CAST name ) ) {
                            xmltv->curchan = xmlGetProp( cur, BAD_CAST "id" );
                            xmlFree( curname );
                            return (char *) xmltv->curchan;
                        }
                        xmlFree( curname );
                    }
                }
                sub = sub->next;
            }
        }
        cur = cur->next;
    }

    return 0;
}

static const char *tv_grab_na_skip( const char *name )
{
    const char *ret = name;
    if( *ret == 'C' ) {
        while( *ret && *ret != ' ' ) ret++;
        if( *ret ) ret++;
    }

    while( *ret && *ret != ' ' ) ret++;
    if( *ret ) ret++;

    return ret;
}

const char *xmltv_lookup_channel_name( xmltv_t *xmltv, const char *id )
{
    xmlNodePtr cur = xmltv->root->xmlChildrenNode;

    if( xmltv->display_chan ) xmlFree( xmltv->display_chan );
    xmltv->display_chan = 0;

    while( cur ) {
        if( !xmlStrcasecmp( cur->name, BAD_CAST "channel" ) ) {
            xmlChar *curid = xmlGetProp( cur, BAD_CAST "id" );
            if ( curid ) {
                if ( !xmlStrcasecmp( curid, BAD_CAST id ) ) {
                    xmlNodePtr sub = cur->xmlChildrenNode;
                    while( sub && xmlStrcasecmp( sub->name, BAD_CAST "display-name" ) ) {
                        sub = sub->next;
                    }
                    if ( sub ) {
                        xmltv->display_chan = xmlNodeGetContent( sub );
                        xmlFree( curid );
                        if( xmltv->is_tv_grab_na ) {
                            return tv_grab_na_skip( (char *) xmltv->display_chan );
                        } else {
                            return (char *) xmltv->display_chan;
                        }
                    }
                }
                xmlFree( curid );
            }
        }
        cur = cur->next;
    }

    return 0;
}

void xmltv_set_language( xmltv_t *xmltv, const char *locale )
{
    language_t *l;
    xmltv->refresh = 1;
    if( !locale ) {
        xmltv->locale = 0;
        return;
    }
    l = xmltv_add_language( xmltv, BAD_CAST locale );
    if( l ) xmltv->locale = l;
}

const char *xmltv_get_language( xmltv_t *xmltv )
{
    if( xmltv->locale ) return (char *) xmltv->locale->code;
    return 0;
}

int xmltv_get_num_languages( xmltv_t *xmltv )
{
    return xmltv->num_languages;
}

void xmltv_select_language( xmltv_t *xmltv, int n )
{
    int i;
    if( n > xmltv->num_languages ) return;
    xmltv->refresh = 1;
    if( n == 0 ) {
        xmltv->locale = 0;
        return;
    }
    xmltv->locale = xmltv->languages;
    for( i=1; i<n; i++ ) xmltv->locale = xmltv->locale->next;
}

int xmltv_get_langnum( xmltv_t *xmltv )
{
    language_t *l = xmltv->languages;
    int i = 1;
    if( !xmltv->locale ) return 0;
    while( l ) {
        if( l == xmltv->locale ) return i;
        l = l->next;
        i++;
    }
    return 0; /* should never happen */
}

const char *xmltv_get_language_code( xmltv_t *xmltv, int n )
{
    language_t *l = xmltv->languages;
    int i;
    if( n==0 || n>xmltv->num_languages ) return 0;
    for( i=1; i<n; i++ ) l = l->next;
    return (char *) l->code;
}

const char *xmltv_get_language_name( xmltv_t *xmltv, int n )
{
    language_t *l = xmltv->languages;
    int i;
    if( n==0 || n>xmltv->num_languages ) return 0;
    for( i=1; i<n; i++ ) l = l->next;
    return (char *) l->name;
}

