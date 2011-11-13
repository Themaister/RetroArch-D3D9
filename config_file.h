/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef __CONFIG_FILE_H
#define __CONFIG_FILE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>

typedef struct config_file config_file_t;

/////
// Config file format
// - # are treated as comments. Rest of the line is ignored.
// - Format is: key = value. There can be as many spaces as you like in-between.
// - Value can be wrapped inside "" for multiword strings. (foo = "hai u")
//
// - #include includes a config file in-place.
// Path is relative to where config file was loaded unless an absolute path is chosen.
// Key/value pairs from an #include are read-only, and cannot be modified.

// Loads a config file. Returns NULL if file doesn't exist.
// NULL path will create an empty config file.
config_file_t *config_file_new(const char *path);
// Frees config file.
void config_file_free(config_file_t *conf);

// All extract functions return true when value is valid and exists.
// Returns false otherwise.

bool config_entry_exists(config_file_t *conf, const char *entry);

// Extracts a double from config file.
bool config_get_double(config_file_t *conf, const char *entry, double *in);
// Extracts an int from config file.
bool config_get_int(config_file_t *conf, const char *entry, int *in);
// Extracts an int from config file. (Hexadecimal)
bool config_get_hex(config_file_t *conf, const char *entry, unsigned *in);
// Extracts a single char. If value consists of several chars, this is an error.
bool config_get_char(config_file_t *conf, const char *entry, char *in);
// Extracts an allocated string in *in. This must be free()-d if this function succeeds.
bool config_get_string(config_file_t *conf, const char *entry, char **in);
// Extracts a string to a preallocated buffer. Avoid memory allocation.
bool config_get_array(config_file_t *conf, const char *entry, char *in, size_t size);
// Extracts a boolean from config. Valid boolean true are "true" and "1". Valid false are "false" and "0". Other values will be treated as an error.
bool config_get_bool(config_file_t *conf, const char *entry, bool *in);

// Setters. Similar to the getters. Will not write to entry if the entry
// was obtained from an #include.
void config_set_double(config_file_t *conf, const char *entry, double value);
void config_set_int(config_file_t *conf, const char *entry, int val);
void config_set_char(config_file_t *conf, const char *entry, char val);
void config_set_string(config_file_t *conf, const char *entry, const char *val);
void config_set_bool(config_file_t *conf, const char *entry, bool val);

// Write the current config to a file.
bool config_file_write(config_file_t *conf, const char *path);

// Dump the current config to an already opened file. Does not close the file.
void config_file_dump(config_file_t *conf, FILE *file);
// Also dumps inherited values, useful for logging.
void config_file_dump_all(config_file_t *conf, FILE *file);

#ifdef __cplusplus
}
#endif

#endif

