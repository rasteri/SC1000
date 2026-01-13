/*
 * Copyright (C) 2014 Mark Hills <mark@xwax.org>
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

#include <fstream>
#include <sstream>
#include <cstdlib>

#include "cues.h"
#include "../util/log.h"

namespace {

// Replace file extension with ".cue"
// Returns empty string if no extension found
std::string get_cue_path(const char* pathname)
{
    if (pathname == nullptr) {
        return {};
    }

    std::string path(pathname);
    auto dot_pos = path.rfind('.');
    if (dot_pos == std::string::npos) {
        return {};
    }

    path.replace(dot_pos, std::string::npos, ".cue");
    return path;
}

} // anonymous namespace

void Cues::set(unsigned int label, double position)
{
    positions_[label] = position;
}

std::optional<double> Cues::get(unsigned int label) const
{
    auto it = positions_.find(label);
    if (it != positions_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void Cues::unset(unsigned int label)
{
    positions_.erase(label);
}

void Cues::reset()
{
    positions_.clear();
}

bool Cues::is_set(unsigned int label) const
{
    return positions_.find(label) != positions_.end();
}

void Cues::load_from_file(const char* pathname)
{
    std::string cuepath = get_cue_path(pathname);
    if (cuepath.empty()) {
        return;
    }

    std::ifstream file(cuepath);
    if (!file) {
        return;
    }

    positions_.clear();

    std::string line;
    unsigned int index = 0;
    while (std::getline(file, line)) {
        if (line.empty()) {
            index++;
            continue;
        }

        std::istringstream iss(line);
        double position;
        if (iss >> position) {
            // Only store if not the sentinel value
            if (position != CUE_FILE_UNSET) {
                positions_[index] = position;
            }
        }
        index++;
    }
}

void Cues::save_to_file(const char* pathname) const
{
    // Don't save if cue 0 is at position 0.0 (likely uninitialized)
    auto cue0 = positions_.find(0);
    if (cue0 != positions_.end() && cue0->second == 0.0) {
        return;
    }

    // Don't save if no cues are set
    if (positions_.empty()) {
        return;
    }

    std::string cuepath = get_cue_path(pathname);
    if (cuepath.empty()) {
        return;
    }

    LOG_DEBUG("Saving cue: %s", cuepath.c_str());

    std::ofstream file(cuepath);
    if (!file) {
        return;
    }

    // Find the highest label to know how many lines to write
    unsigned int max_label = 0;
    for (const auto& [label, pos] : positions_) {
        if (label > max_label) {
            max_label = label;
        }
    }

    // Write positions, using CUE_FILE_UNSET for gaps
    for (unsigned int i = 0; i <= max_label; ++i) {
        auto it = positions_.find(i);
        if (it != positions_.end()) {
            file << it->second << '\n';
        } else {
            file << CUE_FILE_UNSET << '\n';
        }
    }

    file.close();

    // Sync the file to disk
    std::string sync_cmd = "/bin/sync \"" + cuepath + "\"";
    std::system(sync_cmd.c_str());
}
