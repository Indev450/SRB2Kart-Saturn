#include <map>
#include <string>
#include <sstream>
#include <cstring>

#include "m_emotes.h"

extern "C" {
#include "w_wad.h"
}

#include "z_zone.h"

consvar_t cv_emotes = {"emotes", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};;

static std::map<std::string, emote_t> emotes;

// TODO - Would be really nice to generalize soc-like file parsing and not reimplement it 100 times :)))))
void M_LoadEmotes(UINT16 wadnum)
{
	UINT16 lumpnum = W_CheckNumForNamePwad("EMOTES", wadnum, 0);

	if (lumpnum == INT16_MAX)
		return;

	char *lump = static_cast<char*>(W_CacheLumpNumPwad(wadnum, lumpnum, PU_CACHE));
	size_t size = W_LumpLengthPwad(wadnum, lumpnum);

	std::istringstream file(std::string(lump, size));
	std::string line;

	emote_t *emote = nullptr;
	std::string emote_name;

	// Just smol lambda to remove all the trailing chars we might not want
	auto trim = [](std::string &str) {
		str.erase(0, str.find_first_not_of(" \t\n\r\f\v"));
		str.erase(str.find_last_not_of(" \t\n\r\f\v")+1);
	};

	int numemotes = 0;
	int linenum = 0;

	while (std::getline(file, line))
	{
		++linenum;

		trim(line);

		if (line.empty())
			continue;

		size_t split = line.find('=');

		// Didn't find the =, means we're about to parse `Emote <name>`
		if (split == line.npos)
		{
			split = line.find(' ');

			if (split == line.npos)
			{
				CONS_Alert(CONS_WARNING, "EMOTES: Unrecognized line. (file %s, line %d)\n", wadfiles[wadnum]->filename, linenum);
				continue;
			}

			if (strnicmp(line.c_str(), "emote", 5))
			{
				CONS_Alert(CONS_WARNING, "EMOTES: 'Emote' expected. (file %s, line %d)\n", wadfiles[wadnum]->filename, linenum);
				continue;
			}

			emote_name = line.substr(split+1);

			if (emote_name.find_first_of(" \t\n\r\f\v") != emote_name.npos)
			{
				CONS_Alert(CONS_WARNING, "EMOTES: Emote name cannot contain spaces. (file %s, line %d)\n", wadfiles[wadnum]->filename, linenum);
				continue;
			}

			// This first->second looks kind of weird i know
			emote = &emotes[emote_name];

			emote->timeperframe = 1;
			emote->numframes = 0;
			std::memset(emote->frames, 0, sizeof(emote->frames));

			++numemotes;
		}
		else if (emote)
		{
			std::string field = line.substr(0, split);
			std::string value = line.substr(split+1);

			trim(field);
			trim(value);

			if (stricmp(field.c_str(), "frames") == 0)
			{
				UINT8 numframes = 0;

				auto copy_frame = [&] {
					if (numframes == MAXEMOTEFRAMES)
					{
						CONS_Alert(CONS_WARNING, "EMOTES: Too many frames. (file %s, line %d)", wadfiles[wadnum]->filename, linenum);
						return false;
					}

					size_t list_split = value.find(',');
					std::string framelumpname = value.substr(0, list_split);
					trim(framelumpname);

					if (list_split == value.npos)
						value = "";
					else
						value = value.substr(list_split+1, value.npos);

					// End of list of frames
					if (framelumpname.size() == 0)
						return false;

					if (framelumpname.size() > 8)
					{
						CONS_Alert(CONS_WARNING, "EMOTES: Frame %d name is too long. (file %s, line %d)", numframes, wadfiles[wadnum]->filename, linenum);
						framelumpname = framelumpname.substr(0, 8);
					}

					std::strncpy(emote->frames[numframes], framelumpname.c_str(), framelumpname.size()+1);

					++numframes;

					return true;
				};

				while (copy_frame()) {}

				if (numframes == 0)
				{
					CONS_Alert(CONS_WARNING, "EMOTES: Expected list of frames. (file %s, line %d)", wadfiles[wadnum]->filename, linenum);
				}

				emote->numframes = numframes;
			}
			else if (stricmp(field.c_str(), "timeperframe") == 0)
			{
				emote->timeperframe = std::stoi(value);

				if (emote->timeperframe <= 0)
				{
					emote->timeperframe = 1;
					CONS_Alert(CONS_WARNING, "EMOTES: Bad value for 'timeperframe'. (file %s, line %d)", wadfiles[wadnum]->filename, linenum);
				}
			}
			else
			{
				CONS_Alert(CONS_WARNING, "EMOTES: Unrecognized field '%s'. (file %s, line %d)", field.c_str(), wadfiles[wadnum]->filename, linenum);
			}
		}
		else
		{
			CONS_Alert(CONS_WARNING, "EMOTES: Unrecognized line. (file %s, line %d)\n", wadfiles[wadnum]->filename, linenum);
		}
	}

	CONS_Printf("Added %d emotes\n", numemotes);
}

emote_t *M_FindEmote(const char *name, int skip)
{
	if (!cv_emotes.value)
		return nullptr;

	char query[MAXEMOTENAME+1] = {0};
	std::strncpy(query, name, MAXEMOTENAME);

	for (auto &pair: emotes)
	{
		if (pair.first.rfind(query, 0, MAXEMOTENAME) != 0)
			continue;

		if (skip > 0)
		{
			--skip;
			continue;
		}

		return &pair.second;
	}

	return nullptr;
}

emote_t *M_VerifyEmote(const char *name, int *emotelen)
{
	if (!cv_emotes.value)
		return nullptr;

	const char *p = name+1; // skip ':'
	emote_t *match = nullptr;

	while (*p && (p - name) < MAXEMOTENAME)
	{
		if (*p == ':')
		{
			std::string checkname = std::string(name+1, (p-name)-1);
			auto it = emotes.find(checkname);

			if (it != emotes.end())
				match = &it->second;

			break;
		}

		++p;
	}

	if (match && emotelen)
		*emotelen = p-name+1;

	return match;
}
