#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "sc_midimap.h"

void add_MIDI_mapping(struct mapping **maps, unsigned char buf[3], unsigned char DeckNo, unsigned char Action, unsigned char Param){
	struct mapping *new_map = (struct mapping*) malloc(sizeof(struct mapping));
	new_map->MidiBytes[0] = buf[0];new_map->MidiBytes[1] = buf[1];new_map->MidiBytes[2] = buf[2];
	new_map->DeckNo = DeckNo;
	new_map->Action = Action;
	new_map->Param = Param;
	new_map->next = NULL;
	new_map->Type = MAP_MIDI;
	//printf("Adding Mapping - %x %x %x - dn:%d, a:%d, p:%d\n", buf[0], buf[1], buf[2], DeckNo, Action, Param); 
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

void add_IO_mapping(struct mapping **maps, unsigned char Pin, bool Pullup, bool Edge, unsigned char DeckNo, unsigned char Action, unsigned char Param){
	struct mapping *new_map = (struct mapping*) malloc(sizeof(struct mapping));
	new_map->Pin = Pin;
	new_map->Pullup = Pullup;
	new_map->Edge = Edge;
	new_map->DeckNo = DeckNo;
	new_map->Action = Action;
	new_map->Param = Param;
	new_map->next = NULL;
	new_map->Type = MAP_IO;
	//printf("Adding Mapping - pn%x pl:%x ed%x - dn:%d, a:%d, p:%d\n", Pin, Pullup, Edge, DeckNo, Action, Param); 
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

void add_GPIO_mapping(struct mapping **maps, unsigned char port, unsigned char Pin, bool Pullup, bool Edge, unsigned char DeckNo, unsigned char Action, unsigned char Param){
	struct mapping *new_map = (struct mapping*) malloc(sizeof(struct mapping));
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
struct mapping *find_MIDI_mapping(struct mapping *maps, unsigned char buf[3]){
	
	
	struct mapping *last_map = maps;
	// Interpret zero-velocity notes as note-off commands
	if (((buf[0] & 0xF0) == 0x90) && (buf[2] == 0x00)){
		buf[0] = 0x80 | (buf[0] & 0x0F);
	}
	
	while (last_map != NULL){
			
		if ( last_map->Type == MAP_MIDI &&
			(((last_map->MidiBytes[0] & 0xF0) == 0xE0) && last_map->MidiBytes[0] == buf[0]) || //Pitch bend messages only match on first byte
			(last_map->MidiBytes[0] == buf[0] && last_map->MidiBytes[1] == buf[1]) //Everything else matches on first two bytes
			
		
		){
			return last_map;
		} 
		
		last_map = last_map->next;
	}
	return NULL;
}

// Find a mapping from a IO event
struct mapping *find_IO_mapping(struct mapping *maps, unsigned char pin, bool edge){
	
	struct mapping *last_map = maps;
	
	while (last_map != NULL){
			
		if (last_map->Type == MAP_IO && last_map->Pin == pin && last_map->Edge == edge){
			return last_map;
		} 
		
		last_map = last_map->next;
	}
	return NULL;
}

// Find a mapping from a GPIO event
struct mapping *find_GPIO_mapping(struct mapping *maps, unsigned char port, unsigned char pin, bool edge){
	
	struct mapping *last_map = maps;
	
	while (last_map != NULL){
			
		if (last_map->Type == MAP_GPIO && last_map->Pin == pin && last_map->Edge == edge && last_map->port == port){
			return last_map;
		} 
		
		last_map = last_map->next;
	}
	return NULL;
}

