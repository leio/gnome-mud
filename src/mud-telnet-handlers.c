/* GNOME-Mud - A simple Mud CLient
 * Copyright (C) 1998-2006 Robin Ericsson <lobbin@localhost.nu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* Code originally from wxMUD. Converted to a GObject by Les Harris.
 * wxMUD - an open source cross-platform MUD client.
 * Copyright (C) 2003-2008 Mart Raudsepp and Gabriel Levin
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <gnet.h>
#include <string.h>

#include "gnome-mud.h"
#include "mud-telnet.h"
#include "mud-telnet-handlers.h"
#include "mud-telnet-zmp.h"

#ifdef ENABLE_GST
#include "mud-telnet-msp.h"
#endif

#ifdef ENABLE_MCCP
#include <zlib.h>
#include <stdlib.h>
#include "mud-telnet-mccp.h"
#endif

/* TTYPE */

void
MudHandler_TType_Enable(MudTelnet *telnet, MudTelnetHandler *handler)
{
    handler->enabled = TRUE;
    telnet->ttype_iteration = 0;
}

void
MudHandler_TType_Disable(MudTelnet *telnet, MudTelnetHandler *handler)
{
    handler->enabled = FALSE;
}

void
MudHandler_TType_HandleSubNeg(MudTelnet *telnet, guchar *buf,
        guint len, MudTelnetHandler *handler)
{
    switch(telnet->ttype_iteration)
    {
        case 0:
            mud_telnet_send_sub_req(telnet, 11,
                    (guchar)TELOPT_TTYPE,
                    (guchar)TEL_TTYPE_IS,
                    'g','n','o','m','e','-','m','u','d');
            telnet->ttype_iteration++;
            break;

        case 1:
            mud_telnet_send_sub_req(telnet, 7,
                    (guchar)TELOPT_TTYPE,
                    (guchar)TEL_TTYPE_IS,
                    'x','t','e','r','m');
            telnet->ttype_iteration++;
            break;

        case 2:
            mud_telnet_send_sub_req(telnet, 6,
                    (guchar)TELOPT_TTYPE,
                    (guchar)TEL_TTYPE_IS,
                    'a','n','s','i');
            telnet->ttype_iteration++;
            break;

        case 3:
            mud_telnet_send_sub_req(telnet, 9,
                    (guchar)TELOPT_TTYPE,
                    (guchar)TEL_TTYPE_IS,
                    'U','N','K','N','O','W','N');
            telnet->ttype_iteration++;
            break;

        /* RFC 1091 says we need to repeat the last type */
        case 4:
            mud_telnet_send_sub_req(telnet, 9,
                    (guchar)TELOPT_TTYPE,
                    (gchar)TEL_TTYPE_IS,
                    'U','N','K','N','O','W','N');
            telnet->ttype_iteration = 0;
            break;

    }
}

/* NAWS */
void
MudHandler_NAWS_Enable(MudTelnet *telnet, MudTelnetHandler *handler)
{
    gint w, h;

    mud_telnet_get_parent_size(telnet, &w, &h);
    mud_telnet_set_parent_naws(telnet, TRUE);

    handler->enabled = TRUE;

    mud_telnet_send_naws(telnet, w, h);
}

void
MudHandler_NAWS_Disable(MudTelnet *telnet, MudTelnetHandler *handler)
{
    handler->enabled = FALSE;
    mud_telnet_set_parent_naws(telnet, FALSE);
}

void
MudHandler_NAWS_HandleSubNeg(MudTelnet *telnet, guchar *buf,
        guint len, MudTelnetHandler *handler)
{
    return;
}

/* ECHO */
void
MudHandler_ECHO_Enable(MudTelnet *telnet, MudTelnetHandler *handler)
{
    mud_telnet_set_local_echo(telnet, FALSE);
}

void
MudHandler_ECHO_Disable(MudTelnet *telnet, MudTelnetHandler *handler)
{
    mud_telnet_set_local_echo(telnet, TRUE);
}

void
MudHandler_ECHO_HandleSubNeg(MudTelnet *telnet, guchar *buf,
        guint len, MudTelnetHandler *handler)
{
    return;
}

/* EOR */
void
MudHandler_EOR_Enable(MudTelnet *telnet, MudTelnetHandler *handler)
{
    telnet->eor_enabled = TRUE;
}

void
MudHandler_EOR_Disable(MudTelnet *telnet, MudTelnetHandler *handler)
{
    telnet->eor_enabled = FALSE;
}

void
MudHandler_EOR_HandleSubNeg(MudTelnet *telnet, guchar *buf,
        guint len, MudTelnetHandler *handler)
{
    return;
}

/* CHARSET */
void
MudHandler_CHARSET_Enable(MudTelnet *telnet, MudTelnetHandler *handler)
{
    handler->enabled = TRUE;
}

void
MudHandler_CHARSET_Disable(MudTelnet *telnet, MudTelnetHandler *handler)
{
    handler->enabled = FALSE;
    mud_telnet_set_parent_remote_encode(telnet, FALSE, NULL);
}

void
MudHandler_CHARSET_HandleSubNeg(MudTelnet *telnet, guchar *buf,
        guint len, MudTelnetHandler *handler)
{
    gint index = 0;
    guchar sep;
    guchar tbuf[9];
    gchar sep_buf[2];
    GString *encoding;
    gchar **encodings;

    switch(buf[index])
    {
        case TEL_CHARSET_REQUEST:
            // Check for [TTABLE] and
            // reject if found.
            memcpy(&buf[1], tbuf, 8);
            tbuf[8] = '\0';

            if(strcmp((gchar *)tbuf, "[TTABLE]") == 0)
            {
                mud_telnet_send_sub_req(telnet, 2,
                        (guchar)TELOPT_CHARSET,
                        (guchar)TEL_CHARSET_TTABLE_REJECTED);
                return;
            }

            sep = buf[++index];
            index++;

            encoding = g_string_new(NULL);

            while(buf[index] != (guchar)TEL_SE)
                encoding = g_string_append_c(encoding, buf[index++]);

            sep_buf[0] = (gchar)sep;
            sep_buf[1] = '\0';
            encodings = g_strsplit(encoding->str, sep_buf, -1);

            // We are using VTE's locale fallback function
            // to handle a charset we do not support so we
            // just take the first returned and use it.

            if(g_strv_length(encodings) != 0)
                mud_telnet_set_parent_remote_encode(telnet, TRUE, encodings[0]);

            mud_telnet_send_charset_req(telnet, encodings[0]);

            g_string_free(encoding, TRUE);
            g_strfreev(encodings);

            break;
    }
}

/* ZMP */
void
MudHandler_ZMP_Enable(MudTelnet *telnet, MudTelnetHandler *handler)
{
    handler->enabled = TRUE;
    mud_zmp_init(telnet);
}

void
MudHandler_ZMP_Disable(MudTelnet *telnet, MudTelnetHandler *handler)
{
    /* Cannot disable ZMP once enabled per specification */
    return;
}

void
MudHandler_ZMP_HandleSubNeg(MudTelnet *telnet, guchar *buf,
        guint len, MudTelnetHandler *handler)
{
    gchar command_buf[1024];
    gint count = 0;
    gint index = 0;
    GString *args = g_string_new(NULL);
    gchar **argv;
    gint argc;
    MudZMPFunction zmp_handler = NULL;

    while(buf[count] != '\0' && count < len)
        command_buf[index++] = buf[count++];
    command_buf[index] = '\0';

    while(count < len - 1)
    {
        if(buf[count] == '\0')
        {
            args = g_string_append(args,"|gmud_sep|");
            count++;
            continue;
        }

        args = g_string_append_c(args, buf[count++]);
    }

    args = g_string_prepend(args, command_buf);

    argv = g_strsplit(args->str, "|gmud_sep|", -1);
    argc = g_strv_length(argv);

    g_log("Telnet", G_LOG_LEVEL_MESSAGE, "Received: ZMP Command: %s", command_buf);
    for(count = 0; count < argc - 1; ++count)
        g_log("Telnet", G_LOG_LEVEL_MESSAGE, "\t%s", argv[count]);

    if(mud_zmp_has_command(telnet, command_buf))
    {
        zmp_handler = mud_zmp_get_function(telnet, command_buf);

        if(zmp_handler)
            zmp_handler(telnet, argc, argv);
        else
            g_warning("NULL ZMP functioned returned.");
    }

    g_strfreev(argv);
    g_string_free(args, TRUE);

}

#ifdef ENABLE_GST
/* MSP */
void
MudHandler_MSP_Enable(MudTelnet *telnet, MudTelnetHandler *handler)
{
    handler->enabled = TRUE;
    mud_telnet_msp_init(telnet);
    telnet->msp_parser.enabled = TRUE;
}

void
MudHandler_MSP_Disable(MudTelnet *telnet, MudTelnetHandler *handler)
{
    handler->enabled = FALSE;
}

void
MudHandler_MSP_HandleSubNeg(MudTelnet *telnet, guchar *buf,
        guint len, MudTelnetHandler *handler)
{
    return;
}
#endif

#ifdef ENABLE_MCCP
void
MudHandler_MCCP_Enable(MudTelnet *telnet, MudTelnetHandler *handler)
{
    handler->enabled = TRUE;
    telnet->mccp = FALSE;
}

void
MudHandler_MCCP_Disable(MudTelnet *telnet, MudTelnetHandler *handler)
{
    handler->enabled = FALSE;
    telnet->mccp = FALSE;

    if (telnet->compress_out != NULL)
    {
        inflateEnd(telnet->compress_out);

        g_free(telnet->compress_out);
        g_free(telnet->compress_out_buf);
        
        telnet->compress_out = NULL;
    }
}

void
MudHandler_MCCP_HandleSubNeg(MudTelnet *telnet, guchar *buf, 
        guint len, MudTelnetHandler *handler)
{
    telnet->mccp = TRUE;
    telnet->mccp_new = TRUE;

    telnet->compress_out = (z_stream *) calloc(1, sizeof(z_stream));
    telnet->compress_out_buf = (guchar *) calloc(4096, sizeof(guchar));

    telnet->compress_out->next_out = telnet->compress_out_buf;
    telnet->compress_out->avail_out = 4096;

    if(inflateInit(telnet->compress_out) != Z_OK)
    {
        g_critical("Failed to initialize compression.");

        g_free(telnet->compress_out);
        g_free(telnet->compress_out_buf);

        telnet->compress_out = NULL;
        telnet->compress_out_buf = NULL;

        mud_connection_view_disconnect(telnet->parent);
    }
}
#endif

