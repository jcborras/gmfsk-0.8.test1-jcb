/*
 *    server.c  --  gMFSK remote server
 *
 *    Copyright (C) 2006
 *      Tomi Manninen (oh2bns@sral.fi)
 *
 *    This file is part of gMFSK.
 *
 *    gMFSK is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    gMFSK is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with gMFSK; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "server.h"
#include "main.h"
#include "macro.h"
#include "conf.h"

#include "sha1.h"

#define BUFFER_SIZE	1024

typedef enum {
	SERVER_AUTH,
	SERVER_READY,
	SERVER_RECEIVE
} server_state_t;

struct server {
	GIOChannel *ch;
	gint fd;
	server_state_t state;
	gchar *challenge;
};

GList *serverlist = NULL;

gchar *Welcome_Message = "*** Welcome to gMFSK version " VERSION;
gchar *Eol = "\r\n";

/* ---------------------------------------------------------------------- */

static gboolean srv_write(struct server *srv, gchar *msg)
{
	GIOStatus status;
	GError *error = NULL;
	gsize bytes_written = 0;
	gint msglen;

	msglen = strlen(msg);

	status = g_io_channel_write_chars(srv->ch, msg, msglen, &bytes_written, &error);

	if (status != G_IO_STATUS_NORMAL) {
		g_warning("write failed");
	}

	if (error != NULL) {
		g_warning("error->message");
		g_error_free(error);
	}

	if (bytes_written != msglen) {
		g_warning("full message not written");
	}

	return TRUE;
}

static gboolean srv_flush(struct server *srv)
{
	GIOStatus status;
	GError *error = NULL;

	status = g_io_channel_flush(srv->ch, &error);

	if (status != G_IO_STATUS_NORMAL) {
		g_warning("flush failed");
	}

	if (error != NULL) {
		g_warning("error->message");
		g_error_free(error);
	}

	return TRUE;
}

static gboolean srv_response(struct server *srv, gboolean ok, gchar *msg)
{
	gchar *response;
	gboolean ret;

	response = g_strdup_printf("%c%s%s", ok ? '+' : '-', msg, Eol);
	ret = srv_write(srv, response);
	g_free(response);

	return ret;
}

static void srv_disconnect(struct server *srv, gchar *reason)
{
	gchar *str;

	str = g_strdup_printf("Client disconnected: %s", reason);
	statusbar_set("mainstatusbar", str);
	g_free(str);

	/* free resources */
	g_free(srv->challenge);

	/* remove the server from list */
	serverlist = g_list_remove(serverlist, srv);

	if (g_list_length(serverlist) > 0) {
		str = g_strdup_printf("R(%d)", g_list_length(serverlist));
		statusbar_set("remotestatusbar", str);
		g_free(str);
	} else
		statusbar_set("remotestatusbar", "");

	/* shut down -- flush and close */
	g_io_channel_shutdown(srv->ch, TRUE, NULL);
}

/* ---------------------------------------------------------------------- */

#define SHA1_HASH_LEN	20

static gchar tohex[16] = "0123456789abcdef";

static gchar *srv_sha1_hash(gchar *key)
{
	guint8 hash[SHA1_HASH_LEN];
	gchar *ret, *p;
	gint i;

	sha1_buffer(key, strlen(key), &hash);

	ret = g_new(gchar, SHA1_HASH_LEN * 2 + 1);

	for (i = 0, p = ret; i < SHA1_HASH_LEN; i++) {
		*p++ = tohex[hash[i] / 16];
		*p++ = tohex[hash[i] % 16];
	}
	*p = 0;

	return ret;
}

static gboolean srv_check_auth(struct server *srv, gchar *response)
{
	gchar *pw, *buf, *hash;
	gboolean ret;

	if (!response)
		return FALSE;

	pw = conf_get_string("server/password");

	/* empty password is not valid */
	if (!pw || !*pw) {
		g_free(pw);
		return FALSE;
	}

	buf = g_strdup_printf("%s%s", pw, srv->challenge);
	hash = srv_sha1_hash(buf);

	ret = (g_ascii_strcasecmp(response, hash) == 0);

	g_free(pw);
	g_free(buf);
	g_free(hash);

	return ret;
}

/* ---------------------------------------------------------------------- */

static gboolean server_cb(GIOChannel *s, GIOCondition c, gpointer p)
{
	struct server *srv = (struct server *) p;
	GError *error;
	gchar *line, *cmd, *arg;
	gsize linelen;
	GIOStatus status;

	if (c == G_IO_ERR) {
		g_warning("server error.");
		srv_disconnect(srv, "Server error.");
		return FALSE;
	}

	if (c == G_IO_HUP) {
		srv_disconnect(srv, "Connection closed by foreign host.");
		return FALSE;
	}

	error = NULL;
	status = g_io_channel_read_line(s, &line, &linelen, NULL, &error);

	switch (status) {
	case G_IO_STATUS_NORMAL:
		break;

	case G_IO_STATUS_ERROR:
		g_warning("g_io_channel_read_line returned error");
		if (error != NULL) {
			g_warning(error->message);
			g_error_free(error);
		}
		srv_disconnect(srv, "Read error.");
		g_free(line);
		return FALSE;

	case G_IO_STATUS_EOF:
		srv_disconnect(srv, "Connection closed by foreign host.");
		g_free(line);
		return FALSE;

	case G_IO_STATUS_AGAIN:
		g_free(line);
		return TRUE;
	}

	if (line == NULL || linelen == 0)
		return TRUE;

	/* remove eol */
	line[linelen - strlen(Eol)] = 0;

	cmd = line;
	if ((arg = strchr(line, ' ')) != NULL)
		*arg++ = 0;

	if (!g_ascii_strcasecmp(cmd, "quit")) {
		srv_response(srv, TRUE, "Goodbye.");
		srv_disconnect(srv, "QUIT");
		g_free(line);
		return FALSE;
	}

	switch (srv->state) {
	case SERVER_AUTH:
		if (!g_ascii_strcasecmp(cmd, "auth")) {
			if (srv_check_auth(srv, arg)) {
				srv_response(srv, TRUE, "Authentication ok.");
				srv->state = SERVER_READY;
			} else {
				srv_response(srv, FALSE, "Authentication failed.");
				srv_disconnect(srv, "Incorrect password");
				return FALSE;
			}
		} else {
			srv_response(srv, FALSE, "Authentication needed.");
		}
		break;

	case SERVER_READY:
		if (!g_ascii_strcasecmp(cmd, "send")) {
			srv_response(srv, TRUE, "Ok.");
			send_macrotext(arg);
			break;
		}

		if (!g_ascii_strcasecmp(cmd, "recv")) {
			srv_response(srv, TRUE, "Ok. Send blank line to stop receiving.");
			srv->state = SERVER_RECEIVE;
			break;
		}

		srv_response(srv, FALSE, "Unknown command.");
		break;

	case SERVER_RECEIVE:
		srv_response(srv, TRUE, "Receive terminated.");
		srv->state = SERVER_READY;
		break;
	}

	srv_flush(srv);

	g_free(line);

	return TRUE;
}

static gboolean listen_cb(GIOChannel *s, GIOCondition c, gpointer p)
{
	gint fd;
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(struct sockaddr_in);
	struct server *srv;
	GError *error = NULL;
	GIOStatus status;
	gchar *str, *welcome;
	guint32 rnd;

	if (c == G_IO_ERR) {
		fprintf(stderr, "server listen error\n");
		return FALSE;
	}

	if (c == G_IO_HUP) {
		fprintf(stderr, "connection broken while listening\n");
		return FALSE;
	}

	if ((fd = accept(g_io_channel_unix_get_fd(s), (struct sockaddr *) &addr, &addrlen)) < 0) {
		g_warning("accept failed: %m");
		return FALSE;
	}

	str = g_strdup_printf("Client connection from: %s", inet_ntoa(addr.sin_addr));
	statusbar_set("mainstatusbar", str);
	g_free(str);

	srv = g_new0(struct server, 1);

	srv->ch = g_io_channel_unix_new(fd);
	srv->fd = fd;
	srv->state = SERVER_AUTH;

	/* put it on the server list */
	serverlist = g_list_append(serverlist, (gpointer) srv);

	str = g_strdup_printf("R(%d)", g_list_length(serverlist));
	statusbar_set("remotestatusbar", str);
	g_free(str);

	/* make it non-blocking */
	error = NULL;
	status = g_io_channel_set_flags(srv->ch, G_IO_FLAG_NONBLOCK, &error);
	if (status != G_IO_STATUS_NORMAL) {
		g_warning("setting non-blocking flag failed");
	}
	if (error != NULL) {
		g_warning("error->message");
		g_error_free(error);
	}

	/* set end-of-line to CR-LF */
	g_io_channel_set_line_term(srv->ch, Eol, strlen(Eol));

	/* add to main loop */
	g_io_add_watch(srv->ch, G_IO_IN | G_IO_ERR | G_IO_HUP, server_cb, (gpointer) srv);

	/* generate a challenge */
	rnd = g_random_int();
	str = g_strdup_printf("%X", rnd);
	srv->challenge = srv_sha1_hash(str);
	g_free(str);

	/* send our greeting */
	welcome = g_strdup_printf("%s <%s>%s", Welcome_Message, srv->challenge, Eol);
	srv_write(srv, welcome);
	g_free(welcome);

	srv_flush(srv);

	return TRUE;
}

/* ---------------------------------------------------------------------- */

void server_init(void)
{
	GIOChannel *ch;
	gint fd, on = 1;
	gchar *host, *port;
	struct sockaddr_in sa;
	socklen_t salen = sizeof(struct sockaddr_in);
	struct hostent *hp;
	struct servent *sp;

	if (!conf_get_bool("server/enable"))
		return;

	sa.sin_family = AF_INET;

	host = conf_get_string("server/host");
	port = conf_get_string("server/port");

	if ((hp = gethostbyname(host)) == NULL) {
		fprintf(stderr, "Unknown host '%s'.\n", host);
		return;
	}
	sa.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;

	if ((sp = getservbyname(port, "tcp")) != NULL)
		sa.sin_port = sp->s_port;
	else
		sa.sin_port = htons(atoi(port));

	if (sa.sin_port == 0) {
		fprintf(stderr, "Unknown port '%s'\n", port);
		return;
	}

	if ((fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		g_warning("server_init failed: socket: %s", strerror(errno));
		return;
	}

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
		g_warning("server_init failed: setsockopt: %s", strerror(errno));
		close(fd);
		return;
	}

	if (bind(fd, (struct sockaddr *) &sa, salen) < 0) {
		g_warning("server_init failed: bind: %s", strerror(errno));
		close(fd);
		return;
	}

	if (listen(fd, 0) < 0) {
		g_warning("server_init failed: listen: %s", strerror(errno));
		close(fd);
		return;
	}

	ch = g_io_channel_unix_new(fd);

	g_io_add_watch(ch, G_IO_IN | G_IO_ERR | G_IO_HUP, listen_cb, NULL);
}

static void server_write_cb(gpointer data, gpointer user_data)
{
	struct server *srv = (struct server *) data;
	gchar *str = (gchar *) user_data;

	if (srv->state == SERVER_RECEIVE) {
		srv_write(srv, str);
		srv_flush(srv);
	}
}

void server_write(gchar *str)
{
	g_list_foreach(serverlist, server_write_cb, (gpointer) str);
}

/* ---------------------------------------------------------------------- */

