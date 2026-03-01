# Oops 앱 포팅 매뉴얼

본 문서는 `oops_app` Flutter 프로젝트를 새 개발 환경으로 옮길 때 필요한 준비물, 빌드 절차, 백엔드 연동 방법을 정리한 문서입니다.

## 1. 개요
- 프로젝트 타입: Flutter 3.x (Dart 3)
- 주요 의존성: `hooks_riverpod`, `http`, `o3d`, `google_fonts`, `lottie`, `flutter_svg`
- 3D 보기: `o3d` 패키지를 통해 GLB 파일을 네트워크/로컬에서 로드합니다.
- 백엔드 연계: Flutter 코드의 `_scannerBase` 값으로 지정된 라즈베리파이 스캐너 API와 EC2 메쉬 변환 서버에서 전달되는 GLB URL을 사용합니다.

## 2. 선행 설치
1. Flutter SDK 3.9.2 이상을 설치하고 `flutter doctor`로 Android/iOS toolchain이 모두 준비됐는지 확인합니다.
2. IDE는 Android Studio 혹은 VS Code를 권장하며 Flutter·Dart 플러그인을 추가합니다.
3. 모바일 타겟 요구사항
   - Android: Android SDK 34, (필요 시) NDK, Java 17 JDK, USB 디버깅 활성화 단말.
   - iOS: Xcode 15 이상, CocoaPods(`sudo gem install cocoapods`), 팀 프로비저닝 프로파일.
4. Web 빌드 시 WebGL 2와 WASM을 지원하는 최신 브라우저가 필요합니다.

## 3. 코드 구조 요약
```
oops_app/
 ├─ lib/
 │   ├─ main.dart           # 앱 엔트리
 │   ├─ screens/home/...    # 메인 UI 및 발 측정 탭
 │   ├─ widgets/, theme/    # 공용 위젯 및 테마
 │   └─ models/, providers/ # 상태 관련 코드 (추가 예정 placeholder)
 ├─ assets/images/          # 온보딩/히어로 이미지
 ├─ assets/glbs/test.glb    # 네트워크 모델이 없을 때 표시하는 기본 GLB
 └─ pubspec.yaml            # 의존성 및 자원 선언
```

## 4. 환경 구성 절차
1. 레포지토리 클론
  ```bash
  git clone <repo-url>
  cd S13P31A107/oops_app
  ```
2. Flutter 패키지 설치
  ```bash
  flutter pub get
  ```
3. (iOS) `cd ios && pod install && cd ..`
4. Asset 동기화: `pubspec.yaml`에 선언된 `assets/images`, `assets/glbs/` 경로와 실제 파일 구성이 일치하는지 확인합니다.
5. API 엔드포인트 설정
   - `lib/screens/home/main_screen.dart`의 `_FootMeasurementTabState._scannerBase` 기본값은 `http://70.12.245.109:8000`입니다.
   - 배포 환경 주소로 직접 수정하거나 `--dart-define=SCANNER_BASE=...` 형태로 주입하도록 개선하세요.

## 5. 개발/빌드
### 5.1 로컬 실행
```bash
flutter run -d <device_id>     # 모바일 단말 디버깅
```
- 3D 뷰 검증 시 스캐너 서버가 응답 가능한 상태여야 합니다. UI만 확인하려면 `_FootMeasurementTabState`의 `_glbUrl`을 테스트용 GLB URL로 임시 지정하세요.

### 5.2 테스트 빌드
- Android
  ```bash
  flutter build apk --release
  # 또는 App Bundle
  flutter build appbundle --release
  ```
  빌드 전에 `android/app/src/main/AndroidManifest.xml`의 `android:usesCleartextTraffic="true"` 여부를 확인하여 HTTP 스캐너 엔드포인트 허용 여부를 결정합니다.


### 5.3 환경 변수 주입 (선택)
- 상수를 직접 수정하지 않고 런치 스크립트에서 주입하려면:
  ```bash
  flutter run --dart-define=SCANNER_BASE=https://scanner.my-domain.com
  ```
  코드에서는 `const String.fromEnvironment('SCANNER_BASE', defaultValue: 'http://...')` 를 통해 읽어옵니다.

## 6. 백엔드 연동 체크리스트
1. 라즈베리파이 스캐너(`scanner_service.py`)가 `http://<pi>:8000`에서 기동되고 `/health`, `/scan`, `/jobs/{id}` 호출이 모두 정상인지 확인합니다.
2. EC2 변환 서버(`mesh_convert_service.py`) 주소가 스캐너 환경 변수 `CONVERT_SERVER_URL`에 등록돼 있어야 하며, 응답 JSON의 `glb_url`은 앱이 접근 가능한 HTTPS/CORS 정책을 만족해야 합니다.
3. 회사 외부 네트워크에서도 접근하려면 방화벽/보안 그룹에서 8000, 8080 포트를 허용합니다.

## 7. 검증 시나리오
1. 앱 실행 → 홈 탭 이미지/텍스트 확인.
2. 발 측정 탭 진입 → 기본 GLB(`assets/glbs/test.glb`) 렌더링 확인.
3. “족형 측정 시작” → 스캐너 서버에 POST, 버튼 비활성화, 상태 메시지 업데이트.
4. `/jobs/{id}` 폴링 → `state`가 `succeeded`일 때 `glb_url` 수신 후 뷰어 전환.
5. 스캐너 오프라인/GLB 누락 등 실패 케이스에 대한 사용자 메시지도 확인합니다.
