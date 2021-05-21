/*
 * funkinchartpak by Regan "CuckyDev" Green
 * Packs Friday Night Funkin' json formatted charts into a binary file for the PSX port
*/

#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <algorithm>

#include "json.hpp"
using json = nlohmann::json;

#define SECTION_FLAG_ALTANIM  (1 << 0) //Mom/Dad in Week 5
#define SECTION_FLAG_OPPFOCUS (1 << 1) //Focus on opponent

struct Section
{
	uint16_t end;
	uint8_t flag = 0, pad = 0;
};

#define NOTE_FLAG_OPPONENT    (1 << 2) //Note is opponent's
#define NOTE_FLAG_SUSTAIN     (1 << 3) //Note is a sustain note
#define NOTE_FLAG_SUSTAIN_END (1 << 4) //Draw as sustain ending (this sucks)
#define NOTE_FLAG_HIT         (1 << 7) //Note has been hit

struct Note
{
	uint16_t pos; //Quarter steps
	uint8_t type, pad = 0;
};

uint16_t PosRound(double pos, double crochet)
{
	return (uint16_t)std::floor(pos / crochet + 0.5f);
}

void WriteWord(std::ostream &out, uint16_t word)
{
	out.put(word >> 0);
	out.put(word >> 8);
}

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		std::cout << "usage: funkinchartpak in_json" << std::endl;
		return 0;
	}
	
	//Read json
	std::ifstream i(argv[1]);
	if (!i.is_open())
	{
		std::cout << "Failed to open " << argv[1] << std::endl;
		return 1;
	}
	json j;
	i >> j;
	
	auto song_info = j["song"];
	
	double bpm = song_info["bpm"];
	double crochet = (60.0 / bpm) * 1000.0;
	double step_crochet = crochet / 4;
	double speed = song_info["speed"];
	std::cout << "bpm: " << bpm << " crochet: " << crochet << " step_crochet: " << step_crochet << " speed: " << speed << std::endl;
	
	std::vector<Section> sections;
	std::vector<Note> notes;
	
	uint16_t section_end = 0;
	for (auto &i : song_info["notes"]) //Iterate through sections
	{
		bool is_opponent = i["mustHitSection"] != true; //Note: swapped
		
		//Read section
		Section new_section;
		new_section.end = (section_end += (uint16_t)i["lengthInSteps"]);
		if (i["alt_anim"] == true)
			new_section.flag |= SECTION_FLAG_ALTANIM;
		if (is_opponent)
			new_section.flag |= SECTION_FLAG_OPPFOCUS;
		sections.push_back(new_section);
		
		//Read notes
		for (auto &j : i["sectionNotes"])
		{
			//Push main note
			Note new_note;
			new_note.pos = PosRound((double)j[0] * 4.0, step_crochet);
			new_note.type = (uint8_t)j[1];
			if (is_opponent)
				new_note.type ^= NOTE_FLAG_OPPONENT;
			notes.push_back(new_note);
			
			//Push sustain notes
			int sustain = (int)PosRound(j[2], step_crochet) - 1;
			for (int k = 0; k <= sustain; k++)
			{
				Note sus_note; //jerma
				sus_note.pos = new_note.pos + (k << 2); //Starts at first note's position?
				sus_note.type = new_note.type | NOTE_FLAG_SUSTAIN;
				if (k == sustain)
					sus_note.type |= NOTE_FLAG_SUSTAIN_END;
				notes.push_back(sus_note);
			}
		}
	}
	
	//Sort notes
	std::sort(notes.begin(), notes.end(), [](Note a, Note b) {
		if (a.pos == b.pos)
			return (b.type & NOTE_FLAG_SUSTAIN) && !(a.type & NOTE_FLAG_SUSTAIN);
		else
			return a.pos < b.pos;
	});
	
	//Push dummy section and note
	//Section dum_section;
	//dum_section.end = 0xFFFF;
	//sections.push_back(dum_section);
	sections[sections.size() - 1].end = 0xFFFF;
	
	Note dum_note;
	dum_note.pos = 0xFFFF;
	dum_note.type = NOTE_FLAG_HIT;
	notes.push_back(dum_note);
	
	//Write to output
	std::ofstream out(std::string(argv[1]) + ".cht", std::ostream::binary);
	if (!out.is_open())
	{
		std::cout << "Failed to open " << argv[1] << ".cht" << std::endl;
		return 1;
	}
	
	//Write sections
	WriteWord(out, 2 + (sections.size() << 2));
	for (auto &i : sections)
	{
		WriteWord(out, i.end);
		out.put(i.flag);
		out.put(0);
	}
	
	//Write notes
	for (auto &i : notes)
	{
		WriteWord(out, i.pos);
		out.put(i.type);
		out.put(0);
	}
	return 0;
}
