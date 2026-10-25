/*
 * Airports, airlines, cities and the timetable R Departures shows.
 *
 * The flights are generated, not fetched: each airport has a list of real
 * routes, and every day gets the same timetable for them, with a few delays
 * and cancellations decided by a hash of the date.
 */
#ifndef DATA_H
#define DATA_H

#ifdef RDEPARTURES_QT
#include "HaikuTypes.h"
#else
#include <SupportDefs.h>
#endif
#include <time.h>


enum {
	LANG_EN = 0,
	LANG_KO,
	LANG_JA,
	LANG_FR,
	LANG_IT,
	LANG_ZH,
	LANG_DE,
	LANG_ES,
	LANG_ZHT,	// Traditional Chinese, as in Hong Kong and Taipei
	LANG_OTHER,
	// A display phase, not a language: each row in the language of the
	// country it flies to.
	LANG_DEST = 100
};

enum {
	STATUS_ON_TIME = 0,
	STATUS_CHECK_IN,
	STATUS_GATE_OPEN,
	STATUS_BOARDING,
	STATUS_FINAL_CALL,
	STATUS_GATE_CLOSED,
	STATUS_DEPARTED,
	STATUS_DELAYED,
	STATUS_CANCELLED,
	STATUS_COUNT
};

enum {
	LABEL_TIME = 0,
	LABEL_NEW_TIME,
	LABEL_DESTINATION,
	LABEL_AIRLINE,
	LABEL_FLIGHT,
	LABEL_GATE,
	LABEL_REMARKS,
	LABEL_COUNT
};

struct City {
	const char*	code;
	// English, Korean, Japanese, French, Italian, Traditional Chinese: the
	// languages the airports speak.
	const char*	name[6];
	// The name in the city's own language, and that language.
	const char*	native;
	int			lang;
};

struct Airline {
	const char*	code;
	const char*	name;
	uint8		bg[3];
	uint8		fg[3];
};

struct Route {
	const char*	city;
	const char*	airline;
	int			number;
	int			perDay;
};

struct Airport {
	const char*	code;
	int			lang;
	const char*	nameLocal;
	const char*	nameEnglish;
	// Minutes east of UTC in winter, and which summer time rule applies.
	int			utcOffset;
	int			dstRule;
	const char*	gateLetters;
	int			gateMin;
	int			gateMax;
	bool		padNumbers;
	const Route* routes;
	int			routeCount;
	int			phases[3];
	int			phaseCount;
};

struct Flight {
	int			route;
	int			number;
	int64		scheduled;	// minutes since the epoch, airport local time
	int			delay;		// minutes
	bool		cancelled;
	char		gate[8];
};

extern const Airport kAirports[];
extern const int kAirportCount;

const City*		find_city(const char* code);
const Airline*	find_airline(const char* code);
int				airline_count();
const Airline*	airline_at(int index);

const char*		city_name(const City* city, int lang);
int				remarks_lang(int lang);
const char*		status_text(int status, int lang);
const char*		label_text(int label, int lang);
const char*		title_text(int lang);
int				footer_count();
const char*		footer_text(int index, int lang);
void			format_date(char* out, int size, int64 localMinutes, int lang);

enum { DST_NONE = 0, DST_EU, DST_US, DST_AU };

int64			local_minutes(int utcOffset, int dstRule, time_t utc);
int64			airport_local_minutes(const Airport& airport, time_t utc);
int				build_departures(const Airport& airport, int64 now,
					Flight* out, int max);
int				flight_status(const Flight& flight, int64 now);
void			format_flight_number(char* out, int size,
					const Airport& airport, const Flight& flight);

#endif	// DATA_H
