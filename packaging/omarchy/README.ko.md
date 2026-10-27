# Oh My Departure — Omarchy 번들

기본 언어는 영문입니다. 전체 설치·사용 설명은 [README.md](README.md)를 참고하세요.

압축을 푼 디렉터리에서 일반 데스크톱 계정으로 실행합니다.

```sh
python3 install.py
python3 install.py --section left
python3 install.py --uninstall
```

Omarchy의 네이티브 bar-widget API, Python 3, `qt6-base`, `qt6-wayland`,
한중일 문자용 `noto-fonts-cjk`가 필요합니다. sudo는 사용하지 않습니다.

플러그인 ID는 `edwardkim.ohmydeparture`입니다. 설치 프로그램이 관리하던 이전
`rainygirl.rdepartures` 플러그인은 바 위치와 설정을 유지하며 이전됩니다.
기존 앱 창을 한 번 닫은 다음 새 앱을 여세요. 마지막 공항과 플립 설정은 첫 실행 때
이전 설정에서 복사하며, 원본 설정과 백업은 보존합니다.

행을 **더블클릭**하면 선택한 세션·에이전트의 최근 기록을 스플릿 플랩으로 표시합니다.
**← BACK** 또는 키보드 **←**로 코덱스 보드에 돌아오며 **→ / Space**로 히스토리를
넘깁니다. 검색·보관 기록·항목 전문은 **Session browser & search…**에서 확인합니다.
톱니바퀴의 **Display settings…**에서 반복 간격
(기본 3초, 0은 끄기)과 뒤집기 시간(기본 0.65초)을 바꿉니다. F11은 전체 화면,
히스토리 화면의 Esc는 보드 복귀, 세션 보드의 Esc는 창 닫기입니다.
**Codex sessions & agents**와 공항·역 메뉴로 보기를 전환합니다.

Codex 데이터는 읽기 전용입니다. HISTORY는 현재 실행 상태 미확인, IDLE은 현재 활동
없음을 뜻합니다. 턴 완료가 프로젝트 완료를 의미하지 않습니다.
ICN은 내려받은 실제 출발 정보와 KST 조회 시각을 표시합니다. 공동운항 중복은
제외하며 자동 갱신은 하지 않습니다. 날짜가 지나면 `REFRESH REQUIRED`와 빈 행을
표시합니다. 해당일 데이터에서는 시스템 현재 시각의 30분 전부터 이후 운항편만
표시합니다. 변경 시각이 있으면 변경 시각, 없으면 예정 시각을 기준으로 하며,
화면 범위를 이동하는 과정에서 API를 다시 호출하지 않습니다.
나리타는 소스 작업본에서 `python3 tools/check-aviationstack-api.py --airport NRT --install`로
개인 캐시를 활성화하면 실데이터 미리보기를 표시합니다. 앱을 다시 시작하세요.
하단에 부분 데이터·조회 시각·원본 시각 미리보기를 표시합니다.
캐시가 없는 나리타와 다른 공항·철도 화면은 생성 시간표입니다.

ICN 운항정보를 직접 내려받거나 갱신하려면 **사용자마다** [공공데이터포털](https://www.data.go.kr/)
회원가입 후 [인천국제공항공사_항공기 운항 현황 상세 조회](https://www.data.go.kr/data/15140153/openapi.do)
활용신청·승인을 받고 본인 인증키를 발급받아야 합니다. 공용 키는 제공하지 않습니다.
[실데이터 연동 가이드](https://github.com/edwardkim/ohmydeparture/blob/main/docs/REAL_DATA_GUIDE.ko.md)를
참고하세요. 번들에 포함된 조회 결과를 보는 데에는 인증키가 필요하지 않습니다.
소스 작업본에서 `python3 tools/fetch-icn-departures.py`와 `make package-omarchy`를
실행하고 새 설치 프로그램으로 업데이트하세요. 실행 중인 앱은 다시 시작해야 하며,
`.env`와 인증키는 번들에 포함하지 않습니다.

백업 위치는 `~/.local/state/ohmydeparture-omarchy/backups/`입니다. 제거 후에도
환경설정과 백업은 남습니다. 사용자 D-Bus·systemd·접근성 설정을 변경하지 않습니다.
