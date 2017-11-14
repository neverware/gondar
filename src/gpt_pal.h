// Copyright 2017 Neverware
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef GPT_PAL_H
#define GPT_PAL_H

// We use gdisk to clean up the GPT such that Windows is happy writing to
// the disk

// This function is kept in a separate file to avoid an issue with pragmas
// within gdisk affecting rufus-based functionality (getting disk extents)
bool clearMbrGpt(const char* physical_path);
bool makeEmptyPartition(const char* physical_path);

#endif
