/*
 * Copyright (C) 2019 Andrew Tait <rasteri@gmail.com>
 * Copyright (C) 2024-2026 Niklas Klügel <lodsb@lodsb.org>
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


// SC1000 playlist routines
// Manages a tree of folders and audio files using std::vector

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>

#include "playlist.h"
#include "../util/log.h"

bool Playlist::load(const char* base_folder_path)
{
	struct dirent** dir_list;
	struct dirent** file_list;

	LOG_DEBUG("indexing %s", base_folder_path);

	int num_dirs = scandir(base_folder_path, &dir_list, nullptr, alphasort);
	if (num_dirs <= 0) {
		return false;
	}

	folders_.clear();
	all_files_.clear();
	total_files_ = 0;

	for (int d = 0; d < num_dirs; d++) {
		// Skip hidden files/folders
		if (dir_list[d]->d_name[0] == '.') {
			free(dir_list[d]);
			continue;
		}

		std::string subfolder_path = std::string(base_folder_path) + "/" + dir_list[d]->d_name;

		int num_files = scandir(subfolder_path.c_str(), &file_list, nullptr, alphasort);
		if (num_files <= 0) {
			free(dir_list[d]);
			continue;
		}

		ScFolder folder;
		folder.full_path = subfolder_path;

		for (int f = 0; f < num_files; f++) {
			// Skip hidden files and .cue files
			if (file_list[f]->d_name[0] == '.' ||
			    strstr(file_list[f]->d_name, ".cue") != nullptr) {
				free(file_list[f]);
				continue;
			}

			ScFile file;
			file.full_path = subfolder_path + "/" + file_list[f]->d_name;
			file.global_index = static_cast<unsigned int>(total_files_);
			total_files_++;

			folder.files.push_back(std::move(file));
			free(file_list[f]);
		}
		free(file_list);

		// Only add folder if it has files
		if (!folder.files.empty()) {
			folders_.push_back(std::move(folder));
		}

		free(dir_list[d]);
	}
	free(dir_list);

	// Build flat file index for O(1) random access
	all_files_.reserve(total_files_);
	for (auto& folder : folders_) {
		for (auto& file : folder.files) {
			all_files_.push_back(&file);
		}
	}

	LOG_INFO("Added folder %s: %zu files found", base_folder_path, total_files_);

	return total_files_ > 0;
}

ScFile* Playlist::get_file_at_index(unsigned int index)
{
	if (index >= all_files_.size()) {
		return nullptr;
	}
	return all_files_[index];
}

ScFolder* Playlist::get_folder(size_t folder_idx)
{
	if (folder_idx >= folders_.size()) {
		return nullptr;
	}
	return &folders_[folder_idx];
}

ScFile* Playlist::get_file(size_t folder_idx, size_t file_idx)
{
	if (folder_idx >= folders_.size()) {
		return nullptr;
	}
	if (file_idx >= folders_[folder_idx].files.size()) {
		return nullptr;
	}
	return &folders_[folder_idx].files[file_idx];
}

size_t Playlist::file_count_in_folder(size_t folder_idx) const
{
	if (folder_idx >= folders_.size()) {
		return 0;
	}
	return folders_[folder_idx].files.size();
}

bool Playlist::has_next_file(size_t folder_idx, size_t file_idx) const
{
	if (folder_idx >= folders_.size()) {
		return false;
	}
	return file_idx + 1 < folders_[folder_idx].files.size();
}

bool Playlist::has_prev_file(size_t folder_idx, size_t file_idx) const
{
	return file_idx > 0;
}

bool Playlist::has_next_folder(size_t folder_idx) const
{
	return folder_idx + 1 < folders_.size();
}

bool Playlist::has_prev_folder(size_t folder_idx) const
{
	return folder_idx > 0;
}

void Playlist::dump() const
{
	for (const auto& folder : folders_) {
		for (const auto& file : folder.files) {
			LOG_DEBUG("%s - %s", folder.full_path.c_str(), file.full_path.c_str());
		}
	}
}
