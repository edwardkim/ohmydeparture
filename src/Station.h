/*
 * Railway stations: Tokyo, Seoul, London Victoria and Sydney Central, each
 * in the style of its own departure boards rather than split flaps.
 *
 * Tokyo: JR departure boards, in the style of the LED and LCD boards above
 * the platforms there.
 *
 * Four boards take turns: the Tokaido Shinkansen, the Shinkansen to the
 * north, the Tokaido, Ueno-Tokyo and Yokosuka lines, and the Chuo Line. The
 * trains are a generated timetable, like the flights.
 */
#ifndef STATION_H
#define STATION_H

#ifdef RDEPARTURES_QT
#include "HaikuCompat.h"
#else
#include <SupportDefs.h>
#include <String.h>
#endif

#include "Data.h"


struct Service {
	const char*	typeJp;		// のぞみ, 東海道線, 快速 ...
	const char*	typeEn;
	uint8		color[3];
	const char*	destJp;
	const char*	destEn;
	int			number;		// first train number, 0 when trains have none
	int			interval;	// minutes between trains
	int			offset;		// minutes after the hour of the first one
	int			first;		// first and last departure, minutes after 00:00
	int			last;
	int			trackMin;
	int			trackMax;
	bool		underground;	// the Sobu/Yokosuka platforms below ground
	// One character per station column of the board: 'o' stops, '-' passes,
	// ' ' does not go that far. Boards without columns list stops instead.
	const char*	stops;
	const char*	stopsEn;
	// Shinkansen: which cars are unreserved, or NULL when all are reserved.
	const char*	unreserved;
	bool		mayStartHere;
};

struct StationBoard {
	const char*	titleJp;
	const char*	titleEn;
	const char*	directionJp;
	const char*	directionEn;
	bool		shinkansen;
	const char*	stationsJp[12];
	int			stationCount;
	const Service* services;
	int			serviceCount;
};

struct Train {
	const Service* service;
	int			number;
	int64		departs;	// minutes since the epoch, Tokyo time
	int			track;
	bool		startsHere;
};

extern const StationBoard kStationBoards[];
extern const int kStationBoardCount;

int		build_trains(const StationBoard& board, int boardIndex, int64 now,
			Train* out, int max);


// #pragma mark - the stations


enum {
	STYLE_JR = 0,		// Tokyo: JR LED/LCD boards, stopping patterns
	STYLE_KORAIL,		// Seoul: Korail departure screens, four languages
	STYLE_UK,			// London Victoria: a column per train, calling points
	STYLE_SYDNEY		// Sydney Central: Sydney Trains line badges
};

struct Station {
	const char*	code;		// for --airport and the settings file
	const char*	menuName;
	int			style;
	int			utcOffset;	// minutes east of UTC in winter
	int			dstRule;	// DST_* from Data.h
};

extern const Station kStations[];
extern const int kStationCount;


// When a service runs: every `interval` minutes from `first` + `offset` to
// `last` (minutes after midnight). Every service struct below starts with
// one, so build_slots() can walk any of them.
struct Timing {
	int			interval;
	int			offset;
	int			first;
	int			last;
};

struct Slot {
	int			service;
	int			index;		// the how-manieth train of the day
	int64		departs;	// minutes since the epoch, local time
};

int		build_slots(const void* services, int stride, int count, int64 now,
			int64 since, Slot* out, int max);


// Seoul: names in Korean, English, Japanese and Chinese.
struct KorailService {
	Timing		timing;
	const char*	type[4];
	uint8		color[3];
	const char*	dest[4];
	int			number;
	int			trackMin;
	int			trackMax;
	// Where it stops, by Korean name, separated by '|'.
	const char*	stops;
};

extern const KorailService kKorail[];
extern const int kKorailCount;

// The stops of a Seoul train in one of the four languages (0 Korean,
// 1 English, 2 Japanese, 3 Chinese), joined for the scrolling line.
BString	korail_stops(const KorailService& service, int lang);


// London Victoria.
struct CallingPoint {
	const char*	name;
	int			minutes;	// after departure
};

struct UkService {
	Timing		timing;
	const char*	dest;
	const char*	via;
	const CallingPoint* calls;
	int			callCount;
	int			cars;
	int			trackMin;
	int			trackMax;
};

extern const UkService kVictoria[];
extern const int kVictoriaCount;


// Sydney Central: the wall of portrait screens on the concourse, one per
// line or platform, grouped under Sydney services, Intercity, Regional and
// Metro.
enum {
	PANEL_LINE = 0,		// line badge, destination, stops, platform
	PANEL_CITY,			// the City Circle: a black band, many following trains
	PANEL_LONG,			// intercity and regional: the departure time first
	PANEL_NOTICE		// a printed notice instead of trains
};

struct SydneyRun {
	Timing		timing;
	const char*	dest;
	const char*	via;
	// Stations, separated by '|'; an empty one leaves a line free.
	const char*	stops;
	const char*	pattern;	// All Stops, Limited Stops, Express
	int			cars;
	int			platform;
	bool		booked;		// regional: booked seats only
};

struct SydneyPanel {
	int			kind;
	const char*	line;		// T1, M1 ...
	uint8		color[3];
	const SydneyRun* runs;
	int			runCount;
	const char*	notice;
};

extern const SydneyPanel kSydneyPanels[];
extern const int kSydneyPanelCount;	// 16: two rows of eight

#endif	// STATION_H
