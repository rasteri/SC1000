/*
 * Copyright (C) 2014 Mark Hills <mark@xwax.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "cues.h"
#include "debug.h"

void cues_reset(struct cues *q)
{
    size_t n;

    for (n = 0; n < MAX_CUES; n++)
        q->position[n] = CUE_UNSET;
}

/*
 * Unset the given cue point
 */

void cues_unset(struct cues *q, unsigned int label)
{
    debug("clearing cue point %d", label);
	printf("clearing cue point %d\n", label);
    q->position[label] = CUE_UNSET;
}

void cues_set(struct cues *q, unsigned int label, double position)
{
    debug("setting cue point %d to %0.2f", label, position);
	printf("setting cue point %d to %0.2f\n", label, position);
    assert(label < MAX_CUES);
    q->position[label] = position;
}

double cues_get(const struct cues *q, unsigned int label)
{
    assert(label < MAX_CUES);
	printf("getting cue point %d\n", label);
    return q->position[label];
}

/*
 * Return: the previous cue point before the current position, or CUE_UNSET
 */

double cues_prev(const struct cues *q, double current)
{
    size_t n;
    double r;

    r = CUE_UNSET;

    for (n = 0; n < MAX_CUES; n++) {
        double p;

        p = q->position[n];
        if (p == CUE_UNSET)
            continue;

        if (p > r && p < current)
            r = p;
    }

    return r;
}

/*
 * Return: the next cue point after the given position, or CUE_UNSET
 */

double cues_next(const struct cues *q, double current)
{
    size_t n;
    double r;

    r = CUE_UNSET;

    for (n = 0; n < MAX_CUES; n++) {
        double p;

        p = q->position[n];
        if (p == CUE_UNSET)
            continue;

        if (p < r && p > current)
            r = p;
    }

    return r;
}


/*
 * Initialize Cuepoints and read the from *.cue file if present
 * xwaxed
 */
void cues_load_from_file(struct cues *q, char const* pathname)
{
    char* cuepath = replace_path_ext(pathname);
    if (cuepath == NULL)
        return;

    FILE* f = fopen(cuepath, "r");

    int i;
    //for (i = 0; i < MAX_CUEPOINTS; i++)
    //    record->cuepoints[i] = 0.0;

    if (f == NULL) {
        free(cuepath);
        return;
    }

    i = 0;
    while (fscanf(f, "%lf", &(q->position[i])) != EOF)
        i++;

    fclose(f);
    free(cuepath);
}

/*
 * Write Cuepoints to *.cue file if at least one cuepoint is set
 * xwaxed
 */

void cues_save_to_file(struct cues *q, char const* pathname)
{
    static char syncCommandLine[300];
    if (q->position[0] == 0.0)
        return;

    char* cuepath = replace_path_ext(pathname);
    if (cuepath == NULL)
        return;
    fprintf(stdout, "Saving Loc: %s\n", cuepath);

    FILE* f = fopen(cuepath, "w");
    if (f == NULL) {
        free(cuepath);
        return;
    }

    int i;
    for (i = 0; i < MAX_CUES; i++)
        fprintf(f, "%lf\n", q->position[i]);

    fclose(f);
    sprintf(syncCommandLine, "/bin/sync %s", cuepath);
    system(syncCommandLine);
    free(cuepath);

}

/*
 * xwaxed
 * Replace normal extension on given path, with ".cue"
 * 
 * Return: pointer to the new pathname with cue extension
 *         NULL on allocation failure
 *
 * Beware: Free the allocated memory!
 */

char* replace_path_ext(char const* pathname)
{
        char* ext = ".cue";

        char* tempstring = malloc(strlen(pathname) + 1);
        if (tempstring == NULL) {
            perror("malloc");
            return NULL;
        }

        strcpy(tempstring, pathname);
        strcpy(strrchr(tempstring, '.'), "");

        char* cuepath = malloc(strlen(tempstring) + strlen(ext) + 1);
        if (cuepath == NULL) {
            perror("malloc");
            return NULL;
        }

        strcpy(cuepath, tempstring);
        strncat(cuepath, ext, strlen(ext));
        free(tempstring);

        return cuepath;
}
