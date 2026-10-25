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

행을 클릭하면 상세·검색 창을 엽니다. 톱니바퀴의 **Display settings…**에서 반복 간격
(기본 3초, 0은 끄기)과 뒤집기 시간(기본 0.65초)을 바꿉니다. F11은 전체 화면,
Esc는 창 닫기입니다. **Codex sessions & agents**와 공항·역 메뉴로 보기를 전환합니다.

Codex 데이터는 읽기 전용입니다. HISTORY는 현재 실행 상태 미확인, IDLE은 현재 활동
없음을 뜻합니다. 턴 완료가 프로젝트 완료를 의미하지 않습니다. 공항·철도 시간표는
생성된 예시이며 실시간 운항 정보가 아닙니다.

실제 ICN 운항정보를 이용하려면 **사용자마다** [공공데이터포털](https://www.data.go.kr/)
회원가입 후 [인천국제공항공사_항공기 운항 현황 상세 조회](https://www.data.go.kr/data/15140153/openapi.do)
활용신청·승인을 받고 본인 인증키를 발급받아야 합니다. 공용 키는 제공하지 않습니다.
[소스 설정 안내](https://github.com/edwardkim/ohmydeparture#icn-live-data-api-prerequisites)를
참고하세요. 현재 v1.2.0 번들은 생성 시간표를 사용하며, 소스의 API 검사기와 `.env` 설정은
번들의 실시간 전광판 기능을 활성화하지 않습니다.

백업 위치는 `~/.local/state/ohmydeparture-omarchy/backups/`입니다. 제거 후에도
환경설정과 백업은 남습니다. 사용자 D-Bus·systemd·접근성 설정을 변경하지 않습니다.
