/*
 * Sense extraction and printing.
 *
 * Copyright (C) 2007 OSD Team <pvfs-osd@osc.edu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _SENSE_H
#define _SENSE_H

void osd_sense_extract(const uint8_t *sense, int len, int *key, int *asc_ascq);
char *osd_sense_as_string(const uint8_t *sense, int len);
const uint8_t *osd_sense_extract_csi(const uint8_t *sense, int len);

#endif /* _SENSE_H */
