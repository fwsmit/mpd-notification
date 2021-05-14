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

#ifndef CACHE_H
#define CACHE_H

#define _GNU_SOURCE

#include <libnotify/notify.h>
#include <stdbool.h>

struct song_data;

/**
 * Retrieves artwork for the given uri. The uri should be a subpath of the
 * music_dir. This function does not get the pixbuf from disk every time, but
 * instead caches the artworks.
 *
 * Returns a pixbuf which should not be freed by the caller.
 */
GdkPixbuf * retrieve_artwork(const char * music_dir, const char * uri);

bool av_init();
void av_magic_close();
const char * av_get_program_name();

#endif /* CACHE_H */
