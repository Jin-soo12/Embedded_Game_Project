# Embedded_Game_Project

---

## 요약
- **인터럽트, PWM, 타이머** 등의 **임베디드 하드웨어 기술** 기반 BGM, 물리 엔진의 구현으로 게임을 제작하는 것이 목표인 프로젝트입니다.
---
## 목차
- [개요](#개요)
- [상세 설계](#상세-설계)
- [구현 결과](#구현-결과)
- [결론 및 고찰](#결론-및-고찰)

---

## 개요
### GPIO
<img width="497" height="145" alt="image" src="https://github.com/user-attachments/assets/7626ea52-d005-47f3-a837-e9ed8d2041b8" />

- MCU의 외부와 신호를 주고받기 위한 범용 입출력 포트. 
- 각 핀을 입력 모드로 설정하여 센서나 스위치의 신호를 수신하거나, 출력 모드로 설정하여 LED, 릴레이 등 외부 장치를 제어. 
- 레지스터를 통한 핀 제어 및 상태 읽기를 수행하며, 인터럽트 기능과 결합해 효율적인 신호 감지를 구현.

### 인터럽트(Interrupt)
- MCU에서 외부 입력이나 내부 이벤트 발생 시 CPU의 현재 작업을 일시 중단하고 즉시 처리하도록 하는 기능.
- 버튼 입력, 센서 트리거, 통신 데이터 수신 등 예측 불가능한 이벤트 대응에 사용.
- 실시간 제어 시스템 구현을 위한 핵심 요소.

### 타이머
- MCU 내부 클록을 기반으로 일정 주기로 카운트를 수행하는 하드웨어 모듈.
- 시간 측정, 주기 신호 생성, 지연 제어 등의 기능 수행.
- 오버플로 발생 시 인터럽트 발생을 통해 주기적 이벤트 자동 처리.
- 주기적 데이터 수집, 통신 동기화, 시스템 주기 관리 등에서 활용.

### PWM
- 타이머 기반 일정 주파수의 디지털 신호 생성 및 듀티 사이클(Duty Cycle) 조절을 통한 평균 전압 제어 방식.
- MCU의 디지털 신호를 이용한 아날로그 제어 효과 구현.
- 모터 속도 제어, LED 밝기 조절, 서보 모터 위치 제어 등에 활용.
- 이 프로젝트에서는 부저의 주파수를 조절시켜 BGM을 제작하는데 사용.

---

## 상세 설계
### 주요 함수 설명

<img width="623" height="271" alt="image" src="https://github.com/user-attachments/assets/10f8395f-6f7f-4fc2-85a1-f8f3925cb013" />

### 물리 엔진 ASM

<img width="623" height="274" alt="image" src="https://github.com/user-attachments/assets/5f9fd5be-91c2-4723-be6b-50b5e244b8bc" />

### 물리 엔진 상세 코드

**중력 구현**

<img width="359" height="275" alt="image" src="https://github.com/user-attachments/assets/40683a65-89f6-4c02-8525-78b99042ebcc" />

**충돌 구현**

<img width="509" height="235" alt="image" src="https://github.com/user-attachments/assets/0f6f91fb-3d4a-47a5-897e-0f5503e46b17" />

**플레이어 추적 탄 구현**

<img width="623" height="248" alt="image" src="https://github.com/user-attachments/assets/dee99db1-6d51-4962-be55-5cdbcc20fdfb" />

### 보스 구현

<img width="303" height="311" alt="image" src="https://github.com/user-attachments/assets/4d1f9801-b5a7-4e23-958e-5aec23a05bfa" />

- 구조체의 선언을 통해 보스의 체력, 속도, 패턴 등의 스펙을 설정.

### BGM(Back Ground Music) 구현

<img width="521" height="241" alt="image" src="https://github.com/user-attachments/assets/8117cbbd-001d-4492-9757-9b9e68c18342" />

- **PWM**의 주파수를 사전에 설정하여 부저의 음높이와 길이를 제어.

---

## 구현 결과

<img width="272" height="235" alt="image" src="https://github.com/user-attachments/assets/8db62a66-d376-4fd1-bcfd-28060e930934" />

<img width="253" height="241" alt="image" src="https://github.com/user-attachments/assets/ee8c309d-29a0-4a4f-8762-bc30d36862eb" />

---

## 결론 및 고찰
### 결론
- 캐릭터의 움직임 및 물리 엔진(중력, 부딪침)을 성공적으로 구현.
- 다양한 맵과 총 발사를 성공적으로 구현.
- 보스 몬스터의 패턴 및 스펙 설정 구현.

### 고찰
- 제어를 위한 **데이터 시트 및 Schemetic의 분석**을 통해 자료 분석력이 향상되었음.
- **스위치, GPIO, 인터럽트, PWM 등 임베디드 제어**를 게임 구현을 통해 기술 역량을 향상시킬 수 있었음.
- 사전에 학습했던 물리 공식들을 C언어로 코딩하고 실제로 동작시키는 부분에서 재미를 느꼈음.
- main 함수 안에 모든 기능을 다 구현하여 코드 가독성이 떨어지고 수정이 어려웠음. 다음 설계부터는 **기능별로 따로 설계**하고 그 파일들을 import 하는 방식으로 체계적인 설계를 진행할 것임.
