# Oh My Departure

Codex 세션과 에이전트 기록을 공항의 플랩 출발표처럼 보여주는 Linux / Omarchy 앱입니다.
프로젝트의 기본 문서와 UI 언어는 **영문**입니다. 사용자 세션의 제목과 내용은 원문 그대로
표시합니다. 전체 설명은 [English README](README.md)를 참고하세요.

![가상 세션 전광판](docs/codex-board.png)

## 실행

```sh
omarchy pkg add base-devel pkgconf qt6-base qt6-wayland python noto-fonts-cjk
make -j"$(nproc)"
./ohmydeparture --windowed
./ohmydeparture --airport ICN --windowed
```

기본 보기는 Codex입니다. 공항·철도는 `--airport` 또는 톱니바퀴 메뉴로 선택합니다.
`make package-omarchy`로 번들을 만든 뒤, 번들 안의 `install.py`를 sudo 없이 실행하면
상단 바 플러그인·앱 메뉴·아이콘을 설치합니다. 자세한 내용은
[번들 설명](packaging/omarchy/README.ko.md)을 참고하세요.

기존 관리형 `rainygirl.rdepartures` 설치는 새 ID `edwardkim.ohmydeparture`로 이전하며,
바 위치와 인라인 설정을 유지합니다. 기존 앱 창을 한 번 닫고 새 앱을 여세요.
기존 마지막 공항·플립 설정은 첫 실행 때 복사하며 원본은 유지합니다.

## 사용

행을 클릭하면 세션 상세, 검색, 하위 에이전트와 턴·도구 이력을 봅니다.
톱니바퀴의 **Display settings…**에서 반복 간격(기본 3초, 0은 끄기)과 뒤집기 시간
(기본 0.65초, 0.20–2.00초)을 변경합니다. 프로젝트는 고정 색상 플랩으로 표시하며
부모·하위 에이전트의 색이 일치합니다. F11은 전체 화면, Esc는 창 닫기입니다.

HISTORY는 현재 실행 상태를 모르는 저장 기록, IDLE은 현재 활동이 없음을 뜻합니다.
턴 완료는 프로젝트 완료를 뜻하지 않습니다. 기존 Codex 데몬을 5초마다 조회하며,
없으면 로컬 JSONL 기록을 읽습니다. 에이전트 실행·재개·중단·승인, 명령 실행,
Codex 설정·DB·hook 변경은 하지 않습니다. 스크린샷은 개인정보를 확인한 뒤 공유하세요.
이 문서의 화면은 가상 데이터입니다. 공항·철도 시간표도 생성된 예시입니다.

원작은 [rainygirl/R Departures](https://github.com/rainygirl/renkuos-rdeparture)입니다.
MIT 라이선스와 원작 크레딧을 유지하며, 폰트는 GUST Font License를 따릅니다.
[출처와 변경 범위](UPSTREAM.md), [기여 안내](CONTRIBUTING.md)를 확인하세요.
