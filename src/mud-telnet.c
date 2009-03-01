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
#include <stdarg.h>
#include <string.h> // memset

#include "gnome-mud.h"
#include "mud-telnet.h"
#include "mud-telnet-handlers.h"
#include "mud-telnet-zmp.h"

#ifdef ENABLE_MCCP
#include "mud-telnet-mccp.h"
#endif

struct _MudTelnetPrivate
{
};

GType mud_telnet_get_type (void);
static void mud_telnet_init (MudTelnet *pt);
static void mud_telnet_class_init (MudTelnetClass *klass);
static void mud_telnet_finalize (GObject *object);

static void mud_telnet_send_iac(MudTelnet *telnet, guchar ch1, guchar ch2);
static void mud_telnet_on_handle_subnego(MudTelnet *telnet);
static void mud_telnet_on_enable_opt(MudTelnet *telnet,
				     const guchar opt_no, gint him);
static void mud_telnet_on_disable_opt(MudTelnet *telnet,
				      const guchar opt_no, gint him);
static guchar mud_telnet_get_telopt_state(guchar *storage, const guint bitshift);
static gint mud_telnet_get_telopt_queue(guchar *storage, const guint bitshift);
static void  mud_telnet_set_telopt_state(guchar *storage,
					 const enum TelnetOptionState state, const guint bitshift);
static gint mud_telnet_get_index_by_option(MudTelnet *telnet, guchar option_number);
static void mud_telnet_set_telopt_queue(guchar *storage,
					gint bit_on, const guint bitshift);

static gint 
mud_telnet_handle_positive_nego(MudTelnet *telnet,
				const guchar opt_no,
				const guchar affirmative,
				const guchar negative,
				gint him);
static gint
mud_telnet_handle_negative_nego(MudTelnet *telnet,
                                const guchar opt_no,
				const guchar affirmative,
				const guchar negative,
				gint him);


gchar *mud_telnet_get_telopt_string(guchar c);

// MudTelnet class functions
GType
mud_telnet_get_type (void)
{
    static GType object_type = 0;

    g_type_init();

    if (!object_type)
    {
        static const GTypeInfo object_info =
        {
            sizeof (MudTelnetClass),
            NULL,
            NULL,
            (GClassInitFunc) mud_telnet_class_init,
            NULL,
            NULL,
            sizeof (MudTelnet),
            0,
            (GInstanceInitFunc) mud_telnet_init,
        };

        object_type = g_type_register_static(G_TYPE_OBJECT, "MudTelnet", &object_info, 0);
    }

    return object_type;
}

static void
mud_telnet_init (MudTelnet *telnet)
{
    telnet->priv = g_new0(MudTelnetPrivate, 1);

    telnet->processed = g_string_new(NULL);

#ifdef ENABLE_GST
    telnet->prev_buffer = NULL;
    telnet->base_url = NULL;
#endif
}

static void
mud_telnet_class_init (MudTelnetClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = mud_telnet_finalize;
}

static void
mud_telnet_finalize (GObject *object)
{
    MudTelnet *telnet;
    GObjectClass *parent_class;

    telnet = MUD_TELNET(object);

    if(telnet->processed)
        g_string_free(telnet->processed, TRUE);

    if(telnet->buffer)
        g_string_free(telnet->buffer, TRUE);

    if(telnet->mud_name)
        g_free(telnet->mud_name);

#ifdef ENABLE_GST
    mud_telnet_msp_stop_playing(telnet, MSP_TYPE_SOUND);
    mud_telnet_msp_stop_playing(telnet, MSP_TYPE_MUSIC);
    
    if(telnet->prev_buffer)
        g_string_free(telnet->prev_buffer, TRUE);
    if(telnet->base_url)
        g_free(telnet->base_url);
#endif

#ifdef ENABLE_MCCP
    if(telnet->compress_out != NULL)
    {
        inflateEnd(telnet->compress_out);

        g_free(telnet->compress_out);
        g_free(telnet->compress_out_buf);
    }
#endif

    mud_zmp_finalize(telnet);

    g_free(telnet->priv);

    parent_class = g_type_class_peek_parent(G_OBJECT_GET_CLASS(object));
    parent_class->finalize(object);
}

/*** Public Methods ***/

// Instantiate MudTelnet
MudTelnet*
mud_telnet_new(MudConnectionView *parent, GConn *connection, gchar *mud_name)
{
    MudTelnet *telnet;

    telnet = g_object_new(MUD_TYPE_TELNET, NULL);

    telnet->parent = parent;
    telnet->conn = connection;
    telnet->tel_state = TEL_STATE_TEXT;
    telnet->ttype_iteration = 0;

    memset(telnet->telopt_states, 0, sizeof(telnet->telopt_states));
    memset(telnet->handlers, 0, sizeof(telnet->handlers));

    mud_telnet_register_handlers(telnet);

    telnet->eor_enabled = FALSE;

    telnet->mud_name = g_strdup(mud_name);
    telnet->buffer = NULL;
    telnet->pos = 0;
    telnet->subreq_pos = 0;

    telnet->zmp_commands = NULL;

#ifdef ENABLE_GST
    telnet->sound[0].files = NULL;
    telnet->sound[0].current_command = NULL;
    telnet->sound[0].playing = FALSE;
    telnet->sound[0].files_len = 0;

    telnet->sound[1].files = NULL;
    telnet->sound[1].current_command = NULL;
    telnet->sound[1].playing = FALSE;
    telnet->sound[1].files_len = 0;

    telnet->base_url = NULL;
    telnet->msp_parser.enabled = FALSE;

#endif

#ifdef ENABLE_MCCP
    telnet->mccp_new = TRUE;
#endif

    return telnet;
}

void
mud_telnet_register_handlers(MudTelnet *telnet)
{
    gint i;

    for(i = 0; i < TEL_HANDLERS_SIZE; ++i)
    {
        telnet->handlers[i].type = HANDLER_NONE;
        telnet->handlers[i].enabled = FALSE;
        telnet->handlers[i].instance = NULL;
        telnet->handlers[i].enable = NULL;
        telnet->handlers[i].disable = NULL;
        telnet->handlers[i].handle_sub_neg = NULL;
    }

    /* TTYPE */
    telnet->handlers[0].type = HANDLER_TTYPE;
    telnet->handlers[0].option_number = (guchar)TELOPT_TTYPE;
    telnet->handlers[0].enabled = TRUE;
    telnet->handlers[0].enable = MudHandler_TType_Enable;
    telnet->handlers[0].disable = MudHandler_TType_Disable;
    telnet->handlers[0].handle_sub_neg = MudHandler_TType_HandleSubNeg;
    
    /* NAWS */
    telnet->handlers[1].type = HANDLER_NAWS;
    telnet->handlers[1].option_number = (guchar)TELOPT_NAWS;
    telnet->handlers[1].enabled = TRUE;
    telnet->handlers[1].enable = MudHandler_NAWS_Enable;
    telnet->handlers[1].disable = MudHandler_NAWS_Disable;
    telnet->handlers[1].handle_sub_neg = MudHandler_NAWS_HandleSubNeg;

    /* ECHO */
    telnet->handlers[2].type = HANDLER_ECHO;
    telnet->handlers[2].option_number = (guchar)TELOPT_ECHO;
    telnet->handlers[2].enabled = TRUE;
    telnet->handlers[2].enable = MudHandler_ECHO_Enable;
    telnet->handlers[2].disable = MudHandler_ECHO_Disable;
    telnet->handlers[2].handle_sub_neg = MudHandler_ECHO_HandleSubNeg;

    /* EOR */
    telnet->handlers[3].type = HANDLER_EOR;
    telnet->handlers[3].option_number = (guchar)TELOPT_EOR;
    telnet->handlers[3].enabled = TRUE;
    telnet->handlers[3].enable = MudHandler_EOR_Enable;
    telnet->handlers[3].disable = MudHandler_EOR_Disable;
    telnet->handlers[3].handle_sub_neg = MudHandler_EOR_HandleSubNeg;

    /* CHARSET */
    telnet->handlers[4].type = HANDLER_CHARSET;
    telnet->handlers[4].option_number = (guchar)TELOPT_CHARSET;
    telnet->handlers[4].enabled = TRUE;
    telnet->handlers[4].enable = MudHandler_CHARSET_Enable;
    telnet->handlers[4].disable = MudHandler_CHARSET_Disable;
    telnet->handlers[4].handle_sub_neg = MudHandler_CHARSET_HandleSubNeg;

    /* ZMP */
    telnet->handlers[5].type = HANDLER_ZMP;
    telnet->handlers[5].option_number = (guchar)TELOPT_ZMP;
    telnet->handlers[5].enabled = TRUE;
    telnet->handlers[5].enable = MudHandler_ZMP_Enable;
    telnet->handlers[5].disable = MudHandler_ZMP_Disable;
    telnet->handlers[5].handle_sub_neg = MudHandler_ZMP_HandleSubNeg;

#ifdef ENABLE_GST
    /* MSP */
    telnet->handlers[6].type = HANDLER_MSP;
    telnet->handlers[6].option_number = (guchar)TELOPT_MSP;
    telnet->handlers[6].enabled = TRUE;
    telnet->handlers[6].enable = MudHandler_MSP_Enable;
    telnet->handlers[6].disable = MudHandler_MSP_Disable;
    telnet->handlers[6].handle_sub_neg = MudHandler_MSP_HandleSubNeg;
#endif

#ifdef ENABLE_MCCP
    /* MCCP */
    telnet->handlers[7].type = HANDLER_MCCP2;
    telnet->handlers[7].option_number = (guchar)TELOPT_MCCP2;
    telnet->handlers[7].enabled = TRUE;
    telnet->handlers[7].enable = MudHandler_MCCP_Enable;
    telnet->handlers[7].disable = MudHandler_MCCP_Disable;
    telnet->handlers[7].handle_sub_neg = MudHandler_MCCP_HandleSubNeg;
#endif

}

gint
mud_telnet_isenabled(MudTelnet *telnet, guint8 option_number, gint him)
{
    gint i;

    for(i = 0; i < TEL_HANDLERS_SIZE; ++i)
        if(telnet->handlers[i].type != HANDLER_NONE)
            if(option_number == telnet->handlers[i].option_number)
                return telnet->handlers[i].enabled;

    return FALSE;
}

void
mud_telnet_get_parent_size(MudTelnet *telnet, gint *w, gint *h)
{
    mud_connection_view_get_term_size(telnet->parent, w, h);
}

void
mud_telnet_set_parent_naws(MudTelnet *telnet, gint enabled)
{
    mud_connection_view_set_naws(telnet->parent, enabled);
}

void
mud_telnet_set_parent_remote_encode(MudTelnet *telnet, gint enabled, gchar *encoding)
{
    telnet->parent->remote_encode = enabled;

    if(enabled)
        telnet->parent->remote_encoding = g_strdup(encoding);
    else if(telnet->parent->remote_encoding)
        g_free(telnet->parent->remote_encoding);
}

void
mud_telnet_set_local_echo(MudTelnet *telnet, gint enabled)
{
    telnet->parent->local_echo = enabled;
}

void
mud_telnet_send_naws(MudTelnet *telnet, gint width, gint height)
{
    guchar w1, h1, w0, h0;

    w1 = (width >> 8) & 0xff;
    w0 = width & 0xff;

    h1 = (height >> 8) & 0xff;
    h0 = height & 0xff;

    mud_telnet_send_sub_req(telnet, 5, (guchar)TELOPT_NAWS, w1, w0, h1, h0);
}

GString *
mud_telnet_process(MudTelnet *telnet, guchar * buf, guint32 c, gint *len)
{
    guint32 i;
    guint32 count;
    gchar *opt;

    g_assert(telnet != NULL);

    if(telnet->buffer != NULL)
        g_string_free(telnet->buffer, TRUE);

    telnet->buffer = g_string_new(NULL);

#ifdef ENABLE_MCCP
    if(telnet->mccp)
    {
        GString *ret = NULL;

        // decompress the buffer.
        ret = mud_mccp_decompress(telnet, buf, c);

        telnet->mccp_new = FALSE;

        if(ret == NULL)
        {
            if(telnet->buffer)
                g_string_free(telnet->buffer, TRUE);

            return ret;
        }
        else
        {
            telnet->buffer = g_string_append_len(telnet->buffer, ret->str, ret->len);
            g_string_free(ret, TRUE);
        }
    }
    else
#endif
        telnet->buffer = g_string_append_len(telnet->buffer, (gchar *)buf, c);

    count = telnet->buffer->len;

    for (i = 0; i < count; ++i)
    {
        switch (telnet->tel_state)
        {
            case TEL_STATE_TEXT:
#ifdef ENABLE_MCCP
                /* The following is only done when compressing is first
                   enabled in order to decompress any part of the buffer
                   that remains after the subnegotation takes place */
                if(telnet->mccp && telnet->mccp_new)
                {
                    GString *ret = NULL;
                    telnet->mccp_new = FALSE;

                    // decompress the rest of the buffer.
                    ret = mud_mccp_decompress(telnet, &buf[i], count - i);

                    if(telnet->buffer)
                    {
                        g_string_free(telnet->buffer, TRUE);
                        telnet->buffer = NULL;
                    }

                    if(ret == NULL)
                    {
                        GString *ret_string =
                            g_string_new_len(telnet->processed->str, telnet->pos);

                        *len = telnet->pos;
                        telnet->pos= 0;

                        if(telnet->processed)
                        {
                            g_string_free(telnet->processed, TRUE);
                            telnet->processed = g_string_new(NULL);
                        }

                        return ret_string;
                    }

                    telnet->buffer = g_string_new(ret->str);

                    if(telnet->buffer->len == 0)
                    {
                        GString *ret_string =
                            g_string_new_len(telnet->processed->str, telnet->pos);
                        *len = telnet->pos;

                        telnet->pos= 0;

                        if(telnet->processed)
                        {
                            g_string_free(telnet->processed, TRUE);
                            telnet->processed = g_string_new(NULL);
                        }

                        if(telnet->buffer)
                        {
                            g_string_free(telnet->buffer, TRUE);
                            telnet->buffer = NULL;
                        }
                        return ret_string;
                    }

                    i = 0;
                    count = telnet->buffer->len;
                }
#endif
                if ((guchar)telnet->buffer->str[i] == (guchar)TEL_IAC)
                    telnet->tel_state = TEL_STATE_IAC;
                else
                {
                    telnet->processed = 
                        g_string_append_c(telnet->processed, telnet->buffer->str[i]);
                    telnet->pos++;
                }
                break;

            case TEL_STATE_IAC:
                switch ((guchar)telnet->buffer->str[i])
                {
                    case (guchar)TEL_IAC:
                        telnet->pos++;
                        telnet->processed = 
                            g_string_append_c(telnet->processed, telnet->buffer->str[i]);
                        telnet->tel_state = TEL_STATE_TEXT;
                        break;

                    case (guchar)TEL_DO:
                        telnet->tel_state = TEL_STATE_DO;
                        break;

                    case (guchar)TEL_DONT:
                        telnet->tel_state = TEL_STATE_DONT;
                        break;

                    case (guchar)TEL_WILL:
                        telnet->tel_state = TEL_STATE_WILL;
                        break;

                    case (guchar)TEL_NOP:
                        telnet->tel_state = TEL_STATE_TEXT;
                        break;

                    case (guchar)TEL_GA:
                        // TODO: Hook up to triggers.
                        telnet->tel_state = TEL_STATE_TEXT;
                        break;

                    case (guchar)TEL_EOR_BYTE:
                        // TODO: Hook up to triggers.
                        telnet->tel_state = TEL_STATE_TEXT;
                        break;

                    case (guchar)TEL_WONT:
                        telnet->tel_state = TEL_STATE_WONT;
                        break;

                    case (guchar)TEL_SB:
                        telnet->tel_state = TEL_STATE_SB;
                        telnet->subreq_pos = 0;
                        break;

                    default:
                        g_warning("Illegal IAC command %d received", telnet->buffer->str[i]);
                        telnet->tel_state = TEL_STATE_TEXT;
                        break;
                }
                break;

            case TEL_STATE_DO:
                opt = mud_telnet_get_telopt_string((guchar)telnet->buffer->str[i]);
                g_log("Telnet", G_LOG_LEVEL_MESSAGE, "Recieved: DO %s", opt);
                g_free(opt);

                mud_telnet_handle_positive_nego(
                        telnet,
                        (guchar)telnet->buffer->str[i],
                        (guchar)TEL_WILL,
                        (guchar)TEL_WONT, FALSE);

                telnet->tel_state = TEL_STATE_TEXT;
                break;

            case TEL_STATE_WILL:
                opt = mud_telnet_get_telopt_string((guchar)telnet->buffer->str[i]);
                g_log("Telnet", G_LOG_LEVEL_MESSAGE, "Recieved: WILL %s", opt);
                g_free(opt);

                mud_telnet_handle_positive_nego(
                        telnet,
                        (guchar)telnet->buffer->str[i],
                        (guchar)TEL_DO,
                        (guchar)TEL_DONT, TRUE);

                telnet->tel_state = TEL_STATE_TEXT;
                break;

            case TEL_STATE_DONT:
                opt = mud_telnet_get_telopt_string((guchar)telnet->buffer->str[i]);
                g_log("Telnet", G_LOG_LEVEL_MESSAGE, "Recieved: DONT %s", opt);
                g_free(opt);

                mud_telnet_handle_negative_nego(
                        telnet,
                        (guchar)telnet->buffer->str[i],
                        (guchar)TEL_WILL,
                        (guchar)TEL_WONT, FALSE);

                telnet->tel_state = TEL_STATE_TEXT;
                break;

            case TEL_STATE_WONT:
                opt = mud_telnet_get_telopt_string((guchar)telnet->buffer->str[i]);
                g_log("Telnet", G_LOG_LEVEL_MESSAGE, "Recieved: WONT %s", opt);
                g_free(opt);
                mud_telnet_handle_negative_nego(
                        telnet,
                        (guchar)telnet->buffer->str[i],
                        (guchar)TEL_DO,
                        (guchar)TEL_DONT, TRUE);

                telnet->tel_state = TEL_STATE_TEXT;
                break;

            case TEL_STATE_SB:
                if ((guchar)telnet->buffer->str[i] == (guchar)TEL_IAC)
                    telnet->tel_state = TEL_STATE_SB_IAC;
                else
                {
                    // FIXME: Handle overflow
                    if (telnet->subreq_pos >= TEL_SUBREQ_BUFFER_SIZE)
                    {
                        g_warning("Subrequest buffer full. Oddities in output will happen. Sorry.");
                        telnet->subreq_pos = 0;
                        telnet->tel_state = TEL_STATE_TEXT;
                    }
                    else
                        telnet->subreq_buffer[telnet->subreq_pos++] =
                            (guchar)telnet->buffer->str[i];
                }
                break;

            case TEL_STATE_SB_IAC:
                if ((guchar)telnet->buffer->str[i] == (guchar)TEL_IAC)
                {
                    if (telnet->subreq_pos >= TEL_SUBREQ_BUFFER_SIZE)
                    {
                        g_warning("Subrequest buffer full. Oddities in output will happen. Sorry.");
                        telnet->subreq_pos = 0;
                        telnet->tel_state = TEL_STATE_TEXT;
                    }
                    else
                        telnet->subreq_buffer[telnet->subreq_pos++] =
                            (guchar)telnet->buffer->str[i];

                    telnet->tel_state = TEL_STATE_SB;
                }
                else if ((guchar)telnet->buffer->str[i] == (guchar)TEL_SE)
                {
                    telnet->subreq_buffer[telnet->subreq_pos++] =
                        (guchar)telnet->buffer->str[i];

                    mud_telnet_on_handle_subnego(telnet);

                    telnet->subreq_pos = 0;
                    telnet->tel_state = TEL_STATE_TEXT;
                } else {
                    g_warning("Erronous byte %d after an IAC inside a subrequest",
                            telnet->buffer->str[i]);

                    telnet->subreq_pos = 0;
                    telnet->tel_state = TEL_STATE_TEXT;
                }
                break;
        }
    }

    if(telnet->buffer)
    {
        g_string_free(telnet->buffer, TRUE);
        telnet->buffer = NULL;
    }

    if(telnet->tel_state == TEL_STATE_TEXT)
    {
        GString *ret =
            g_string_new_len(telnet->processed->str, telnet->pos);
        *len = telnet->pos;

        telnet->pos= 0;

        if(telnet->processed)
        {
            g_string_free(telnet->processed, TRUE);
            telnet->processed = g_string_new(NULL);
        }

        return ret;
    }

    return NULL;
}

gchar*
mud_telnet_get_telnet_string(guchar ch)
{
    GString *string = g_string_new(NULL);
    gchar *ret;

    switch (ch)
    {
    case TEL_WILL:
	string = g_string_append(string, "WILL");
	break;
    case TEL_WONT:
	string = g_string_append(string, "WONT");
	break;
    case TEL_DO:
	string = g_string_append(string, "DO");
	break;
    case TEL_DONT:
	string = g_string_append(string, "DONT");
	break;
    case TEL_IAC:
	string = g_string_append(string, "IAC");
	break;
    default:
	string = g_string_append_c(string,ch);
	break;
    }

    ret = g_strdup(string->str);
    g_string_free(string, TRUE);

    return ret;
}

gchar*
mud_telnet_get_telopt_string(guchar ch)
{
    GString *string = g_string_new(NULL);

    switch (ch)
    {
    case TELOPT_ECHO:
	string = g_string_append(string, "ECHO");
	break;

    case TELOPT_TTYPE:
	string = g_string_append(string, "TTYPE");
	break;

    case TELOPT_EOR:
	string = g_string_append(string, "END-OF-RECORD");
	break;

    case TELOPT_NAWS:
	string = g_string_append(string, "NAWS");
	break;

    case TELOPT_CHARSET:
	string = g_string_append(string, "CHARSET");
	break;

    case TELOPT_MCCP:
	string = g_string_append(string, "COMPRESS");
	break;

    case TELOPT_MCCP2:
	string = g_string_append(string, "COMPRESS2");
	break;

    case TELOPT_CLIENT:
	string = g_string_append(string, "CLIENT");
	break;

    case TELOPT_CLIENTVER:
	string = g_string_append(string, "CLIENTVER");
	break;

    case TELOPT_MSP:
	string = g_string_append(string, "MSP");
	break;

    case TELOPT_MXP:
	string = g_string_append(string, "MXP");
	break;

    case TELOPT_ZMP:
	string = g_string_append(string, "ZMP");
	break;

    default:
        g_string_printf(string, "Dec: %d Hex: %x Ascii: %c", ch, ch, ch);
    }

    return g_string_free(string, FALSE);
}

void
mud_telnet_send_charset_req(MudTelnet *telnet, gchar *encoding)
{
    guchar byte;
    guint32 i;

    if(!encoding)
	return;

    g_log("Telnet", G_LOG_LEVEL_DEBUG, "Sending Charset Accepted SubReq");

    /* Writes IAC SB CHARSET ACCEPTED <charset> IAC SE to server */
    byte = (guchar)TEL_IAC;

    gnet_conn_write(telnet->conn, (gchar *)&byte, 1);
    byte = (guchar)TEL_SB;
    gnet_conn_write(telnet->conn, (gchar *)&byte, 1);
    byte = (guchar)TELOPT_CHARSET;
    gnet_conn_write(telnet->conn, (gchar *)&byte, 1);
    byte = (guchar)TEL_CHARSET_ACCEPT;
    gnet_conn_write(telnet->conn, (gchar *)&byte, 1);

    for (i = 0; i < strlen(encoding); ++i)
    {
	byte = (guchar)encoding[i];
	gnet_conn_write(telnet->conn, (gchar *)&byte, 1);

	if (byte == (guchar)TEL_IAC)
	    gnet_conn_write(telnet->conn, (gchar *)&byte, 1);
    }

    byte = (guchar)TEL_IAC;
    gnet_conn_write(telnet->conn, (gchar *)&byte, 1);
    byte = (guchar)TEL_SE;
    gnet_conn_write(telnet->conn, (gchar *)&byte, 1);
}

void
mud_telnet_send_sub_req(MudTelnet *telnet, guint32 count, ...)
{
    guchar byte;
    guint32 i;
    va_list va;
    va_start(va, count);

    g_log("Telnet", G_LOG_LEVEL_DEBUG, "Sending Subreq...");
    
    byte = (guchar)TEL_IAC;

    gnet_conn_write(telnet->conn, (gchar *)&byte, 1);
    byte = (guchar)TEL_SB;
    gnet_conn_write(telnet->conn, (gchar *)&byte, 1);

    for (i = 0; i < count; ++i)
    {
	byte = (guchar)va_arg(va, gint);
	gnet_conn_write(telnet->conn, (gchar *)&byte, 1);

	if (byte == (guchar)TEL_IAC)
	    gnet_conn_write(telnet->conn, (gchar *)&byte, 1);
    }

    va_end(va);

    byte = (guchar)TEL_IAC;
    gnet_conn_write(telnet->conn, (gchar *)&byte, 1);
    byte = (guchar)TEL_SE;
    gnet_conn_write(telnet->conn, (gchar *)&byte, 1);
}

void
mud_telnet_send_raw(MudTelnet *telnet, guint32 count, ...)
{
    guchar byte;
    guint32 i;
    va_list va;
    va_start(va, count);

    for (i = 0; i < count; ++i)
    {
	byte = (guchar)va_arg(va, gint);
	gnet_conn_write(telnet->conn, (gchar *)&byte, 1);

	if (byte == (guchar)TEL_IAC)
	    gnet_conn_write(telnet->conn, (gchar *)&byte, 1);
    }

    va_end(va);
}

/*** Private Methods ***/
static void
mud_telnet_send_iac(MudTelnet *telnet, guchar ch1, guchar ch2)
{
    guchar buf[3];
    buf[0] = (guchar)TEL_IAC;
    buf[1] = ch1;
    buf[2] = ch2;

    gnet_conn_write(telnet->conn, (gchar *)buf, 3);
}

static void
mud_telnet_on_handle_subnego(MudTelnet *telnet)
{
    int index;

    if (telnet->subreq_pos < 1)
	return;

    if((index = mud_telnet_get_index_by_option(telnet, telnet->subreq_buffer[0])) == -1)
    {
        g_warning("Invalid Telnet Option passed: %d", telnet->subreq_buffer[0]);
        return;
    }

    if (mud_telnet_isenabled(telnet, telnet->subreq_buffer[0], FALSE))
        telnet->handlers[index].handle_sub_neg(telnet, telnet->subreq_buffer + 1,
					       telnet->subreq_pos - 1, &telnet->handlers[index]);
}

static void
mud_telnet_on_enable_opt(MudTelnet *telnet, const guchar opt_no, gint him)
{
    int index;

    if((index = mud_telnet_get_index_by_option(telnet, opt_no)) == -1)
    {
        g_warning("Invalid Telnet Option passed: %d", opt_no);
        return;
    }

    telnet->handlers[index].enable(telnet, &telnet->handlers[index]);
}

static void
mud_telnet_on_disable_opt(MudTelnet *telnet, const guchar opt_no, gint him)
{
    int index;

    if((index = mud_telnet_get_index_by_option(telnet, opt_no)) == -1)
    {
        g_warning("Invalid Telnet Option passed: %d", opt_no);
        return;
    }

    telnet->handlers[index].disable(telnet, &telnet->handlers[index]);
}

static guchar
mud_telnet_get_telopt_state(guint8 *storage, const guint bitshift)
{
    return (*storage >> bitshift) & 0x03u;
}

static gint
mud_telnet_get_telopt_queue(guchar *storage, const guint bitshift)
{
    return !!((*storage >> bitshift) & 0x04u);
}

static void
mud_telnet_set_telopt_state(guchar *storage, const enum TelnetOptionState state,
			    const guint bitshift)
{
    *storage = (*storage & ~(0x03u << bitshift)) | (state << bitshift);
}

static void
mud_telnet_set_telopt_queue(guchar *storage, gint bit_on, const guint bitshift)
{
    *storage = bit_on ? (*storage | (0x04u << bitshift)) : (*storage & ~(0x04u << bitshift));
}

// Otherwise handlers called on state changes could see the wrong options
// (I think theoretically they should not care at all, but in practice...)
static gint
mud_telnet_handle_positive_nego(MudTelnet *telnet,
                                const guchar opt_no,
				const guchar affirmative,
				const guchar negative,
				gint him)
{
    const guint bitshift = him ? 4 : 0;
    guchar * opt = &(telnet->telopt_states[opt_no]);
    gchar *tel_string, *opt_string;

    switch (mud_telnet_get_telopt_state(opt, bitshift))
    {
    case TELOPT_STATE_NO:
	// If we agree that server should enable telopt, set
	// his state to YES and send DO; otherwise send DONT
	// FIXME-US/HIM
	// FIXME: What to do in the opposite "him" gint value case?
	if (mud_telnet_isenabled(telnet, opt_no, him))
	{
            tel_string = mud_telnet_get_telnet_string(affirmative);
            opt_string = mud_telnet_get_telopt_string(opt_no);
            g_log("Telnet", G_LOG_LEVEL_DEBUG, "Sent: %s %s",
                    tel_string, opt_string );
            g_free(tel_string);
            g_free(opt_string);

	    mud_telnet_set_telopt_state(opt, TELOPT_STATE_YES, bitshift);
	    mud_telnet_send_iac(telnet, affirmative, opt_no);
	    mud_telnet_on_enable_opt(telnet, opt_no, him);
	    return TRUE;
	} else {
            tel_string = mud_telnet_get_telnet_string(negative);
            opt_string = mud_telnet_get_telopt_string(opt_no);
            g_log("Telnet", G_LOG_LEVEL_DEBUG, "Sent: %s %s",
                    tel_string, opt_string );
            g_free(tel_string);
            g_free(opt_string);

	    mud_telnet_send_iac(telnet, negative, opt_no);
	    return FALSE;
	}

    case TELOPT_STATE_YES:
	// Ignore, he already supposedly has it enabled. Includes the case where
	// DONT was answered by WILL with himq = OPPOSITE to prevent loop.
	return FALSE;

    case TELOPT_STATE_WANTNO:
	if (mud_telnet_get_telopt_queue(opt, bitshift) == TELOPT_STATE_QUEUE_EMPTY)
	{
	    mud_telnet_set_telopt_state(opt, TELOPT_STATE_NO, bitshift);
	    g_warning("TELNET NEGOTIATION: DONT answered by WILL; ill-behaved server. Ignoring IAC WILL %d. him = NO\n", opt_no);
	    return FALSE;
	} else { // The opposite is queued
	    mud_telnet_set_telopt_state(opt, TELOPT_STATE_YES, bitshift);
	    mud_telnet_set_telopt_queue(opt, TELOPT_STATE_QUEUE_EMPTY, bitshift);
	    g_warning("TELNET NEGOTIATION: DONT answered by WILL; ill-behaved server. Ignoring IAC WILL %d. him = YES, himq = EMPTY\n", opt_no);
	    return FALSE;
	}
	break;

    case TELOPT_STATE_WANTYES:
	if (mud_telnet_get_telopt_queue(opt, bitshift) == TELOPT_STATE_QUEUE_EMPTY)
	{
            tel_string = mud_telnet_get_telnet_string(affirmative);
            opt_string = mud_telnet_get_telopt_string(opt_no);
            g_log("Telnet", G_LOG_LEVEL_DEBUG, "Sent: %s %s",
                    tel_string, opt_string );
            g_free(tel_string);
            g_free(opt_string);

	    mud_telnet_set_telopt_state(opt, TELOPT_STATE_YES, bitshift);
	    mud_telnet_send_iac(telnet, affirmative, opt_no);
	    mud_telnet_on_enable_opt(telnet, opt_no, him);
	    return TRUE;
	} else { // The opposite is queued
            tel_string = mud_telnet_get_telnet_string(negative);
            opt_string = mud_telnet_get_telopt_string(opt_no);
            g_log("Telnet", G_LOG_LEVEL_DEBUG, "Sent: %s %s",
                    tel_string, opt_string );
            g_free(tel_string);
            g_free(opt_string);

	    mud_telnet_set_telopt_state(opt, TELOPT_STATE_WANTNO, bitshift);
	    mud_telnet_set_telopt_queue(opt, TELOPT_STATE_QUEUE_EMPTY, bitshift);
	    mud_telnet_send_iac(telnet, negative, opt_no);
	    return FALSE;
	}
    default:
	g_warning("Something went really wrong\n");
	return FALSE;
    }
}

static gint
mud_telnet_handle_negative_nego(MudTelnet *telnet,
                                const guchar opt_no,
				const guchar affirmative,
				const guchar negative,
				gint him)
{
    const guint bitshift = him ? 4 : 0;
    guchar * opt = &(telnet->telopt_states[opt_no]);
    gchar *opt_string, *tel_string;

    switch (mud_telnet_get_telopt_state(opt, bitshift))
    {
    case TELOPT_STATE_NO:
	// Ignore, he already supposedly has it disabled
	return FALSE;

    case TELOPT_STATE_YES:
        tel_string = mud_telnet_get_telnet_string(negative);
        opt_string = mud_telnet_get_telopt_string(opt_no);
        g_log("Telnet", G_LOG_LEVEL_DEBUG, "Sent: %s %s",
            tel_string, opt_string );
        g_free(tel_string);
        g_free(opt_string);

	mud_telnet_set_telopt_state(opt, TELOPT_STATE_NO, bitshift);
	mud_telnet_send_iac(telnet, negative, opt_no);
	mud_telnet_on_disable_opt(telnet, opt_no, him);
	return TRUE;

    case TELOPT_STATE_WANTNO:
        if (mud_telnet_get_telopt_queue(opt, bitshift) == TELOPT_STATE_QUEUE_EMPTY)
        {
            mud_telnet_set_telopt_state(opt, TELOPT_STATE_NO, bitshift);
            return FALSE;
        } else {
            tel_string = mud_telnet_get_telnet_string(affirmative);
            opt_string = mud_telnet_get_telopt_string(opt_no);
            g_log("Telnet", G_LOG_LEVEL_DEBUG, "Sent: %s %s",
                    tel_string, opt_string );
            g_free(tel_string);
            g_free(opt_string);

            mud_telnet_set_telopt_state(opt, TELOPT_STATE_WANTYES, bitshift);
            mud_telnet_set_telopt_queue(opt, TELOPT_STATE_QUEUE_EMPTY, bitshift);
            mud_telnet_send_iac(telnet, affirmative, opt_no);
            mud_telnet_on_enable_opt(telnet, opt_no, him); // FIXME: Is this correct?
            return TRUE;
        }

    case TELOPT_STATE_WANTYES:
	if (mud_telnet_get_telopt_queue(opt, bitshift) == TELOPT_STATE_QUEUE_EMPTY)
	{
	    mud_telnet_set_telopt_state(opt, TELOPT_STATE_NO, bitshift);
	    return FALSE;
	} else { // The opposite is queued
	    mud_telnet_set_telopt_state(opt, TELOPT_STATE_NO, bitshift);
	    mud_telnet_set_telopt_queue(opt, TELOPT_STATE_QUEUE_EMPTY, bitshift);
	    return FALSE;
	}
    default:
	g_warning("TELNET NEGOTIATION: Something went really wrong\n");
	return FALSE;
    }
}

static gint
mud_telnet_get_index_by_option(MudTelnet *telnet, guchar option_number)
{
    gint i;

    for(i = 0; i < TEL_HANDLERS_SIZE; i++)
        if(telnet->handlers[i].type != HANDLER_NONE)
            if(telnet->handlers[i].option_number == option_number)
                return i;

    return -1;
}
