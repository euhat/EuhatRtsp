/*****************************************************************************
 * vlc_url.h: URL related macros
 *****************************************************************************
 * Copyright (C) 2002-2006 VLC authors and VideoLAN
 * $Id: 994d8ff24e3e897bc19e02476842b3d4cd29a336 $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Rémi Denis-Courmont <rem # videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_URL_H
# define VLC_URL_H
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef WIN32
	#include <direct.h>
	#define DIR_SEP_CHAR '\\'
	#define DIR_SEP "\\"

#else
	#define DIR_SEP_CHAR '/'
	#define DIR_SEP "/"
#endif

#ifdef _MSC_VER
#define strcasecmp stricmp
#define strncasecmp  strnicmp
#define snprintf _snprintf
#define va_copy(dest, src) (dest = src)
#define getcwd _getcwd
#endif

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>

/**
 * \file
 * This file defines functions for manipulating URL in vlc
 */

struct vlc_url_t
{
    char *psz_protocol;
    char *psz_username;
    char *psz_password;
    char *psz_host;
    int  i_port;

    char *psz_path;

    char *psz_option;

    char *psz_buffer; /* to be freed */
};

#ifndef __USE_XOPEN2K8
#if 0
static char *
	strndup (const char *s, size_t size)
{
	char *r;
	char *end = (char*)memchr(s, 0, size);

	if (end)
		/* Length + 1 */
		size = end - s + 1;

	r = (char*)malloc(size);

	if (size)
	{
		memcpy(r, s, size-1);
		r[size-1] = '\0';
	}
	return r;
}
#endif
#endif

static char *decode_URI( char *psz )
{
    unsigned char *in = (unsigned char *)psz, *out = in, c;

    if( psz == NULL )
        return NULL;

    while( ( c = *in++ ) != '\0' )
    {
        switch( c )
        {
            case '%':
            {
                char hex[3];

                if( ( ( hex[0] = *in++ ) == 0 )
                 || ( ( hex[1] = *in++ ) == 0 ) )
                    return NULL;

                hex[2] = '\0';
                *out++ = (unsigned char)strtoul( hex, NULL, 0x10 );
                break;
            }

            default:
                /* Inserting non-ASCII or non-printable characters is unsafe,
                 * and no sane browser will send these unencoded */
                if( ( c < 32 ) || ( c > 127 ) )
                    *out++ = '?';
                else
                    *out++ = c;
        }
    }
    *out = '\0';
    return psz;
}

static inline bool isurisafe( int c )
{
    /* These are the _unreserved_ URI characters (RFC3986 §2.3) */
    return ( (unsigned char)( c - 'a' ) < 26 )
            || ( (unsigned char)( c - 'A' ) < 26 )
            || ( (unsigned char)( c - '0' ) < 10 )
            || ( strchr( "-._~", c ) != NULL );
}

#ifdef WIN32
static int asprintf(char **strp, const char *fmt, ...)
{
	va_list args;
	va_list args_copy;
	int length;
	size_t size;

	va_start(args, fmt);

	va_copy(args_copy, args);

#ifdef WIN32
	/* We need to use _vcsprintf to calculate the length as vsnprintf returns -1
	* if the number of characters to write is greater than count.
	*/
	length = _vscprintf(fmt, args_copy);
#else
	char dummy;
	length = vsnprintf(&dummy, sizeof dummy, fmt, args_copy);
#endif

	va_end(args_copy);

	assert(length >= 0);
	size = length + 1;

	*strp = (char*)malloc(size);
	if (!*strp) {
		return -1;
	}

	va_start(args, fmt);
	vsnprintf(*strp, size, fmt, args);
	va_end(args);

	return length;
}
#endif

static char *encode_URI_bytes (const char *psz_uri, size_t len)
{
    char *psz_enc = (char*)malloc (3 * len + 1), *out = psz_enc;
    if (psz_enc == NULL)
        return NULL;

    for (size_t i = 0; i < len; i++)
    {
        static const char hex[] = "0123456789ABCDEF";
        uint8_t c = *psz_uri;

        if( isurisafe( c ) )
            *out++ = c;
        /* This is URI encoding, not HTTP forms:
         * Space is encoded as '%20', not '+'. */
        else
        {
            *out++ = '%';
            *out++ = hex[c >> 4];
            *out++ = hex[c & 0xf];
        }
        psz_uri++;
    }
    *out++ = '\0';

    out = (char*)realloc (psz_enc, out - psz_enc);
    return out ? out : psz_enc; /* realloc() can fail (safe) */
}

/*static char *decode_URI_duplicate( const char *psz )
{
	char *psz_dup = strdup( psz );
	decode_URI( psz_dup );
	return psz_dup;
}*/

/*static char *encode_URI_component( const char *psz_uri )
{
	return encode_URI_bytes (psz_uri, strlen (psz_uri));
}*/

static char *make_URI (const char *path, const char *scheme)
{
    if (path == NULL)
        return NULL;
    if (scheme == NULL && !strcmp (path, "-"))
        return strdup ("fd://0"); // standard input
    if (strstr (path, "://") != NULL)
        return strdup (path); /* Already a URI */
    /* Note: VLC cannot handle URI schemes without double slash after the
     * scheme name (such as mailto: or news:). */

    char *buf;

#ifdef __OS2__
    char p[strlen (path) + 1];

    for (buf = p; *path; buf++, path++)
        *buf = (*path == '/') ? DIR_SEP_CHAR : *path;
    *buf = '\0';

    path = p;
#endif
	

#if defined( WIN32 ) || defined( __OS2__ )
    /* Drive letter */
    if (isalpha ((unsigned char)path[0]) && (path[1] == ':'))
    {
        asprintf (&buf, "%s:///%c:", scheme ? scheme : "file",
                      path[0]);
 
        path += 2;

        if (path[0] != DIR_SEP_CHAR)
            return NULL;
    }
    else
#endif
    if (!strncmp (path, "\\\\", 2))
    {   /* Windows UNC paths */
#if !defined( WIN32 ) && !defined( __OS2__ )
        if (scheme != NULL)
            return NULL; /* remote files not supported */

        /* \\host\share\path -> smb://host/share/path */
        if (strchr (path + 2, '\\') != NULL)
        {   /* Convert backslashes to slashes */
            char *dup = strdup (path);
            if (dup == NULL)
                return NULL;
            for (size_t i = 2; dup[i]; i++)
                if (dup[i] == '\\')
                    dup[i] = DIR_SEP_CHAR;

            char *ret = make_URI (dup, scheme);
            free (dup);
            return ret;
        }
# define SMB_SCHEME "smb"
#else
        /* \\host\share\path -> file://host/share/path */
# define SMB_SCHEME "file"
#endif
        size_t hostlen = strcspn (path + 2, DIR_SEP);

        buf = (char*)malloc (sizeof (SMB_SCHEME) + 3 + hostlen);
        if (buf != NULL)
            snprintf (buf, sizeof (SMB_SCHEME) + 3 + hostlen,
                      SMB_SCHEME"://%s", path + 2);
        path += 2 + hostlen;

        if (path[0] == '\0')
            return buf; /* Hostname without path */
    }
    else
    if (path[0] != DIR_SEP_CHAR)
    {   /* Relative path: prepend the current working directory */
        char *cwd, *ret;
		char cwd_buf[256] = "";

        if ((cwd = getcwd(cwd_buf, sizeof(cwd_buf))) == NULL)
            return NULL;
        if (asprintf (&buf, "%s"DIR_SEP"%s", cwd, path) == -1)
            buf = NULL;

        free (cwd);
        ret = (buf != NULL) ? make_URI (buf, scheme) : NULL;
        free (buf);
        return ret;
    }
    else
    if (asprintf (&buf, "%s://", scheme ? scheme : "file") == -1)
        buf = NULL;
    if (buf == NULL)
        return NULL;

    assert (path[0] == DIR_SEP_CHAR);

    /* Absolute file path */
    for (const char *ptr = path + 1;; ptr++)
    {
        size_t len = strcspn (ptr, DIR_SEP);
        char *component = encode_URI_bytes (ptr, len);
        if (component == NULL)
        {
            free (buf);
            return NULL;
        }
        char *uri;
        int val = asprintf (&uri, "%s/%s", buf, component);
        free (component);
        free (buf);
        if (val == -1)
            return NULL;
        buf = uri;
        ptr += len;
        if (*ptr == '\0')
            return buf;
    }
}

#if 0
static char *make_path (const char *url)
{
	char *ret = NULL;
	char *end;

	char *path = (char*)strstr (url, "://");
	if (path == NULL)
		return NULL; /* unsupported scheme or invalid syntax */

	end = (char*)memchr (url, '/', path - url);
	size_t schemelen = ((end != NULL) ? end : path) - url;
	path += 3; /* skip "://" */

	/* Remove HTML anchor if present */
	end = strchr (path, '#');
	if (end)
		path = strndup (path, end - path);
	else
		path = strdup (path);
	if ((path == NULL))
		return NULL; /* boom! */

	/* Decode path */
	decode_URI (path);

	if (schemelen == 4 && !strncasecmp (url, "file", 4))
	{
#if (!defined (WIN32) && !defined (__OS2__)) || defined (UNDER_CE)
		/* Leading slash => local path */
		if (*path == '/')
			return path;
		/* Local path disguised as a remote one */
		if (!strncasecmp (path, "localhost/", 10))
            return (char*)memmove (path, path + 9, strlen (path + 9) + 1);
#else
		for (char *p = strchr (path, '/'); p; p = strchr (p + 1, '/'))
			*p = '\\';

		/* Leading backslash => local path */
		if (*path == '\\')
			return (char*)memmove ((void*)path, (const void*)(path + 1), strlen (path + 1) + 1);
		/* Local path disguised as a remote one */
		if (!strncasecmp (path, "localhost\\", 10))
			return (char*)memmove (path, path + 10, strlen (path + 10) + 1);
		/* UNC path */
		if (*path && asprintf (&ret, "\\\\%s", path) == -1)
			ret = NULL;
#endif
		/* non-local path :-( */
	}
	else
		if (schemelen == 2 && !strncasecmp (url, "fd", 2))
		{
			int fd = strtol (path, &end, 0);

			if (*end)
				goto out;

#if !defined( WIN32 ) && !defined( __OS2__ )
			switch (fd)
			{
			case 0:
				ret = strdup ("/dev/stdin");
				break;
			case 1:
				ret = strdup ("/dev/stdout");
				break;
			case 2:
				ret = strdup ("/dev/stderr");
				break;
			default:
				if (asprintf (&ret, "/dev/fd/%d", fd) == -1)
					ret = NULL;
			}
#else
			/* XXX: Does this work on WinCE? */
			if (fd < 2)
				ret = strdup ("CON");
			else
				ret = NULL;
#endif
		}

out:
		free (path);
		return ret; /* unknown scheme */
}
#endif

/*****************************************************************************
 * vlc_UrlParse:
 *****************************************************************************
 * option : if != 0 then path is split at this char
 *
 * format [protocol://[login[:password]@]][host[:port]]/path[OPTIONoption]
 *****************************************************************************/
static inline void vlc_UrlParse( vlc_url_t *url, const char *psz_url,
                                 char option )
{
    char *psz_dup;
    char *psz_parse;
    char *p;
    char *p2;

    url->psz_protocol = NULL;
    url->psz_username = NULL;
    url->psz_password = NULL;
    url->psz_host     = NULL;
    url->i_port       = 0;
    url->psz_path     = NULL;
    url->psz_option   = NULL;

    if( psz_url == NULL )
    {
        url->psz_buffer = NULL;
        return;
    }
    url->psz_buffer = psz_parse = psz_dup = strdup( psz_url );

    /* Search a valid protocol */
    p  = strstr( psz_parse, ":/" );
    if( p != NULL )
    {
        for( p2 = psz_parse; p2 < p; p2++ )
        {
#define I(i,a,b) ( (a) <= (i) && (i) <= (b) )
            if( !I(*p2, 'a', 'z' ) && !I(*p2, 'A', 'Z') && !I(*p2, '0', '9') && *p2 != '+' && *p2 != '-' && *p2 != '.' )
            {
                p = NULL;
                break;
            }
#undef I
        }
    }

    if( p != NULL )
    {
        /* we have a protocol */

        /* skip :// */
        *p++ = '\0';
        if( p[1] == '/' )
            p += 2;
        url->psz_protocol = psz_parse;
        psz_parse = p;
    }
    p = strchr( psz_parse, '@' );
    p2 = strchr( psz_parse, '/' );
    if( p != NULL && ( p2 != NULL ? p < p2 : 1 ) )
    {
        /* We have a login */
        url->psz_username = psz_parse;
        *p++ = '\0';

        psz_parse = strchr( psz_parse, ':' );
        if( psz_parse != NULL )
        {
            /* We have a password */
            *psz_parse++ = '\0';
            url->psz_password = psz_parse;
            decode_URI( url->psz_password );
        }
        decode_URI( url->psz_username );
        psz_parse = p;
    }

    p = strchr( psz_parse, '/' );
    if( !p || psz_parse < p )
    {
        /* We have a host[:port] */
        url->psz_host = strdup( psz_parse );
        if( p )
        {
            url->psz_host[p - psz_parse] = '\0';
        }

        if( *url->psz_host == '[' )
        {
            /* Ipv6 address */
            p2 = strchr( url->psz_host, ']' );
            if( p2 )
            {
                p2 = strchr( p2, ':' );
            }
        }
        else
        {
            p2 = strchr( url->psz_host, ':' );
        }
        if( p2 )
        {
            *p2++ = '\0';
            url->i_port = atoi( p2 );
        }
    }
    psz_parse = p;

    /* Now parse psz_path and psz_option */
    if( psz_parse )
    {
        url->psz_path = psz_parse;
        if( option != '\0' )
        {
            p = strchr( url->psz_path, option );
            if( p )
            {
                *p++ = '\0';
                url->psz_option = p;
            }
        }
    }
}

/*****************************************************************************
 * vlc_UrlClean:
 *****************************************************************************/
static inline void vlc_UrlClean( vlc_url_t *url )
{
    free( url->psz_buffer );
    free( url->psz_host );

    url->psz_protocol = NULL;
    url->psz_username = NULL;
    url->psz_password = NULL;
    url->psz_host     = NULL;
    url->i_port       = 0;
    url->psz_path     = NULL;
    url->psz_option   = NULL;

    url->psz_buffer   = NULL;
}



/** Check whether a given string is not a valid URL and must hence be
 *  encoded */
static inline int vlc_UrlIsNotEncoded( const char *psz_url )
{
    const char *ptr;

    for( ptr = psz_url; *ptr; ptr++ )
    {
        unsigned char c = *ptr;

        if( c == '%' )
        {
            if( !isxdigit( (unsigned char)ptr[1] )
             || !isxdigit( (unsigned char)ptr[2] ) )
                return 1; /* not encoded */
            ptr += 2;
        }
        else
        if(  ( (unsigned char)( c - 'a' ) < 26 )
          || ( (unsigned char)( c - 'A' ) < 26 )
          || ( (unsigned char)( c - '0' ) < 10 )
          || ( strchr( "-_.", c ) != NULL ) )
            return 1;
    }
    return 0; /* looks fine - but maybe it is not encoded */
}

#endif
