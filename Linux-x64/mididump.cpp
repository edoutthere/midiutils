//  mididump  (Linux version)
//
//  Utility to dump a MIDI binary files contents to the console
//
//  I could not find anything that just dumped the contents of a MIDI file
//  so I wrote this.  Not intended to be complete, but it shows the notes
//  and controls is a MIDI file.
//  Example output is at the end of this file
//
// 06/16/2021   edouththere
//              created
//
// TODO:
//
//  Copyright (C) 2022  Ed Out There  edoutthere@outlook.com
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//  
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//  
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.
//-------------------------------------------------------------------------------------

//#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include "midi_notes.h"


typedef struct mthd
{
	unsigned int name;
	unsigned int len;
	unsigned short format;
	unsigned short tracks;
	unsigned short division;
} MT_HEADER;

typedef struct mtrk
{
	unsigned int name;
	unsigned int len;
} MT_TRACK;

int print_mthd(unsigned char*);
unsigned int endian32(unsigned int);
unsigned short endian16(unsigned short);

int infile;
unsigned char* filebuf;
MT_HEADER* MThd;
MT_TRACK* MTrk;
struct stat fstatus;

int main(int argc, char **argv)
{
	int i, len, tempo;
	unsigned char* chptr;
	int num_tracks,  record_len;
	int track;
	int event_time, track_event, midi_channel;

	if( argc < 2 )
		{
		printf("Usage: mididump file\n");
		return(-1);
		}

	printf("Dumping %s\n", argv[1]);

	infile = open( argv[1], O_RDONLY);
	if (infile == -1)
	{
		printf("cant open input file %s\n", argv[1]);
		return(-1);
	}

	// read the entire file into memory
	fstat(infile, &fstatus);

	filebuf = (unsigned char*)malloc(fstatus.st_size);
	if (filebuf == NULL)
	{
		printf("Unable to malloc\n");
		return(-2);
	}

	printf("Reading %d bytes\n", fstatus.st_size);
	read(infile, filebuf, fstatus.st_size);

	chptr = filebuf;
	num_tracks = print_mthd(chptr);
	printf("File has %d tracks\n", num_tracks);

	// skip header record to first track
	chptr += 14;

	// loop on the track records
	// these start with a MTrk record header
	// end with FF 2F 00 (end of track)
	for (track = 0; track < num_tracks; track++)
	{
		// start by getting the length of the track record
		MTrk = (MT_TRACK*)chptr;
		record_len = (int)endian32(MTrk->len);
		printf("TRACK %d:  len: %d\n", track, record_len);
		chptr += 8;  // skip the track record

		// for each track, process each track event
		track_event = 1;
		while (track_event > 0)
		{
			// track event
			// events start with the time: 7 bits per byte, most significant bits first 
			// All bytes except the last have bit 7 set, and the last byte has bit 7 clear
			// just add all the value of the 7 bits
			// Note: system events have 0 for time and use one byte
			printf("  ");
			for (i = 0; i < 10; i++)
				printf("%02X ", *(chptr + i));
			printf("\n");

			event_time = 0;
			while (1)
				{
				event_time <<= 7;
				if ((*chptr & 0x80) == 0)
					{
					event_time += (*chptr & 0x7f);
					chptr++;
					break;
					}
				else
					event_time += (*chptr & 0x7f);
				chptr++;
				}
			printf("  Event %d\n    time: %d\n", track_event, event_time);

			// now for the actual event
			//  <MIDI event> | <sysex event> | <meta-event> 
			// midi events have lots of types and have variable lens
			// all of these have the MSB of the command byte set to 1
			if (*chptr == 0xff)
			{
				printf("    Meta:\n");
				chptr++;
				switch (*chptr)
				{
				case 0x03:
					printf("      Track Name\n");
					chptr++;
					len = *chptr++;
					printf("      Len: %d\n      ", len);
					for (i = 0; i < len ; i++)
						printf("%c", *chptr++);
					printf("\n");
					break;
				case 0x2f:
					printf("      End of track\n");
					chptr += 2;
					track_event = -1;
					break;
				case 0x51:
					// FF 51 03 07 A1 20
					printf("      Tempo:\n");
					chptr++;
					len = *chptr++;
					printf("      Len: %d\n", len);
					tempo = 0;
					for (i = 0; i < len; i++)
						tempo += *chptr++;
					printf("      Tempo: %d us/quarter note\n", tempo);
					break;
				case 0x58:
					// FF 58 04 04 02 18 08
					printf("      Time Signature:\n");
					chptr++;
					printf("        %d/%d\n", *chptr++, *chptr++);
					chptr += 3;
					break;
				default:
					break;
				}
			}
			else if ((*chptr & 0x80) == 0x80)
			{
				// <time> 90 41 40  note on
				// <time> 80 41 00  note off
				printf("    Midi:\n");
				switch (*chptr & 0xf0)
				{
				case 0x90:
					printf("      note on: ");
					midi_channel = *chptr & 0xf;
					chptr++;
					printf("%s (%d)\n", midi_notes[*chptr], *chptr );
					//printf("CH: %d\n", midi_channel);
					chptr+=2;
					break;
				case 0x80:
					printf("      note off: ");
					midi_channel = *chptr & 0xf;
					chptr++;
					printf("%s (%d)\n", midi_notes[*chptr], *chptr );
					//printf("CH: %d\n", midi_channel);
					chptr+=2;
					break;
				default:
					break;
				}
			}
			track_event++;
		}
		//chptr += record_len+8;
	}
}

//-------------------------------------------------------------------------
// Print the header record
// header_chunk = "MThd" + <header_length> + <format> + <n> + <division>
//                 uchar32   uint32           uint16    uint16   uint16
//
int print_mthd(unsigned char* arg)
{
	MThd = (MT_HEADER*)arg;

	printf("Header: \n");
	printf("      len: %d\n", endian32(MThd->len));
	printf("      fmt: %d\n", endian16(MThd->format));
	printf(" num trks: %d\n", endian16(MThd->tracks));
	printf("      div: %d ticks/quarter note\n\n", endian16(MThd->division));  // ticks/quarter note

	return((int)endian16(MThd->tracks));
}

// Print the track record
// "MTrk" + <header_length> + <len>
//             uint32         uint32
//
int print_mtrk(unsigned char* arg)
{
	MTrk = (MT_TRACK*)arg;

	printf("Track: \n");
	printf("  len: %d\n\n", endian32(MTrk->len));

	return((int)endian32(MTrk->len));
}

unsigned int endian32(unsigned int arg)
{
	unsigned int retval;

	retval = (arg >> 24) & 0xff;
	retval |= (arg >> 8) & 0xff00;
	retval |= (arg << 8) & 0xff0000;
	retval |= (arg << 24) & 0xff000000;

	return(retval);

}

unsigned short endian16(unsigned short arg)
{
	unsigned short retval;

	retval = (arg >> 8) & 0xff;
	retval |= (arg << 8) & 0xff00;

	return(retval);

}

/*  EXAMPLE OUTPUT
Dumping test.mid
Reading 117 bytes
Header:
	  len: 6
	  fmt: 1
 num trks: 2
	  div: 192 ticks/quarter note

File has 2 tracks
TRACK 0:  len: 35
  00 FF 58 04 04 02 18 08 00 FF
  Event 1
	time: 0
	Meta:
	  Time Signature:
		4/4
  00 FF 51 03 07 A1 20 00 FF 03
  Event 2
	time: 0
	Meta:
	  Tempo:
	  Len: 3
	  Tempo: 200 us/quarter note
  00 FF 03 0B 54 65 6D 70 6F 20
  Event 3
	time: 0
	Meta:
	  Track Name
	  Len: 11
	  Tempo Track
  BC 00 FF 2F 00 4D 54 72 6B 00
  Event 4
	time: 7680
	Meta:
	  End of track
TRACK 1:  len: 52
  00 FF 03 0E 4E 65 77 20 49 6E
  Event 1
	time: 0
	Meta:
	  Track Name
	  Len: 14
	  New Instrument
  84 7F 90 3C 50 87 0E 80 3C 00
  Event 2
	time: 639
	Midi:
	  note on: C  (60)
  87 0E 80 3C 00 81 33 90 3E 50
  Event 3
	time: 910
	Midi:
	  note off: C  (60)
  81 33 90 3E 50 87 19 80 3E 00
  Event 4
	time: 179
	Midi:
	  note on: D  (62)
  87 19 80 3E 00 7F 90 40 50 85
  Event 5
	time: 921
	Midi:
	  note off: D  (62)
  7F 90 40 50 85 72 80 40 00 A0
  Event 6
	time: 127
	Midi:
	  note on: E  (64)
  85 72 80 40 00 A0 36 FF 2F 00
  Event 7
	time: 754
	Midi:
	  note off: E  (64)
  A0 36 FF 2F 00 00 00 00 A1 2E
  Event 8
	time: 4150
	Meta:
	  End of track

*/
