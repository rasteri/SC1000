// SC1000 playlist routines
// Build a linked-list tree of every track on the usb stick


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdint.h>
#include <stdbool.h>
#include <dirent.h>
#include <string.h>
#include "sc_playlist.h"

struct File * GetFileAtIndex(unsigned int index, struct Folder *FirstFolder) {

	struct Folder *CurrFolder = FirstFolder;
	struct File *CurrFile = FirstFolder->FirstFile;

	bool FoundIt = false;

	while (!FoundIt) {
		if (CurrFile->Index == index) {
			return CurrFile;
		}

		else {
			CurrFile = CurrFile->next;
			if (CurrFile == NULL) {
				CurrFolder = CurrFolder->next;
				if (CurrFolder == NULL)
					return NULL;

				CurrFile = CurrFolder->FirstFile;
			}
		}
	}
	return NULL;
}

struct Folder * LoadFileStructure(char *BaseFolderPath,
		unsigned int *TotalNum) {

	struct Folder *prevFold = NULL;
	struct File *prevFile = NULL;

	struct dirent **dirList, **fileList;
	int n, m, o, p;

	struct Folder *FirstFolder = NULL;
	struct File *FirstFile = NULL;

	struct File* new_file;
	struct Folder* new_folder;

	char tempName[257];
	unsigned int FilesCount = 0;
	
	*TotalNum = 0;

	n = scandir(BaseFolderPath, &dirList, 0, alphasort);
	if (n <= 0){
		return NULL;
	}
	for (o = 0; o < n; o++) {
		if (dirList[o]->d_name[0] != '.') {

			sprintf(tempName, "%s%s", BaseFolderPath, dirList[o]->d_name);

			m = scandir(tempName, &fileList, 0, alphasort);

			FirstFile = NULL;
			prevFile = NULL;

			for (p = 0; p < m; p++) { 
				if (fileList[p]->d_name[0] != '.' && strstr(fileList[p]->d_name, ".cue") == NULL) {

					new_file = (struct File*) malloc(sizeof(struct File));
					snprintf(new_file->FullPath, 256, "%s/%s", tempName,
							fileList[p]->d_name);

					new_file->Index = FilesCount++;

					// set up prev connection
					new_file->prev = prevFile;

					prevFile = new_file;

					// next connection (NULL FOR NOW)
					new_file->next = NULL;

					// and prev's next connection (points to this object)
					if (new_file->prev != NULL)
						new_file->prev->next = new_file;

					if (FirstFile == NULL)
						FirstFile = new_file;

					/*printf("Added file %d - %s\n", new_file->Index,
							new_file->FullPath);*/

				}
				free(fileList[p]);
			}

			if (FirstFile != NULL) { // i.e. we found at least one file

				new_folder = (struct Folder*) malloc(sizeof(struct Folder));
				sprintf(new_folder->FullPath, "%s%s", BaseFolderPath,
						dirList[o]->d_name);

				// set up prev connection
				new_folder->prev = prevFold;

				prevFold = new_folder;

				// next connection (NULL FOR NOW)
				new_folder->next = NULL;

				// and prev's next connection (points to this object)
				if (new_folder->prev != NULL)
					new_folder->prev->next = new_folder;

				if (FirstFolder == NULL) {
					FirstFolder = new_folder;
				}

				new_folder->FirstFile = FirstFile;

				//printf("Added Subfolder %s\n", new_folder->FullPath);

			}

			free(fileList);

		}

		free(dirList[o]);

	}

	free(dirList);

	*TotalNum = FilesCount;
	
	printf ("Added folder %s : %d files found\n", BaseFolderPath, FilesCount);

	return FirstFolder;

}

void DumpFileStructure(struct Folder *FirstFolder) {

	struct Folder *currFold = FirstFolder;
	struct File *currFile;

	do {
		currFile = currFold->FirstFile;

		if (currFile != NULL) {

			do {
				printf("%s - %s\n", currFold->FullPath, currFile->FullPath);

			} while ((currFile = currFile->next) != NULL);
		}

	} while ((currFold = currFold->next) != NULL);

}
