// SONIC ROBO BLAST 2 KART SATURN
//-----------------------------------------------------------------------------
// Copyright (C) 2025 by Indev.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  m_emotes.cpp
/// \brief Chat emotes handling

#include <map>
#include <list>
#include <string>
#include <sstream>
#include <cstring>

#include "m_emotes.h"
#include "i_time.h" // I_GetTime

extern "C" {
#include "w_wad.h"
#include "z_zone.h"
#include "hu_stuff.h"
#include "v_video.h"
#include "command.h"
}

static void Command_ListEmotes_f(void);

consvar_t cv_emotes = {"emotes", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

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

			if (emote_name.find_first_of(" \t\n\r\f\v:") != emote_name.npos)
			{
				CONS_Alert(CONS_WARNING, "EMOTES: Emote name cannot contain spaces or ':' symbols. (file %s, line %d)\n", wadfiles[wadnum]->filename, linenum);
				continue;
			}

			if (emote_name.size() > MAXEMOTENAME)
			{
				CONS_Alert(CONS_WARNING, "EMOTES: Emote name is too long, truncating. (file %s, line %d)\n", wadfiles[wadnum]->filename, linenum);
				emote_name.resize(MAXEMOTENAME);
			}

			emote = &emotes[emote_name];
			strlcpy(emote->name, emote_name.c_str(), MAXEMOTENAME);

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

			if (fasticmp(field.c_str(), "frames"))
			{
				UINT8 numframes = 0;

				auto copy_frame = [&] {
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

					// Only print this warning if we actually got new frame
					if (numframes == MAXEMOTEFRAMES)
					{
						CONS_Alert(CONS_WARNING, "EMOTES: Too many frames. (file %s, line %d)\n", wadfiles[wadnum]->filename, linenum);
						return false;
					}

					if (framelumpname.size() > 8)
					{
						CONS_Alert(CONS_WARNING, "EMOTES: Frame %d name is too long. (file %s, line %d)\n", numframes, wadfiles[wadnum]->filename, linenum);
						framelumpname.resize(8);
					}

					std::strncpy(emote->frames[numframes], framelumpname.c_str(), framelumpname.size()+1);

					++numframes;

					return true;
				};

				while (copy_frame()) {}

				if (numframes == 0)
				{
					CONS_Alert(CONS_WARNING, "EMOTES: Expected list of frames. (file %s, line %d)\n", wadfiles[wadnum]->filename, linenum);
				}

				emote->numframes = numframes;
			}
			else if (fasticmp(field.c_str(), "timeperframe"))
			{
				emote->timeperframe = std::stoi(value);

				if (emote->timeperframe <= 0)
				{
					emote->timeperframe = 1;
					CONS_Alert(CONS_WARNING, "EMOTES: Bad value for 'timeperframe'. (file %s, line %d)\n", wadfiles[wadnum]->filename, linenum);
				}
			}
			else
			{
				CONS_Alert(CONS_WARNING, "EMOTES: Unrecognized field '%s'. (file %s, line %d)\n", field.c_str(), wadfiles[wadnum]->filename, linenum);
			}
		}
		else
		{
			CONS_Alert(CONS_WARNING, "EMOTES: Unrecognized line. (file %s, line %d)\n", wadfiles[wadnum]->filename, linenum);
		}
	}

	CONS_Printf("Added %d emotes\n", numemotes);
}

void M_InitEmotes(void)
{
	UINT16 i;
	for (i = 0; i < numwadfiles; i++)
		M_LoadEmotes(i);

	COM_AddCommand("listemotes", Command_ListEmotes_f);
}

emote_t *M_FindEmote(const char *name, int len, int skip)
{
	if (!cv_emotes.value)
		return nullptr;

	char query[MAXEMOTENAME+1] = {0};
	std::strncpy(query, name, std::min(len, MAXEMOTENAME));

	for (auto &pair: emotes)
	{
		if (strcasestr(pair.first.c_str(), query) == NULL)
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
	if (*name != ':')
		return nullptr;

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

void M_DrawScaledEmote(fixed_t x, fixed_t y, fixed_t scale, emote_t *emote, INT32 flags)
{
	if (emote->numframes == 0)
		return;

	const char *lumpname = emote->frames[(I_GetTime()/emote->timeperframe) % emote->numframes];
	patch_t *emotepatch = (patch_t*)W_CachePatchName(lumpname, PU_CACHE);

	const int CHARHEIGHT = 6;

	if (emotepatch->width > EMOTEWIDTH)
		scale = FixedMul(scale, (FRACUNIT/emotepatch->width)*EMOTEWIDTH);
	else if (emotepatch->width < EMOTEWIDTH)
		x += (EMOTEWIDTH-emotepatch->width)*FRACUNIT/2;

	y -= (scale*emotepatch->height-CHARHEIGHT*FRACUNIT)/2;

	V_DrawFixedPatch(x, y, scale, flags, emotepatch, NULL);
}

static void Command_ListEmotes_f(void)
{
	const int EMOTES_PER_PAGE = 24;
	const int NUMPAGES = emotes.size()/EMOTES_PER_PAGE + (emotes.size() % EMOTES_PER_PAGE != 0 ? 1 : 0);
	int page = 1;

	if (COM_Argc() > 1)
		page = std::atoi(COM_Argv(1));

	if (page <= 0 || page > NUMPAGES)
	{
		if (NUMPAGES > 1)
			CONS_Printf("Enter page number between 1 and %d\n", NUMPAGES);
		else
			CONS_Printf("Bad page number, try using this command without arguments\n");
		return;
	}

	int i = 0;
	for (auto &pair: emotes)
	{
		++i;

		if (i <= (page-1)*EMOTES_PER_PAGE)
			continue;
		if (i > page*EMOTES_PER_PAGE)
			break;

		CONS_Printf("%s - :%s:\n", pair.first.c_str(), pair.first.c_str());
	}

	CONS_Printf("Page %d/%d\n", page, NUMPAGES);
}
