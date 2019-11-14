#include <stdio.h>
#include <string.h>
#include "sc_midimap.h"

void add_mapping(struct mapping **maps, unsigned char buf[3], unsigned char DeckNo, unsigned char Action, unsigned char Param){
	struct mapping *new_map = (struct mapping*) malloc(sizeof(struct mapping));
	new_map->MidiBytes[0] = buf[0];new_map->MidiBytes[1] = buf[1];new_map->MidiBytes[2] = buf[2];
	new_map->DeckNo = DeckNo;
	new_map->Action = Action;
	new_map->Param = Param;
	new_map->next = NULL;
	printf("Adding Mapping - %x %x %x - dn:%d, a:%d, p:%d\n", buf[0], buf[1], buf[2], DeckNo, Action, Param); 
	if (*maps == NULL){
		*maps = new_map;
	}
	else {
		struct mapping *last_map = *maps;
		
		while (last_map->next != NULL){
			last_map = last_map->next;
		}
		
		last_map->next = new_map;
	}
	
}

// Find a mapping from a MIDI event
struct mapping *find_mapping(struct mapping *maps, unsigned char buf[3]){
	
	
	struct mapping *last_map = maps;
	
	while (last_map != NULL){
			// Ignore last byte
		if (last_map->MidiBytes[0] == buf[0] && last_map->MidiBytes[1] == buf[1]){
			return last_map;
		}
		
		last_map = last_map->next;
	}
	return NULL;
}

