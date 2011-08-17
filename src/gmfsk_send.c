/*
 *    gmfsk_send.c  --  test program to demonstrate gMFSK remote capabilities
 *
 *    Copyright (C) 2001, 2002, 2003, 2007
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

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "sha1.h"

static char *Eol = "\r\n";

/* ---------------------------------------------------------------------- */

#define SHA1_HASH_LEN	20

static char tohex[16] = "0123456789abcdef";

static char *srv_sha1_hash(char *key)
{
        unsigned char hash[SHA1_HASH_LEN];
        char *ret, *p;
        int i;

        sha1_buffer(key, strlen(key), &hash);

	if ((ret = calloc(sizeof(char), SHA1_HASH_LEN * 2 + 1)) == NULL)
		return NULL;

        for (i = 0, p = ret; i < SHA1_HASH_LEN; i++) {
                *p++ = tohex[hash[i] / 16];
                *p++ = tohex[hash[i] % 16];
        }
        *p = 0;

        return ret;
}

static char *mk_response(char *str, char *key)
{
	char *p, *challenge, *response;

	if ((p = strchr(str, '<')) == NULL)
		return NULL;

	challenge = ++p;

	if ((p = strchr(str, '>')) == NULL)
		return NULL;

	*p = 0;

	if((p = calloc(sizeof(char), strlen(challenge) + strlen(key) + 1)) == NULL)
		return NULL;

	sprintf(p, "%s%s", key, challenge);

	response = srv_sha1_hash(p);

	free(p);

	return response;
}

/* ---------------------------------------------------------------------- */

int main(int argc, char **argv)
{
	char *key = "", *host = "127.0.0.1", *port = "9999", *file = NULL;
	int i, s;
	struct sockaddr_in sa;
	socklen_t salen = sizeof(struct sockaddr_in);
	struct hostent *hp;
	struct servent *sp;
	char buf[1024], *cp;
	FILE *fp = NULL;

	while ((i = getopt(argc, argv, "f:h:k:p:")) != -1) {
		switch (i) {
		case 'f':
			file = optarg;
			break;
		case 'h':
			host = optarg;
			break;
		case 'k':
			key = optarg;
			break;
		case 'p':
			port = optarg;
			break;
		case ':':
		case '?':
			fprintf(stderr, "Usage\n");
			return 1;
		}
	}

	if (file && (fp = fopen(file, "r")) == NULL) {
		perror("fopen");
		return 1;
	}

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		return 1;
	}

	printf("Resolving %s:%s...\n", host, port);

	sa.sin_family = AF_INET;

	if ((hp = gethostbyname(host)) == NULL) {
		fprintf(stderr, "Unknown host '%s'.\n", host);
		return 1;
	}
	sa.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;

	if ((sp = getservbyname(port, "tcp")) != NULL)
		sa.sin_port = sp->s_port;
	else
		sa.sin_port = htons(atoi(port));
	if (sa.sin_port == 0) {
		fprintf(stderr, "Unknown port '%s'\n", port);
		return 1;
	}

	printf("Connecting...\n");

	if (connect(s, (struct sockaddr *)&sa, salen) < 0) {
		perror("connect");
		close(s);
		return 1;
	}

	printf("Authenticating...\n");

	i = read(s, &buf, sizeof(buf) - 1);
	buf[i] = 0;

	if ((cp = mk_response(buf, key)) == NULL) {
		perror("Error generating authentication response");
		return 1;
	}

	sprintf(buf, "AUTH %s%s", cp, Eol);

	if (write(s, buf, strlen(buf)) != strlen(buf)) {
		perror("write");
		return 1;
	}

	i = read(s, &buf, sizeof(buf) - 1);
	buf[i] = 0;

	if (buf[0] == '-') {
		fprintf(stderr, "Authentication failed\n");
		close(s);
		return 1;
	}

	printf("Sending text...\n");

	if (fp != NULL) {
		if (write(s, "SEND ", 5) != 5) {
			perror("write");
			return 1;
		}

		while ((i = fread(buf, sizeof(char), 1024, fp)) > 0) {
			if (write(s, buf, i) != i) {
				perror("write");
				return 1;
			}
		}

		if (write(s, Eol, sizeof(Eol)) != sizeof(Eol)) {
			perror("write");
			return 1;
		}
	}

	if (optind < argc) {
		i = optind;

		if (write(s, "SEND", 4) != 4) {
			perror("write");
			return 1;
		}

		while (i < argc) {
			cp = argv[i++];

			if (write(s, " ", 1) != 1) {
				perror("write");
				return 1;
			}
			if (write(s, cp, strlen(cp)) != strlen(cp)) {
				perror("write");
				return 1;
			}
		}
		if (write(s, Eol, strlen(Eol)) != strlen(Eol)) {
			perror("write");
			return 1;
		}
	}

	if (write(s, "QUIT", 4) != 4) {
		perror("write");
		return 1;
	}
	if (write(s, Eol, strlen(Eol)) != strlen(Eol)) {
		perror("write");
		return 1;
	}

	sleep(1);

	printf("Done.\n");
	close(s);
	return 0;
}

/* ---------------------------------------------------------------------- */

