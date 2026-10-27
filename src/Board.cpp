/*
 * The board. Every character sits on a cell of its own -- a flap -- and a
 * changed cell rattles through a few other characters before it settles,
 * one flap at a time, like the mechanical boards.
 *
 * Faces (one cell's picture of one character) are drawn once with the
 * Interface Kit and cached; the flips are composed in software straight into
 * the board bitmap, a row of pixels at a time, which is what keeps several
 * hundred moving flaps affordable on an Atom.
 *
 * Written so that gcc2 builds it: no C++11, no STL.
 */

#include "Board.h"
#include "Station.h"
#ifdef RDEPARTURES_QT
#include <QDateTime>
#include <QJsonObject>
#include <QColor>
#include <QDir>
#include <QFontMetricsF>
#include <QTimeZone>
#include <QMouseEvent>
#include <QPolygonF>
#include <QTransform>
#endif

#ifndef RDEPARTURES_QT
#include <Application.h>
#include <Bitmap.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <PopUpMenu.h>
#include <Region.h>
#include <Shape.h>
#include <Window.h>
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


extern int64 gTimeOffset;
void save_airport_setting(const char* code);


static const int kTotalCells = 68;
static const bigtime_t kPhaseLength = 8000000;
static const bigtime_t kRattleStep = 70000;
static const bigtime_t kFinalStep = 130000;
static const bigtime_t kAirlineStep = 110000;

static const rgb_color kBackground = { 8, 10, 16, 255 };
static const rgb_color kHeader = { 0, 0, 0, 255 };
static const rgb_color kFlap = { 33, 33, 36, 255 };
static const rgb_color kFlapLight = { 50, 50, 54, 255 };
static const rgb_color kLabel = { 200, 200, 196, 255 };
static const rgb_color kLabelEnglish = { 140, 140, 138, 255 };

static const rgb_color kColors[COLOR_COUNT] = {
	{ 236, 236, 228, 255 },	// white
	{ 255, 204, 50, 255 },	// yellow
	{ 110, 230, 120, 255 },	// green
	{ 255, 150, 40, 255 },	// orange
	{ 255, 72, 60, 255 },	// red
	{ 140, 140, 140, 255 },	// gray
	{ 255, 70, 60, 255 },	// date
};

static const char* kRattle = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
static const char* kRattleHangul = "가나다라마바사아자차카타파하";
static const char* kRattleKana = "アイウエオカキクケコサシスセソ";
static const char* kRattleHan = "東西南北中大小上下山川日月";

// The columns of a row, in cells.
static const int kColumnStart[] = { 0, 6, 12, 29, 39, 47, 52 };
static const int kColumnCells[] = { 5, 5, 16, 9, 7, 4, 16 };
enum { COL_TIME = 0, COL_NEW, COL_DEST, COL_AIRLINE, COL_FLIGHT, COL_GATE,
	COL_REMARKS, COL_COUNT };
#ifdef RDEPARTURES_QT
static const int kHistoryColumnStart[] = {0,6,17,56};
static const int kHistoryColumnCells[] = {5,10,38,12};
static const int kHistoryColumnCount = 4;
#endif

static const char* kMenuNames[] = {
	"Seoul Incheon  인천 (ICN)",
	"Tokyo Narita  成田 (NRT)",
	"Hong Kong  香港 (HKG)",
	"London Heathrow (LHR)",
	"Paris Charles de Gaulle (CDG)",
	"Milano Malpensa (MXP)",
	"New York JFK (JFK)",
	"San Francisco (SFO)",
};
static const bigtime_t kStationBoardLength = 12000000;
static const bigtime_t kStationLangLength = 6000000;


struct FaceEntry {
	BString		key;
	BBitmap*	face;
};


// #pragma mark - UTF-8


static int
utf8_length(uint8 c)
{
	if (c < 0x80)
		return 1;
	if ((c >> 5) == 6)
		return 2;
	if ((c >> 4) == 14)
		return 3;
	if ((c >> 3) == 30)
		return 4;
	return 1;
}


static uint32
utf8_code(const char* s, int length)
{
	const uint8* b = (const uint8*)s;
	switch (length) {
		case 2: return ((b[0] & 0x1f) << 6) | (b[1] & 0x3f);
		case 3: return ((b[0] & 0x0f) << 12) | ((b[1] & 0x3f) << 6)
			| (b[2] & 0x3f);
		case 4: return ((b[0] & 0x07) << 18) | ((b[1] & 0x3f) << 12)
			| ((b[2] & 0x3f) << 6) | (b[3] & 0x3f);
		default: return b[0];
	}
}


// Hangul, kana and Han take two cells, as on the boards in Seoul and Tokyo.
static bool
is_wide(uint32 c)
{
	return (c >= 0x1100 && c <= 0x115f) || (c >= 0x2e80 && c <= 0xa4cf)
		|| (c >= 0xac00 && c <= 0xd7a3) || (c >= 0xf900 && c <= 0xfaff)
		|| (c >= 0xfe30 && c <= 0xfe4f) || (c >= 0xff00 && c <= 0xff60)
		|| (c >= 0xffe0 && c <= 0xffe6);
}


static bool
has_wide(const char* s)
{
	while (*s != '\0') {
		int n = utf8_length((uint8)*s);
		if (is_wide(utf8_code(s, n)))
			return true;
		s += n;
	}
	return false;
}


struct Unit {
	char	glyph[8];
	int		width;
};


static int
split_units(const char* text, Unit* units, int max)
{
	int count = 0;
	while (*text != '\0' && count < max) {
		int n = utf8_length((uint8)*text);
		Unit& u = units[count++];
		memcpy(u.glyph, text, n);
		u.glyph[n] = '\0';
		if (n == 1 && text[0] == ' ')
			u.glyph[0] = '\0';
		u.width = is_wide(utf8_code(text, n)) ? 2 : 1;
		text += n;
	}
	return count;
}


// #pragma mark - BoardView


BoardView::BoardView(BRect frame, int airport)
	:
	BView(frame, "board", B_FOLLOW_ALL, B_WILL_DRAW | B_FRAME_EVENTS),
	fAirport(airport < kAirportCount ? airport : 0),
	fStation(airport >= kAirportCount ? airport - kAirportCount : -1),
	fStationKey(-1),
	fStationMinute(-1),
	fMarqueeCount(0),
	fMarqueeStart(0),
	fPhase(0),
	fFixedPhase(-1),
	fFooter(0),
	fPhaseStart(0),
	fLastUpdate(0),
	fLastMouse(0),
	fRunner(NULL),
	fBoard(NULL),
	fBoardView(NULL),
	fScratch(NULL),
	fScratchView(NULL),
	fHaveCJK(false),
	fCells(NULL),
	fCellCount(0),
	fCellCapacity(0),
	fFields(NULL),
	fFieldCount(0),
	fFieldCapacity(0),
	fRows(0),
	fFaces(NULL),
	fFaceCapacity(16384),
	fRandom(12345)
{
	SetViewColor(B_TRANSPARENT_COLOR);
	fFaces = new FaceEntry[fFaceCapacity];
	for (int i = 0; i < fFaceCapacity; i++)
		fFaces[i].face = NULL;
	_PickFonts();
}


BoardView::~BoardView()
{
	delete fRunner;
	_FreeLayout();
	_ClearCache();
	delete[] fFaces;
}


void
BoardView::AttachedToWindow()
{
	MakeFocus(true);
	if (fBoard == NULL || Bounds() != fBounds)
		LayoutBoard(Bounds());
	BMessage tick(MSG_TICK);
	fRunner = new BMessageRunner(BMessenger(this), &tick, 33333);
	fLastMouse = system_time();
}


void
BoardView::Draw(BRect updateRect)
{
	if (fBoard != NULL)
		DrawBitmap(fBoard, updateRect, updateRect);
}


void
BoardView::FrameResized(float width, float height)
{
	LayoutBoard(Bounds());
	Invalidate();
}


void
BoardView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_TICK:
			Tick();
			break;
		case MSG_SET_AIRPORT:
		{
			int32 index;
			if (message->FindInt32("airport", &index) == B_OK)
				SetAirport(index);
			break;
		}
		default:
			BView::MessageReceived(message);
	}
}


void
BoardView::MouseDown(BPoint where)
{
	fLastMouse = system_time();
	if (fGearRect.Contains(where))
		_ShowMenu(fGearRect.LeftBottom());
#ifdef RDEPARTURES_QT
    else if (fCodexHistoryMode && fBackHandler
        && where.x >= fCells[fFields[fDateField].first].frame.left
        && where.x <= fCells[fFields[fDateField].first + fFields[fDateField].count-1].frame.right
        && where.y >= fCells[fFields[fDateField].first].frame.top
        && where.y <= fCells[fFields[fDateField].first].frame.bottom) fBackHandler();
#endif
}

#ifdef RDEPARTURES_QT
void BoardView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button()==Qt::LeftButton && fCodexMode && !fCodexHistoryMode && fRowHandler) {
        const BPoint where(event->position().x(),event->position().y());
        for (int row = 0; row < fRows; ++row) {
            const Cell& cell = fCells[fFields[fRowFields + row * COL_COUNT].first];
            if (where.y >= cell.frame.top && where.y <= cell.frame.bottom) {
                int index = fCodexPage * fRows + row;
                if (index < fCodexRows.size()) fRowHandler(index);
                break;
            }
        }
        event->accept();return;
    }
    BView::mouseDoubleClickEvent(event);
}
#endif


void
BoardView::MouseMoved(BPoint where, uint32 transit, const BMessage* drag)
{
	fLastMouse = system_time();
}


void
BoardView::KeyDown(const char* bytes, int32 numBytes)
{
#ifdef RDEPARTURES_QT
    if (fCodexHistoryMode && bytes[0]==B_LEFT_ARROW && fBackHandler) { fBackHandler();return; }
    if (fCodexMode && (bytes[0] == B_RIGHT_ARROW || bytes[0] == B_LEFT_ARROW || bytes[0] == ' ')) {
        int pages = qMax(1, (int(fCodexRows.size()) + fRows - 1) / fRows);
        fCodexPage = (fCodexPage + (bytes[0] == B_LEFT_ARROW ? pages - 1 : 1)) % pages;
        _UpdateCodex();
        return;
    }
#endif
	switch (bytes[0]) {
		case B_ESCAPE:
		case 'q':
		case 'Q':
			be_app->PostMessage(B_QUIT_REQUESTED);
			break;
		case B_RIGHT_ARROW:
		case ' ':
			SetAirport((AirportIndex() + 1) % (kAirportCount + kStationCount));
			break;
		case B_LEFT_ARROW:
			SetAirport((AirportIndex() + kAirportCount + kStationCount - 1)
				% (kAirportCount + kStationCount));
			break;
		default:
			BView::KeyDown(bytes, numBytes);
	}
}


bool
BoardView::Animating() const
{
	for (int i = 0; i < fCellCount; i++) {
		if (fCells[i].flipping || fCells[i].queued > 0)
			return true;
	}
	return false;
}


void
BoardView::SetAirport(int index)
{
#ifdef RDEPARTURES_QT
    if (fCodexMode && fAirportHandler) { fAirportHandler(index); return; }
#endif
	if (index < 0 || index >= kAirportCount + kStationCount)
		return;
	fStation = index >= kAirportCount ? index - kAirportCount : -1;
	fPhaseStart = system_time();
	if (fStation >= 0) {
		save_airport_setting(kStations[fStation].code);
		fStationKey = -1;
		if (fBoard != NULL)
			_TickStation(fPhaseStart);
		return;
	}
	fAirport = index;
	save_airport_setting(kAirports[index].code);
	// The labels and the airport name are drawn, not flapped: redraw them,
	// put the current faces back on top, and let the new content flip in.
	fPhase = 0;
	fFooter = 0;
	fPhaseStart = system_time();
	if (fBoard != NULL) {
		_DrawStatic();
		for (int i = 0; i < fCellCount; i++)
			_Compose(fCells[i], fPhaseStart);
		_UpdateContent(true);
		if (Window() != NULL)
			Invalidate();
	}
}


#ifdef RDEPARTURES_QT
void BoardView::SetCodexRows(const QJsonArray& rows, const QString& notice)
{
    bool changed = !fCodexMode;
    fCodexMode = true;
    fStation = -1;
    fFixedPhase = 0;
    fCodexRows = rows;
    fCodexNotice = notice;
    if (changed || !fBoard) { fLastCodexFlip = system_time(); LayoutBoard(Bounds()); }
    else _UpdateCodex();
}

void BoardView::SetCodexHistory(const QJsonArray& rows, const QJsonObject& session, const QString& notice)
{
    const bool changed=!fCodexHistoryMode || fHistorySession!=session;
    fCodexMode=true;fCodexHistoryMode=true;fStation=-1;fFixedPhase=0;
    if (fHistorySession.value("id")!=session.value("id")) fCodexPage=0;
    fHistorySession=session;fCodexRows=rows;fCodexNotice=notice;
    if (changed || !fBoard) {fLastCodexFlip=system_time();LayoutBoard(Bounds());}
    else _UpdateCodexHistory();
}

void BoardView::_UpdateCodexHistory()
{
    if (!fBoard) return;
    fLastUpdate=system_time();
    _SetField(fDateField,"← BACK",COLOR_YELLOW,0);
    const auto clock=QDateTime::currentDateTime().toString("HH:mm");
    _SetField(fClockField,clock.toUtf8().constData(),COLOR_YELLOW,0);
    _SetField(fTitleField,"HISTORY",COLOR_WHITE,0);
    const int pages=qMax(1,(int(fCodexRows.size())+fRows-1)/fRows);
    fCodexPage=qMin(fCodexPage,pages-1);
    const auto footer=fCodexNotice+QString(" | ← BACK | → PAGE %1/%2").arg(fCodexPage+1).arg(pages);
    _SetField(fFooterField,footer.toUtf8().constData(),COLOR_YELLOW,0);
    for (int row=0;row<fRows;++row) {
        const int first=fRowFields+row*kHistoryColumnCount,index=fCodexPage*fRows+row;
        const auto item=index<fCodexRows.size()?fCodexRows[index].toObject():QJsonObject();
        const QString values[]={item["time"].toString(),item["event"].toString(),item["text"].toString(),item["result"].toString()};
        const auto state=item["status"].toString();
        const int resultColor=state=="failed"||state=="error"?COLOR_RED
            :state=="inProgress"||state=="running"?COLOR_YELLOW:state=="completed"?COLOR_GREEN:COLOR_GRAY;
        for (int column=0;column<kHistoryColumnCount;++column)
            _SetField(first+column,values[column].toUtf8().constData(),column==0?COLOR_YELLOW:column==3?resultColor:COLOR_WHITE,row*30000);
    }
}

void BoardView::_UpdateCodex()
{
    if (fCodexHistoryMode) {_UpdateCodexHistory();return;}
    if (!fBoard) return;
    fLastUpdate = system_time();
    QDateTime now = QDateTime::currentDateTime();
    char date[64];
    format_date(date, sizeof(date), (now.toSecsSinceEpoch() + now.offsetFromUtc()) / 60, LANG_EN);
    _SetField(fDateField, date, COLOR_DATE, 0);
    _SetField(fClockField, now.toString("HH:mm").toUtf8().constData(), COLOR_YELLOW, 0);
    _SetField(fTitleField, "CODEX", COLOR_WHITE, 0);
    int pages = qMax(1, (int(fCodexRows.size()) + fRows - 1) / fRows);
    fCodexPage = qMin(fCodexPage, pages - 1);
    QString footer = fCodexNotice + QString("  %1/%2").arg(fCodexPage + 1).arg(pages);
    _SetField(fFooterField, footer.toUtf8().constData(), COLOR_YELLOW, 0);
    for (int r = 0; r < fRows; ++r) {
        const int first = fRowFields + r * COL_COUNT;
        const int index = fCodexPage * fRows + r;
        if (index >= fCodexRows.size()) {
            for (int col = 0; col < COL_COUNT; ++col) {
                if (col == COL_AIRLINE) _SetProject(first + col, {}, {}, r * 30000);
                else _SetField(first + col, "", COLOR_WHITE, r * 30000);
            }
            continue;
        }
        const auto row = fCodexRows[index].toObject();
        QString state = row["status"].toString();
        QString badge = "UNKNOWN";
        int color = COLOR_GRAY;
        if (state == "running") {badge = "RUNNING"; color = COLOR_GREEN;}
        else if (state == "approval") {badge = "APPROVAL WAIT"; color = COLOR_ORANGE;}
        else if (state == "input") {badge = "INPUT WAIT"; color = COLOR_YELLOW;}
        else if (state == "error") {badge = "ERROR"; color = COLOR_RED;}
        else if (state == "idle") badge = "IDLE";
        else if (state == "history") badge = "HISTORY";
        else if (state == "stale") badge = "OFFLINE";
        const qint64 updated = qint64(row["updatedAt"].toDouble());
        QString agent = row["agent"].toString();
        if (agent.isEmpty()) agent = row["source"].toString() == "subAgent" ? "AGENT" : "ROOT";
        QString task = row["title"].toString();
        if (row["_depth"].toInt() > 0) task = "> " + task;
        const QString values[] = {
            updated > 0 ? QDateTime::fromSecsSinceEpoch(updated).toString("HH:mm") : "--:--",
            row["id"].toString().right(5), task, row["project"].toString(), agent,
            row["_children"].toInt() > 0 ? QString::number(row["_children"].toInt()) : "", badge};
        for (int col = 0; col < COL_COUNT; ++col) {
            if (col == COL_AIRLINE) _SetProject(first + col, row["project"].toString(), row["cwd"].toString(), r * 30000);
            else _SetField(first + col, values[col].toUtf8().constData(), col == COL_REMARKS ? color : col == COL_TIME ? COLOR_YELLOW : COLOR_WHITE, r * 30000);
        }
    }
}

void BoardView::SetCodexFlipInterval(int seconds)
{
    fCodexFlipInterval = qBound(0, seconds, 3600);
    fLastCodexFlip = system_time();
}

void BoardView::SetCodexFlipDurationMs(int milliseconds)
{
    fCodexFlipDurationMs = qBound(200, milliseconds, 2000);
    fLastCodexFlip = system_time();
}

void BoardView::_FlipCodexRows(bigtime_t now)
{
    const int columns=fCodexHistoryMode?kHistoryColumnCount:COL_COUNT;
    const int maxCharacter=fCodexHistoryMode?37:15;
    const int count = qMin(fRows, int(fCodexRows.size()) - fCodexPage * fRows);
    if (count <= 0) return;
    const bigtime_t duration = bigtime_t(fCodexFlipDurationMs) * 1000;
    const bigtime_t budget = qMax<bigtime_t>(0, bigtime_t(fCodexFlipInterval) * 1000000 - duration - 200000);
    const bigtime_t desired = (count - 1) * 80000 + (columns-1) * 45000 + maxCharacter * 18000;
    const double pace = qMin(1.0, double(budget) / qMax<bigtime_t>(1, desired));
    const bigtime_t rowStep = bigtime_t(80000 * pace), colStep = bigtime_t(45000 * pace), charStep = bigtime_t(18000 * pace);
    fCodexSweepEnd = now + (count - 1) * rowStep + (columns-1) * colStep + maxCharacter * charStep + duration + 100000;
    for (int row = 0; row < fRows && fCodexPage * fRows + row < fCodexRows.size(); ++row) {
        for (int col = 0; col < columns; ++col) {
            const Field& field = fFields[fRowFields + row * columns + col];
            Unit units[96];
            const int unitCount = split_units(field.text.String(), units, 96);
            int position = 0;
            for (int unit = 0; unit < unitCount && position < field.count; ++unit) {
                const int width = field.sizeClass == SIZE_AIRLINE ? 1 : units[unit].width;
                if (position + width > field.count) break;
                const bigtime_t start = now + row * rowStep + col * colStep + position * charStep;
                for (int part = 0; part < width; ++part) {
                    Cell& cell = fCells[field.first + position + part];
                    // Replay the same face; both halves of a CJK glyph move together.
                    if (!cell.flipping && cell.queued == 0) _Enqueue(cell, cell.current, NULL, 0, start);
                }
                position += width;
            }
        }
    }
}
#endif

// #pragma mark - fonts


static bool
set_face(BFont& font, const char* family, const char* style)
{
	return font.SetFamilyAndStyle(family, style) == B_OK;
}


void
BoardView::_PickFonts()
{
	// DIN's narrow, straight-sided lettering suits mechanical departure
	// boards. Keep the original Heros face as a fallback for older installs.
	fMain = *be_bold_font;
	if (!set_face(fMain, "D-DIN Condensed", "DINCondensed-Bold")
		&& !set_face(fMain, "TeX Gyre Heros Cn", "Bold")
		&& !set_face(fMain, "Oswald", "Bold"))
		set_face(fMain, "Noto Sans Display", "Bold");

	fFallback = *be_bold_font;
	set_face(fFallback, "Noto Sans", "Bold");

	// Whichever CJK family is installed; Korean first, since a Korean CJK
	// face has the kana and Han as well.
	static const char* kPreferred[] = { "CJK KR", "CJK JP", "CJK SC", "CJK TC",
		"CJK", NULL };
	int32 families = count_font_families();
	for (int p = 0; kPreferred[p] != NULL && !fHaveCJK; p++) {
		for (int32 i = 0; i < families; i++) {
			font_family family;
			uint32 flags;
			if (get_font_family(i, &family, &flags) != B_OK
				|| strstr(family, kPreferred[p]) == NULL
				|| strstr(family, "Mono") != NULL)
				continue;
			fCJK = *be_bold_font;
			if (!set_face(fCJK, family, "Bold")) {
				font_style style;
				if (get_font_style(family, 0, &style, &flags) != B_OK)
					continue;
				set_face(fCJK, family, style);
			}
			fHaveCJK = true;
			break;
		}
	}
	if (!fHaveCJK)
		fCJK = *be_bold_font;

	fMain.SetFlags(B_FORCE_ANTIALIASING);
	fFallback.SetFlags(B_FORCE_ANTIALIASING);
	fCJK.SetFlags(B_FORCE_ANTIALIASING);
}


static BRect
glyph_box(const BFont& font, const char* text)
{
	const char* strings[1] = { text };
	BRect boxes[1];
	escapement_delta delta = { 0, 0 };
	font.GetBoundingBoxesForStrings(strings, 1, B_SCREEN_METRIC, &delta,
		boxes);
	return boxes[0];
}


// The font a piece of text is drawn in: the CJK face for Hangul, kana and
// Han, the board face where it has the glyphs, Noto Sans otherwise (Greek,
// Cyrillic, Vietnamese).
static BFont
font_for(const char* text, const BFont& main, const BFont& fallback,
	const BFont& cjk)
{
	if (has_wide(text)) {
		BFont font(cjk);
#ifdef RDEPARTURES_QT
        // Keep Hangul's height and weight, with a narrower face to match DIN.
        // Apply this before measuring so labels and project badges fit too.
        for (QChar c : QString::fromUtf8(text)) {
            if (c.script() == QChar::Script_Hangul) {
                font.SetStretch(85);
                break;
            }
        }
#endif
		return font;
	}
	int32 chars = 0;
	for (const char* s = text; *s != '\0'; s += utf8_length((uint8)*s))
		chars++;
	if (chars == 0)
		return main;
	bool* has = new bool[chars];
	main.GetHasGlyphs(text, chars, has);
	bool all = true;
	for (int32 i = 0; i < chars; i++)
		all = all && has[i];
	delete[] has;
	return all ? main : fallback;
}


// #pragma mark - layout


void
BoardView::_FreeLayout()
{
	delete fBoard;
	fBoard = NULL;
	fBoardView = NULL;
	delete fScratch;
	fScratch = NULL;
	fScratchView = NULL;
	delete[] fCells;
	fCells = NULL;
	fCellCount = fCellCapacity = 0;
	delete[] fFields;
	fFields = NULL;
	fFieldCount = fFieldCapacity = 0;
}


int
BoardView::_AddField(int sizeClass, BPoint origin, int cells, int align)
{
	Field& f = fFields[fFieldCount];
	f.first = fCellCount;
	f.count = cells;
	f.sizeClass = sizeClass;
	f.align = align;
	f.text = "";
	f.color = -1;
	float w = fCellWidth[sizeClass];
	float h = fCellHeight[sizeClass];
	for (int i = 0; i < cells; i++) {
		Cell& c = fCells[fCellCount++];
		c.frame.Set(origin.x + i * w, origin.y, origin.x + (i + 1) * w - 1,
			origin.y + h - 1);
		c.sizeClass = sizeClass;
		c.color = COLOR_WHITE;
		c.current = sizeClass == SIZE_AIRLINE ? _AirlineFace(-1)
			: _Blank(sizeClass);
		c.queued = 0;
		c.flipping = false;
		c.startAt = 0;
	}
	return fFieldCount++;
}


void
BoardView::LayoutBoard(BRect bounds)
{
	_FreeLayout();
	_ClearCache();
	fBounds = bounds;
	int width = bounds.IntegerWidth() + 1;
	int height = bounds.IntegerHeight() + 1;

	float cw = floorf(width * 0.97f / kTotalCells);
	if (cw < 6)
		cw = 6;
	float rh = floorf(cw * 1.55f);
	fCW = cw;
	fCellWidth[SIZE_ROW] = cw;
	fCellHeight[SIZE_ROW] = rh;
	fCellWidth[SIZE_TITLE] = floorf(cw * 1.15f);
	fCellHeight[SIZE_TITLE] = floorf(rh * 1.2f);
	fCellWidth[SIZE_FOOTER] = floorf(cw * 0.8f);
	fCellHeight[SIZE_FOOTER] = floorf(rh * 0.78f);
	fCellWidth[SIZE_AIRLINE] = cw * kColumnCells[COL_AIRLINE];
	fCellHeight[SIZE_AIRLINE] = rh;

	// Glyph metrics per cell size: digits and capitals share a baseline,
	// CJK is centred on its own box.
	for (int s = 0; s < SIZE_COUNT; s++) {
		float h = fCellHeight[s];
		fFontSize[s] = h * 0.80f;
		BFont font(fMain);
		font.SetSize(fFontSize[s]);
		BRect cap = glyph_box(font, "H");
		float capHeight = cap.IsValid() && cap.Height() > 1 ? cap.Height()
			: h * 0.55f;
		if (capHeight > h * 0.62f) {
			fFontSize[s] *= h * 0.62f / capHeight;
			capHeight = h * 0.62f;
		}
		fBaseline[s] = floorf((h + capHeight) / 2);
		BFont cjk(fCJK);
		cjk.SetSize(h * 0.62f);
		BRect box = glyph_box(cjk, "国");
		fCJKBaseline[s] = box.IsValid() && box.Height() > 1
			? floorf((h + box.Height()) / 2 - box.bottom) : floorf(h * 0.8f);
	}

	float x0 = floorf((width - kTotalCells * cw) / 2);
	fX0 = x0;
	float pad = floorf(rh * 0.35f);
	float tcw = fCellWidth[SIZE_TITLE];
	float th = fCellHeight[SIZE_TITLE];
	float headerTop = pad;
	fLabelTop = headerTop + th + floorf(pad * 0.8f);
	float labelHeight = floorf(rh * 0.95f);
	float rowsTop = fLabelTop + labelHeight + floorf(pad * 0.3f);
	float footerHeight = fCellHeight[SIZE_FOOTER];
	float footerTop = height - pad - footerHeight;
	float rowStep = rh + (rh * 0.12f > 2 ? floorf(rh * 0.12f) : 2);
	fRows = (int)((footerTop - pad * 0.6f - rowsTop + (rowStep - rh)) / rowStep);
	if (fRows < 1)
		fRows = 1;
	if (fRows > 40)
		fRows = 40;
	int footerCells = (int)(kTotalCells * cw / fCellWidth[SIZE_FOOTER]);

	fCellCapacity = 8 + 5 + 10 + footerCells + fRows * 68;
	fCells = new Cell[fCellCapacity];
	fFieldCapacity = 4 + fRows * COL_COUNT;
	fFields = new Field[fFieldCapacity];

	int scratchWidth = (int)fCellWidth[SIZE_AIRLINE];
	if (scratchWidth < (int)(tcw * 2))
		scratchWidth = (int)(tcw * 2);
	fScratch = new BBitmap(BRect(0, 0, scratchWidth - 1, th - 1), B_RGB32,
		true);
	fScratchView = new BView(fScratch->Bounds(), "scratch", B_FOLLOW_NONE,
		B_WILL_DRAW);
	fScratch->AddChild(fScratchView);

	fBoard = new BBitmap(BRect(0, 0, width - 1, height - 1), B_RGB32, true);
	fBoardView = new BView(fBoard->Bounds(), "board", B_FOLLOW_NONE,
		B_WILL_DRAW);
	fBoard->AddChild(fBoardView);

	fDateField = _AddField(SIZE_TITLE, BPoint(x0, headerTop), 8, ALIGN_LEFT);
	fClockField = _AddField(SIZE_TITLE, BPoint(x0 + 9 * tcw, headerTop), 5,
		ALIGN_LEFT);
	fTitleField = _AddField(SIZE_TITLE,
		BPoint(x0 + 15 * tcw + floorf(th * 1.25f), headerTop), 10,
		ALIGN_LEFT);
	fGearRect.Set(x0 + kTotalCells * cw - th, headerTop,
		x0 + kTotalCells * cw - 1, headerTop + th - 1);

	fFooterField = _AddField(SIZE_FOOTER,
		BPoint(x0 + floorf((kTotalCells * cw
			- footerCells * fCellWidth[SIZE_FOOTER]) / 2), footerTop),
		footerCells, ALIGN_CENTER);

	fRowFields = fFieldCount;
	for (int r = 0; r < fRows; r++) {
		float y = rowsTop + r * rowStep;
#ifdef RDEPARTURES_QT
        if (fCodexHistoryMode) {
            for (int c=0;c<kHistoryColumnCount;++c)
                _AddField(SIZE_ROW,BPoint(x0+kHistoryColumnStart[c]*cw,y),kHistoryColumnCells[c],ALIGN_LEFT);
            continue;
        }
#endif
		for (int c = 0; c < COL_COUNT; c++) {
			BPoint at(x0 + kColumnStart[c] * cw, y);
			if (c == COL_AIRLINE)
				_AddField(SIZE_AIRLINE, at, 1, ALIGN_LEFT);
			else
				_AddField(SIZE_ROW, at, kColumnCells[c], ALIGN_LEFT);
		}
	}

	_DrawStatic();
	bigtime_t now = system_time();
	for (int i = 0; i < fCellCount; i++)
		_Compose(fCells[i], now);
	fPhaseStart = now;
	_UpdateContent(true);
	if (fStation >= 0) {
		fStationKey = -1;
		_TickStation(now);
	}
}


// #pragma mark - faces


static uint32
hash_key(const BString& key)
{
	uint32 h = 2166136261u;
	for (const char* s = key.String(); *s != '\0'; s++)
		h = (h ^ (uint8)*s) * 16777619u;
	return h;
}


BBitmap*
BoardView::_Cache(const BString& key) const
{
	uint32 i = hash_key(key) % fFaceCapacity;
	while (fFaces[i].face != NULL) {
		if (fFaces[i].key == key)
			return fFaces[i].face;
		i = (i + 1) % fFaceCapacity;
	}
	return NULL;
}


void
BoardView::_Store(const BString& key, BBitmap* face)
{
	uint32 i = hash_key(key) % fFaceCapacity;
	int probes = 0;
	while (fFaces[i].face != NULL && probes++ < fFaceCapacity)
		i = (i + 1) % fFaceCapacity;
	// Cells point at cached faces, so the cache is never emptied while the
	// layout stands; a full table (thousands of faces) just stops caching.
	if (fFaces[i].face != NULL)
		return;
	fFaces[i].key = key;
	fFaces[i].face = face;
}


void
BoardView::_ClearCache()
{
	for (int i = 0; i < fFaceCapacity; i++) {
		delete fFaces[i].face;
		fFaces[i].face = NULL;
		fFaces[i].key = "";
	}
}


// Copies a rectangle of the scratch bitmap into a bitmap of its own.
static BBitmap*
copy_out(BBitmap* from, BRect rect)
{
	int w = rect.IntegerWidth() + 1;
	int h = rect.IntegerHeight() + 1;
	BBitmap* face = new BBitmap(BRect(0, 0, w - 1, h - 1), B_RGB32);
	const uint8* src = (const uint8*)from->Bits() + (int)rect.top
		* from->BytesPerRow() + (int)rect.left * 4;
	uint8* dst = (uint8*)face->Bits();
	for (int y = 0; y < h; y++) {
		memcpy(dst, src, w * 4);
		src += from->BytesPerRow();
		dst += face->BytesPerRow();
	}
	return face;
}


void
BoardView::_DrawFlap(BView* view, BRect r)
{
	BRect flap(r.left + 1, r.top, r.right - 1, r.bottom);
	view->SetHighColor(kFlap);
	view->FillRoundRect(flap, 2, 2);
	view->SetHighColor(kFlapLight);
	view->StrokeLine(BPoint(flap.left + 2, flap.top),
		BPoint(flap.right - 2, flap.top));
}


static void
draw_split(BView* view, BRect r)
{
	float mid = floorf(r.top + (r.Height() + 1) / 2);
	view->SetHighColor(0, 0, 0);
	view->StrokeLine(BPoint(r.left, mid), BPoint(r.right, mid));
	view->SetHighColor(20, 20, 22);
	view->StrokeLine(BPoint(r.left + 1, mid + 1), BPoint(r.right - 1, mid + 1));
}


void
BoardView::_DrawGlyph(BView* view, BRect r, const char* glyph, int sizeClass,
	int color)
{
	BFont font(font_for(glyph, fMain, fFallback, fCJK));
	bool wide = has_wide(glyph);
	font.SetSize(wide ? fCellHeight[sizeClass] * 0.62f : fFontSize[sizeClass]);
	float w = font.StringWidth(glyph);
	float room = (r.Width() + 1) * (wide ? 0.86f : 0.9f);
	if (w > room) {
		font.SetSize(font.Size() * room / w);
		w = font.StringWidth(glyph);
	}
	view->SetFont(&font);
	view->SetHighColor(kColors[color]);
	view->SetLowColor(kFlap);
	view->SetDrawingMode(B_OP_OVER);
	float baseline = wide ? fCJKBaseline[sizeClass] : fBaseline[sizeClass];
	view->DrawString(glyph, BPoint(r.left + floorf((r.Width() + 1 - w) / 2),
		r.top + baseline));
	view->SetDrawingMode(B_OP_COPY);
}


BBitmap*
BoardView::_Face(int sizeClass, int color, const char* glyph, int width,
	int part)
{
	BString key;
	key << sizeClass << "|" << color << "|" << width << "|" << part << "|"
		<< glyph;
	BBitmap* face = _Cache(key);
	if (face != NULL)
		return face;

	float cw = fCellWidth[sizeClass];
	float ch = fCellHeight[sizeClass];
	BRect unit(0, 0, cw * width - 1, ch - 1);
	fScratch->Lock();
	fScratchView->SetHighColor(kBackground);
	fScratchView->FillRect(unit);
	// A two-cell character gets one wide flap, as Hangul and kana have on
	// real boards; its two cells still flip separately.
	_DrawFlap(fScratchView, unit);
	// The split goes over Latin letters, as on a real flap, but under
	// Hangul and Han: their short middle strokes (the stem of ㅜ in 중 and
	// 울) sit right on it and would vanish, turning 중 into 증.
	bool under = has_wide(glyph);
	if (under)
		draw_split(fScratchView, unit.InsetByCopy(1, 0));
	if (glyph[0] != '\0')
		_DrawGlyph(fScratchView, unit, glyph, sizeClass, color);
	if (!under)
		draw_split(fScratchView, unit.InsetByCopy(1, 0));
	fScratchView->Sync();
	face = copy_out(fScratch, BRect(part * cw, 0, (part + 1) * cw - 1, ch - 1));
	fScratch->Unlock();
	_Store(key, face);
	return face;
}


BBitmap*
BoardView::_Blank(int sizeClass)
{
	return _Face(sizeClass, COLOR_WHITE, "", 1, 0);
}


// An airline's flap: its name in its colours.
// index -1 is the blank flap.
BBitmap*
BoardView::_AirlineFace(int index)
{
	BString key;
	key << "AL|" << index;
    const Airline* airline = index < 0 ? NULL : airline_at(index);
    rgb_color bg = airline ? make_color(airline->bg[0], airline->bg[1], airline->bg[2]) : kFlap;
    rgb_color fg = airline ? make_color(airline->fg[0], airline->fg[1], airline->fg[2]) : kColors[COLOR_WHITE];
    return _ColoredFace(key, airline ? airline->name : NULL, bg, fg);
}

BBitmap*
BoardView::_ColoredFace(const BString& key, const char* label, rgb_color bg, rgb_color fg)
{
	BBitmap* face = _Cache(key);
	if (face != NULL)
		return face;

	float w = fCellWidth[SIZE_AIRLINE];
	float h = fCellHeight[SIZE_AIRLINE];
	BRect r(0, 0, w - 1, h - 1);
	fScratch->Lock();
	BView* v = fScratchView;
	v->SetHighColor(kBackground);
	v->FillRect(r);
	BRect panel(1, 0, w - 2, h - 1);
    const bool splitUnderText = label != NULL && has_wide(label);
	if (label == NULL) {
		_DrawFlap(v, r);
	} else {
		v->SetHighColor(bg);
		v->FillRoundRect(panel, 3, 3);
		rgb_color light = { (uint8)(bg.red + (255 - bg.red) / 5),
			(uint8)(bg.green + (255 - bg.green) / 5),
			(uint8)(bg.blue + (255 - bg.blue) / 5), 255 };
		v->SetHighColor(light);
		v->StrokeLine(BPoint(panel.left + 3, panel.top),
			BPoint(panel.right - 3, panel.top));
        if (splitUnderText) draw_split(v, panel);

		BFont font(font_for(label, fMain, fFallback, fCJK));
		v->SetDrawingMode(B_OP_OVER);
		BRect capBox;
		float left = panel.left + floorf(h * 0.25f);
		float room = panel.right - floorf(h * 0.25f) - left;
		font.SetSize(fFontSize[SIZE_ROW] * 0.78f);
		float tw = font.StringWidth(label);
		if (tw > room) {
			font.SetSize(font.Size() * room / tw);
			tw = font.StringWidth(label);
		}
		capBox = glyph_box(font, has_wide(label) ? label : "H");
		v->SetFont(&font);
		v->SetHighColor(fg);
		v->DrawString(label, BPoint(left + floorf((room - tw) / 2),
			floorf((h - capBox.Height()) / 2 - capBox.top)));
		v->SetDrawingMode(B_OP_COPY);
	}
    if (!splitUnderText) draw_split(v, panel);
	v->Sync();
	face = copy_out(fScratch, r);
	fScratch->Unlock();
	_Store(key, face);
	return face;
}


#ifdef RDEPARTURES_QT
BBitmap* BoardView::_ProjectFace(const QString& project, const QString& path)
{
    QString identity = path.isEmpty() ? project : QDir::cleanPath(path);
    BString key; key << "PROJECT|" << identity.toUtf8().constData() << "|" << project.toUtf8().constData();
    BString colorKey(identity.toUtf8().constData());
    uint32 hash = hash_key(colorKey); // fixed FNV-1a; independent of row order / process seed
    QColor color = QColor::fromHsl(hash % 360, 150 + (hash >> 8) % 40, 95 + (hash >> 16) % 30);
    auto linear = [](double c) { return c <= .04045 ? c / 12.92 : pow((c + .055) / 1.055, 2.4); };
    double luminance = .2126 * linear(color.redF()) + .7152 * linear(color.greenF()) + .0722 * linear(color.blueF());
    rgb_color bg = make_color(color.red(), color.green(), color.blue());
    rgb_color fg = luminance > .179 ? make_color(0,0,0) : make_color(255,255,255);
    BFont font(font_for(project.toUtf8().constData(), fMain, fFallback, fCJK));
    font.SetSize(fFontSize[SIZE_ROW] * .78f);
    const QString label = QFontMetricsF(font.QtFont()).elidedText(project, Qt::ElideRight,
        fCellWidth[SIZE_AIRLINE] - fCellHeight[SIZE_AIRLINE] * .5f - 3);
    return _ColoredFace(key, project.isEmpty() ? NULL : label.toUtf8().constData(), bg, fg);
}

void BoardView::_SetProject(int index, const QString& project, const QString& path, bigtime_t delay)
{
    Field& field = fFields[index];
    BString text; text << path.toUtf8().constData() << "|" << project.toUtf8().constData();
    if (field.text == text) return;
    field.text = text;
    _Enqueue(fCells[field.first], _ProjectFace(project,path), NULL, 0, system_time() + delay);
}

void BoardView::_SetFlightAirline(int index, const QString& code, const QString& name, bigtime_t delay, bool replay)
{
    for (int airline = 0; airline < airline_count(); ++airline) {
        if (!code.isEmpty() && code == QString::fromLatin1(airline_at(airline)->code)) {
            _SetAirline(index, airline, delay, replay); return;
        }
    }
    const QString identity = code.isEmpty() ? name : code;
    const QString label = code.isEmpty() ? name : code;
    Field& field = fFields[index];
    BString text; text << "FLIGHT-AIRLINE|" << identity.toUtf8().constData();
    if (field.text == text) {
        Cell& cell = fCells[field.first];
        if (replay && !identity.isEmpty() && !cell.flipping && cell.queued == 0)
            _Enqueue(cell, cell.current, NULL, 0, system_time() + delay);
        return;
    }
    field.text = text;
    BString key; key << "AIRLINE|" << identity.toUtf8().constData();
    const uint32 hash = hash_key(key);
    const QColor color = QColor::fromHsl(hash % 360, 165, 95);
    BBitmap* face = _ColoredFace(key, label.toUtf8().constData(),
        make_color(color.red(), color.green(), color.blue()), make_color(255,255,255));
    _Enqueue(fCells[field.first], face, NULL, 0, system_time() + delay);
}

void BoardView::_UpdateProviderDepartures(DepartureProvider& source, bool phaseChanged)
{
    const auto metadata = source.info();
    const QDateTime now = QDateTime::fromSecsSinceEpoch(time(NULL) + gTimeOffset)
        .toTimeZone(metadata.timezone);
    const int lang = _PhaseLang();
    const bool korean = lang == LANG_KO;
    const int titleLang = lang == LANG_DEST ? LANG_EN : lang;
    const int64 local = airport_local_minutes(kAirports[fAirport], time(NULL) + gTimeOffset);
    char buffer[64];
    format_date(buffer, sizeof(buffer), local, titleLang);
    _SetField(fDateField, buffer, COLOR_DATE, 0);
    snprintf(buffer, sizeof(buffer), "%02d:%02d", int(local % 1440 / 60), int(local % 60));
    _SetField(fClockField, buffer, COLOR_YELLOW, 0);
    _SetField(fTitleField, title_text(titleLang), COLOR_WHITE, 0);
    const auto rows = source.visibleRows(now, fRows);
    const auto notice = source.notice(now, rows.isEmpty());
    _SetField(fFooterField, notice.toUtf8().constData(), COLOR_YELLOW, phaseChanged ? 300000 : 0);
    for (int row = 0; row < fRows; ++row) {
        const int first = fRowFields + row * COL_COUNT;
        const bigtime_t delay = row * 45000;
        if (row >= rows.size()) {
            for (int column = 0; column < COL_COUNT; ++column) {
                if (column == COL_AIRLINE) _SetAirline(first + column, -1, delay);
                else _SetField(first + column, "", COLOR_WHITE, delay);
            }
            continue;
        }
        const auto& flight = rows[row];
        const auto scheduled = flight.scheduled.toString("HH:mm");
        const auto changed = flight.estimated.isValid() && flight.estimated != flight.scheduled
            ? flight.estimated.toString("HH:mm") : QString();
        _SetField(first + COL_TIME, scheduled.toUtf8().constData(), COLOR_YELLOW, delay);
        _SetField(first + COL_NEW, changed.toUtf8().constData(), COLOR_ORANGE, delay);
        auto destination = korean ? flight.destinationKorean : flight.destinationEnglish;
        if (lang == LANG_JA && !flight.destinationJapanese.isEmpty()) destination = flight.destinationJapanese;
        int destinationCells = 0;
        for (const auto codepoint : destination.toUcs4()) destinationCells += is_wide(codepoint) ? 2 : 1;
        if (destinationCells > fFields[first + COL_DEST].count && destination.contains('/'))
            destination = destination.section('/', 0, 0).trimmed();
        _SetField(first + COL_DEST, destination.toUtf8().constData(), COLOR_WHITE, delay);
        _SetFlightAirline(first + COL_AIRLINE, flight.airlineCode, flight.airlineName, delay, phaseChanged);
        _SetField(first + COL_FLIGHT, flight.flightId.toUtf8().constData(), COLOR_WHITE, delay);
        _SetField(first + COL_GATE, flight.gate.toUtf8().constData(), COLOR_WHITE, delay);
        const int color = flight.tone == Departure::Muted ? COLOR_GRAY
            : flight.tone == Departure::Attention ? COLOR_ORANGE
            : flight.tone == Departure::Boarding ? COLOR_YELLOW
            : flight.tone == Departure::Error ? COLOR_RED : COLOR_WHITE;
        auto remark = korean ? flight.remarkKorean : flight.remarkEnglish;
        if (lang == LANG_JA && !flight.remarkJapanese.isEmpty()) remark = flight.remarkJapanese;
        _SetField(first + COL_REMARKS, remark.toUtf8().constData(), color, delay);
    }
}
#endif

// #pragma mark - flipping


void
BoardView::_Enqueue(Cell& cell, BBitmap* target, BBitmap** rattle,
	int rattleCount, bigtime_t startAt)
{
	// A flap that is mid-flip finishes that flip first.
	cell.queued = cell.flipping ? 1 : 0;
	for (int i = 0; i < rattleCount && cell.queued < 7; i++)
		cell.queue[cell.queued++] = rattle[i];
	cell.queue[cell.queued++] = target;
	if (!cell.flipping)
		cell.startAt = startAt;
}


void
BoardView::_SetField(int index, const char* text, int color, bigtime_t delay)
{
	Field& field = fFields[index];
	if (field.text == text && field.color == color)
		return;
	field.text = text;
	field.color = color;

	Unit units[96];
	int count = split_units(text, units, 96);
	int used = 0;
	int fit = 0;
	while (fit < count && used + units[fit].width <= field.count)
		used += units[fit++].width;

	int offset = 0;
	if (field.align == ALIGN_RIGHT)
		offset = field.count - used;
	else if (field.align == ALIGN_CENTER)
		offset = (field.count - used) / 2;

	// What each cell should end up showing, and which character (unit) it
	// is part of: the two cells of a Hangul or Han character flip together,
	// through the same characters.
	BBitmap* targets[128];
	int unitOf[128];
	for (int i = 0; i < field.count; i++) {
		targets[i] = _Blank(field.sizeClass);
		unitOf[i] = -1;
	}
	int at = offset;
	for (int u = 0; u < fit; u++) {
		for (int p = 0; p < units[u].width; p++) {
			unitOf[at] = u;
			targets[at++] = _Face(field.sizeClass, color, units[u].glyph,
				units[u].width, p);
		}
	}

	bigtime_t now = system_time();
	for (int i = 0; i < field.count; i++) {
		int width = unitOf[i] >= 0 ? units[unitOf[i]].width : 1;
		bool changed = false;
		for (int p = 0; p < width; p++) {
			Cell& cell = fCells[field.first + i + p];
			BBitmap* last = cell.queued > 0 ? cell.queue[cell.queued - 1]
				: cell.current;
			changed = changed || targets[i + p] != last;
		}
		if (!changed) {
			i += width - 1;
			continue;
		}

		// A mechanical flap passes through characters of its own kind:
		// Latin letters for Latin, Hangul for Hangul, kana for kana.
		const char* set = NULL;
		if (unitOf[i] >= 0 && width == 2) {
			const char* g = units[unitOf[i]].glyph;
			uint32 c = utf8_code(g, utf8_length((uint8)g[0]));
			set = c >= 0xac00 && c <= 0xd7a3 ? kRattleHangul
				: c >= 0x3040 && c <= 0x30ff ? kRattleKana : kRattleHan;
		}
		fRandom = fRandom * 1103515245 + 12345;
		int n = 1 + (fRandom >> 16) % 3;
		char glyphs[4][8];
		for (int k = 0; k < n; k++) {
			fRandom = fRandom * 1103515245 + 12345;
			if (set != NULL) {
				// Every character in the set is three bytes of UTF-8.
				int count = strlen(set) / 3;
				memcpy(glyphs[k], set + ((fRandom >> 16) % count) * 3, 3);
				glyphs[k][3] = '\0';
			} else {
				glyphs[k][0] = kRattle[(fRandom >> 16) % 36];
				glyphs[k][1] = '\0';
			}
		}
		fRandom = fRandom * 1103515245 + 12345;
		bigtime_t start = now + delay + i * 18000 + (fRandom >> 16) % 90000;
		for (int p = 0; p < width; p++) {
			BBitmap* rattle[4];
			for (int k = 0; k < n; k++)
				rattle[k] = _Face(field.sizeClass, color, glyphs[k], width, p);
			_Enqueue(fCells[field.first + i + p], targets[i + p], rattle, n,
				start);
		}
		i += width - 1;
	}
}


void
BoardView::_SetAirline(int index, int airline, bigtime_t delay, bool replay)
{
	Field& field = fFields[index];
	BString text;
	text << airline;
	if (field.text == text) {
		Cell& cell = fCells[field.first];
		if (replay && airline >= 0 && !cell.flipping && cell.queued == 0)
			_Enqueue(cell, cell.current, NULL, 0, system_time() + delay);
		return;
	}
	field.text = text;
	Cell& cell = fCells[field.first];
	BBitmap* target = _AirlineFace(airline);
#ifdef RDEPARTURES_QT
    // A wide colour card turns once directly to its next airline.
    _Enqueue(cell, target, NULL, 0, system_time() + delay);
#else
	BBitmap* rattle[4];
	fRandom = fRandom * 1103515245 + 12345;
	int n = airline < 0 ? 1 : 2 + (fRandom >> 16) % 3;
	for (int k = 0; k < n; k++) {
		fRandom = fRandom * 1103515245 + 12345;
		rattle[k] = _AirlineFace((fRandom >> 16) % airline_count());
	}
	fRandom = fRandom * 1103515245 + 12345;
	_Enqueue(cell, target, rattle, n,
		system_time() + delay + (fRandom >> 16) % 120000);
#endif
}


static inline void
copy_row(uint8* dst, const uint8* src, int pixels, int dark)
{
	if (dark >= 256) {
		memcpy(dst, src, pixels * 4);
		return;
	}
	for (int i = 0; i < pixels * 4; i += 4) {
		dst[i] = (uint8)(src[i] * dark >> 8);
		dst[i + 1] = (uint8)(src[i + 1] * dark >> 8);
		dst[i + 2] = (uint8)(src[i + 2] * dark >> 8);
		dst[i + 3] = 255;
	}
}


#ifdef RDEPARTURES_QT
// Wide, solid-colour cards need a visible moving outline: folding the text
// alone against an identical background looks like a stationary centre line.
static void
draw_colored_flip(QImage& surface, QPoint origin, const QImage& from,
    const QImage& to, float progress)
{
    const qreal w = from.width(), h = from.height(), half = int(h) / 2;
    const bool closing = progress < .5f;
    const qreal angle = (closing ? progress * 2 : (progress - .5f) * 2) * M_PI / 2;
    const qreal extent = closing ? cos(angle) : sin(angle);
    const qreal fold = sqrt(qMax<qreal>(0, 1 - extent * extent));
    const qreal inset = qMin<qreal>(12, w * .045) * fold;
    const QRectF top(0, 0, w, half), bottom(0, half, w, h - half);
    QPainter painter(&surface);
    painter.translate(origin);
    painter.setClipRect(QRectF(0, 0, w, h));
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(top, to, top);
    painter.drawImage(bottom, from, bottom);
    // The recessed face stays in shadow behind the rotating panel. This
    // preserves visible depth even when both faces have the same colour.
    painter.fillRect(closing ? top.adjusted(1, 0, -1, 0) : bottom.adjusted(1, 0, -1, 0),
        QColor(0, 0, 0, 155));
    const qreal edge = closing ? half - half * extent : half + (h - half) * extent;
    const QRectF source = closing ? top : bottom;
    const QPolygonF original{source.topLeft(), source.topRight(), source.bottomRight(), source.bottomLeft()};
    const QPolygonF panel = closing
        ? QPolygonF{{inset, edge}, {w - inset, edge}, {w, half}, {0, half}}
        : QPolygonF{{0, half}, {w, half}, {w - inset, edge}, {inset, edge}};
    QTransform transform;
    if (source.height() * extent > .35 && QTransform::quadToQuad(original, panel, transform)) {
        painter.save();
        painter.setTransform(transform, true);
        painter.drawImage(source, closing ? from : to, source);
        painter.restore();
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, int(125 * (1 - extent))));
        painter.drawPolygon(panel);
        painter.setPen(QPen(QColor(255, 255, 255, int(110 * fold)), 1));
        painter.drawLine(QPointF(inset + 1, edge), QPointF(w - inset - 1, edge));
    }
    painter.setPen(QPen(QColor(0, 0, 0), 1));
    painter.drawLine(QPointF(1, half), QPointF(w - 2, half));
}
#endif

// Draws a cell into the board: its current face, or the flip from it to the
// next one. The top half of the old face folds down onto the hinge, then
// the bottom half of the new one unfolds from it.
void
BoardView::_Compose(Cell& cell, bigtime_t now)
{
	BBitmap* a = cell.current;
	BBitmap* b = cell.flipping ? cell.queue[0] : NULL;
	int x = (int)cell.frame.left;
	int y0 = (int)cell.frame.top;
	int w = a->Bounds().IntegerWidth() + 1;
	int h = a->Bounds().IntegerHeight() + 1;
	int boardWidth = fBoard->Bounds().IntegerWidth() + 1;
	int boardHeight = fBoard->Bounds().IntegerHeight() + 1;
	if (x < 0 || y0 < 0 || x + w > boardWidth || y0 + h > boardHeight)
		return;

	uint8* base = (uint8*)fBoard->Bits();
	int32 bpr = fBoard->BytesPerRow();
	int32 abpr = a->BytesPerRow();
	const uint8* abits = (const uint8*)a->Bits();

	if (b == NULL) {
		for (int y = 0; y < h; y++)
			copy_row(base + (y0 + y) * bpr + x * 4, abits + y * abpr, w, 256);
		return;
	}

	int32 bbpr = b->BytesPerRow();
	const uint8* bbits = (const uint8*)b->Bits();
	float t = (float)(now - cell.stepStart) / cell.stepLength;
	if (t < 0)
		t = 0;
	if (t > 1)
		t = 1;
    bool deliberate = false;
#ifdef RDEPARTURES_QT
    deliberate = fCodexMode || cell.sizeClass == SIZE_AIRLINE;
#endif
    const float progress = t;
    if (deliberate) {
        // Linger edge-on at the hinge between closing and opening the flap.
        if (t < .44f) t /= .88f;
        else if (t < .56f) t = .5f;
        else t = .5f + (t - .56f) / .88f;
    }
#ifdef RDEPARTURES_QT
    if (cell.sizeClass == SIZE_AIRLINE) {
        // Borrow the board's buffer without detaching or changing cached faces.
        QImage surface(base, boardWidth, boardHeight, bpr, QImage::Format_RGB32);
        draw_colored_flip(surface, QPoint(x, y0), a->Image(), b->Image(), t);
        return;
    }
#endif
	int half = h / 2;
	for (int y = 0; y < h; y++) {
		const uint8* src;
		int dark = 256;
		if (t < 0.5f) {
			float u = t * 2;
			float k = deliberate ? cosf(u * M_PI / 2) : 1 - u * u;
			int flap = (int)(half * k);
			if (y < half - flap) {
				src = bbits + y * bbpr;
				dark = 256 - (int)(90 * k);
			} else if (y < half) {
				int sy = flap > 0 ? (y - (half - flap)) * half / flap : 0;
				src = abits + sy * abpr;
				dark = 256 - (int)(120 * (1 - k));
			} else
				src = abits + y * abpr;
		} else {
			float u = (t - 0.5f) * 2;
			float k = deliberate ? sinf(u * M_PI / 2) : 1 - (1 - u) * (1 - u);
			int flap = (int)((h - half) * k);
			if (y < half)
				src = bbits + y * bbpr;
			else if (y < half + flap) {
				int sy = half + (y - half) * (h - half) / flap;
				if (sy >= h)
					sy = h - 1;
				src = bbits + sy * bbpr;
				dark = 256 - (int)(120 * (1 - k));
			} else {
				src = abits + y * abpr;
				dark = 256 - (int)(90 * (1 - k));
			}
		}
		copy_row(base + (y0 + y) * bpr + x * 4, src, w, dark);
	}
    if (deliberate) {
        const float folded = sinf(progress * M_PI);
        const int band = h > 20 ? 3 : 2;
        for (int line = -band; line <= band; ++line) {
            const int y = half + line;
            if (y < 0 || y >= h) continue;
            uint8* pixels = base + (y0 + y) * bpr + x * 4;
            const int dark = 256 - int(125 * folded * (band + 1 - abs(line)) / (band + 1));
            if (dark < 256) copy_row(pixels, pixels, w, dark);
        }
        // A thin illuminated leading edge makes the moving panel legible even
        // when both faces contain the same text during a periodic replay.
        const float u = t < .5f ? t * 2 : (t - .5f) * 2;
        const int edge = t < .5f ? half - int(half * cosf(u * M_PI / 2))
            : half + int((h - half) * sinf(u * M_PI / 2)) - 1;
        if (edge >= 0 && edge < h && folded > .05f) {
            uint8* pixels = base + (y0 + edge) * bpr + x * 4;
            const int shine = int(65 * folded);
            for (int pixel = 1; pixel < w - 1; ++pixel)
                for (int channel = 0; channel < 3; ++channel) {
                    uint8& component = pixels[pixel * 4 + channel];
                    component += (255 - component) * shine / 256;
                }
        }
    }
}

bigtime_t BoardView::_StepLength(const Cell& cell) const
{
#ifdef RDEPARTURES_QT
    if (fCodexMode || cell.sizeClass == SIZE_AIRLINE)
        return bigtime_t(fCodexFlipDurationMs) * (cell.queued == 1 ? 1000 : 550);
#endif
    return cell.sizeClass == SIZE_AIRLINE ? kAirlineStep : cell.queued == 1 ? kFinalStep : kRattleStep;
}

void
BoardView::Tick()
{
	if (fBoard == NULL)
		return;
	bigtime_t now = system_time();
	if (fStation >= 0) {
		_TickStation(now);
		return;
	}
#ifdef RDEPARTURES_QT
    if (fCodexMode && fCodexFlipInterval > 0 && isVisible()
        && now >= fCodexSweepEnd
        && now - fLastCodexFlip >= bigtime_t(fCodexFlipInterval) * 1000000) {
        fLastCodexFlip = now;
        _FlipCodexRows(now);
    }
#endif

	if (fFixedPhase < 0 && now - fPhaseStart >= kPhaseLength) {
		fPhase = (fPhase + 1) % kAirports[fAirport].phaseCount;
		if (fPhase == 0)
			fFooter++;
		fPhaseStart = now;
		_UpdateContent(true);
	} else if (now - fLastUpdate >= 1000000)
		_UpdateContent(false);

	BRegion dirty;
	for (int i = 0; i < fCellCount; i++) {
		Cell& cell = fCells[i];
		if (!cell.flipping) {
			if (cell.queued == 0 || now < cell.startAt)
				continue;
			cell.flipping = true;
			cell.stepStart = now;
			cell.stepLength = _StepLength(cell);
		}
		if (now - cell.stepStart >= cell.stepLength) {
			cell.current = cell.queue[0];
			for (int q = 1; q < cell.queued; q++)
				cell.queue[q - 1] = cell.queue[q];
			cell.queued--;
			cell.flipping = cell.queued > 0;
			cell.stepStart = now;
            if (cell.flipping) cell.stepLength = _StepLength(cell);
		}
		_Compose(cell, now);
		dirty.Include(cell.frame);
	}

	if (Window() != NULL) {
		if (now - fLastMouse > 3000000) {
			be_app->ObscureCursor();
			fLastMouse = now + 3600000000LL;
		}
		if (dirty.CountRects() > 0)
			Invalidate(&dirty);
	}
}


// #pragma mark - content


int
BoardView::_PhaseLang() const
{
	const Airport& airport = kAirports[fAirport];
	int phase = fFixedPhase >= 0 ? fFixedPhase % airport.phaseCount : fPhase;
#ifdef RDEPARTURES_QT
    if (const auto* source = fDepartureSources.current(QString::fromLatin1(airport.code))) {
        const auto languages = source->info().languages;
        if (!languages.isEmpty()) {
            const auto language = languages[phase % languages.size()];
            return language == "ko" ? LANG_KO : language == "ja" ? LANG_JA : LANG_EN;
        }
    }
#endif
	return airport.phases[phase];
}


static int
status_color(int status)
{
	switch (status) {
		case STATUS_ON_TIME:
		case STATUS_GATE_OPEN:
			return COLOR_GREEN;
		case STATUS_BOARDING:
			return COLOR_YELLOW;
		case STATUS_FINAL_CALL:
		case STATUS_DELAYED:
			return COLOR_ORANGE;
		case STATUS_CANCELLED:
			return COLOR_RED;
		case STATUS_DEPARTED:
		case STATUS_GATE_CLOSED:
			return COLOR_GRAY;
		default:
			return COLOR_WHITE;
	}
}


void
BoardView::_UpdateContent(bool phaseChanged)
{
#ifdef RDEPARTURES_QT
    if (fCodexMode) { _UpdateCodex(); return; }
#endif
	fLastUpdate = system_time();
#ifdef RDEPARTURES_QT
    if (auto* source = fDepartureSources.source(QString::fromLatin1(kAirports[fAirport].code))) {
        _UpdateProviderDepartures(*source, phaseChanged); return;
    }
#endif
	const Airport& airport = kAirports[fAirport];
	int64 now = airport_local_minutes(airport, time(NULL) + gTimeOffset);
	int lang = _PhaseLang();
	int titleLang = lang == LANG_DEST ? LANG_EN : lang;

	char buffer[64];
	format_date(buffer, sizeof(buffer), now, titleLang);
	_SetField(fDateField, buffer, COLOR_DATE, 0);
	snprintf(buffer, sizeof(buffer), "%02d:%02d", (int)(now % 1440 / 60),
		(int)(now % 60));
	_SetField(fClockField, buffer, COLOR_YELLOW, 0);
	_SetField(fTitleField, title_text(titleLang), COLOR_WHITE, 0);
	_SetField(fFooterField, footer_text(fFooter, titleLang), COLOR_YELLOW,
		phaseChanged ? 300000 : 0);

	Flight flights[40];
	int count = build_departures(airport, now, flights, fRows);
	for (int r = 0; r < fRows; r++) {
		int f0 = fRowFields + r * COL_COUNT;
		bigtime_t delay = r * 45000;
		if (r >= count) {
			for (int c = 0; c < COL_COUNT; c++) {
				if (c == COL_AIRLINE)
					_SetAirline(f0 + c, -1, delay);
				else
					_SetField(f0 + c, "", COLOR_WHITE, delay);
			}
			continue;
		}
		const Flight& flight = flights[r];
		const Route& route = airport.routes[flight.route];
		const City* city = find_city(route.city);
		int status = flight_status(flight, now);

		snprintf(buffer, sizeof(buffer), "%02d:%02d",
			(int)(flight.scheduled % 1440 / 60), (int)(flight.scheduled % 60));
		_SetField(f0 + COL_TIME, buffer, COLOR_YELLOW, delay);
		if (flight.delay > 0 && !flight.cancelled) {
			int64 t = flight.scheduled + flight.delay;
			snprintf(buffer, sizeof(buffer), "%02d:%02d", (int)(t % 1440 / 60),
				(int)(t % 60));
			_SetField(f0 + COL_NEW, buffer, COLOR_ORANGE, delay);
		} else
			_SetField(f0 + COL_NEW, "", COLOR_WHITE, delay);
		_SetField(f0 + COL_DEST, city_name(city, lang), COLOR_WHITE, delay);
		_SetAirline(f0 + COL_AIRLINE,
			(int)(find_airline(route.airline) - airline_at(0)), delay, phaseChanged);
		format_flight_number(buffer, sizeof(buffer), airport, flight);
		_SetField(f0 + COL_FLIGHT, buffer, COLOR_WHITE, delay);
		_SetField(f0 + COL_GATE, status == STATUS_CANCELLED ? "" : flight.gate,
			COLOR_WHITE, delay);
		int remarksLang = lang == LANG_DEST ? remarks_lang(city->lang) : lang;
		_SetField(f0 + COL_REMARKS, status_text(status, remarksLang),
			status_color(status), delay);
	}
}


// #pragma mark - static parts


static void
draw_text(BView* view, const char* text, BPoint at, float size,
	rgb_color color, const BFont& main, const BFont& fallback,
	const BFont& cjk)
{
	BFont font(font_for(text, main, fallback, cjk));
	font.SetSize(size);
	view->SetFont(&font);
	view->SetHighColor(color);
	view->SetDrawingMode(B_OP_OVER);
	view->DrawString(text, at);
	view->SetDrawingMode(B_OP_COPY);
}


// A departing aircraft seen from above, nose up and to the right.
static void
draw_plane(BView* view, BRect box)
{
	static const float kPoints[][2] = {
		{ 1.0f, 0.5f }, { 0.86f, 0.45f }, { 0.6f, 0.45f }, { 0.36f, 0.06f },
		{ 0.27f, 0.06f }, { 0.42f, 0.45f }, { 0.2f, 0.45f }, { 0.1f, 0.27f },
		{ 0.04f, 0.27f }, { 0.1f, 0.47f }, { 0.1f, 0.53f }, { 0.04f, 0.73f },
		{ 0.1f, 0.73f }, { 0.2f, 0.55f }, { 0.42f, 0.55f }, { 0.27f, 0.94f },
		{ 0.36f, 0.94f }, { 0.6f, 0.55f }, { 0.86f, 0.55f }
	};
	const int count = sizeof(kPoints) / sizeof(kPoints[0]);
	float angle = -0.45f;
	float cs = cosf(angle);
	float sn = sinf(angle);
	float size = box.Width() * 0.8f;
	BPoint center((box.left + box.right) / 2, (box.top + box.bottom) / 2);
	BShape shape;
	for (int i = 0; i < count; i++) {
		float px = (kPoints[i][0] - 0.5f) * size;
		float py = (kPoints[i][1] - 0.5f) * size;
		BPoint p(center.x + px * cs - py * sn, center.y + px * sn + py * cs);
		if (i == 0)
			shape.MoveTo(p);
		else
			shape.LineTo(p);
	}
	shape.Close();
	view->MovePenTo(B_ORIGIN);
	view->FillShape(&shape);
}


static void
draw_gear(BView* view, BRect box, rgb_color color, rgb_color background)
{
	BPoint c((box.left + box.right) / 2, (box.top + box.bottom) / 2);
	float outer = box.Width() * 0.42f;
	float inner = box.Width() * 0.31f;
	BShape shape;
	const int teeth = 8;
	for (int i = 0; i < teeth; i++) {
		float a = i * 2 * M_PI / teeth;
		float spread[4] = { -0.30f, -0.17f, 0.17f, 0.30f };
		float radius[4] = { inner, outer, outer, inner };
		for (int k = 0; k < 4; k++) {
			BPoint p(c.x + radius[k] * cosf(a + spread[k]),
				c.y + radius[k] * sinf(a + spread[k]));
			if (i == 0 && k == 0)
				shape.MoveTo(p);
			else
				shape.LineTo(p);
		}
	}
	shape.Close();
	view->SetHighColor(color);
	view->MovePenTo(B_ORIGIN);
	view->FillShape(&shape);
	view->FillEllipse(c, inner, inner);
	view->SetHighColor(background);
	view->FillEllipse(c, inner * 0.45f, inner * 0.45f);
}


void
BoardView::_DrawStatic()
{
	const Airport& airport = kAirports[fAirport];
	BView* v = fBoardView;
	fBoard->Lock();
	v->SetHighColor(kBackground);
	v->FillRect(v->Bounds());
	float th = fCellHeight[SIZE_TITLE];
	float headerBottom = fLabelTop - floorf(fCellHeight[SIZE_ROW] * 0.15f);
	v->SetHighColor(kHeader);
	v->FillRect(BRect(0, 0, v->Bounds().right, headerBottom));

	// The aircraft in a white square, as on Japanese boards.
	Field& title = fFields[fTitleField];
	BRect icon = fCells[title.first].frame;
	icon.right = icon.left - floorf(th * 0.25f);
	icon.left = icon.right - th + 1;
	v->SetHighColor(240, 240, 236);
	v->FillRoundRect(icon, 3, 3);
	v->SetHighColor(0, 0, 0);
#ifdef RDEPARTURES_QT
    if (fCodexMode)
        draw_text(v, ">_", BPoint(icon.left + th * .10f, icon.bottom - th * .20f), th * .60f,
            make_color(0, 0, 0), fMain, fFallback, fCJK);
    else
#endif
	    draw_plane(v, icon);

	// The airport's name, after the title.
	float nameLeft = fCells[title.first + title.count - 1].frame.right
		+ fCW * 1.5f;
	BRect titleFrame = fCells[title.first].frame;
	float big = th * 0.42f;
	float small = th * 0.30f;
#ifdef RDEPARTURES_QT
    if (fCodexMode) {
        QString heading=fCodexHistoryMode?fHistorySession["title"].toString():QString("Oh My Departure");
        QString subheading=fCodexHistoryMode?fHistorySession["project"].toString()+" / "+fHistorySession["id"].toString().right(8)+" / READ ONLY"
            :QString("CODEX SESSIONS / READ ONLY");
        BFont headingFont(font_for(heading.toUtf8().constData(),fMain,fFallback,fCJK));headingFont.SetSize(big);
        heading=QFontMetricsF(headingFont.QtFont()).elidedText(heading,Qt::ElideRight,qMax(1.0f,fGearRect.left-nameLeft-fCW));
        BFont subtitleFont(font_for(subheading.toUtf8().constData(),fMain,fFallback,fCJK));subtitleFont.SetSize(small);
        subheading=QFontMetricsF(subtitleFont.QtFont()).elidedText(subheading,Qt::ElideRight,qMax(1.0f,fGearRect.left-nameLeft-fCW));
        draw_text(v,heading.toUtf8().constData(),BPoint(nameLeft,titleFrame.top+th*.44f),big,kLabel,fMain,fFallback,fCJK);
        draw_text(v,subheading.toUtf8().constData(),BPoint(nameLeft,titleFrame.top+th*.88f),small,kLabelEnglish,fMain,fFallback,fCJK);
    } else {
#endif
	if (airport.nameEnglish[0] != '\0') {
		draw_text(v, airport.nameLocal,
			BPoint(nameLeft, titleFrame.top + th * 0.44f), big, kLabel,
			fMain, fFallback, fCJK);
		draw_text(v, airport.nameEnglish,
			BPoint(nameLeft, titleFrame.top + th * 0.88f), small,
			kLabelEnglish, fMain, fFallback, fCJK);
	} else {
		draw_text(v, airport.nameLocal,
			BPoint(nameLeft, titleFrame.top + th * 0.68f), big, kLabel,
			fMain, fFallback, fCJK);
	}
#ifdef RDEPARTURES_QT
    }
#endif

	draw_gear(v, fGearRect, make_color(150, 150, 150), kHeader);

	// Column labels: the airport's language above English, as at Narita.
	float rh = fCellHeight[SIZE_ROW];
	int columns=COL_COUNT;
	const int* starts=kColumnStart;
#ifdef RDEPARTURES_QT
    if (fCodexHistoryMode) {columns=kHistoryColumnCount;starts=kHistoryColumnStart;}
#endif
	for (int c = 0; c < columns; c++) {
		float x = fX0 + starts[c] * fCW + 2;
#ifdef RDEPARTURES_QT
        if (fCodexHistoryMode) {
            static const char* primary[]={"TIME","EVENT","RECENT HISTORY","RESULT"};
            static const char* secondary[]={"TURN","TYPE","CONTENT PREVIEW","TURN / ITEM"};
            draw_text(v,primary[c],BPoint(x,fLabelTop+rh*.40f),rh*.36f,kLabel,fMain,fFallback,fCJK);
            draw_text(v,secondary[c],BPoint(x,fLabelTop+rh*.80f),rh*.30f,kLabelEnglish,fMain,fFallback,fCJK);
            continue;
        }
        if (fCodexMode) {
            static const char* primary[] = {"UPDATED", "SESSION", "TASK", "PROJECT", "AGENT", "SUB", "STATUS"};
            static const char* en[] = {"TIME", "ID", "TITLE", "FOLDER", "ROLE", "COUNT", "RUNTIME"};
            draw_text(v, primary[c], BPoint(x, fLabelTop + rh * .40f), rh * .36f, kLabel, fMain, fFallback, fCJK);
            draw_text(v, en[c], BPoint(x, fLabelTop + rh * .80f), rh * .30f, kLabelEnglish, fMain, fFallback, fCJK);
            continue;
        }
#endif
		if (airport.lang != LANG_EN) {
			draw_text(v, label_text(c, airport.lang),
				BPoint(x, fLabelTop + rh * 0.40f), rh * 0.36f, kLabel,
				fMain, fFallback, fCJK);
			draw_text(v, label_text(c, LANG_EN),
				BPoint(x, fLabelTop + rh * 0.80f), rh * 0.30f, kLabelEnglish,
				fMain, fFallback, fCJK);
		} else {
			draw_text(v, label_text(c, LANG_EN),
				BPoint(x, fLabelTop + rh * 0.70f), rh * 0.40f, kLabel,
				fMain, fFallback, fCJK);
		}
	}
	v->Sync();
	fBoard->Unlock();
}


void
BoardView::_ShowMenu(BPoint where)
{
	BPopUpMenu menu("airports", false, false);
	bool travelMode = true;
#ifdef RDEPARTURES_QT
    travelMode = !fCodexMode;
#endif
	for (int i = 0; i < kAirportCount; i++) {
		BMessage* message = new BMessage(MSG_SET_AIRPORT);
		message->AddInt32("airport", i);
		BMenuItem* item = new BMenuItem(kMenuNames[i], message);
		item->SetMarked(travelMode && fStation < 0 && i == fAirport);
		menu.AddItem(item);
	}
	for (int i = 0; i < kStationCount; i++) {
		BMessage* message = new BMessage(MSG_SET_AIRPORT);
		message->AddInt32("airport", kAirportCount + i);
		BMenuItem* item = new BMenuItem(kStations[i].menuName, message);
		item->SetMarked(travelMode && fStation == i);
		menu.AddItem(item);
	}
	// A line, then quitting, in the board's language.
#ifdef RDEPARTURES_QT
    menu.AddSeparatorItem();
    BMenuItem* codex = new BMenuItem("Codex sessions & agents", new BMessage('cdex'));
    codex->SetMarked(fCodexMode);
    menu.AddItem(codex);
    if (fCodexMode) menu.AddItem(new BMenuItem("Session browser & search…", new BMessage('cbrw')));
    if (fCodexMode) menu.AddItem(new BMenuItem("Display settings…", new BMessage('cset')));
#endif
	int lang = fStation < 0 ? kAirports[fAirport].lang
		: kStations[fStation].style == STYLE_JR ? LANG_JA
		: kStations[fStation].style == STYLE_KORAIL ? LANG_KO : LANG_EN;
#ifdef RDEPARTURES_QT
    if (fCodexMode) lang = LANG_EN;
#endif
	const char* quit = "Quit";
	switch (lang) {
		case LANG_KO: quit = "종료"; break;
		case LANG_JA: quit = "終了"; break;
		case LANG_FR: quit = "Quitter"; break;
		case LANG_IT: quit = "Esci"; break;
		case LANG_ZHT: quit = "結束"; break;
	}
	menu.AddSeparatorItem();
	menu.AddItem(new BMenuItem(quit, new BMessage(B_QUIT_REQUESTED)));

	BMenuItem* chosen = menu.Go(ConvertToScreen(where), false, true);
	if (chosen == NULL)
		return;
	BMessage* message = chosen->Message();
	if (message->what == B_QUIT_REQUESTED)
		be_app->PostMessage(B_QUIT_REQUESTED);
#ifdef RDEPARTURES_QT
    else if (message->what == 'cdex') {
        if (fCodexHandler) fCodexHandler();
    }
    else if (message->what == 'cset') {
        if (fSettingsHandler) fSettingsHandler();
    }
    else if (message->what == 'cbrw') {
        if (fBrowserHandler) fBrowserHandler();
    }
#endif
	else {
		int32 index;
		if (message->FindInt32("airport", &index) == B_OK)
			SetAirport(index);
	}
}


// #pragma mark - Tokyo Station


static const rgb_color kLedBackground = { 6, 6, 8, 255 };
static const rgb_color kLedText = { 238, 238, 232, 255 };
static const rgb_color kLedDim = { 150, 150, 146, 255 };
static const rgb_color kLedRule = { 44, 44, 48, 255 };
static const rgb_color kLedAmber = { 255, 170, 40, 255 };


// Text fitted into a rectangle: shrunk to its width, centred on its height.
void
BoardView::_DrawIn(BView* view, const char* text, BRect r, float size,
	rgb_color color, int align, const BFont* latin)
{
	if (text == NULL || text[0] == '\0')
		return;
	BFont font(font_for(text, latin != NULL ? *latin : fMain, fFallback,
		fCJK));
	font.SetSize(size);
	float w = font.StringWidth(text);
	float room = r.Width() + 1;
	if (w > room) {
		font.SetSize(size * room / w);
		w = font.StringWidth(text);
	}
	font_height fh;
	font.GetHeight(&fh);
	float x = r.left;
	if (align == ALIGN_CENTER)
		x = r.left + floorf((room - w) / 2);
	else if (align == ALIGN_RIGHT)
		x = r.right + 1 - w;
	float y = floorf(r.top + (r.Height() + 1 + fh.ascent - fh.descent) / 2);
	view->SetFont(&font);
	view->SetHighColor(color);
	view->SetDrawingMode(B_OP_OVER);
	view->DrawString(text, BPoint(x, y));
	view->SetDrawingMode(B_OP_COPY);
}


void
BoardView::_TickStation(bigtime_t now)
{
	const Station& station = kStations[fStation];
	int64 minute = local_minutes(station.utcOffset, station.dstRule,
		time(NULL) + gTimeOffset);
	bigtime_t elapsed = now - fPhaseStart;

	// What changes on each kind of board between minutes: Tokyo's boards
	// and languages, Seoul's four languages, Victoria's pages of calling
	// points. --phase pins it for snapshots.
	int key = 0;
	switch (station.style) {
		case STYLE_JR:
			if (fFixedPhase >= 0)
				key = fFixedPhase % (kStationBoardCount * 2);
			else {
				key = (int)(elapsed / kStationBoardLength) % kStationBoardCount
					* 2 + (int)(elapsed / kStationLangLength) % 2;
			}
			break;
		case STYLE_KORAIL:
			key = fFixedPhase >= 0 ? fFixedPhase % 4
				: (int)(elapsed / kStationLangLength) % 4;
			break;
		case STYLE_UK:
			key = fFixedPhase >= 0 ? fFixedPhase
				: (int)(elapsed / 5000000);
			break;
	}

	if (key != fStationKey || minute != fStationMinute) {
		fStationKey = key;
		fStationMinute = minute;
		switch (station.style) {
			case STYLE_JR:
				_DrawStation(key / 2, key % 2, minute);
				break;
			case STYLE_KORAIL:
				_DrawSeoul(key, minute);
				break;
			case STYLE_UK:
				_DrawVictoria(key, minute);
				break;
			case STYLE_SYDNEY:
				_DrawSydney(minute);
				break;
		}
		if (Window() != NULL)
			Invalidate();
	}
	if (station.style == STYLE_KORAIL || station.style == STYLE_SYDNEY)
		_DrawMarquees(now);

	if (Window() != NULL && now - fLastMouse > 3000000) {
		be_app->ObscureCursor();
		fLastMouse = now + 3600000000LL;
	}
}


// One board, drawn whole: these boards change line by line, they do not
// flap.
void
BoardView::_DrawStation(int boardIndex, int lang, int64 now)
{
	const StationBoard& board = kStationBoards[boardIndex];
	bool english = lang == 1;
	BView* v = fBoardView;
	fBoard->Lock();
	BRect bounds = v->Bounds();
	float W = bounds.Width() + 1;
	float H = bounds.Height() + 1;
	v->SetHighColor(kLedBackground);
	v->FillRect(bounds);

	float m = floorf(W * 0.012f);
	float header = floorf(H * 0.24f);
	float labelH = floorf(H * 0.05f);
	float lineH = floorf(H * 0.045f);

	// Which board, and which way.
	BString line;
	if (english)
		line << board.titleEn << "  " << board.directionEn;
	else
		line << board.titleJp << "　" << board.directionJp;
	_DrawIn(v, line.String(), BRect(m, m, W * 0.5f, m + lineH - 1),
		lineH * 0.78f, kLedDim, ALIGN_LEFT);

	// Clock, and what the board is.
	float top = m + lineH + floorf(m * 0.5f);
	float bottom = header - labelH - floorf(m * 0.5f);
	BRect clock(m, top, m + floorf(W * 0.1f), bottom);
	v->SetHighColor(30, 30, 34);
	v->FillRoundRect(clock, 4, 4);
	char buffer[64];
	snprintf(buffer, sizeof(buffer), "%02d:%02d", (int)(now % 1440 / 60),
		(int)(now % 60));
	_DrawIn(v, buffer, clock.InsetByCopy(6, 2), clock.Height() * 0.8f,
		kLedText, ALIGN_CENTER);
	float titleLeft = clock.right + m * 1.5f;
	float titleH = bottom - top + 1;
	_DrawIn(v, "発車時刻・のりば案内",
		BRect(titleLeft, top, W * 0.5f, top + titleH * 0.6f), titleH * 0.5f,
		kLedText, ALIGN_LEFT);
	_DrawIn(v, "Departure Time & Track Information",
		BRect(titleLeft, top + titleH * 0.6f, W * 0.5f, bottom),
		titleH * 0.3f, kLedDim, ALIGN_LEFT);

	draw_gear(v, fGearRect, make_color(150, 150, 150), kLedBackground);

	// Columns.
	float x = m;
	float timeX = x;
	float timeW = floorf(W * 0.08f);
	x += timeW;
	float destX = x;
	float destW = floorf(W * (board.stationCount > 0 ? 0.13f : 0.16f));
	x += destW;
	float trackX = x;
	float trackW = floorf(W * 0.075f);
	x += trackW + floorf(m * 0.5f);
	float originX = x;
	float originW = floorf(W * 0.045f);
	x += originW;
	float trainX = x;
	float trainW = floorf(W * (board.stationCount > 0 ? 0.17f : 0.2f));
	x += trainW + m;
	float carsX = x;
	float carsW = board.shinkansen ? floorf(W * 0.085f) : 0;
	x += carsW;
	float patternX = x;
	float tagW = board.stationCount > 0 ? floorf(W * 0.022f) : 0;
	float columnsX = patternX + tagW + floorf(m * 0.5f);
	float patternRight = W - m;
	float colW = board.stationCount > 0
		? floorf((patternRight - columnsX) / board.stationCount) : 0;

	// Column labels, Japanese over English as on the boards.
	float labelTop = header - labelH;
	struct { float x, w; const char* jp; const char* en; } labels[] = {
		{ timeX, timeW, "時刻", "Time" },
		{ destX, destW, "行先", "Destination" },
		{ trackX, trackW, "のりば", "Track" },
		{ trainX, trainW, "種別・列車名", "Train" },
		{ carsX, carsW, "自由席", "Non-reserved" },
	};
	for (int i = 0; i < 5; i++) {
		if (labels[i].w <= 0)
			continue;
		BRect r(labels[i].x, labelTop, labels[i].x + labels[i].w - 4,
			labelTop + labelH * 0.55f);
		_DrawIn(v, labels[i].jp, r, labelH * 0.5f, kLedDim, ALIGN_CENTER);
		r.OffsetBy(0, labelH * 0.5f);
		_DrawIn(v, labels[i].en, r, labelH * 0.38f, kLedDim, ALIGN_CENTER);
	}

	// The stations the pattern columns stand for, written downwards, and
	// the yellow tag that says so.
	if (board.stationCount > 0) {
		float namesTop = fGearRect.bottom + floorf(m * 0.5f);
		float namesBottom = header - 3;
		BRect tag(patternX, namesTop, patternX + tagW - 2, namesBottom);
		v->SetHighColor(240, 200, 30);
		v->FillRoundRect(tag, 3, 3);
		const char* tagText[] = { "主", "な", "停", "車", "駅" };
		float th = (tag.Height() + 1) / 5;
		for (int i = 0; i < 5; i++) {
			_DrawIn(v, tagText[i], BRect(tag.left, tag.top + i * th, tag.right,
				tag.top + (i + 1) * th - 1), th * 0.8f, make_color(20, 20, 20),
				ALIGN_CENTER);
		}
		for (int s = 0; s < board.stationCount; s++) {
			Unit units[8];
			int n = split_units(board.stationsJp[s], units, 8);
			float ch = (namesBottom - namesTop + 1) / 4;
			if (ch > colW * 0.8f)
				ch = floorf(colW * 0.8f);
			float cx = columnsX + s * colW;
			for (int k = 0; k < n; k++) {
				_DrawIn(v, units[k].glyph, BRect(cx, namesTop + k * ch,
					cx + colW - 1, namesTop + (k + 1) * ch - 1), ch * 0.9f,
					kLedText, ALIGN_CENTER);
			}
		}
	}

	v->SetHighColor(kLedRule);
	v->StrokeLine(BPoint(m, header), BPoint(W - m, header));

	// The trains.
	const int kRows = 10;
	float rowsTop = header + 2;
	float rowH = floorf((H - rowsTop - m) / kRows);
	Train trains[kRows];
	int count = build_trains(board, boardIndex, now, trains, kRows);
	for (int r = 0; r < count; r++) {
		const Train& train = trains[r];
		const Service& service = *train.service;
		rgb_color color = { service.color[0], service.color[1],
			service.color[2], 255 };
		float y = rowsTop + r * rowH;
		BRect row(m, y, W - m, y + rowH - 1);
		float pad = floorf(rowH * 0.1f);

		snprintf(buffer, sizeof(buffer), "%d:%02d",
			(int)(train.departs % 1440 / 60), (int)(train.departs % 60));
		_DrawIn(v, buffer, BRect(timeX, y, timeX + timeW - 6, y + rowH - 1),
			rowH * 0.66f, kLedText, ALIGN_RIGHT);
		_DrawIn(v, english ? service.destEn : service.destJp,
			BRect(destX + m * 0.6f, y, destX + destW - 4, y + rowH - 1),
			rowH * 0.62f, kLedText, ALIGN_LEFT);

		// Track: a box in the line's colour, the number, and 番線.
		BRect box(trackX, y + pad, trackX + trackW - 4, y + rowH - 1 - pad);
		v->SetHighColor(color);
		v->FillRect(box);
		// The Sobu/Yokosuka platforms are below ground: 地下1番線, B1.
		float left = box.left + 2;
		if (service.underground && !english) {
			float w = box.Width() * 0.24f;
			_DrawIn(v, "地下", BRect(left, box.top + box.Height() * 0.45f,
				left + w, box.bottom - 1), box.Height() * 0.4f, kLedText,
				ALIGN_LEFT);
			left += w + 2;
		}
		snprintf(buffer, sizeof(buffer), service.underground && english
			? "B%d" : "%d", train.track);
		float numW = box.Width() * (service.underground ? 0.62f : 0.55f);
		_DrawIn(v, buffer, BRect(left, box.top, box.left + numW, box.bottom),
			box.Height() * 0.95f, kLedText, ALIGN_RIGHT);
		_DrawIn(v, english ? "Track" : "番線", BRect(box.left + numW + 3,
			box.top + box.Height() * 0.45f, box.right - 2, box.bottom - 1),
			box.Height() * 0.4f, kLedText, ALIGN_LEFT);

		if (train.startsHere) {
			_DrawIn(v, english ? "Orig." : "始発", BRect(originX, y,
				originX + originW - 4, y + rowH - 1), rowH * 0.5f, kLedText,
				ALIGN_CENTER);
		}

		// Train: the name on the line's colour, with its number.
		BRect name(trainX, y + pad, trainX + trainW - 1, y + rowH - 1 - pad);
		v->SetHighColor(color);
		v->FillRect(name);
		BString label(english ? service.typeEn : service.typeJp);
		if (train.number > 0) {
			if (english)
				label << " " << train.number;
			else
				label << " " << train.number << "号";
		}
		_DrawIn(v, label.String(), name.InsetByCopy(6, 0), name.Height() * 0.8f,
			kLedText, ALIGN_CENTER);

		if (board.shinkansen) {
			BRect cars(carsX, y, carsX + carsW - 4, y + rowH - 1);
			if (service.unreserved != NULL) {
				BString text;
				if (english)
					text << "Cars " << service.unreserved;
				else
					text << service.unreserved << "号車";
				_DrawIn(v, text.String(), cars, rowH * 0.5f, kLedText,
					ALIGN_CENTER);
			} else {
				_DrawIn(v, english ? "All reserved" : "全車指定", cars,
					rowH * 0.48f, kLedAmber, ALIGN_CENTER);
			}
		}

		if (board.stationCount > 0) {
			// ○ stops, ─ passes, nothing beyond where it goes.
			float d = floorf((colW < rowH ? colW : rowH) * 0.52f);
			v->SetPenSize(floorf(d * 0.13f) > 2 ? floorf(d * 0.13f) : 2);
			v->SetHighColor(color);
			int n = strlen(service.stops);
			for (int s = 0; s < board.stationCount && s < n; s++) {
				BPoint c(columnsX + s * colW + colW / 2, y + rowH / 2);
				if (service.stops[s] == 'o')
					v->StrokeEllipse(c, d / 2, d / 2);
				else if (service.stops[s] == '-') {
					v->StrokeLine(BPoint(c.x - d * 0.35f, c.y),
						BPoint(c.x + d * 0.35f, c.y));
				}
			}
			v->SetPenSize(1);
		} else {
			_DrawIn(v, english ? service.stopsEn : service.stops,
				BRect(patternX, y, patternRight, y + rowH - 1), rowH * 0.42f,
				kLedDim, ALIGN_LEFT);
		}

		v->SetHighColor(kLedRule);
		v->StrokeLine(BPoint(m, row.bottom), BPoint(W - m, row.bottom));
	}
	v->Sync();
	fBoard->Unlock();
}


// #pragma mark - Seoul, Victoria, Sydney


static uint32
station_hash(int64 departs, int service)
{
	uint32 h = (uint32)(departs * 2654435761u) ^ (uint32)(service * 40503);
	h ^= h >> 15;
	h *= 0x2c1b3c6d;
	h ^= h >> 12;
	return h;
}


// The top band shared by Seoul and Sydney: a title, a subtitle, the clock
// and the gear.
void
BoardView::_DrawStationHeader(const char* title, const char* subtitle,
	int64 now, rgb_color band, rgb_color accent)
{
	BView* v = fBoardView;
	BRect bounds = v->Bounds();
	float W = bounds.Width() + 1;
	float H = bounds.Height() + 1;
	float m = floorf(W * 0.012f);
	float bandH = floorf(H * 0.11f);
	v->SetHighColor(band);
	v->FillRect(BRect(0, 0, W - 1, bandH - 1));
	v->SetHighColor(accent);
	v->FillRect(BRect(0, bandH - 4, W - 1, bandH - 1));
	_DrawIn(v, title, BRect(m, 0, W * 0.6f, bandH * 0.62f), bandH * 0.42f,
		kLedText, ALIGN_LEFT, &fFallback);
	_DrawIn(v, subtitle, BRect(m, bandH * 0.55f, W * 0.6f, bandH - 6),
		bandH * 0.26f, accent, ALIGN_LEFT, &fFallback);
	char clock[16];
	snprintf(clock, sizeof(clock), "%02d:%02d", (int)(now % 1440 / 60),
		(int)(now % 60));
	_DrawIn(v, clock, BRect(fGearRect.left - W * 0.13f, 0, fGearRect.left - m,
		bandH - 6), bandH * 0.5f, kLedText, ALIGN_RIGHT, &fFallback);
	draw_gear(v, fGearRect, make_color(170, 170, 170), band);
}


// #pragma mark - Seoul


void
BoardView::_DrawSeoul(int lang, int64 now)
{
	static const char* kTitle[4] = { "서울역  출발 열차 안내",
		"Seoul Station  Departures", "ソウル駅  出発案内", "首尔站  出发信息" };
	static const char* kLabels[4][6] = {
		{ "열차종류", "열차번호", "행선지", "출발시각", "타는곳", "비고" },
		{ "Train", "No.", "Destination", "Departure", "Platform", "Remarks" },
		{ "列車種別", "列車番号", "行先", "発車時刻", "のりば", "備考" },
		{ "车次类型", "车次", "终点", "发车时间", "站台", "备注" },
	};
	static const char* kStatus[4][4] = {
		{ "정상운행", "탑승중", "탑승마감", "지연" },
		{ "On time", "Boarding", "Closing", "Delayed" },
		{ "定刻", "乗車中", "締切", "遅れ" },
		{ "正点", "检票中", "停止检票", "晚点" },
	};

	BView* v = fBoardView;
	fBoard->Lock();
	BRect bounds = v->Bounds();
	float W = bounds.Width() + 1;
	float H = bounds.Height() + 1;
	float m = floorf(W * 0.012f);
	v->SetHighColor(kLedBackground);
	v->FillRect(bounds);
	fMarqueeCount = 0;
	fMarqueeStart = system_time();
	_DrawStationHeader(kTitle[lang], lang == 0 ? "Seoul Station" : "서울역",
		now, make_color(12, 32, 80),
		make_color(0, 120, 210));

	float bandH = floorf(H * 0.11f);
	float labelH = floorf(H * 0.06f);
	float labelTop = bandH;
	v->SetHighColor(28, 30, 40);
	v->FillRect(BRect(0, labelTop, W - 1, labelTop + labelH - 1));
	float fractions[6] = { 0.15f, 0.09f, 0.36f, 0.11f, 0.09f, 0.0f };
	float xs[7];
	xs[0] = m;
	for (int i = 0; i < 5; i++)
		xs[i + 1] = xs[i] + floorf(W * fractions[i]);
	xs[6] = W - m;
	for (int i = 0; i < 6; i++) {
		_DrawIn(v, kLabels[lang][i], BRect(xs[i], labelTop, xs[i + 1] - 8,
			labelTop + labelH - 1), labelH * 0.5f, kLedDim, ALIGN_CENTER,
			&fFallback);
	}

	const int kRows = 10;
	float rowsTop = labelTop + labelH;
	float rowH = floorf((H - rowsTop - m) / kRows);
	Slot slots[24];
	int count = build_slots(kKorail, sizeof(KorailService), kKorailCount, now,
		now - 30, slots, 24);
	int row = 0;
	char buffer[64];
	for (int i = 0; i < count && row < kRows; i++) {
		const KorailService& service = kKorail[slots[i].service];
		uint32 h = station_hash(slots[i].departs, slots[i].service);
		int delay = h % 100 < 8 ? 5 + (int)((h >> 8) % 4) * 5 : 0;
		int64 leaves = slots[i].departs + delay;
		if (leaves < now)
			continue;
		float y = rowsTop + row * rowH;
		if (row % 2 == 1) {
			v->SetHighColor(14, 15, 20);
			v->FillRect(BRect(0, y, W - 1, y + rowH - 1));
		}
		row++;
		float pad = floorf(rowH * 0.16f);

		rgb_color color = { service.color[0], service.color[1],
			service.color[2], 255 };
		BRect badge(xs[0], y + pad, xs[1] - 10, y + rowH - 1 - pad);
		v->SetHighColor(color);
		v->FillRoundRect(badge, 4, 4);
		_DrawIn(v, service.type[lang], badge.InsetByCopy(6, 0),
			badge.Height() * 0.68f, kLedText, ALIGN_CENTER, &fFallback);

		snprintf(buffer, sizeof(buffer), service.number < 1000 ? "%03d" : "%d",
			service.number + 2 * slots[i].index);
		_DrawIn(v, buffer, BRect(xs[1], y, xs[2] - 8, y + rowH - 1),
			rowH * 0.55f, kLedText, ALIGN_CENTER, &fFallback);
		_DrawIn(v, service.dest[lang], BRect(xs[2] + 6, y, xs[3] - 8,
			y + rowH - 1), rowH * 0.6f, kLedText, ALIGN_LEFT, &fFallback);
		// After the destination, small, the stations it stops at: they
		// scroll, redrawn on every tick by _DrawMarquees().
		BFont destFont(font_for(service.dest[lang], fFallback, fFallback,
			fCJK));
		destFont.SetSize(rowH * 0.6f);
		float marqueeLeft = xs[2] + 6 + destFont.StringWidth(service.dest[lang])
			+ rowH * 0.5f;
		if (fMarqueeCount < 16 && marqueeLeft < xs[3] - 40) {
			Marquee& marquee = fMarquees[fMarqueeCount++];
			marquee.vertical = false;
			marquee.area.Set(marqueeLeft, y + rowH * 0.2f, xs[3] - 10,
				y + rowH * 0.8f);
			marquee.text = korail_stops(service, lang);
			marquee.background = row % 2 == 0 ? make_color(14, 15, 20)
				: kLedBackground;
			BFont font(font_for(marquee.text.String(), fFallback, fFallback,
				fCJK));
			font.SetSize(rowH * 0.32f);
			marquee.width = font.StringWidth(marquee.text.String());
		}
		snprintf(buffer, sizeof(buffer), "%02d:%02d",
			(int)(slots[i].departs % 1440 / 60), (int)(slots[i].departs % 60));
		_DrawIn(v, buffer, BRect(xs[3], y, xs[4] - 8, y + rowH - 1),
			rowH * 0.6f, make_color(255, 210, 60), ALIGN_CENTER, &fFallback);

		int track = service.trackMin
			+ (int)((h >> 16) % (service.trackMax - service.trackMin + 1));
		static const char* kTrack[4] = { "%d", "%d", "%d番線", "%d站台" };
		snprintf(buffer, sizeof(buffer), kTrack[lang], track);
		_DrawIn(v, buffer, BRect(xs[4], y, xs[5] - 8, y + rowH - 1),
			rowH * 0.55f, kLedText, ALIGN_CENTER, &fFallback);

		int left = (int)(leaves - now);
		rgb_color statusColor = kLedDim;
		BString status;
		if (left <= 2) {
			status = kStatus[lang][2];
			statusColor = make_color(255, 80, 70);
		} else if (left <= 12) {
			status = kStatus[lang][1];
			statusColor = make_color(110, 230, 120);
		} else
			status = kStatus[lang][0];
		if (delay > 0) {
			BString late;
			switch (lang) {
				case 0: late << "지연 " << delay << "분"; break;
				case 1: late << "Delayed " << delay << " min"; break;
				case 2: late << delay << "分遅れ"; break;
				default: late << "晚点" << delay << "分"; break;
			}
			if (left > 12) {
				status = late;
				statusColor = make_color(255, 150, 40);
			}
		}
		_DrawIn(v, status.String(), BRect(xs[5] + 6, y, xs[6], y + rowH - 1),
			rowH * 0.5f, statusColor, ALIGN_LEFT, &fFallback);
	}
	v->Sync();
	fBoard->Unlock();
}


// #pragma mark - London Victoria


// A raised palm: wait.
static void
draw_hand(BView* v, BRect r, rgb_color color)
{
	v->SetHighColor(color);
	float w = r.Width();
	float h = r.Height();
	BRect palm(r.left + w * 0.2f, r.top + h * 0.45f, r.left + w * 0.82f,
		r.bottom);
	v->FillRoundRect(palm, w * 0.15f, w * 0.15f);
	for (int i = 0; i < 4; i++) {
		float fx = r.left + w * (0.2f + i * 0.16f);
		float top = r.top + h * (i == 0 || i == 3 ? 0.14f : 0.0f);
		v->FillRoundRect(BRect(fx, top, fx + w * 0.13f, r.top + h * 0.6f),
			w * 0.06f, w * 0.06f);
	}
	BShape thumb;
	thumb.MoveTo(BPoint(r.left + w * 0.22f, r.top + h * 0.62f));
	thumb.LineTo(BPoint(r.left, r.top + h * 0.42f));
	thumb.LineTo(BPoint(r.left + w * 0.06f, r.top + h * 0.36f));
	thumb.LineTo(BPoint(r.left + w * 0.3f, r.top + h * 0.52f));
	thumb.Close();
	v->MovePenTo(B_ORIGIN);
	v->FillShape(&thumb);
}


// Someone walking, and the way to go: go to the platform.
static void
draw_walker(BView* v, BRect r, rgb_color color)
{
	v->SetHighColor(color);
	float w = r.Width();
	float h = r.Height();
	float pen = floorf(w * 0.1f) > 2 ? floorf(w * 0.1f) : 2;
	v->SetPenSize(pen);
	BPoint head(r.left + w * 0.62f, r.top + h * 0.12f);
	v->FillEllipse(head, w * 0.1f, w * 0.1f);
	BPoint neck(r.left + w * 0.6f, r.top + h * 0.3f);
	BPoint hip(r.left + w * 0.55f, r.top + h * 0.62f);
	v->StrokeLine(neck, hip);
	v->StrokeLine(hip, BPoint(r.left + w * 0.38f, r.bottom));
	v->StrokeLine(hip, BPoint(r.left + w * 0.72f, r.top + h * 0.8f));
	v->StrokeLine(BPoint(r.left + w * 0.72f, r.top + h * 0.8f),
		BPoint(r.left + w * 0.7f, r.bottom));
	v->StrokeLine(BPoint(r.left + w * 0.58f, r.top + h * 0.38f),
		BPoint(r.left + w * 0.82f, r.top + h * 0.55f));
	v->StrokeLine(BPoint(r.left + w * 0.58f, r.top + h * 0.38f),
		BPoint(r.left + w * 0.4f, r.top + h * 0.5f));
	// The arrow, pointing left.
	v->StrokeLine(BPoint(r.left, r.top + h * 0.3f),
		BPoint(r.left + w * 0.3f, r.top + h * 0.3f));
	v->StrokeLine(BPoint(r.left, r.top + h * 0.3f),
		BPoint(r.left + w * 0.12f, r.top + h * 0.18f));
	v->StrokeLine(BPoint(r.left, r.top + h * 0.3f),
		BPoint(r.left + w * 0.12f, r.top + h * 0.42f));
	v->SetPenSize(1);
}


void
BoardView::_DrawVictoria(int page, int64 now)
{
	static const rgb_color kBlue = { 32, 104, 210, 255 };
	static const rgb_color kGreen = { 54, 160, 72, 255 };
	static const rgb_color kGrey = { 70, 72, 76, 255 };
	static const rgb_color kYellow = { 250, 200, 40, 255 };
	static const rgb_color kBody = { 14, 16, 20, 255 };

	BView* v = fBoardView;
	fBoard->Lock();
	BRect bounds = v->Bounds();
	float W = bounds.Width() + 1;
	float H = bounds.Height() + 1;
	v->SetHighColor(kBody);
	v->FillRect(bounds);

	// "Welcome to Victoria Station", on the grey band above the screens.
	float bannerH = floorf(H * 0.11f);
	v->SetHighColor(64, 68, 74);
	v->FillRect(BRect(0, 0, W - 1, bannerH - 1));
	float m = floorf(W * 0.012f);
	BFont big(fFallback);
	big.SetSize(bannerH * 0.62f);
	BFont small(fFallback);
	small.SetFace(B_REGULAR_FACE);
	small.SetSize(bannerH * 0.4f);
	font_height fh;
	big.GetHeight(&fh);
	float baseline = floorf((bannerH + fh.ascent - fh.descent) / 2);
	v->SetHighColor(240, 240, 240);
	v->SetDrawingMode(B_OP_OVER);
	v->SetFont(&small);
	v->DrawString("Welcome to ", BPoint(m * 2, baseline));
	float x = m * 2 + small.StringWidth("Welcome to ");
	v->SetFont(&big);
	v->DrawString("Victoria Station", BPoint(x, baseline));
	v->SetDrawingMode(B_OP_COPY);
	char buffer[64];
	snprintf(buffer, sizeof(buffer), "%02d:%02d", (int)(now % 1440 / 60),
		(int)(now % 60));
	_DrawIn(v, buffer, BRect(fGearRect.left - W * 0.12f, 0,
		fGearRect.left - m, bannerH - 1), bannerH * 0.5f, kLedText,
		ALIGN_RIGHT, &fFallback);
	draw_gear(v, fGearRect, make_color(200, 200, 200), make_color(64, 68, 74));

	const int kColumns = 8;
	Slot slots[kColumns];
	int count = build_slots(kVictoria, sizeof(UkService), kVictoriaCount, now,
		now, slots, kColumns);
	float colW = floorf(W / kColumns);
	float top = bannerH + 2;
	float headH = floorf(H * 0.25f);
	float lineH = floorf(H * 0.031f);
	float stripH = floorf(H * 0.045f);
	for (int c = 0; c < count; c++) {
		const UkService& service = kVictoria[slots[c].service];
		uint32 h = station_hash(slots[c].departs, slots[c].service);
		bool cancelled = h % 100 < 5;
		int delay = !cancelled && h % 100 < 15 ? 2 + (int)((h >> 8) % 4) * 3
			: 0;
		int left = (int)(slots[c].departs - now);
		bool platform = !cancelled && left <= 12;
		int track = service.trackMin
			+ (int)((h >> 16) % (service.trackMax - service.trackMin + 1));

		float x0 = c * colW;
		float pad = floorf(colW * 0.05f);
		BRect head(x0 + 1, top, x0 + colW - 2, top + headH - 1);
		v->SetHighColor(cancelled ? kGrey : platform ? kGreen : kBlue);
		v->FillRect(head);

		snprintf(buffer, sizeof(buffer), "%02d:%02d",
			(int)(slots[c].departs % 1440 / 60), (int)(slots[c].departs % 60));
		_DrawIn(v, buffer, BRect(head.left + pad, head.top + headH * 0.04f,
			head.left + colW * (cancelled ? 0.5f : 0.6f),
			head.top + headH * 0.26f), headH * 0.2f, kLedText, ALIGN_LEFT,
			&fFallback);
		if (cancelled) {
			_DrawIn(v, "Cancelled", BRect(head.left + colW * 0.56f,
				head.top + headH * 0.04f, head.right - pad,
				head.top + headH * 0.26f), headH * 0.12f, kYellow,
				ALIGN_RIGHT, &fFallback);
		}
		_DrawIn(v, service.dest, BRect(head.left + pad, head.top + headH * 0.27f,
			head.right - pad, head.top + headH * 0.5f), headH * 0.19f,
			kLedText, ALIGN_LEFT, &fFallback);
		_DrawIn(v, service.via, BRect(head.left + pad, head.top + headH * 0.5f,
			head.right - pad, head.top + headH * 0.64f), headH * 0.11f,
			kLedText, ALIGN_LEFT, &fFallback);

		BRect status(head.left + pad, head.top + headH * 0.66f,
			head.right - pad, head.bottom - pad * 0.5f);
		float icon = status.Height();
		BRect iconRect(status.right - icon * 0.8f, status.top + icon * 0.05f,
			status.right, status.bottom - icon * 0.05f);
		if (cancelled) {
			v->SetHighColor(220, 220, 220);
			v->SetPenSize(floorf(icon * 0.08f) > 2 ? floorf(icon * 0.08f) : 2);
			BPoint c0(head.left + colW / 2, status.top + icon / 2);
			float r = icon * 0.35f;
			v->StrokeLine(BPoint(c0.x - r, c0.y - r), BPoint(c0.x + r, c0.y + r));
			v->StrokeLine(BPoint(c0.x - r, c0.y + r), BPoint(c0.x + r, c0.y - r));
			v->SetPenSize(1);
		} else if (platform) {
			_DrawIn(v, "Platform", BRect(status.left, status.top,
				status.left + colW * 0.5f, status.top + icon * 0.38f),
				icon * 0.32f, kLedText, ALIGN_LEFT, &fFallback);
			snprintf(buffer, sizeof(buffer), "%d", track);
			_DrawIn(v, buffer, BRect(status.left, status.top + icon * 0.3f,
				status.left + colW * 0.4f, status.bottom), icon * 0.72f,
				kLedText, ALIGN_LEFT, &fFallback);
			draw_walker(v, iconRect, kLedText);
		} else {
			if (delay > 0) {
				int64 expected = slots[c].departs + delay;
				snprintf(buffer, sizeof(buffer), "Exp %02d:%02d",
					(int)(expected % 1440 / 60), (int)(expected % 60));
				_DrawIn(v, buffer, BRect(status.left, status.top,
					iconRect.left - 4, status.top + icon * 0.45f), icon * 0.36f,
					kYellow, ALIGN_LEFT, &fFallback);
			}
			_DrawIn(v, "Wait", BRect(status.left, status.top + icon * 0.35f,
				iconRect.left - 4, status.bottom), icon * 0.55f, kLedText,
				ALIGN_LEFT, &fFallback);
			draw_hand(v, iconRect.InsetByCopy(icon * 0.08f, icon * 0.05f),
				kLedText);
		}

		// Calling points, a page at a time.
		float bodyTop = head.bottom + floorf(lineH * 0.4f);
		float stripTop = H - stripH - floorf(lineH * 0.3f);
		_DrawIn(v, cancelled ? "Was calling at:" : "Calling at:",
			BRect(x0 + pad, bodyTop, x0 + colW * 0.62f, bodyTop + lineH - 1),
			lineH * 0.82f, cancelled ? make_color(255, 150, 40) : kYellow,
			ALIGN_LEFT, &fFallback);
		float listTop = bodyTop + lineH * 1.15f;
		int perPage = (int)((stripTop - listTop) / lineH);
		if (perPage < 1)
			perPage = 1;
		int pages = (service.callCount + perPage - 1) / perPage;
		int shownPage = pages > 1 ? page % pages : 0;
		if (pages > 1) {
			snprintf(buffer, sizeof(buffer), "%d of %d", shownPage + 1, pages);
			BRect box(x0 + colW * 0.64f, bodyTop + 1, x0 + colW - pad,
				bodyTop + lineH - 2);
			v->SetHighColor(200, 200, 200);
			v->StrokeRect(box);
			_DrawIn(v, buffer, box.InsetByCopy(2, 1), lineH * 0.7f, kLedText,
				ALIGN_CENTER, &fFallback);
		}
		BFont regular(fFallback);
		regular.SetFace(B_REGULAR_FACE);
		regular.SetSize(lineH * 0.78f);
		// Names give way to their times, with a gap.
		float nameRight = x0 + colW - pad - regular.StringWidth("00:00")
			- lineH * 0.4f;
		for (int i = 0; i < perPage; i++) {
			int k = shownPage * perPage + i;
			if (k >= service.callCount)
				break;
			float y = listTop + i * lineH;
			_DrawIn(v, service.calls[k].name, BRect(x0 + pad, y,
				nameRight, y + lineH - 1), lineH * 0.78f, kLedText,
				ALIGN_LEFT, &regular);
			int64 at = slots[c].departs + delay + service.calls[k].minutes;
			snprintf(buffer, sizeof(buffer), "%02d:%02d", (int)(at % 1440 / 60),
				(int)(at % 60));
			_DrawIn(v, buffer, BRect(nameRight, y, x0 + colW - pad,
				y + lineH - 1), lineH * 0.78f, kLedText, ALIGN_RIGHT, &regular);
		}

		// How long the train is: a carriage per car, and the count.
		float carW = floorf((colW * 0.7f) / service.cars);
		float carH = floorf(stripH * 0.38f);
		float cy = stripTop + floorf((stripH - carH) / 2);
		v->SetHighColor(200, 200, 200);
		for (int k = 0; k < service.cars; k++) {
			BRect car(x0 + pad + k * carW, cy, x0 + pad + (k + 1) * carW - 2,
				cy + carH);
			v->StrokeRoundRect(car, 2, 2);
		}
		snprintf(buffer, sizeof(buffer), "%d", service.cars);
		_DrawIn(v, buffer, BRect(x0 + pad + service.cars * carW + 4, stripTop,
			x0 + colW - pad, stripTop + stripH - 1), stripH * 0.5f, kLedText,
			ALIGN_LEFT, &fFallback);

		v->SetHighColor(0, 0, 0);
		v->FillRect(BRect(x0 + colW - 2, top, x0 + colW - 1, H - 1));
	}
	v->Sync();
	fBoard->Unlock();
}


// #pragma mark - Sydney Central


static void
format_wait(char* out, int size, int minutes)
{
	if (minutes <= 0)
		snprintf(out, size, "Now");
	else if (minutes < 60)
		snprintf(out, size, "%d min", minutes);
	else
		snprintf(out, size, "%d hr %dm", minutes / 60, minutes % 60);
}


// A dark pill with white text: 8 cars, All Stops, Booked seats only.
static float
draw_pill(BView* v, const char* text, BPoint at, float height,
	const BFont& base)
{
	BFont font(base);
	font.SetSize(height * 0.72f);
	float w = font.StringWidth(text) + height * 0.8f;
	BRect r(at.x, at.y, at.x + w, at.y + height);
	v->SetHighColor(50, 52, 56);
	v->FillRoundRect(r, 2, 2);
	font_height fh;
	font.GetHeight(&fh);
	v->SetFont(&font);
	v->SetHighColor(255, 255, 255);
	v->SetDrawingMode(B_OP_OVER);
	v->DrawString(text, BPoint(at.x + height * 0.4f,
		floorf(at.y + (height + fh.ascent - fh.descent) / 2)));
	v->SetDrawingMode(B_OP_COPY);
	return w;
}


// Words laid into lines no wider than `width`; '\n' breaks a line.
static void
draw_wrapped(BView* v, const char* text, BRect r, const BFont& font,
	rgb_color color)
{
	font_height fh;
	font.GetHeight(&fh);
	float lineH = ceilf(fh.ascent + fh.descent + fh.leading) * 1.08f;
	v->SetFont(&font);
	v->SetHighColor(color);
	v->SetDrawingMode(B_OP_OVER);
	float y = r.top + fh.ascent;
	BString line;
	const char* s = text;
	while (*s != '\0' && y < r.bottom) {
		const char* end = s;
		while (*end != '\0' && *end != ' ' && *end != '\n')
			end++;
		BString word(s, end - s);
		BString candidate(line);
		if (candidate.Length() > 0)
			candidate << " ";
		candidate << word;
		if (line.Length() > 0 && font.StringWidth(candidate.String())
				> r.Width()) {
			v->DrawString(line.String(), BPoint(r.left, y));
			y += lineH;
			line = word;
		} else
			line = candidate;
		if (*end == '\n') {
			v->DrawString(line.String(), BPoint(r.left, y));
			y += lineH;
			line = "";
		}
		s = *end == '\0' ? end : end + 1;
	}
	if (line.Length() > 0 && y < r.bottom)
		v->DrawString(line.String(), BPoint(r.left, y));
	v->SetDrawingMode(B_OP_COPY);
}


void
BoardView::_DrawSydney(int64 now)
{
	static const rgb_color kInk = { 28, 28, 30, 255 };
	static const rgb_color kGreyInk = { 110, 110, 112, 255 };
	static const rgb_color kOrange = { 233, 104, 45, 255 };
	static const rgb_color kTeal = { 22, 150, 160, 255 };
	static const rgb_color kPaper = { 244, 244, 242, 255 };

	BView* v = fBoardView;
	fBoard->Lock();
	BRect bounds = v->Bounds();
	float W = bounds.Width() + 1;
	float H = bounds.Height() + 1;
	v->SetHighColor(52, 54, 58);
	v->FillRect(bounds);
	fMarqueeCount = 0;
	fMarqueeStart = system_time();

	BFont bold(fFallback);
	BFont regular(fFallback);
	regular.SetFace(B_REGULAR_FACE);

	// "Next departures", the information sign, and the clock in yellow.
	float headH = floorf(H * 0.1f);
	float m = floorf(W * 0.01f);
	BRect info(m * 2, floorf(headH * 0.14f), m * 2 + floorf(headH * 0.72f),
		floorf(headH * 0.86f));
	v->SetHighColor(96, 98, 104);
	v->FillRoundRect(info, 4, 4);
	_DrawIn(v, "i", info, info.Height() * 0.9f, kLedText, ALIGN_CENTER,
		&fFallback);
	_DrawIn(v, "Next departures", BRect(info.right + m * 1.5f, 0, W * 0.6f,
		headH), headH * 0.62f, kLedText, ALIGN_LEFT, &fFallback);
	char buffer[64];
	snprintf(buffer, sizeof(buffer), "%d:%02d", (int)(now % 1440 / 60),
		(int)(now % 60));
	_DrawIn(v, buffer, BRect(fGearRect.left - W * 0.15f, 0,
		fGearRect.left - m, headH), headH * 0.66f, make_color(255, 214, 0),
		ALIGN_RIGHT, &fFallback);
	draw_gear(v, fGearRect, make_color(190, 190, 190), make_color(52, 54, 58));

	// The tabs over the columns. A screen wider than 16:9 has one row of
	// taller screens, and Sydney services loses its fifth column.
	bool wide = W * 9 > H * 16;
	int columns = wide ? 7 : 8;
	int rows = wide ? 1 : 2;
	int sydney = wide ? 4 : 5;
	float colW = floorf((W - m) / columns);
	float tabTop = headH;
	float tabH = floorf(H * 0.055f);
	struct { int from, to; const char* name; rgb_color color; } tabs[] = {
		{ 0, sydney - 1, "Sydney services", kOrange },
		{ sydney, sydney, "Intercity & Events", kOrange },
		{ sydney + 1, sydney + 1, "Regional services", kOrange },
		{ sydney + 2, sydney + 2, "Metro", kTeal },
	};
	for (int t = 0; t < 4; t++) {
		BRect tab(m + tabs[t].from * colW, tabTop, m + (tabs[t].to + 1) * colW
			- 4, tabTop + tabH - 1);
		v->SetHighColor(tabs[t].color);
		v->FillRect(tab);
		_DrawIn(v, tabs[t].name, tab.InsetByCopy(m, 0), tabH * 0.62f,
			kLedText, ALIGN_LEFT, &fFallback);
	}

	// Rows of portrait screens, eight to a row in kSydneyPanels.
	float gridTop = tabTop + tabH + floorf(m * 0.8f);
	float panelH = floorf((H - gridTop - m) / rows) - floorf(m * 0.6f);
	for (int i = 0; i < kSydneyPanelCount; i++) {
		const SydneyPanel& panel = kSydneyPanels[i];
		int col = i % 8;
		int row = i / 8;
		if (row >= rows || (col == 4 && sydney == 4))
			continue;
		if (col > 4 && sydney == 4)
			col--;
		BRect r(m + col * colW, gridTop + row * (panelH + m * 1.2f),
			m + (col + 1) * colW - 5, gridTop + row * (panelH + m * 1.2f)
				+ panelH);
		v->SetHighColor(kPaper);
		v->FillRect(r);
		float p = floorf(r.Width() * 0.06f);
		// One line of stations; a tall screen is no wider for it.
		float tall = panelH < r.Width() * 1.6f ? panelH : r.Width() * 1.6f;
		float unit = floorf(tall * 0.052f);
		rgb_color color = { panel.color[0], panel.color[1], panel.color[2],
			255 };

		if (panel.kind == PANEL_NOTICE) {
			BFont font(bold);
			font.SetSize(unit * 0.95f);
			draw_wrapped(v, panel.notice, r.InsetByCopy(p, p), font, kInk);
			continue;
		}

		Slot slots[8];
		int count = build_slots(panel.runs, sizeof(SydneyRun), panel.runCount,
			now, now, slots, panel.kind == PANEL_CITY ? 7 : wide ? 6 : 3);
		if (count == 0)
			continue;
		const SydneyRun& run = panel.runs[slots[0].service];
		int wait = (int)(slots[0].departs - now);

		// The head of the screen.
		float y = r.top + p;
		if (panel.kind == PANEL_CITY) {
			BRect band(r.left + p * 0.6f, y, r.right - p * 0.6f,
				y + unit * 1.5f);
			v->SetHighColor(color);
			v->FillRoundRect(band, 3, 3);
			BFont font(bold);
			font.SetSize(unit * 1.15f);
			v->SetFont(&font);
			v->SetHighColor(255, 255, 255);
			v->SetDrawingMode(B_OP_OVER);
			font_height fh;
			font.GetHeight(&fh);
			float base = floorf(band.top + (band.Height() + fh.ascent
				- fh.descent) / 2);
			v->DrawString("City", BPoint(band.left + p * 0.6f, base));
			BFont small(regular);
			small.SetSize(unit * 0.8f);
			v->SetFont(&small);
			v->DrawString(run.via, BPoint(band.left + p * 0.8f
				+ font.StringWidth("City"), base));
			v->SetDrawingMode(B_OP_COPY);
			y = band.bottom + unit * 0.6f;
		} else if (panel.kind == PANEL_LONG) {
			float d = unit * 1.3f;
			BPoint c(r.left + p + d / 2, y + d / 2);
			v->SetHighColor(color);
			v->FillEllipse(c, d / 2, d / 2);
			_DrawIn(v, "T", BRect(c.x - d / 2, c.y - d / 2, c.x + d / 2,
				c.y + d / 2), d * 0.75f, kLedText, ALIGN_CENTER, &bold);
			snprintf(buffer, sizeof(buffer), "%02d:%02d",
				(int)(slots[0].departs % 1440 / 60),
				(int)(slots[0].departs % 60));
			_DrawIn(v, buffer, BRect(c.x + d * 0.7f, y - unit * 0.1f, r.right - p,
				y + d), unit * 1.3f, kInk, ALIGN_LEFT, &bold);
			y += d + unit * 0.15f;
			_DrawIn(v, run.dest, BRect(r.left + p, y, r.right - p,
				y + unit * 1.3f), unit * 1.15f, kInk, ALIGN_LEFT, &bold);
			y += unit * 1.3f;
			if (run.via[0] != '\0') {
				_DrawIn(v, run.via, BRect(r.left + p, y, r.right - p,
					y + unit * 0.8f), unit * 0.7f, kInk, ALIGN_LEFT, &regular);
				y += unit * 0.85f;
			}
			if (run.booked) {
				draw_pill(v, "Booked seats only", BPoint(r.left + p, y + 2),
					unit * 0.85f, bold);
				y += unit;
			}
			y += unit * 0.4f;
		} else {
			float side = unit * 1.45f;
			BRect badge(r.left + p * 0.6f, y, r.left + p * 0.6f + side,
				y + side);
			v->SetHighColor(color);
			v->FillRect(badge);
			_DrawIn(v, panel.line, badge.InsetByCopy(2, 0), side * 0.66f,
				kLedText, ALIGN_CENTER, &bold);
			float tx = badge.right + p * 0.5f;
			_DrawIn(v, run.dest, BRect(tx, y - unit * 0.1f, r.right - p * 0.5f,
				y + unit * 0.95f), unit * 1.05f, kInk, ALIGN_LEFT, &bold);
			if (run.via[0] != '\0') {
				_DrawIn(v, run.via, BRect(tx, y + unit * 0.9f,
					r.right - p * 0.5f, y + side), unit * 0.62f, kInk,
					ALIGN_LEFT, &regular);
			}
			y += side + unit * 0.6f;
		}

		// The stations, with the platform beside the first of them.
		float stopsTop = y;
		float stopsRight = r.right - p - unit * 1.8f;
		BFont stopFont(regular);
		const char* s = run.stops;
		int lines = 0;
		float limit = r.top + panelH * (panel.kind == PANEL_CITY ? 0.5f
			: 0.66f);
		while (*s != '\0' && y + unit <= limit) {
			const char* end = strchr(s, '|');
			if (end == NULL)
				end = s + strlen(s);
			BString name(s, end - s);
			if (name.Length() > 0) {
				_DrawIn(v, name.String(), BRect(r.left + p, y, stopsRight,
					y + unit - 1), unit * 0.95f, kInk, ALIGN_LEFT, &stopFont);
			}
			y += unit;
			lines++;
			s = *end == '\0' ? end : end + 1;
		}
		// More stations than lines: they scroll up, redrawn on every tick
		// by _DrawMarquees().
		if (*s != '\0' && lines > 0 && fMarqueeCount < 16) {
			int total = 1;
			for (const char* c = run.stops; *c != '\0'; c++)
				total += *c == '|';
			Marquee& marquee = fMarquees[fMarqueeCount++];
			marquee.vertical = true;
			marquee.area.Set(r.left + p, stopsTop, stopsRight - 1,
				stopsTop + lines * unit - 1);
			marquee.text = run.stops;
			marquee.background = kPaper;
			marquee.lineH = unit;
			marquee.width = total * unit;
		}
		_DrawIn(v, "Platform", BRect(stopsRight, stopsTop, r.right - p,
			stopsTop + unit * 0.6f), unit * 0.5f, kInk, ALIGN_RIGHT, &regular);
		snprintf(buffer, sizeof(buffer), "%d", run.platform);
		_DrawIn(v, buffer, BRect(stopsRight - unit, stopsTop + unit * 0.5f,
			r.right - p, stopsTop + unit * 2.1f), unit * 1.6f, kInk,
			ALIGN_RIGHT, &regular);

		// Cars, the stopping pattern and how long to wait.
		float footTop = r.top + panelH * (panel.kind == PANEL_CITY ? 0.55f
			: 0.7f);
		float pillH = floorf(unit * 0.62f);
		snprintf(buffer, sizeof(buffer), "%d cars", run.cars);
		float pw = draw_pill(v, buffer, BPoint(r.left + p,
			footTop + unit * 0.7f), pillH, bold);
		if (panel.kind != PANEL_CITY) {
			draw_pill(v, run.pattern, BPoint(r.left + p + pw + 4,
				footTop + unit * 0.7f), pillH, bold);
		}
		if (panel.kind != PANEL_CITY) {
			_DrawIn(v, "Departs", BRect(r.right - p - unit * 4, footTop - unit * 0.1f,
				r.right - p, footTop + unit * 0.5f), unit * 0.48f, kInk,
				ALIGN_RIGHT, &regular);
			format_wait(buffer, sizeof(buffer), wait);
			_DrawIn(v, buffer, BRect(r.right - p - unit * 4.5f,
				footTop + unit * 0.35f, r.right - p, footTop + unit * 1.45f),
				unit * 1.05f, kInk, ALIGN_RIGHT, &bold);
		}

		// Following services.
		float listTop = footTop + unit * 1.7f;
		v->SetHighColor(190, 190, 190);
		v->StrokeLine(BPoint(r.left + p * 0.5f, listTop),
			BPoint(r.right - p * 0.5f, listTop));
		listTop += 3;
		float platX = r.left + r.Width() * 0.62f;
		_DrawIn(v, "Following Services", BRect(r.left + p, listTop,
			platX - 4, listTop + unit * 0.55f), unit * 0.45f, kGreyInk,
			ALIGN_LEFT, &regular);
		_DrawIn(v, "Platform", BRect(platX - unit, listTop,
			platX + unit * 0.8f, listTop + unit * 0.55f), unit * 0.45f,
			kGreyInk, ALIGN_CENTER, &regular);
		_DrawIn(v, "Departs", BRect(platX + unit, listTop, r.right - p,
			listTop + unit * 0.55f), unit * 0.45f, kGreyInk, ALIGN_RIGHT,
			&regular);
		float ry = listTop + unit * 0.65f;
		float rowH = panel.kind == PANEL_CITY ? unit * 0.95f : unit * 1.3f;
		for (int k = 1; k < count && ry + rowH * 0.8f <= r.bottom; k++) {
			const SydneyRun& next = panel.runs[slots[k].service];
			const char* label = panel.kind == PANEL_CITY ? next.via : next.dest;
			_DrawIn(v, label, BRect(r.left + p, ry, platX - unit, ry
				+ unit * 0.8f), unit * 0.72f, kInk, ALIGN_LEFT, &regular);
			snprintf(buffer, sizeof(buffer), "%d", next.platform);
			_DrawIn(v, buffer, BRect(platX - unit, ry, platX + unit * 0.8f,
				ry + unit * 0.8f), unit * 0.72f, kInk, ALIGN_CENTER, &regular);
			format_wait(buffer, sizeof(buffer), (int)(slots[k].departs - now));
			_DrawIn(v, buffer, BRect(platX + unit, ry, r.right - p,
				ry + unit * 0.8f), unit * 0.72f, kInk, ALIGN_RIGHT, &regular);
			if (panel.kind != PANEL_CITY && next.via[0] != '\0') {
				_DrawIn(v, next.via, BRect(r.left + p, ry + unit * 0.72f,
					platX - unit, ry + unit * 1.2f), unit * 0.42f, kGreyInk,
					ALIGN_LEFT, &regular);
			}
			ry += rowH;
		}
		(void)lines;
	}
	v->Sync();
	fBoard->Unlock();
}



// The stopping stations of Seoul's trains. A list longer than its strip
// travels left until its end shows, rests, and travels back; one that fits
// stays put. Sydney's go up instead (_DrawStopsScroll). Only these strips
// are redrawn.
void
BoardView::_DrawMarquees(bigtime_t now)
{
	if (fBoard == NULL || fMarqueeCount == 0)
		return;
	const float kSpeed = 70;	// pixels a second
	BView* v = fBoardView;
	fBoard->Lock();
	BRegion dirty;
	for (int i = 0; i < fMarqueeCount; i++) {
		Marquee& marquee = fMarquees[i];
		BRect area = marquee.area;
		if (marquee.vertical) {
			_DrawStopsScroll(marquee, now);
			dirty.Include(area);
			continue;
		}
		float overflow = marquee.width - area.Width();
		float offset = 0;
		if (overflow > 0) {
			// Out, rest, back, rest.
			const float kRest = 1.5f;
			float travel = overflow / kSpeed;
			float cycle = 2 * (travel + kRest);
			float t = fmodf((float)((now - fMarqueeStart) / 1000) / 1000,
				cycle);
			if (t < kRest)
				offset = 0;
			else if (t < kRest + travel)
				offset = (t - kRest) * kSpeed;
			else if (t < 2 * kRest + travel)
				offset = overflow;
			else
				offset = overflow - (t - 2 * kRest - travel) * kSpeed;
		}
		BRegion clip(area);
		v->ConstrainClippingRegion(&clip);
		v->SetHighColor(marquee.background);
		v->FillRect(area);
		BFont font(font_for(marquee.text.String(), fFallback, fFallback,
			fCJK));
		font.SetFace(B_REGULAR_FACE);
		font.SetSize(area.Height() * 0.53f);
		font_height fh;
		font.GetHeight(&fh);
		float y = floorf(area.top + (area.Height() + 1 + fh.ascent
			- fh.descent) / 2);
		v->SetFont(&font);
		v->SetHighColor(170, 190, 215);
		v->SetDrawingMode(B_OP_OVER);
		v->DrawString(marquee.text.String(), BPoint(area.left - offset, y));
		v->SetDrawingMode(B_OP_COPY);
		v->ConstrainClippingRegion(NULL);
		dirty.Include(area);
	}
	v->Sync();
	fBoard->Unlock();
	if (Window() != NULL)
		Invalidate(&dirty);
}


// A Sydney screen's stations, more than it has lines for: they rest, go up
// until the last of them sits on the bottom line, rest there, and start
// again from the top. Called by _DrawMarquees() with the board locked.
void
BoardView::_DrawStopsScroll(const Marquee& marquee, bigtime_t now)
{
	static const rgb_color kInk = { 28, 28, 30, 255 };
	const float kRest = 2.0f;
	float speed = marquee.lineH * 1.5f;	// pixels a second
	BRect area = marquee.area;
	float overflow = marquee.width - (area.Height() + 1);
	float travel = overflow / speed;
	float t = fmodf((float)((now - fMarqueeStart) / 1000) / 1000,
		travel + 2 * kRest);
	float offset = 0;
	if (t >= kRest + travel)
		offset = overflow;
	else if (t >= kRest)
		offset = floorf((t - kRest) * speed);

	BView* v = fBoardView;
	BRegion clip(area);
	v->ConstrainClippingRegion(&clip);
	v->SetHighColor(marquee.background);
	v->FillRect(area);
	BFont font(fFallback);
	font.SetFace(B_REGULAR_FACE);
	float y = area.top - offset;
	const char* s = marquee.text.String();
	while (*s != '\0' && y < area.bottom) {
		const char* end = strchr(s, '|');
		if (end == NULL)
			end = s + strlen(s);
		if (y + marquee.lineH > area.top && end > s) {
			BString name(s, end - s);
			_DrawIn(v, name.String(), BRect(area.left, y, area.right,
				y + marquee.lineH - 1), marquee.lineH * 0.95f, kInk,
				ALIGN_LEFT, &font);
		}
		y += marquee.lineH;
		s = *end == '\0' ? end : end + 1;
	}
	v->ConstrainClippingRegion(NULL);
}
