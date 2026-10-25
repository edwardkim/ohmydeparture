/*
 * The Tokyo Station boards' data. Patterns and calling points are those of
 * the real services, simplified; numbers count up by two (down trains are
 * odd) through the day.
 */

#include "Station.h"

#include <string.h>


const Station kStations[] = {
	{ "TOKYO", "Tokyo Station  東京駅 (JR)", STYLE_JR, 9 * 60, DST_NONE },
	{ "SEOUL", "Seoul Station  서울역 (Korail)", STYLE_KORAIL, 9 * 60, DST_NONE },
	{ "VICTORIA", "London Victoria", STYLE_UK, 0, DST_EU },
	{ "SYDNEY", "Sydney Central", STYLE_SYDNEY, 10 * 60, DST_AU },
};
const int kStationCount = sizeof(kStations) / sizeof(kStations[0]);

#define C(r, g, b) { r, g, b }



// #pragma mark - Tokaido Shinkansen


// 品川 新横浜 小田原 熱海 静岡 浜松 名古屋 京都 新大阪
static const Service kTokaido[] = {
	{ "のぞみ", "Nozomi", C(230, 120, 0), "博多", "Hakata", 1, 30, 0,
		360, 1200, 14, 19, false, "oo----ooo", NULL, "1-3", false },
	{ "のぞみ", "Nozomi", C(230, 120, 0), "新大阪", "Shin-Osaka", 201, 30, 9,
		360, 1290, 14, 19, false, "oo----ooo", NULL, "1-3", false },
	{ "のぞみ", "Nozomi", C(230, 120, 0), "広島", "Hiroshima", 79, 60, 21,
		420, 1140, 14, 19, false, "oo----ooo", NULL, "1-3", false },
	{ "ひかり", "Hikari", C(210, 30, 40), "新大阪", "Shin-Osaka", 631, 60, 3,
		390, 1260, 14, 19, false, "oooo--ooo", NULL, "1-5", false },
	{ "ひかり", "Hikari", C(210, 30, 40), "岡山", "Okayama", 501, 120, 33,
		390, 1140, 14, 19, false, "oo-o-oooo", NULL, "1-5", false },
	{ "こだま", "Kodama", C(30, 80, 200), "名古屋", "Nagoya", 701, 60, 27,
		360, 1320, 14, 19, false, "ooooooo  ", NULL, "1-7", false },
	{ "こだま", "Kodama", C(30, 80, 200), "新大阪", "Shin-Osaka", 731, 120, 57,
		360, 1260, 14, 19, false, "ooooooooo", NULL, "1-7", false },
};


// #pragma mark - Shinkansen to the north


static const Service kNorth[] = {
	{ "はやぶさ", "Hayabusa", C(0, 150, 80), "新函館北斗", "Shin-Hakodate-Hokuto",
		1, 120, 32, 360, 1140, 20, 23, false, "大宮・仙台・盛岡・新青森",
		"Omiya, Sendai, Morioka, Shin-Aomori", NULL, false },
	{ "はやぶさ", "Hayabusa", C(0, 150, 80), "新青森", "Shin-Aomori",
		11, 60, 20, 360, 1200, 22, 22, false, "大宮・仙台・盛岡・八戸",
		"Omiya, Sendai, Morioka, Hachinohe", NULL, false },
	{ "こまち", "Komachi", C(220, 30, 60), "秋田", "Akita",
		11, 60, 20, 360, 1200, 22, 22, false, "大宮・仙台・盛岡・角館",
		"Omiya, Sendai, Morioka, Kakunodate", NULL, false },
	{ "やまびこ", "Yamabiko", C(60, 170, 100), "仙台", "Sendai",
		51, 60, 40, 360, 1320, 20, 23, false, "大宮・宇都宮・郡山・福島",
		"Omiya, Utsunomiya, Koriyama, Fukushima", "1-5", false },
	{ "なすの", "Nasuno", C(120, 190, 80), "郡山", "Koriyama",
		251, 120, 52, 420, 1320, 20, 23, false, "上野・大宮・宇都宮・那須塩原",
		"Ueno, Omiya, Utsunomiya, Nasushiobara", "1-7", false },
	{ "つばさ", "Tsubasa", C(150, 60, 180), "山形", "Yamagata",
		121, 60, 0, 360, 1260, 20, 23, false, "大宮・福島・米沢",
		"Omiya, Fukushima, Yonezawa", NULL, false },
	{ "とき", "Toki", C(220, 70, 130), "新潟", "Niigata",
		301, 60, 12, 360, 1320, 20, 23, false, "大宮・高崎・越後湯沢・長岡",
		"Omiya, Takasaki, Echigo-Yuzawa, Nagaoka", "1-5", false },
	{ "たにがわ", "Tanigawa", C(235, 130, 170), "越後湯沢", "Echigo-Yuzawa",
		401, 120, 48, 420, 1260, 20, 23, false, "上野・大宮・高崎",
		"Ueno, Omiya, Takasaki", "1-7", false },
	{ "かがやき", "Kagayaki", C(40, 80, 190), "敦賀", "Tsuruga",
		501, 120, 24, 384, 1140, 20, 23, false, "大宮・長野・富山・金沢",
		"Omiya, Nagano, Toyama, Kanazawa", NULL, false },
	{ "はくたか", "Hakutaka", C(70, 130, 210), "金沢", "Kanazawa",
		551, 60, 36, 396, 1236, 20, 23, false, "大宮・高崎・長野・富山",
		"Omiya, Takasaki, Nagano, Toyama", "1-4", false },
	{ "あさま", "Asama", C(180, 110, 40), "長野", "Nagano",
		601, 60, 52, 412, 1332, 20, 23, false, "大宮・高崎・軽井沢",
		"Omiya, Takasaki, Karuizawa", "1-4", false },
};


// #pragma mark - Tokaido, Ueno-Tokyo and Yokosuka lines


// 新橋 品川 川崎 横浜 戸塚 大船 藤沢 茅ヶ崎 平塚 小田原
static const Service kTokaidoLine[] = {
	{ "東海道線", "Tokaido Line", C(240, 120, 30), "熱海", "Atami", 0, 30, 4,
		300, 1410, 7, 10, false, "oooooooooo", NULL, NULL, true },
	{ "上野東京ライン", "Ueno-Tokyo Line", C(240, 120, 30), "小田原", "Odawara",
		0, 30, 19, 300, 1410, 7, 10, false, "oooooooooo", NULL, NULL, false },
	{ "東海道線", "Tokaido Line", C(240, 120, 30), "平塚", "Hiratsuka", 0, 60, 47,
		360, 1440, 7, 10, false, "ooooooooo ", NULL, NULL, true },
	{ "上野東京ライン", "Ueno-Tokyo Line", C(240, 120, 30), "沼津", "Numazu",
		0, 120, 34, 360, 1320, 7, 10, false, "oooooooooo", NULL, NULL, false },
	{ "横須賀線", "Yokosuka Line", C(30, 70, 190), "逗子", "Zushi", 0, 20, 7,
		300, 1440, 1, 2, true, "oo-ooo    ", NULL, NULL, true },
	{ "横須賀線", "Yokosuka Line", C(30, 70, 190), "久里浜", "Kurihama", 0, 30, 15,
		300, 1410, 1, 2, true, "oo-ooo    ", NULL, NULL, false },
	{ "横須賀線", "Yokosuka Line", C(30, 70, 190), "大船", "Ofuna", 0, 60, 52,
		360, 1440, 1, 2, true, "oo-ooo    ", NULL, NULL, true },
	{ "踊り子", "Odoriko", C(0, 140, 90), "伊豆急下田", "Izukyu-Shimoda",
		1, 120, 0, 540, 1080, 9, 10, false, "-ooo-o---o", NULL, NULL, true },
};


// #pragma mark - Chuo Line


// 神田 御茶ノ水 四ツ谷 新宿 中野 高円寺 吉祥寺 三鷹 国分寺 立川
static const Service kChuo[] = {
	{ "快速", "Rapid", C(240, 80, 30), "高尾", "Takao", 0, 10, 2,
		270, 1440, 1, 2, false, "oooooooooo", NULL, NULL, true },
	{ "快速", "Rapid", C(240, 80, 30), "豊田", "Toyoda", 0, 30, 7,
		300, 1410, 1, 2, false, "oooooooooo", NULL, NULL, true },
	{ "中央特快", "Chuo Special Rapid", C(40, 90, 200), "高尾", "Takao", 0, 20, 15,
		360, 1320, 1, 2, false, "ooooo--ooo", NULL, NULL, true },
	{ "青梅特快", "Ome Special Rapid", C(0, 150, 70), "青梅", "Ome", 0, 60, 45,
		420, 1260, 1, 2, false, "ooooo--ooo", NULL, NULL, true },
	{ "あずさ", "Azusa", C(130, 60, 180), "松本", "Matsumoto", 1, 60, 0,
		420, 1200, 1, 2, false, "---o-----o", NULL, NULL, true },
	{ "かいじ", "Kaiji", C(70, 80, 170), "甲府", "Kofu", 1, 120, 30,
		450, 1230, 1, 2, false, "---o-----o", NULL, NULL, true },
};


#define SERVICES(s) s, (int)(sizeof(s) / sizeof(s[0]))

const StationBoard kStationBoards[] = {
	{ "東海道新幹線", "Tokaido Shinkansen",
		"名古屋・京都・新大阪方面", "for Nagoya, Kyoto & Shin-Osaka", true,
		{ "品川", "新横浜", "小田原", "熱海", "静岡", "浜松", "名古屋", "京都",
			"新大阪" }, 9, SERVICES(kTokaido) },
	{ "東北・山形・秋田・上越・北陸新幹線",
		"Tohoku, Yamagata, Akita, Joetsu & Hokuriku Shinkansen",
		"仙台・新潟・金沢方面", "for Sendai, Niigata & Kanazawa", true,
		{ NULL }, 0, SERVICES(kNorth) },
	{ "東海道線・上野東京ライン・横須賀線", "Tokaido, Ueno-Tokyo & Yokosuka Lines",
		"品川・横浜・小田原方面", "for Shinagawa, Yokohama & Odawara", false,
		{ "新橋", "品川", "川崎", "横浜", "戸塚", "大船", "藤沢", "茅ヶ崎", "平塚",
			"小田原" }, 10, SERVICES(kTokaidoLine) },
	{ "中央線", "Chuo Line",
		"新宿・立川・高尾方面", "for Shinjuku, Tachikawa & Takao", false,
		{ "神田", "御茶ノ水", "四ツ谷", "新宿", "中野", "高円寺", "吉祥寺", "三鷹",
			"国分寺", "立川" }, 10, SERVICES(kChuo) },
};
const int kStationBoardCount = sizeof(kStationBoards) / sizeof(kStationBoards[0]);


static uint32
mix(uint32 h)
{
	h ^= h >> 16;
	h *= 0x7feb352d;
	h ^= h >> 15;
	h *= 0x846ca68b;
	h ^= h >> 16;
	return h;
}


int
build_trains(const StationBoard& board, int boardIndex, int64 now, Train* out,
	int max)
{
	int count = 0;
	int64 today = now / 1440;
	for (int64 day = today - 1; day <= today + 1; day++) {
		for (int s = 0; s < board.serviceCount; s++) {
			const Service& service = board.services[s];
			int k = 0;
			for (int t = service.first + service.offset; t <= service.last;
					t += service.interval, k++) {
				int64 departs = day * 1440 + t;
				if (departs < now)
					continue;
				Train train;
				train.service = &service;
				train.number = service.number > 0 ? service.number + 2 * k : 0;
				train.departs = departs;
				uint32 h = mix((uint32)(departs * 31 + s * 7 + boardIndex));
				train.track = service.trackMin
					+ (int)(h % (service.trackMax - service.trackMin + 1));
				train.startsHere = service.mayStartHere && (h >> 8) % 3 == 0;
				// Keep the earliest `max`.
				if (count < max) {
					out[count++] = train;
				} else {
					int latest = 0;
					for (int i = 1; i < count; i++) {
						if (out[i].departs > out[latest].departs)
							latest = i;
					}
					if (departs < out[latest].departs)
						out[latest] = train;
				}
			}
		}
	}
	for (int i = 1; i < count; i++) {
		Train t = out[i];
		int j = i - 1;
		while (j >= 0 && out[j].departs > t.departs) {
			out[j + 1] = out[j];
			j--;
		}
		out[j + 1] = t;
	}
	return count;
}


// #pragma mark - timetables


int
build_slots(const void* services, int stride, int count, int64 now,
	int64 since, Slot* out, int max)
{
	int shown = 0;
	int64 today = now / 1440;
	for (int64 day = today - 1; day <= today + 1; day++) {
		for (int s = 0; s < count; s++) {
			const Timing& t = *(const Timing*)((const char*)services
				+ s * stride);
			int k = 0;
			for (int m = t.first + t.offset; m <= t.last; m += t.interval, k++) {
				int64 departs = day * 1440 + m;
				if (departs < since)
					continue;
				Slot slot;
				slot.service = s;
				slot.index = k;
				slot.departs = departs;
				if (shown < max) {
					out[shown++] = slot;
					continue;
				}
				int latest = 0;
				for (int i = 1; i < shown; i++) {
					if (out[i].departs > out[latest].departs)
						latest = i;
				}
				if (departs < out[latest].departs)
					out[latest] = slot;
			}
		}
	}
	for (int i = 1; i < shown; i++) {
		Slot slot = out[i];
		int j = i - 1;
		while (j >= 0 && (out[j].departs > slot.departs
				|| (out[j].departs == slot.departs
					&& out[j].service > slot.service))) {
			out[j + 1] = out[j];
			j--;
		}
		out[j + 1] = slot;
	}
	return shown;
}


// #pragma mark - Seoul


#define T(interval, offset, first, last) { interval, offset, first, last }

const KorailService kKorail[] = {
	{ T(30, 0, 310, 1320), { "KTX", "KTX", "KTX", "KTX" }, C(0, 84, 166),
		{ "부산", "Busan", "釜山", "釜山" }, 1, 9, 14,
		"광명|천안아산|오송|대전|김천구미|동대구|신경주|울산|부산" },
	{ T(120, 50, 330, 1260), { "KTX-청룡", "KTX-Cheongryong", "KTX青龍",
		"KTX青龙" }, C(20, 50, 120), { "부산", "Busan", "釜山", "釜山" }, 101,
		9, 14,
		"광명|대전|동대구|부산" },
	{ T(120, 15, 330, 1200), { "KTX-산천", "KTX-Sancheon", "KTX山川",
		"KTX山川" }, C(0, 130, 140), { "마산", "Masan", "馬山", "马山" }, 201,
		7, 12,
		"광명|천안아산|오송|대전|동대구|밀양|진영|창원중앙|창원|마산" },
	{ T(180, 75, 330, 1200), { "KTX-산천", "KTX-Sancheon", "KTX山川",
		"KTX山川" }, C(0, 130, 140), { "진주", "Jinju", "晋州", "晋州" }, 281,
		7, 12,
		"광명|대전|동대구|밀양|창원중앙|창원|마산|진주" },
	{ T(120, 45, 345, 1215), { "KTX", "KTX", "KTX", "KTX" }, C(0, 84, 166),
		{ "포항", "Pohang", "浦項", "浦项" }, 241, 7, 12,
		"광명|대전|동대구|신경주|포항" },
	{ T(180, 20, 380, 1220), { "KTX", "KTX", "KTX", "KTX" }, C(0, 84, 166),
		{ "목포", "Mokpo", "木浦", "木浦" }, 401, 5, 8,
		"광명|천안아산|오송|공주|익산|정읍|광주송정|나주|목포" },
	{ T(60, 35, 360, 1290), { "KTX-산천", "KTX-Sancheon", "KTX山川",
		"KTX山川" }, C(0, 130, 140), { "강릉", "Gangneung", "江陵", "江陵" },
		801, 1, 4,
		"청량리|상봉|양평|만종|횡성|둔내|평창|진부|강릉" },
	{ T(120, 65, 360, 1260), { "ITX-새마을", "ITX-Saemaeul", "ITXセマウル",
		"ITX新村" }, C(200, 30, 70), { "부산", "Busan", "釜山", "釜山" }, 1001,
		13, 14,
		"영등포|수원|천안|대전|김천|구미|동대구|밀양|구포|부산" },
	{ T(120, 25, 335, 1340), { "무궁화호", "Mugunghwa", "ムグンファ号",
		"无穷花号" }, C(240, 120, 20), { "부산", "Busan", "釜山", "釜山" }, 1201,
		13, 14,
		"영등포|수원|평택|천안|조치원|대전|영동|김천|구미|왜관|대구|밀양|구포|부산" },
	{ T(180, 100, 400, 1300), { "무궁화호", "Mugunghwa", "ムグンファ号",
		"无穷花号" }, C(240, 120, 20), { "대전", "Daejeon", "大田", "大田" },
		1251, 13, 14,
		"영등포|수원|평택|천안|조치원|대전" },
};
const int kKorailCount = sizeof(kKorail) / sizeof(kKorail[0]);


// Korean, English, Japanese, Chinese.
static const char* kKorailStations[][4] = {
	{ "광명", "Gwangmyeong", "光明", "光明" },
	{ "천안아산", "Cheonan-Asan", "天安牙山", "天安牙山" },
	{ "오송", "Osong", "五松", "五松" },
	{ "대전", "Daejeon", "大田", "大田" },
	{ "김천구미", "Gimcheon-Gumi", "金泉亀尾", "金泉龟尾" },
	{ "동대구", "Dongdaegu", "東大邱", "东大邱" },
	{ "신경주", "Singyeongju", "新慶州", "新庆州" },
	{ "울산", "Ulsan", "蔚山", "蔚山" },
	{ "부산", "Busan", "釜山", "釜山" },
	{ "밀양", "Miryang", "密陽", "密阳" },
	{ "진영", "Jinyeong", "進永", "进永" },
	{ "창원중앙", "Changwon Jungang", "昌原中央", "昌原中央" },
	{ "창원", "Changwon", "昌原", "昌原" },
	{ "마산", "Masan", "馬山", "马山" },
	{ "진주", "Jinju", "晋州", "晋州" },
	{ "포항", "Pohang", "浦項", "浦项" },
	{ "공주", "Gongju", "公州", "公州" },
	{ "익산", "Iksan", "益山", "益山" },
	{ "정읍", "Jeongeup", "井邑", "井邑" },
	{ "광주송정", "Gwangju Songjeong", "光州松汀", "光州松汀" },
	{ "나주", "Naju", "羅州", "罗州" },
	{ "목포", "Mokpo", "木浦", "木浦" },
	{ "청량리", "Cheongnyangni", "清涼里", "清凉里" },
	{ "상봉", "Sangbong", "上鳳", "上凤" },
	{ "양평", "Yangpyeong", "楊平", "杨平" },
	{ "만종", "Manjong", "万鍾", "万钟" },
	{ "횡성", "Hoengseong", "横城", "横城" },
	{ "둔내", "Dunnae", "屯内", "屯内" },
	{ "평창", "Pyeongchang", "平昌", "平昌" },
	{ "진부", "Jinbu", "珍富", "珍富" },
	{ "강릉", "Gangneung", "江陵", "江陵" },
	{ "영등포", "Yeongdeungpo", "永登浦", "永登浦" },
	{ "수원", "Suwon", "水原", "水原" },
	{ "평택", "Pyeongtaek", "平沢", "平泽" },
	{ "천안", "Cheonan", "天安", "天安" },
	{ "조치원", "Jochiwon", "鳥致院", "鸟致院" },
	{ "영동", "Yeongdong", "永同", "永同" },
	{ "김천", "Gimcheon", "金泉", "金泉" },
	{ "구미", "Gumi", "亀尾", "龟尾" },
	{ "왜관", "Waegwan", "倭館", "倭馆" },
	{ "대구", "Daegu", "大邱", "大邱" },
	{ "구포", "Gupo", "亀浦", "龟浦" },
};


BString
korail_stops(const KorailService& service, int lang)
{
	static const char* kSeparator[4] = { " · ", " · ", "・", "・" };
	BString out;
	const int known = sizeof(kKorailStations) / sizeof(kKorailStations[0]);
	const char* s = service.stops;
	bool first = true;
	while (*s != '\0') {
		const char* end = strchr(s, '|');
		if (end == NULL)
			end = s + strlen(s);
		BString name(s, end - s);
		const char* shown = name.String();
		for (int i = 0; i < known; i++) {
			if (name == kKorailStations[i][0]) {
				shown = kKorailStations[i][lang];
				break;
			}
		}
		if (!first)
			out << kSeparator[lang];
		out << shown;
		first = false;
		s = *end == '\0' ? end : end + 1;
	}
	return out;
}


// #pragma mark - London Victoria


static const CallingPoint kDartford[] = {
	{ "Denmark Hill", 6 }, { "Peckham Rye", 9 }, { "Nunhead", 12 },
	{ "Lewisham", 17 }, { "Blackheath", 20 }, { "Kidbrooke", 24 },
	{ "Eltham", 27 }, { "Falconwood", 30 }, { "Welling", 33 },
	{ "Bexleyheath", 36 }, { "Barnehurst", 39 }, { "Dartford", 46 },
};
static const CallingPoint kRamsgateRochester[] = {
	{ "Bromley South", 16 }, { "Rochester", 42 }, { "Chatham", 45 },
	{ "Gillingham", 49 }, { "Rainham", 54 }, { "Sittingbourne", 62 },
	{ "Faversham", 70 }, { "Whitstable", 78 }, { "Chestfield", 82 },
	{ "Herne Bay", 86 }, { "Birchington-on-Sea", 96 },
	{ "Westgate-on-Sea", 99 }, { "Margate", 103 }, { "Broadstairs", 109 },
	{ "Ramsgate", 115 },
};
static const CallingPoint kGillingham[] = {
	{ "Bromley South", 16 }, { "St Mary Cray", 22 }, { "Swanley", 27 },
	{ "Farningham Road", 31 }, { "Longfield", 36 }, { "Meopham", 40 },
	{ "Sole Street", 43 }, { "Rochester", 52 }, { "Chatham", 55 },
	{ "Gillingham", 59 },
};
static const CallingPoint kBeckenham[] = {
	{ "Brixton", 4 }, { "Herne Hill", 7 }, { "West Dulwich", 10 },
	{ "Sydenham Hill", 12 }, { "Penge East", 16 }, { "Kent House", 18 },
	{ "Beckenham Junction", 21 },
};
static const CallingPoint kOrpington[] = {
	{ "Brixton", 4 }, { "Herne Hill", 7 }, { "West Dulwich", 10 },
	{ "Sydenham Hill", 12 }, { "Penge East", 16 }, { "Kent House", 18 },
	{ "Beckenham Junction", 21 }, { "Shortlands", 24 },
	{ "Bromley South", 27 }, { "Bickley", 30 }, { "Petts Wood", 34 },
	{ "Orpington", 38 },
};
static const CallingPoint kDover[] = {
	{ "Bromley South", 16 }, { "Longfield", 33 }, { "Meopham", 37 },
	{ "Rochester", 48 }, { "Chatham", 51 }, { "Gillingham", 56 },
	{ "Rainham", 61 }, { "Newington", 66 }, { "Sittingbourne", 70 },
	{ "Teynham", 76 }, { "Faversham", 82 }, { "Selling", 88 },
	{ "Canterbury East", 95 }, { "Bekesbourne", 99 }, { "Adisham", 103 },
	{ "Aylesham", 106 }, { "Snowdown", 109 }, { "Shepherds Well", 113 },
	{ "Kearsney", 119 }, { "Dover Priory", 125 },
};
static const CallingPoint kRamsgateMaidstone[] = {
	{ "Bromley South", 16 }, { "St Mary Cray", 22 }, { "Swanley", 27 },
	{ "Otford", 35 }, { "Borough Green & Wrotham", 42 },
	{ "West Malling", 47 }, { "East Malling", 50 }, { "Barming", 53 },
	{ "Maidstone East", 58 }, { "Bearsted", 64 }, { "Hollingbourne", 68 },
	{ "Harrietsham", 72 }, { "Lenham", 76 }, { "Ashford International", 90 },
	{ "Canterbury West", 108 }, { "Ramsgate", 128 },
};

#define CALLS(c) c, (int)(sizeof(c) / sizeof(c[0]))

const UkService kVictoria[] = {
	{ T(30, 4, 330, 1410), "Dartford", "via Bexleyheath", CALLS(kDartford),
		8, 1, 2 },
	{ T(60, 10, 330, 1350), "Ramsgate", "via Rochester",
		CALLS(kRamsgateRochester), 12, 3, 6 },
	{ T(60, 12, 330, 1380), "Gillingham", "via Meopham", CALLS(kGillingham),
		8, 3, 6 },
	{ T(30, 13, 330, 1410), "Beckenham Jcn", "via Penge East",
		CALLS(kBeckenham), 8, 1, 2 },
	{ T(30, 28, 330, 1410), "Orpington", "via Penge East", CALLS(kOrpington),
		8, 1, 2 },
	{ T(60, 40, 330, 1320), "Dover Priory", "via Meopham", CALLS(kDover),
		8, 3, 6 },
	{ T(60, 55, 330, 1320), "Ramsgate", "via Maidstone East",
		CALLS(kRamsgateMaidstone), 8, 3, 6 },
};
const int kVictoriaCount = sizeof(kVictoria) / sizeof(kVictoria[0]);


// #pragma mark - Sydney Central


static const SydneyRun kGordon[] = {
	{ T(15, 2, 300, 1440), "Gordon", "",
		"Town Hall|Wynyard|Milsons Point|North Sydney|Chatswood|Gordon",
		"Express", 8, 16, false },
	{ T(30, 9, 300, 1440), "Berowra", "via Gordon",
		"Town Hall|Wynyard|North Sydney|Chatswood|Gordon|Hornsby|Berowra",
		"All Stops", 8, 16,
		false },
	{ T(30, 24, 300, 1440), "North Sydney", "",
		"Town Hall|Wynyard|Milsons Point|North Sydney", "All Stops", 8, 16,
		false },
};
static const SydneyRun kHornsby[] = {
	{ T(15, 5, 300, 1440), "Hornsby", "via Strathfield",
		"Pennant Hills|Thornleigh|Normanhurst|Hornsby||Redfern|Burwood|"
		"Strathfield", "All Stops", 8, 18, false },
};
static const SydneyRun kEmuPlains[] = {
	{ T(30, 4, 300, 1440), "Emu Plains", "via Parramatta",
		"Werrington|Kingswood|Penrith|Emu Plains||Redfern|Strathfield|"
		"Lidcombe|Parramatta", "Limited Stops", 8, 18, false },
	{ T(30, 19, 300, 1440), "Richmond", "via Parramatta",
		"Redfern|Strathfield|Lidcombe|Parramatta|Blacktown|Schofields|Windsor|"
		"Richmond", "Limited Stops",
		8, 18, false },
	{ T(60, 34, 300, 1440), "Penrith", "via Parramatta",
		"Redfern|Strathfield|Lidcombe|Parramatta|Blacktown|St Marys|Penrith",
		"Limited Stops",
		8, 18, false },
};
static const SydneyRun kMacarthur[] = {
	{ T(15, 4, 300, 1440), "Macarthur", "via Airport stations",
		"Campbelltown|Macarthur||Green Square|Mascot|Domestic|International|"
		"Wolli Creek", "Limited Stops", 8, 22, false },
	{ T(30, 7, 300, 1440), "Revesby", "via Airport stations",
		"Green Square|Mascot|Domestic|International|Wolli Creek|Turrella|"
		"Bardwell Park|Revesby", "All Stops", 8, 23, false },
	{ T(30, 13, 300, 1440), "Sydenham", "",
		"Redfern|Erskineville|St Peters|Sydenham", "All Stops", 8, 22, false },
};
static const SydneyRun kLeppington[] = {
	{ T(15, 8, 300, 1440), "Leppington", "via Granville",
		"Lidcombe|Granville|Liverpool|Leppington||Redfern|Macdonaldtown|"
		"Newtown|Stanmore|Petersham", "All Stops", 8, 23, false },
	{ T(30, 23, 300, 1440), "Homebush", "via Strathfield",
		"Redfern|Newtown|Petersham|Ashfield|Burwood|Strathfield|Homebush",
		"All Stops", 8,
		23, false },
};
static const SydneyRun kNewcastle[] = {
	{ T(60, 20, 300, 1380), "Newcastle Intg", "via Strathfield",
		"Wyong|Warnervale|Wyee|Morisset|Dora Creek|Awaba|Fassifern|Booragul|"
		"Teralba", "Limited Stops", 10, 9, false },
};
static const SydneyRun kBrisbane[] = {
	{ T(1440, 0, 887, 887), "Brisbane", "",
		"Broadmeadow|Maitland|Dungog|Gloucester|Wingham|Taree",
		"Limited Stops", 8, 5, true },
	{ T(1440, 0, 450, 450), "Grafton", "",
		"Broadmeadow|Maitland|Dungog|Gloucester|Wingham|Taree|Kempsey|Grafton",
		"Limited Stops", 8, 5, true },
};
static const SydneyRun kTallawong[] = {
	{ T(4, 1, 300, 1440), "Tallawong", "",
		"Martin Place|Barangaroo|Victoria Cross|Crows Nest|Chatswood|Epping|"
		"Tallawong", "All Stops", 6, 26, false },
};
static const SydneyRun kBondi[] = {
	{ T(10, 0, 300, 1440), "Bondi Jn", "",
		"Town Hall|Martin Place|Kings Cross|Edgecliff|Bondi Jn", "All Stops",
		8, 24, false },
};
static const SydneyRun kWaterfall[] = {
	{ T(15, 10, 300, 1440), "Waterfall", "via Banksia",
		"Allawah|Hurstville|Penshurst|Mortdale|Sutherland|Loftus|Engadine|"
		"Heathcote|Waterfall", "Limited Stops", 8, 25, false },
	{ T(15, 3, 300, 1440), "Cronulla", "via Kogarah",
		"Tempe|Wolli Creek|Rockdale|Kogarah|Hurstville|Sutherland|Jannali|"
		"Cronulla", "Limited Stops", 8,
		25, false },
};
static const SydneyRun kViaMuseum[] = {
	{ T(6, 0, 300, 1440), "City", "via Museum",
		"Museum|St James|Circular Quay|Wynyard|Town Hall", "All Stops", 8, 20,
		false },
	{ T(6, 3, 300, 1440), "City", "via Museum",
		"Museum|St James|Circular Quay|Wynyard|Town Hall", "All Stops", 8, 21,
		false },
};
static const SydneyRun kViaTownHall[] = {
	{ T(6, 1, 300, 1440), "City", "via Town Hall",
		"Town Hall|Wynyard|Circular Quay|St James|Museum", "All Stops", 8, 17,
		false },
	{ T(6, 4, 300, 1440), "City", "via Town Hall",
		"Town Hall|Wynyard|Circular Quay|St James|Museum", "All Stops", 8, 17,
		false },
};
static const SydneyRun kBomaderry[] = {
	{ T(120, 23, 300, 1380), "Bomaderry", "via Wolli Creek",
		"Albion Park|Oak Flats|Shellharbour Jn|Minnamurra|Bombo|Kiama|"
		"Gerringong|Berry", "Limited Stops", 4, 14, false },
	{ T(120, 83, 300, 1380), "Kiama", "via Wolli Creek",
		"Wolli Creek|Hurstville|Sutherland|Helensburgh|Thirroul|Wollongong|"
		"Kiama", "Limited Stops",
		4, 14, false },
};
static const SydneyRun kCanberra[] = {
	{ T(1440, 0, 1036, 1036), "Canberra", "", "Bungendore|Queanbeyan|Canberra||"
		"Mittagong|Bowral|Moss Vale", "Limited Stops", 4, 2, true },
	{ T(1440, 0, 1209, 1209), "Melbourne", "",
		"Campbelltown|Moss Vale|Goulburn|Wagga Wagga|Albury|Melbourne",
		"Limited Stops", 8, 2,
		true },
	{ T(1440, 0, 465, 465), "Canberra", "", "Bungendore|Queanbeyan|Canberra||"
		"Mittagong|Bowral|Moss Vale", "Limited Stops", 4, 2, true },
};
static const SydneyRun kSydenham[] = {
	{ T(4, 3, 300, 1440), "Sydenham", "", "Waterloo|Sydenham", "All Stops", 6,
		27, false },
};

#define RUNS(r) r, (int)(sizeof(r) / sizeof(r[0]))
#define NO_RUNS NULL, 0

// Row by row, eight to a row; the first five columns are Sydney services,
// then Intercity & Events, Regional services and Metro.
const SydneyPanel kSydneyPanels[] = {
	{ PANEL_LINE, "T1", C(247, 144, 30), RUNS(kGordon), NULL },
	{ PANEL_LINE, "T9", C(209, 31, 47), RUNS(kHornsby), NULL },
	{ PANEL_LINE, "T1", C(247, 144, 30), RUNS(kEmuPlains), NULL },
	{ PANEL_LINE, "T8", C(0, 149, 74), RUNS(kMacarthur), NULL },
	{ PANEL_LINE, "T2", C(0, 152, 205), RUNS(kLeppington), NULL },
	{ PANEL_LONG, "T", C(209, 31, 47), RUNS(kNewcastle), NULL },
	{ PANEL_LONG, "T", C(209, 31, 47), RUNS(kBrisbane), NULL },
	{ PANEL_LINE, "M1", C(22, 132, 147), RUNS(kTallawong), NULL },

	{ PANEL_NOTICE, "", C(0, 0, 0), NO_RUNS,
		"T6 Lidcombe & Bankstown Line\n\nFor customers travelling to Birrong, "
		"Yagoona and Bankstown, catch a T1, T2 or T3 train to Lidcombe\n\n"
		"Change at Lidcombe for T6 trains towards Bankstown" },
	{ PANEL_LINE, "T4", C(0, 94, 184), RUNS(kBondi), NULL },
	{ PANEL_LINE, "T4", C(0, 94, 184), RUNS(kWaterfall), NULL },
	{ PANEL_CITY, "City", C(20, 20, 20), RUNS(kViaMuseum), NULL },
	{ PANEL_CITY, "City", C(20, 20, 20), RUNS(kViaTownHall), NULL },
	{ PANEL_LONG, "T", C(209, 31, 47), RUNS(kBomaderry), NULL },
	{ PANEL_LONG, "T", C(209, 31, 47), RUNS(kCanberra), NULL },
	{ PANEL_LINE, "M1", C(22, 132, 147), RUNS(kSydenham), NULL },
};
const int kSydneyPanelCount = sizeof(kSydneyPanels) / sizeof(kSydneyPanels[0]);
