/*
 * (C) 2011-2021 by Christian Hesse <mail@eworm.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#define _POSIX_C_SOURCE 200809L
#include <mpd/client.h>

#ifdef HAVE_LIBAV
#include <libavformat/avformat.h>
#include <magic.h>
#endif
#include <regex.h>
#include <string.h>

#include "mpd-notification.h"
#include "cache.h"

#ifdef HAVE_LIBAV
	magic_t magic = NULL;
#endif

#define CACHE_SIZE 3

struct song_data {
	struct mpd_song * song;
	unsigned id;
	GdkPixbuf * pixbuf;
};

/*** retrieve_artwork ***/
GdkPixbuf * retrieve_artwork_from_disk(const char * music_dir, const char * uri) {
	GdkPixbuf * pixbuf = NULL;
	char * uri_path = NULL, * imagefile = NULL;
	DIR * dir;
	struct dirent * entry;
	regex_t regex;

#ifdef HAVE_LIBAV
	int i;
	const char *magic_mime;
	AVFormatContext * pFormatCtx = NULL;
	GdkPixbufLoader * loader;

	/* try album artwork first */
	if ((uri_path = malloc(strlen(music_dir) + strlen(uri) + 2)) == NULL) {
		fprintf(stderr, "%s: malloc() failed.\n", get_program_name());
		goto fail;
	}

	sprintf(uri_path, "%s/%s", music_dir, uri);

	if ((magic_mime = magic_file(magic, uri_path)) == NULL) {
		fprintf(stderr, "%s: We did not get a MIME type...\n", get_program_name());
		goto image;
	}

	if (get_verbose() > 0)
		printf("%s: MIME type for %s is: %s\n", get_program_name(), uri_path, magic_mime);

	if (strcmp(magic_mime, "audio/mpeg") != 0)
		goto image;

	if ((pFormatCtx = avformat_alloc_context()) == NULL) {
		fprintf(stderr, "%s: avformat_alloc_context() failed.\n", get_program_name());
		goto image;
	}

	if (avformat_open_input(&pFormatCtx, uri_path, NULL, NULL) != 0) {
		fprintf(stderr, "%s: avformat_open_input() failed.\n", get_program_name());
		goto image;
	}

	if (pFormatCtx->iformat->read_header(pFormatCtx) < 0) {
		fprintf(stderr, "%s: Could not read the format header.\n", get_program_name());
		goto image;
	}

	/* find the first attached picture, if available */
	for (i = 0; i < pFormatCtx->nb_streams; i++) {
		if (pFormatCtx->streams[i]->disposition & AV_DISPOSITION_ATTACHED_PIC) {
			AVPacket pkt;

			if (get_verbose() > 0)
				printf("%s: Found artwork in media file.\n", get_program_name());

			pkt = pFormatCtx->streams[i]->attached_pic;

			loader = gdk_pixbuf_loader_new();
			if (gdk_pixbuf_loader_write(loader, pkt.data, pkt.size, NULL) == FALSE) {
				fprintf(stderr, "%s: gdk_pixbuf_loader_write() failed parsing buffer.\n", get_program_name());
				goto image;
			}

			if ((pixbuf = gdk_pixbuf_loader_get_pixbuf(loader)) == NULL) {
				fprintf(stderr, "%s: gdk_pixbuf_loader_get_pixbuf() failed creating pixbuf.\n", get_program_name());
				goto image;
			}

			gdk_pixbuf_loader_close(loader, NULL);
			goto done;
		}
	}

image:
#endif

	/* cut the file name from path for current directory */
	*strrchr(uri_path, '/') = 0;

	if ((dir = opendir(uri_path)) == NULL) {
		fprintf(stderr, "%s: Could not open directory '%s': %s", get_program_name(), uri_path, strerror(errno));
		goto fail;
	}

	if (regcomp(&regex, REGEX_ARTWORK, REG_NOSUB + REG_ICASE) != 0) {
		fprintf(stderr, "%s: Could not compile regex.\n", get_program_name());
		goto fail;
	}

	while ((entry = readdir(dir))) {
		if (*entry->d_name == '.')
			continue;

		if (regexec(&regex, entry->d_name, 0, NULL, 0) == 0) {
			if (get_verbose() > 0)
				printf("%s: Found image file: %s\n", get_program_name(), entry->d_name);

			if ((imagefile = malloc(strlen(uri_path) + strlen(entry->d_name) + 2)) == NULL) {
				fprintf(stderr, "%s: malloc() failed.\n", get_program_name());
				goto fail;
			}

			sprintf(imagefile, "%s/%s", uri_path, entry->d_name);

			if ((pixbuf = gdk_pixbuf_new_from_file(imagefile, NULL)) == NULL) {
				fprintf(stderr, "%s: gdk_pixbuf_new_from_file() failed loading file: %s\n",
						get_program_name(), imagefile);
				goto fail;
			}

			free(imagefile);
			break;
		}
	}

	regfree(&regex);
	closedir(dir);

fail:
#ifdef HAVE_LIBAV
done:
	if (pFormatCtx != NULL) {
		avformat_close_input(&pFormatCtx);
		avformat_free_context(pFormatCtx);
	}
#endif

	free(uri_path);

	return pixbuf;
}

int cache_tail = 0;
GdkPixbuf * pixbuf_cache[CACHE_SIZE]; // A single pixbuf that's cached
char * uri_cache[CACHE_SIZE]; // The uri of the cached pixbuf

// Add a pixbuf to the cache
void add_to_cache(GdkPixbuf * pixbuf, const char * uri) {
	if (pixbuf_cache[cache_tail])
	{
		printf("Replacing object from the cache\n");
		g_object_unref(pixbuf_cache[cache_tail]);
	}

	pixbuf_cache[cache_tail] = pixbuf;

	if (uri_cache[cache_tail])
		free(uri_cache[cache_tail]);

	uri_cache[cache_tail] = strdup(uri);

	cache_tail = (cache_tail+1) % CACHE_SIZE;
}

GdkPixbuf * retrieve_artwork_from_cache(const char * uri) {
	if (!uri)
		return NULL;

	for (size_t i = 0; i < CACHE_SIZE; i++) {
		if (uri_cache[i] && strcmp(uri, uri_cache[i]) == 0)
			return pixbuf_cache[i];
	}
	return NULL;
}

GdkPixbuf * retrieve_artwork(const char * music_dir, const char * uri) {
	printf("Retrieving artwork %s\n", uri);
	for (size_t i = 0; i < CACHE_SIZE; i++) {
		printf("Cached artworks [%zu] %s\n", i, uri_cache[i]);
	}

	GdkPixbuf * pixbuf = retrieve_artwork_from_cache(uri);
	if (pixbuf) {
		printf("Getting icon from cache\n");
		return pixbuf;
	}

	pixbuf = retrieve_artwork_from_disk(music_dir, uri);
	add_to_cache(pixbuf, uri);
	return pixbuf;
}

struct song_data * get_song_data(struct mpd_connection * conn, unsigned id, const char * music_dir) {
	struct song_data * ret = malloc(sizeof(struct song_data));
	ret->id = id;
	ret->song = mpd_run_get_queue_song_id(conn, id);
	const char * uri = mpd_song_get_uri(ret->song);
	ret->pixbuf = retrieve_artwork(music_dir, uri);
	return ret;
}

bool av_init() {
#ifdef HAVE_LIBAV
	/* libav */
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
	av_register_all();
#endif

	/* only fatal messages from libav */
	if (get_verbose() == 0)
		av_log_set_level(AV_LOG_FATAL);

	if ((magic = magic_open(MAGIC_MIME_TYPE)) == NULL) {
		fprintf(stderr, "%s: Could not initialize magic library.\n", get_program_name());
		return false;
	}

	if (magic_load(magic, NULL) != 0) {
		fprintf(stderr, "%s: Could not load magic database: %s\n", get_program_name(), magic_error(magic));
		magic_close(magic);
		return false;
	}
#endif
	return true;
}

void av_magic_close() {
	if (magic != NULL)
		magic_close(magic);
}
