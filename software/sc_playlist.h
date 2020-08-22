#ifndef PLAYLIST_H
#define PLAYLIST_H

// Folders contain files
struct Folder {
	char FullPath[260];
	struct File* FirstFile;
	struct Folder* next;
	struct Folder* prev;
};

// Struct to hold file (beat or sample)
struct File {
	char FullPath[260];
	unsigned int Index;
	struct File* next;
	struct File* prev;
};

struct File * GetFileAtIndex(unsigned int index, struct Folder *FirstFolder);

struct Folder * LoadFileStructure(char *BaseFolderPath, unsigned int *TotalNum);

void DumpFileStructure(struct Folder *FirstFolder);
#endif