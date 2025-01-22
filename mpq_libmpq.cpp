/*
 * This file is part of the Continued-MaNGOS Project
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "mpq_libmpq.h"
#include <deque>
#include <stdio.h>

ArchiveSet gOpenArchives;

MPQArchive::MPQArchive(const char* filename)
{
    int result = libmpq__archive_open(&mpq_a, filename, -1);
    printf("Opening %s\n", filename);
    if (result)
    {
        switch (result)
        {
            case LIBMPQ_ERROR_MALLOC:                   /* error on file operation */
                printf("Error opening archive '%s': Run off free RAM memory\n", filename);
                break;
            case LIBMPQ_ERROR_OPEN:
                printf("Error opening archive '%s': Can't open archive\n", filename);
                break;
            case LIBMPQ_ERROR_SEEK:
                printf("Error opening archive '%s': Can't seek begin of archive. File corrupt?\n", filename);
                break;
            case LIBMPQ_ERROR_FORMAT:
                printf("Error opening archive '%s': Invalid MPQ format. File corrupt?\n", filename);
                break;
            case LIBMPQ_ERROR_READ:
                printf("Error opening archive '%s': Can't read MPQ file. File corrupt?\n", filename);
                break;
        }
        return;
    }
    gOpenArchives.push_front(this);
}

void MPQArchive::close()
{
    libmpq__archive_close(mpq_a);
}

void MPQArchive::ListFiles() {
    vector<string> files;
    GetFileListTo(files);

    printf("Files in archive %p:\n", mpq_a);
    for (const auto& file : files) {
        printf("  %s\n", file.c_str());
    }
}

MPQFile::MPQFile(const char* filename):
    eof(false),
    buffer(0),
    pointer(0),
    size(0)
{
    printf("Attempting to open MPQ file: %s\n", filename);

    for (ArchiveSet::iterator i = gOpenArchives.begin(); i != gOpenArchives.end(); ++i)
    {
        mpq_archive* mpq_a = (*i)->mpq_a;

        uint32 filenum;

        printf("Searching archive %p for file...\n", mpq_a);

        if (libmpq__file_number(mpq_a, filename, &filenum))
        {
            printf("File not found in this archive\n");
            continue;
        }

        libmpq__off_t transferred;
        //libmpq__off_t size;
        libmpq__file_size_unpacked(mpq_a, filenum, &size);

        // HACK: in patch.mpq some files don't want to open and give 1 for filesize
        if (size <= 1)
        {
            // printf("info: file %s has size %d; considered dummy file.\n", filename, size);
            eof = true;
            buffer = 0;
            return;
        }
        buffer = new char[size];

        //libmpq_file_getdata
        libmpq__file_read(mpq_a, filenum, (unsigned char*)buffer, size, &transferred);
        /*libmpq_file_getdata(&mpq_a, hash, fileno, (unsigned char*)buffer);*/

        printf("Successfully read file. Size: %lu, Transferred: %lu\n", (unsigned long)size, (unsigned long)transferred);

        return;

    }
    printf("Error: File %s not found in any open archive\n", filename);
    eof = true;
    buffer = 0;
}

size_t MPQFile::read(void* dest, size_t bytes)
{
    // Validate inputs and state
    if (eof || !dest || !buffer || bytes == 0) {
        return 0;
    }

    // Validate pointer is within bounds
    if (pointer >= size) {
        eof = true;
        return 0;
    }

    // Calculate remaining bytes and validate read size
    size_t remaining = size - pointer;
    size_t bytesToRead = std::min(bytes, remaining);

    // Bounds check the read operation
    try {
        memcpy(dest, &(buffer[pointer]), bytesToRead);
        pointer += bytesToRead;

        if (pointer >= size) {
            eof = true;
        }

        return bytesToRead;
    }
    catch (...) {
        eof = true;
        return 0;
    }
}

void MPQFile::seek(int offset)
{
    pointer = offset;
    eof = (pointer >= size);
}

void MPQFile::seekRelative(int offset)
{
    pointer += offset;
    eof = (pointer >= size);
}

void MPQFile::close()
{
    if (buffer) delete[] buffer;
    buffer = 0;
    eof = true;
}
