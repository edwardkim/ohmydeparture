# 실데이터 연동 가이드

[English](REAL_DATA_GUIDE.md) · [프로젝트 README](../README.ko.md)

항공편 데이터를 내려받고 공항 보드에 적용한 뒤, 설치된 앱이 어떤 자료를 표시하는지
확인하는 절차입니다. 소스 관련 명령은 저장소 루트에서 실행합니다. 플러그인 번들에는
다운로드 도구가 없으므로 명령 실행에 사용할 소스 작업본을 준비합니다. 나리타 캐시
어댑터를 포함한 현재 소스로 빌드한 앱을 사용하세요. 캐시를 복사하는 것만으로
이전 실행 파일에 새 데이터 소스 지원이 추가되지는 않습니다.

항공편 데이터는 수동으로 갱신합니다. 시스템 시각에 따른 필터와 플립 애니메이션은
이미 받은 데이터를 사용하며 API를 호출하지 않습니다.

## 데이터 소스 선택

| 보드 | 데이터 소스와 범위 | 저장 위치 | 화면 반영 절차 |
| --- | --- | --- | --- |
| ICN | 인천공항 API의 요청일 전체 출발 데이터 | `data/icn-departures.json`, 실행 파일에 내장 | 다운로드 → 빌드 → 플러그인 재설치 → 앱 종료 후 재실행 |
| NRT | aviationstack 응답 한 페이지 | `~/.local/share/ohmydeparture/aviationstack/nrt.json` | 캐시 활성화 → 앱 종료 후 재실행. 이후 데이터만 바꿀 때는 재빌드 불필요 |
| 나머지 공항·철도 | 생성 예제 시간표 | 실행 파일의 시간표 데이터 | 아직 실데이터 어댑터 미연결 |

나리타는 캐시 설치 전까지 생성 예제를 표시합니다. 캐시를 설치한 뒤 자료가 잘못되거나
날짜가 지나면 안내 문구를 표시하며 생성 시간표로 조용히 바꾸지 않습니다.
나리타 저장 경로는 절대 경로인 `XDG_DATA_HOME`을 따르며, 기본값은 `~/.local/share`입니다.

## 공통 데이터 유틸리티 사용

보드와 같은 검증기를 사용하도록 앱을 한 번 빌드한 뒤 관리 명령을 사용합니다.

```sh
make -j2
python3 tools/flight-data.py providers
python3 tools/flight-data.py status --airport NRT
```

결과는 JSON입니다. `providers`·`status`·`validate`·`install`은 API를 호출하지 않습니다.
`status`는 선택한 실행 파일과 데이터 파일을 검사하며, 실행 중인 GUI의 메모리를
조회하지는 않습니다. 설치본을 검사하려면 `--app /path/to/ohmydeparture`를 지정합니다.
선택적인 캐시가 없으면 `missing` 상태와 종료 코드 1을 반환합니다.

기존 나리타 응답을 공통 검증기로 확인하고 적용합니다.

```sh
python3 tools/flight-data.py validate --airport NRT --file test-output/aviationstack-nrt-check.json
python3 tools/flight-data.py install --airport NRT --file test-output/aviationstack-nrt-check.json --dry-run
python3 tools/flight-data.py install --airport NRT --file test-output/aviationstack-nrt-check.json
```

실제 내용이 바뀔 때 이전 파일을 백업합니다. 날짜가 지난 자료는 기본적으로 거절하며,
보관 자료로 교체하려는 경우에만 `--allow-stale`을 명시합니다. 백업 경로는
`~/.local/state/ohmydeparture/flight-data/backups/`이며 `XDG_STATE_HOME`을 따릅니다.
복구할 때에는 반환된 백업 파일 경로를 같은 명령으로 검증하고 설치합니다.
적용 후에는 GUI를 종료하고 다시 엽니다.

키를 설정한 뒤 수집 계획을 확인하거나 후보 데이터를 받을 수 있습니다.

```sh
python3 tools/flight-data.py fetch --airport NRT --dry-run
python3 tools/flight-data.py fetch --airport NRT --max-requests 1
```

`fetch`는 일치하는 후보 캐시를 재사용합니다. 새 요청에는 `--refresh`, 캐시만
사용하려면 `--max-requests 0`을 지정합니다. 후보 파일을 준비할 뿐 활성화하지 않습니다.
aviationstack은 여전히 한 페이지이며 호출 예산을 늘려도 페이지를 합치거나 추가로
모으지 않습니다. ICN은 전체 결과 수집에 기본 최대 10회를 사용합니다.

```sh
python3 tools/flight-data.py fetch --airport ICN --output test-output/icn-candidate.json --max-requests 10
python3 tools/flight-data.py install --airport ICN --file test-output/icn-candidate.json
python3 tools/flight-data.py status --airport ICN
```

고정 후보 파일에 다른 날짜의 ICN 자료가 있으면 `--refresh`를 명시해 교체합니다.
ICN `status`는 소스 파일과 실행 파일의 내장 데이터가 다르면 `needs_rebuild`를
표시합니다. 활성화한 뒤 아래 빌드·재설치 절차를 계속 진행합니다. 수집이나 검증이
실패하면 기존 후보와 활성 데이터를 보존합니다. 아래의 제공자별 명령도 계속 사용할
수 있습니다. 새 API 추가 방법은 [제공자 구조 안내](DATA_PROVIDERS.md)를 참고하세요.

## 본인 인증키 설정

`.env`가 없으면 [.env.example](../.env.example)을 참고해 만듭니다. 기존 파일이 있으면
다른 서비스 설정을 덮어쓰지 말고 필요한 항목을 추가하거나 수정합니다.

```dotenv
ICN_EP=https://apis.data.go.kr/B551177/statusOfAllFltDeOdp
ICN_KEY=YOUR_OWN_DATA_GO_KR_SERVICE_KEY
AVIATIONSTACK_KEY=YOUR_OWN_AVIATIONSTACK_ACCESS_KEY
```

실제 키는 Git에서 제외되는 로컬 `.env`에 보관합니다. 도구가 이를 일반 데이터로
읽으므로 `source`로 실행하지 마세요. aviationstack은 프로세스 환경변수도 지원하며,
export된 `AVIATIONSTACK_KEY`가 파일보다 우선합니다. ICN 도구는 `.env`를 읽습니다.
다른 설정 파일은 두 다운로드 도구의 `--env /path/to/file`로 지정할 수 있습니다.
인증키는 다운로드 도구가 사용하며 보드가 직접 사용하지 않습니다.

`ACODE_EP`·`ACODE_KEY`는 정적 공항 코드 목록을 관리할 때만 필요한 별도 설정입니다.
운항정보 인증키가 아니며, 내장된 공항 이름을 사용하는 일반 사용자는 필요 없습니다.

## 인천공항 출발 데이터 갱신

### 회원가입과 활용 승인

데이터를 직접 내려받거나 갱신하는 사용자마다
[공공데이터포털](https://www.data.go.kr/) 계정,
[인천국제공항공사 항공기 운항 현황 상세 조회](https://www.data.go.kr/data/15140153/openapi.do)
활용 승인, 해당 API의 본인 인증키가 필요합니다. 서비스 페이지에서 **활용신청**을
완료하고 승인 상태를 확인한 뒤 키를 사용합니다. 회원가입만 하거나 다른 API의 키를
받은 것으로는 충분하지 않습니다. 번들에 이미 포함된 자료를 보는 데에는 키가 필요 없습니다.

### 오늘 자료 다운로드

`ICN_EP`·`ICN_KEY`를 설정한 뒤 실행합니다.

```sh
python3 tools/fetch-icn-departures.py
```

기본 조회일은 `Asia/Seoul` 기준 오늘입니다. HTTPS의 `getFltDeparturesDeOdp`를 사용해
예정 시각 기준 `searchdtCode=S`, `searchFrom=0000`, `searchTo=2400`으로 전체 페이지를
받습니다. 전체 건수·고유 편명 식별자·예정일을 검증한 뒤 `data/icn-departures.json`을
교체하며, 실패하거나 불완전하면 기존 파일을 보존합니다. `fid`는 응답에 들어 있는
운항편 식별자이며 클라이언트가 요청 시각으로 만들어 보내는 값이 아닙니다.
지원 범위 안의 다른 날짜는 `--date YYYYMMDD`로 지정합니다.

선택 사항인 아래 진단은 출발·도착을 JSON·XML로 확인합니다. 요청 4회를 사용하고
인증키를 숨긴 결과를 `test-output/`에 저장하며, 보드용 데이터 파일은 교체하지 않습니다.

```sh
python3 tools/check-icn-api.py --time-basis S
```

### 새 데이터를 앱에 반영

소스에서 직접 실행할 경우 먼저 빌드합니다.

```sh
make -j2
```

실행 중인 앱을 톱니바퀴 메뉴에서 종료한 뒤, ICN을 명시해 다시 엽니다.

```sh
./ohmydeparture --airport ICN --windowed
```

설치된 Omarchy 플러그인은 아래 **설치된 플러그인 갱신** 절차를 사용합니다.
저장소의 실행 파일만 새로 빌드해도 플러그인의 설치 파일은 바뀌지 않습니다.

ICN 화면은 `Master` 여객편만 표시합니다. 공동운항 `Slave`와 화물편도 원본 JSON에
보존하지만 화면에서는 제외합니다. 시각은 날짜를 포함한 KST를 유지하며,
하단에서 실제 다운로드 시각을 확인할 수 있습니다.

## aviationstack 페이지를 나리타 보드에 적용

### 가입과 호출 한도 확인

[aviationstack](https://aviationstack.com/signup/free)에 가입해 본인 키를 발급받고
`AVIATIONSTACK_KEY`에 설정합니다. 2026-10-27 확인한
[무료 플랜](https://aviationstack.com/pricing)은 월 100회, HTTPS, 비상업적 이용을
제공합니다. 계정 대시보드에서 현재 플랜과 남은 호출량을 확인하세요.

[제공자 명세](https://api.swaggerhub.com/apis/apilayer-863/AviationstackAPI/1.0.0/swagger.json)상
Professional 미만 플랜은 페이지당 최대 100건입니다. 페이지를 받을 때마다 요청 1회를
사용합니다. 무료 100회가 항공편 100개를 뜻하는 것은 아닙니다.

### 한 페이지 다운로드 또는 확인

```sh
python3 tools/check-aviationstack-api.py --airport NRT
```

`https://api.aviationstack.com/v1/flights`에 `dep_iata=NRT`, `limit=100`, `offset=0`으로
요청합니다. 결과 파일이 없으면 요청 1회를 사용합니다. 일치하는 캐시가 있으면 오래된
것이어도 재사용하고 원래 조회 시각과 `API calls this run: 0`을 표시합니다.
자동 재시도나 추가 페이지 수집은 없습니다. 결과는 사용자만 읽고 쓸 수 있는
`test-output/aviationstack-nrt-check.json`에 저장합니다. 현재 도구는 `flight_date`
매개변수를 보내지 않습니다.
기존 파일이 잘못되거나 조회 조건과 다르면 네트워크 요청 없이 실패합니다.
이를 교체할 때는 `--refresh`를 명시합니다.

### 다운로드한 페이지 활성화

```sh
python3 tools/check-aviationstack-api.py --airport NRT --install
```

`test-output/`의 진단 결과를 보드가 자동으로 읽지는 않습니다.
일치하는 캐시가 있으면 추가 API 호출은 0회입니다. 검증된 복사본을 앞에서 설명한
사용자 전용 나리타 데이터 경로에 저장합니다. 실행 중인 앱을 톱니바퀴 메뉴에서
종료하고 사용할 실행 파일을 다시 엽니다.

```sh
./ohmydeparture --airport NRT --windowed
```

이미 업데이트한 플러그인은 설치된 실행 경로를 사용합니다.

```sh
~/.config/omarchy/plugins/edwardkim.ohmydeparture/launch --view departures --airport NRT --windowed
```

설치본에 나리타 어댑터가 아직 없다면 먼저 플러그인을 업데이트합니다. 그 이후에는
나리타 데이터만 교체할 때 캐시 활성화와 앱 재실행으로 충분하며 재빌드는 필요 없습니다.

### 새 응답을 받아 활성화

```sh
python3 tools/check-aviationstack-api.py --airport NRT --refresh --install
```

요청 1회로 새 응답을 받아 성공한 경우 활성화합니다. 이후 앱을 종료하고 다시 엽니다.
요청 실패 시 기존 캐시를 보존합니다.

다음 페이지를 별도로 확인하려면 다음을 사용합니다.

```sh
python3 tools/check-aviationstack-api.py --airport NRT --offset 100
```

별도 결과 파일을 사용하며 **페이지를 합치지 않습니다**. 다른 offset의 페이지를
설치하면 기존 페이지를 대체할 뿐 항목을 추가하지 않습니다. 현재 `--install`이
지원되는 보드는 NRT뿐입니다.

2026-10-27 첫 조회는 총 1,418건 중 100건을 반환했습니다. 그중 코드셰어가 50건으로,
중복 없는 비코드셰어 50편이 남았습니다. 같은 조회 조건 전체를 100건씩 받으려면
15회, 이를 30일 동안 매일 반복하면 450회가 필요합니다. 이는 조회 조건의 건수이며
검증된 하루 전체 여객편 수를 뜻하지 않습니다. 도구는 다른 프로그램의 사용량을
추적하거나 조회 조건 전체를 자동 수집하지 않습니다.

### 나리타 미리보기의 의미

코드셰어와 같은 편명·예정 시각의 중복은 제외합니다. ICN처럼 여객·화물 구분 필드가
없으므로 화물편은 포함합니다. 정적 공항 이름 번역을 사용하고 원본 게이트·운항 상태를
표시하며, 게이트가 없으면 빈칸으로 둡니다. 영문·일문·한글로 순환합니다.

하단에는 `PARTIAL`, 저장 건수/전체 건수, JST 조회 시각, **REPORTED TIMES**가 나옵니다.
샘플 응답에는 `+00:00`과 `Asia/Tokyo`가 함께 있습니다. 현재 미리보기는 반환된 날짜와
시각 숫자를 그대로 표시하고 이를 도쿄 현지 시각으로 취급해 필터링하며, 9시간을
더하지 않습니다. 원본 문자열은 캐시에 남습니다. 제공자의 시각 의미가 확인되기 전까지는
검증된 UTC 변환 결과가 아닌 원본 시각 미리보기로 사용합니다.

## 설치된 플러그인 갱신

인천 데이터 변경 또는 앱 코드 업데이트 후에 사용합니다. 저장소 루트에서 실행합니다.

```sh
make package-omarchy
omd_version=$(cat VERSION)
omd_arch=$(uname -m)
python3 "dist/ohmydeparture-omarchy-${omd_version}-${omd_arch}/install.py"
```

설치 프로그램은 기존 바 설정을 유지하며 플러그인을 갱신합니다. 이전 설치 내용은
`~/.local/state/ohmydeparture-omarchy/backups/`에 백업합니다. 설치 후 이전 앱을
톱니바퀴 메뉴에서 종료하고 선택한 보드를 다시 엽니다.

```sh
~/.config/omarchy/plugins/edwardkim.ohmydeparture/launch --view departures --airport ICN --windowed
```

나리타를 보려면 `ICN`을 `NRT`로 바꿉니다. 이전 프로세스가 살아 있는 상태에서
다시 실행하면 그 프로세스의 화면만 전환하며, 새 실행 파일이나 캐시를 다시 읽지 않습니다.
공항을 지정하지 않으면 기본 화면은 Codex입니다.

ICN 데이터는 새 실행 파일에 내장됩니다. 나리타 데이터는 플러그인과 별개인 사용자
데이터 디렉터리에 남습니다. aviationstack 실응답과 인증키는 공개 소스 릴리스나
플러그인 번들에 포함하지 않습니다.

## 화면 반영 확인

두 보드는 예정 시각으로 정렬합니다. 변경 시각이 있으면 변경 시각, 없으면 예정 시각이
**현재 공항 시각의 30분 전 이상**인 편을 표시합니다. 불러온 날짜 안에서는 미래 상한이
없으며 창에 들어가는 행 수만큼 표시합니다. 시각에 따라 필터가 움직여도 API는 호출하지
않습니다. 지연 상태라도 시간 조건을 벗어나면 제외합니다.

| 화면 또는 증상 | 의미와 조치 |
| --- | --- |
| `ICN SNAPSHOT … KST` | 다운로드한 자료의 조회 시각과 비교합니다 |
| `NRT AVIATIONSTACK 100/1418 PARTIAL … JST … REPORTED TIMES` | 일부 페이지를 원본 시각 미리보기 기준으로 표시하고 있습니다 |
| `REFRESH REQUIRED` | 불러온 날짜가 지났습니다. 새로 받아 적용합니다 |
| `DATA UNAVAILABLE` | 파일을 읽거나 검증할 수 없습니다. 경로와 반영 절차를 확인합니다 |
| 나리타 캐시에 이후 운항편이 없다는 안내 | 이 페이지에서 시간 조건을 만족하는 편이 없다는 뜻이며 공항 전체의 출발편이 없다는 뜻은 아닙니다 |
| 나리타가 계속 생성 예제를 표시함 | 같은 Linux 사용자와 `XDG_DATA_HOME`에 캐시를 설치했는지 확인하고 이전 바이너리를 업데이트한 뒤 재실행합니다 |
| ICN이 이전 데이터를 표시함 | 빌드와 플러그인 재설치를 마친 뒤 이전 프로세스를 종료합니다 |
| ICN 진단이 HTTP 200이지만 유효한 자료가 없음 | API 결과 코드와 실제 항목도 확인합니다. HTTP 상태만으로 성공을 판단하지 않습니다 |
| aviationstack 갱신 실패 | 키·환경변수 우선순위·플랜·한도를 확인합니다. 기존 캐시는 계속 남습니다 |
| DNS·네트워크 오류 | DNS와 제공자에 대한 외부 HTTPS 접속이 가능한 환경에서 다운로드 도구를 실행합니다 |

화면을 새로 그린 것과 항공편 자료를 새로 받은 것을 구분하려면 하단 조회 시각을
확인합니다. 시계와 플랩이 움직이는 것만으로 운항 상태가 갱신된 것은 아닙니다.

## 데이터 형식과 제공자 추가

[data/README.md](../data/README.md)에 저장 필드와 출처를 설명했습니다.
공통 화면 행은 [Departure.h](../platform/qt/Departure.h), 검증·선택 로직은
[IcnDepartures.cpp](../platform/qt/IcnDepartures.cpp)와
[AviationstackDepartures.cpp](../platform/qt/AviationstackDepartures.cpp),
화면 출력은 [Board.cpp](../src/Board.cpp)를 참고합니다.

제공자를 추가할 때에는 시각과 수집 범위를 먼저 정의합니다. 원본 메타데이터와 누락
가능한 필드를 보존하고, 제공된 상태만 매핑하며 키는 다운로드 도구에서만 사용합니다.
페이지네이션·코드셰어·알 수 없는 공항/항공사·자정을 넘는 변경 시각·30분 경계·지난
날짜/잘못된 데이터에 대한 오프라인 예제를 추가합니다. 데이터를 번들에 넣을지 개인
캐시로 둘지는 제공자의 이용 조건에 맞춰 결정합니다.

기존 제공자 검증은 외부 API 요청 없이 실행합니다.

```sh
make test-icn test-aviationstack
```
