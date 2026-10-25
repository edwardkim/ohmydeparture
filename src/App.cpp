/*
 * R Departures: an airport departures board, full screen, no window border.
 *
 *   RDepartures                     the board, at the last airport chosen
 *   RDepartures --airport CDG       start at this airport
 *
 * For checking it without a screen (over ssh, say):
 *
 *   RDepartures --snapshot out.png [--size 1600x768] [--wait 4]
 *       [--airport ICN|TOKYO|SEOUL|VICTORIA|SYDNEY] [--phase 1]
 *       [--offset minutes]
 *
 * renders the board off-screen after --wait seconds of animation and saves
 * it. --phase fixes the language phase (TOKYO: board * 2 + language, SEOUL:
 * language, VICTORIA: page of calling points),
 * --offset moves the clock.
 *
 * Esc or Q quits, the arrow keys and space change the airport, the gear in
 * the top right corner picks one.
 */

#include <Application.h>
#include <Bitmap.h>
#include <BitmapStream.h>
#include <File.h>
#include <FindDirectory.h>
#include <Path.h>
#include <Screen.h>
#include <TranslatorRoster.h>
#include <Window.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Board.h"
#include "Data.h"
#include "Station.h"


static const char* kSignature = "application/x-vnd.rainygirl-RDepartures";

int64 gTimeOffset = 0;


static BPath
settings_path()
{
	BPath path;
	find_directory(B_USER_SETTINGS_DIRECTORY, &path);
	path.Append("RDepartures_settings");
	return path;
}


void
save_airport_setting(const char* code)
{
	BFile file(settings_path().Path(),
		B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() == B_OK)
		file.Write(code, strlen(code));
}


static int
airport_index(const char* code)
{
	for (int i = 0; i < kAirportCount; i++) {
		if (strcasecmp(kAirports[i].code, code) == 0)
			return i;
	}
	for (int i = 0; i < kStationCount; i++) {
		if (strcasecmp(kStations[i].code, code) == 0)
			return kAirportCount + i;
	}
	return -1;
}


static int
saved_airport()
{
	BFile file(settings_path().Path(), B_READ_ONLY);
	char code[8] = { 0 };
	if (file.InitCheck() != B_OK || file.Read(code, sizeof(code) - 1) <= 0)
		return 0;
	int index = airport_index(code);
	return index >= 0 ? index : 0;
}


class BoardWindow : public BWindow {
public:
	BoardWindow(BRect frame, int airport)
		:
		BWindow(frame, "R Departures", B_NO_BORDER_WINDOW_LOOK,
			B_NORMAL_WINDOW_FEEL, B_NOT_MOVABLE | B_NOT_RESIZABLE
				| B_NOT_ZOOMABLE | B_NOT_MINIMIZABLE | B_ASYNCHRONOUS_CONTROLS)
	{
		AddChild(new BoardView(Bounds(), airport));
	}

	virtual void ScreenChanged(BRect frame, color_space mode)
	{
		MoveTo(frame.LeftTop());
		ResizeTo(frame.Width(), frame.Height());
	}

	virtual bool QuitRequested()
	{
		be_app->PostMessage(B_QUIT_REQUESTED);
		return true;
	}
};


static int
snapshot(const char* path, int width, int height, int airport, int phase,
	float wait)
{
	BoardView* board = new BoardView(BRect(0, 0, width - 1, height - 1),
		airport);
	if (phase >= 0)
		board->SetFixedPhase(phase);
	board->LayoutBoard(BRect(0, 0, width - 1, height - 1));
	bigtime_t end = system_time() + (bigtime_t)(wait * 1000000);
	while (system_time() < end) {
		board->Tick();
		snooze(33333);
	}

	BBitmap* copy = new BBitmap(board->Bitmap());
	BBitmapStream stream(copy);
	BFile file(path, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	status_t status = BTranslatorRoster::Default()->Translate(&stream, NULL,
		NULL, &file, B_PNG_FORMAT);
	printf("%s: %s\n", path, strerror(status));
	delete board;
	return status == B_OK ? 0 : 1;
}


int
main(int argc, char** argv)
{
	BApplication app(kSignature);

	int airport = saved_airport();
	const char* snapshotPath = NULL;
	int width = 0;
	int height = 0;
	int phase = -1;
	float wait = 4;
	for (int i = 1; i < argc; i++) {
		bool more = i + 1 < argc;
		if (strcmp(argv[i], "--airport") == 0 && more) {
			int index = airport_index(argv[++i]);
			if (index >= 0)
				airport = index;
		} else if (strcmp(argv[i], "--snapshot") == 0 && more)
			snapshotPath = argv[++i];
		else if (strcmp(argv[i], "--size") == 0 && more)
			sscanf(argv[++i], "%dx%d", &width, &height);
		else if (strcmp(argv[i], "--phase") == 0 && more)
			phase = atoi(argv[++i]);
		else if (strcmp(argv[i], "--wait") == 0 && more)
			wait = atof(argv[++i]);
		else if (strcmp(argv[i], "--offset") == 0 && more)
			gTimeOffset = (int64)atoi(argv[++i]) * 60;
	}

	BRect frame = BScreen().Frame();
	if (width <= 0 || height <= 0) {
		width = frame.IntegerWidth() + 1;
		height = frame.IntegerHeight() + 1;
	}
	if (snapshotPath != NULL)
		return snapshot(snapshotPath, width, height, airport, phase, wait);

	BoardWindow* window = new BoardWindow(frame, airport);
	window->Show();
	app.Run();
	return 0;
}
