/**
 * Copyright (c) 2003 Aleander Belov <asbel@mail.ru>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
 
/* For saving configuration in file use function 
   int configsave(const char *INIT_name, const char *INIT_val, const int INIT_num).
   
   INIT_name - Parameter name
   INIT_val - Parameter value
   INIT_num - number of parameter (f.e. for key_quit)
   
   look at the end of file for example and change 
   #define CFGFILE "/home/asbel/.tvtime/tvtimerc".
   for compile use gcc configsave.c && ./a.out
*/

#include <string.h>
#include <libxml/parser.h>

char *configFile;
xmlDocPtr Doc;

xmlNodePtr find_node( const char *str, xmlNodePtr node)
{
    while(node != NULL) {
        if(!xmlStrcasecmp(node->name, (const xmlChar *)str)) return node;
        node=node->next;
    }

    return NULL;
}

/* Attempt to parse the file for key elements and create them if they don't exist */
int configsave_open(const char *filename)
{
    xmlNodePtr top, node;

    if( filename == NULL) return 0;
    free(configFile);
    configFile = strdup(filename);

    if( (Doc=xmlParseFile(configFile)) == NULL)
        if( (Doc=xmlNewDoc((const xmlChar *)"1.0")) == NULL) {
            fprintf(stderr,"configsave: Could not create new Doc element.\n");
            free(configFile);
            return 0;
         }

    if( (top=xmlDocGetRootElement(Doc)) == NULL) {
        if( (top=xmlNewDocNode(Doc, NULL, (const xmlChar *) "Conf", NULL)) == NULL) {
            fprintf(stderr,"configsave: Could not create toplevel element 'Conf'.\n");
            xmlFreeDoc(Doc);
            free(configFile);
            return 0;
        } else {
            xmlDocSetRootElement(Doc, top);
        }
    }

    if( xmlStrcasecmp(top->name, (const xmlChar *) "Conf")) {
        fprintf(stderr, "configsave: Root node in file %s should be 'Conf'.\n", configFile);
        xmlFreeDoc(Doc);
        free(configFile);
        return 0;
    }

    if( (node=find_node("global", top->xmlChildrenNode)) == NULL)
        if( (node=xmlNewTextChild(top, NULL, (const xmlChar *) "global", NULL)) == NULL) {
            fprintf(stderr,"configsave: Could not create element 'global'.\n");
            xmlFreeDoc(Doc);
            free(configFile);
            return 0;
        }

    if( (node=find_node("mousebindings", top->xmlChildrenNode)) == NULL)
        if( (node=xmlNewTextChild(top, NULL, (const xmlChar *) "mousebindings", NULL)) == NULL) {
            fprintf(stderr,"configsave: Could not create element 'mousebindings'.\n");
            xmlFreeDoc(Doc);
            free(configFile);
            return 0;
        }

    if( (node=find_node("keybindings", top->xmlChildrenNode)) == NULL)
        if( (node=xmlNewTextChild(top, NULL, (const xmlChar *) "keybindings", NULL)) == NULL) {
            fprintf(stderr,"configsave: Could not create element 'keybindings'.\n");
            xmlFreeDoc(Doc);
            free(configFile);
            return 0;
        }

    xmlKeepBlanksDefault(0);
    xmlSaveFormatFile(configFile, Doc, 1);
    return 1;
}

void configsave_close(void)
{
xmlFreeDoc(Doc);
}

int configsave(const char *INIT_name, const char *INIT_val, const int INIT_num)
{
    xmlNodePtr top, section, node;
    xmlAttrPtr attr;

    if((configFile == NULL) || (Doc == NULL)) return 0;
    top=xmlDocGetRootElement(Doc);

    if( !xmlStrcasecmp((const xmlChar *)INIT_name, (const xmlChar *)"keybindings") ) {
        if( (section=find_node("keybindings", top->xmlChildrenNode)) == NULL) {
            fprintf(stderr, "configsave: No 'keybindings' section in %s. KeyBindings not saved.\n",configFile);
            return 0;
        }

    } else if (!xmlStrcasecmp((const xmlChar *)INIT_name, (const xmlChar *)"mousebindings")) {
        if( (section=find_node("mousebindings", top->xmlChildrenNode)) == NULL) {
            fprintf(stderr, "configsave: No 'mousebindings' section in %s. MouseBindings not saved.\n",configFile);
            return 0;
        }

    } else {
        if( (section=find_node("global", top->xmlChildrenNode)) == NULL) {
            fprintf(stderr, "configsave: No 'global' section in %s. Global option not saved.\n",configFile);
            return 0;
        }
        if( (node=find_node(INIT_name, section->xmlChildrenNode)) == NULL) {
            node=xmlNewTextChild(section,NULL, (const xmlChar *)INIT_name, NULL);
            attr=xmlNewProp(node, (const xmlChar *) "value", (const xmlChar *)INIT_val);
        } else {
            xmlSetProp(node, (const xmlChar *) "value", (const xmlChar *)INIT_val);
        }
    }

    xmlKeepBlanksDefault(0);
    xmlSaveFormatFile(configFile, Doc, 1);
    return 1;
}
