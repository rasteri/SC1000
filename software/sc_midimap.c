#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "sc_midimap.h"
#include "xwax.h"

/*
void add_IO_mapping(struct mapping **maps, unsigned char Pin, bool Pullup, bool Edge, unsigned char DeckNo, unsigned char Action, unsigned char Param)
{
	struct mapping *new_map = (struct mapping *)malloc(sizeof(struct mapping));
	new_map->Pin = Pin;
	new_map->Pullup = Pullup;
	new_map->Edge = Edge;
	new_map->DeckNo = DeckNo;
	new_map->Action = Action;
	new_map->Param = Param;
	new_map->next = NULL;
	new_map->Type = MAP_IO;
	new_map->port = 0;
	//printf("Adding Mapping - pn%x pl:%x ed%x - dn:%d, a:%d, p:%d\n", Pin, Pullup, Edge, DeckNo, Action, Param);
	if (*maps == NULL)
	{
		*maps = new_map;
	}
	else
	{
		struct mapping *last_map = *maps;

		while (last_map->next != NULL)
		{
			last_map = last_map->next;
		}

		last_map->next = new_map;
	}
}*/

/*void add_GPIO_mapping(struct mapping **maps, unsigned char port, unsigned char Pin, bool Pullup, char Edge, unsigned char DeckNo, unsigned char Action, unsigned char Param)
{
	struct mapping *new_map = (struct mapping *)malloc(sizeof(struct mapping));
	new_map->Pin = Pin;
	new_map->port = port;
	new_map->Pullup = Pullup;
	new_map->Edge = Edge;
	new_map->DeckNo = DeckNo;
	new_map->Action = Action;
	new_map->Param = Param;
	new_map->next = NULL;
	new_map->Type = MAP_GPIO;
	printf("Adding Mapping - po:%d pn%x pl:%x ed%x - dn:%d, a:%d, p:%d\n", port, Pin, Pullup, Edge, DeckNo, Action, Param);
	if (*maps == NULL)
	{
		*maps = new_map;
	}
	else
	{
		struct mapping *last_map = *maps;

		while (last_map->next != NULL)
		{
			last_map = last_map->next;
		}

		last_map->next = new_map;
	}
}*/

/*
 * Process an IO event
 */
extern bool shifted;
extern int pitchMode;
void IOevent(struct mapping *map, unsigned char MidiBuffer[3])
{

	if (map != NULL)
	{
		//printf("Map notnull deck:%d edge:%d pin:%d action:%d param:%d\n", map->DeckNo, map->Edge, map->Pin, map->Action, map->Param);

		if (map->Action == ACTION_CUE)
		{
			unsigned int cuenum = 0;
			if (map->Type == MAP_MIDI)
				cuenum = map->MidiBytes[1];
			else
				cuenum = (map->port * 32) + map->Pin + 128;

			/*if (shifted)
				deck_unset_cue(&deck[map->DeckNo], cuenum);
			else*/
			deck_cue(&deck[map->DeckNo], cuenum);
		}
		else if (map->Action == ACTION_DELETECUE)
		{
			unsigned int cuenum = 0;
			if (map->Type == MAP_MIDI)
				cuenum = map->MidiBytes[1];
			else
				cuenum = (map->port * 32) + map->Pin + 128;

			//if (shifted)
				deck_unset_cue(&deck[map->DeckNo], cuenum);
			/*else
				deck_cue(&deck[map->DeckNo], cuenum);*/
		}
		else if (map->Action == ACTION_NOTE)
		{
			deck[map->DeckNo].player.nominal_pitch = pow(pow(2, (double)1 / 12), map->Param - 0x3C); // equal temperament
		}
		else if (map->Action == ACTION_STARTSTOP)
		{
			deck[map->DeckNo].player.stopped = !deck[map->DeckNo].player.stopped;
		}
		else if (map->Action == ACTION_SHIFTON)
		{
			shifted = 1;
		}
		else if (map->Action == ACTION_SHIFTOFF)
		{
			shifted = 0;
		}
		else if (map->Action == ACTION_NEXTFILE)
		{
			deck_next_file(&deck[map->DeckNo]);
		}
		else if (map->Action == ACTION_PREVFILE)
		{
			deck_prev_file(&deck[map->DeckNo]);
		}
		else if (map->Action == ACTION_RANDOMFILE)
		{
			deck_random_file(&deck[map->DeckNo]);
		}
		else if (map->Action == ACTION_NEXTFOLDER)
		{
			deck_next_folder(&deck[map->DeckNo]);
		}
		else if (map->Action == ACTION_PREVFOLDER)
		{
			deck_prev_folder(&deck[map->DeckNo]);
		}
		else if (map->Action == ACTION_RECORD)
		{
			deck_record(&deck[0]); // Always record on deck 0
		}
		else if (map->Action == ACTION_VOLUME)
		{
			deck[map->DeckNo].player.setVolume = 128.0 / (double)MidiBuffer[2];
		}
		else if (map->Action == ACTION_PITCH)
		{
			if (map->Type == MAP_MIDI)
			{
				double pitch = 0.0;
				// If this came from a pitch bend message, use 14 bit accuracy
				if ((MidiBuffer[0] & 0xF0) == 0xE0)
				{
					unsigned int pval = (((unsigned int)MidiBuffer[2]) << 7) | ((unsigned int)MidiBuffer[1]);
					pitch = (((double)pval - 8192.0) * ((double)scsettings.pitchrange / 819200.0)) + 1;
				}
				// Otherwise 7bit (boo)
				else
				{
					pitch = (((double)MidiBuffer[2] - 64.0) * ((double)scsettings.pitchrange / 6400.0) + 1);
				}

				deck[map->DeckNo].player.nominal_pitch = pitch;
			}
		}
		else if (map->Action == ACTION_JOGPIT)
		{
			pitchMode = map->DeckNo + 1;
			printf("Set Pitch Mode %d\n", pitchMode);
		}
		else if (map->Action == ACTION_JOGPSTOP)
		{
			pitchMode = 0;
		}
		else if (map->Action == ACTION_SC500)
		{

			printf("SC500 detected\n");
			
		}
		else if (map->Action == ACTION_VOLUP)
		{
			deck[map->DeckNo].player.setVolume += scsettings.volAmount;
			if (deck[map->DeckNo].player.setVolume > 1.0)
				deck[map->DeckNo].player.setVolume = 1.0;
		}
		else if (map->Action == ACTION_VOLDOWN)
		{
			deck[map->DeckNo].player.setVolume -= scsettings.volAmount;
			if (deck[map->DeckNo].player.setVolume < 0.0)
				deck[map->DeckNo].player.setVolume = 0.0;	
		}
		else if (map->Action == ACTION_VOLUHOLD)
		{
			deck[map->DeckNo].player.setVolume += scsettings.volAmountHeld;
			if (deck[map->DeckNo].player.setVolume > 1.0)
				deck[map->DeckNo].player.setVolume = 1.0;
		}
		else if (map->Action == ACTION_VOLDHOLD)
		{
			deck[map->DeckNo].player.setVolume -= scsettings.volAmountHeld;
			if (deck[map->DeckNo].player.setVolume < 0.0)
				deck[map->DeckNo].player.setVolume = 0.0;	
		}
	}
}

// Add a mapping from an action string and other params
void add_config_mapping(struct mapping **maps, unsigned char Type, unsigned char buf[3], unsigned char port, unsigned char Pin, bool Pullup, char Edge, unsigned char *actions)
{
	unsigned char deckno, action, parameter;

	// Extract deck no from action (CHx)
	if (actions[2] == '0')
		deckno = 0;
	if (actions[2] == '1')
		deckno = 1;

	// figure out which action it is
	if (strstr(actions + 4, "CUE") != NULL)
		action = ACTION_CUE;
	if (strstr(actions + 4, "DELETECUE") != NULL)
		action = ACTION_DELETECUE;
	else if (strstr(actions + 4, "SHIFTON") != NULL)
		action = ACTION_SHIFTON;
	else if (strstr(actions + 4, "SHIFTOFF") != NULL)
		action = ACTION_SHIFTOFF;
	else if (strstr(actions + 4, "STARTSTOP") != NULL)
		action = ACTION_STARTSTOP;
	else if (strstr(actions + 4, "GND") != NULL)
		action = ACTION_GND;
	else if (strstr(actions + 4, "NEXTFILE") != NULL)
		action = ACTION_NEXTFILE;
	else if (strstr(actions + 4, "PREVFILE") != NULL)
		action = ACTION_PREVFILE;
	else if (strstr(actions + 4, "RANDOMFILE") != NULL)
		action = ACTION_RANDOMFILE;
	else if (strstr(actions + 4, "NEXTFOLDER") != NULL)
		action = ACTION_NEXTFOLDER;
	else if (strstr(actions + 4, "PREVFOLDER") != NULL)
		action = ACTION_PREVFOLDER;
	else if (strstr(actions + 4, "PITCH") != NULL)
		action = ACTION_PITCH;
	else if (strstr(actions + 4, "JOGPIT") != NULL)
		action = ACTION_JOGPIT;
	else if (strstr(actions + 4, "JOGPSTOP") != NULL)
		action = ACTION_JOGPSTOP;
	else if (strstr(actions + 4, "RECORD") != NULL)
		action = ACTION_RECORD;
	else if (strstr(actions + 4, "VOLUME") != NULL)
		action = ACTION_VOLUME;
	else if (strstr(actions + 4, "VOLUP") != NULL)
		action = ACTION_VOLUP;
	else if (strstr(actions + 4, "VOLDOWN") != NULL)
		action = ACTION_VOLDOWN;
	else if (strstr(actions + 4, "VOLUHOLD") != NULL)
		action = ACTION_VOLUHOLD;
	else if (strstr(actions + 4, "VOLDHOLD") != NULL)
		action = ACTION_VOLDHOLD;
	else if (strstr(actions + 4, "NOTE") != NULL)
	{
		action = ACTION_NOTE;
		parameter = atoi(actions + 9);
	}
	add_mapping(maps, Type, deckno, buf, port, Pin, Pullup, Edge, action, parameter);
}

void add_mapping(struct mapping **maps, unsigned char Type, unsigned char deckno, unsigned char buf[3], unsigned char port, unsigned char Pin, bool Pullup, char Edge, unsigned char action, unsigned char parameter)
{
	struct mapping *new_map = (struct mapping *)malloc(sizeof(struct mapping));
	if (Type == MAP_IO)
	{
		new_map->Pin = Pin;
		new_map->port = port;
		new_map->Pullup = Pullup;
	}
	else if (Type == MAP_MIDI)
	{
		new_map->MidiBytes[0] = buf[0];
		new_map->MidiBytes[1] = buf[1];
		new_map->MidiBytes[2] = buf[2];
	}

	new_map->Edge = Edge;
	new_map->Action = action;
	new_map->Param = parameter;
	new_map->next = NULL;
	new_map->Type = Type;
	new_map->DeckNo = deckno;
	new_map->debounce = 0;

	printf("Adding Mapping - po:%d pn%x pl:%x ed%x - dn:%d, a:%d, p:%d\n", port, Pin, Pullup, Edge, deckno, action, parameter);

	if (*maps == NULL)
	{
		*maps = new_map;
	}
	else
	{
		struct mapping *last_map = *maps;

		while (last_map->next != NULL)
		{
			last_map = last_map->next;
		}

		last_map->next = new_map;
	}
}

// Find a mapping from a MIDI event
struct mapping *find_MIDI_mapping(struct mapping *maps, unsigned char buf[3], char edge)
{

	struct mapping *last_map = maps;
	// Interpret zero-velocity notes as note-off commands
	if (((buf[0] & 0xF0) == 0x90) && (buf[2] == 0x00))
	{
		buf[0] = 0x80 | (buf[0] & 0x0F);
	}

	while (last_map != NULL)
	{

		if (last_map->Type == MAP_MIDI &&
				(((last_map->MidiBytes[0] & 0xF0) == 0xE0) && last_map->MidiBytes[0] == buf[0]) || //Pitch bend messages only match on first byte
			(last_map->MidiBytes[0] == buf[0] && last_map->MidiBytes[1] == buf[1])				   //Everything else matches on first two bytes
			&& last_map->Edge == edge
		)
		{
			return last_map;
		}

		last_map = last_map->next;
	}
	return NULL;
}

// Find a mapping from a GPIO event
struct mapping *find_IO_mapping(struct mapping *maps, unsigned char port, unsigned char pin, char edge)
{

	struct mapping *last_map = maps;

	while (last_map != NULL)
	{

		if (last_map->Type == MAP_IO && last_map->Pin == pin && last_map->Edge == edge && last_map->port == port)
		{
			return last_map;
		}

		last_map = last_map->next;
	}
	return NULL;
}
