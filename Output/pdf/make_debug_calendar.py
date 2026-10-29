from reportlab.pdfgen import canvas
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib.pagesizes import A3,landscape
from reportlab.lib.colors import HexColor,white
from reportlab.platypus import Paragraph
from reportlab.lib.styles import ParagraphStyle
import calendar
pdfmetrics.registerFont(TTFont('Malgun',r'C:\Windows\Fonts\malgun.ttf'));pdfmetrics.registerFont(TTFont('MalgunBold',r'C:\Windows\Fonts\malgunbd.ttf'))
out=r'C:\Users\dnwls\Desktop\WinapiEngine-main\output\pdf\Virus_Striker_독립검증_일별달력.pdf'
# Each entry: individual deliverable and same-day independent check.
data={
'11/01':('소켓 초기화 / 생성 결과 확인','메시지 필드 정의 / 예시 값 작성','접속 UI / 가짜 입력으로 확인'),
'11/02':('접속 대기·수락 / 테스트 연결','인코딩 / 예시 메시지 변환','접속 요청 / 가짜 성공 응답 확인'),
'11/03':('수신 버퍼 / 분할 입력 확인','디코딩 / 변환 전후 값 비교','플레이어 번호 / 가짜 승인 적용'),
'11/04':('부분 송수신 / 분할 데이터 시험','길이·종류 검사 / 오류 데이터 시험','접속 실패 안내 / 가짜 실패 확인'),
'11/05':('준비 상태 / 가짜 접속자 시험','세션 필드 / 잘못된 번호 검사','준비 버튼 / 가짜 응답 표시'),
'11/06':('UDP 토큰 발급 / 토큰 값 확인','UDP 등록 검사 / 잘못된 토큰 거부','UDP 등록 요청 / 전송 내용 확인'),
'11/07':('공동: 두 PC 접속·UDP 등록','공동: 메시지 해석 결과 비교','공동: 두 PC 준비 화면 확인'),
'11/08':('고정 시간 루프 / 계산 횟수 확인','플레이어 상태 정의 / 예시 값 확인','입력 수집 분리 / 가짜 키 확인'),
'11/09':('입력 수신 큐 / 가짜 입력 처리','입력 검증 / 범위 밖 값 거부','UDP 입력 송신 / 데이터 확인'),
'11/10':('호스트 입력 연결 / 가짜 입력 시험','2인 생성·식별 / 각 슬롯 확인','로컬 ID 구분 / 조작 대상 확인'),
'11/11':('입력 시간 초과 / 중립 전환 시험','위치·체력 추출 / 가짜 상태 비교','상태 적용 / 가짜 위치로 확인'),
'11/12':('연결 확인 / 시간 초과 재현','객체 번호 등록 / 중복 방지 확인','위치 보간 / 두 위치 사이 확인'),
'11/13':('큐 크기 제한 / 상한 초과 시험','발사자·피해 정보 / 값 비교','2인 HUD / 가짜 체력 표시'),
'11/14':('공동: 두 PC 입력 지연 측정','공동: 호스트 발사 간격 확인','공동: 독립 이동·발사 시험'),
'11/15':('생성 메시지 / 가짜 전송 확인','일반 적 행동 분리 / 가짜 틱 시험','적 생성 표시 / 가짜 생성 정보'),
'11/16':('삭제 메시지 / 가짜 삭제 확인','발사자·피해 저장 / 탄환별 비교','탄환 표시 / 가짜 위치 적용'),
'11/17':('상태 선도착 / 순서 바꿔 시험','체력 판정 / 가짜 탄환 피해 시험','피격 표시 / 가짜 피해 결과'),
'11/18':('이벤트 큐 / 메시지 누락 확인','아이템 획득 / 동시 접촉 1회 확인','아이템 삭제 / 가짜 획득 적용'),
'11/19':('중복 이벤트 / 반복 주입 시험','사망·점수 / 중복 판정 방지','팀 점수 표시 / 가짜 점수 적용'),
'11/20':('전투 로그 / 가짜 이벤트 기록','동시 공격 / 두 탄환 피해 확인','아이템 표시 / 가짜 획득 시험'),
'11/21':('공동: 일반 전투 송수신 시험','공동: HP·사망·점수 비교','공동: 동시 아이템 획득 시험'),
'11/22':('상태 크기 계측 / 가짜 객체 입력','보스 FSM / 가짜 체력 전환','보스 표시 / 가짜 상태 적용'),
'11/23':('상태 분할 / 크기 상한 확인','특수 탄환 정보 / 예시 값 확인','특수 탄환 / 가짜 생성 표시'),
'11/24':('연결 끊김 / 시간 초과 재현','개인 사망 / 생존 상태 주입','사망 화면 / 가짜 상태 표시'),
'11/25':('종료 이벤트 / 가짜 이탈 전송','협의한 팀 승패 / 상태 주입','결과 화면 / 가짜 승패 주입'),
'11/26':('로딩 중 이탈 / 가짜 종료 시험','클리어 / 중복 확정 방지 시험','이탈 안내 / 가짜 오류 주입'),
'11/27':('소켓·스레드 종료 / 대기 중 시험','객체 참조 정리 / 삭제 후 검사','타이틀 복귀 / 가짜 종료 확인'),
'11/28':('공동: 보스전 송수신 시험','공동: 보스·팀 승패 비교','공동: 두 PC 전 구간 확인'),
'11/29':('지연 재현 / 테스트 송신기','객체 상태 / 틱 순서 비교','상태 역전 / 가짜 위치 주입'),
'11/30':('UDP 유실 / 테스트 송신기','삭제 후 상태 / 재등장 방지 시험','입력 중단 / 고착 여부 확인'),
'12/01':('세션 초기화 / 재연결 시험','이전 이벤트 제거 / 가짜 회차 시험','재접속 화면 / 가짜 연결 반복'),
'12/02':('송신량·큐 / 가짜 대량 상태 계측','탄막 부하 / 가짜 탄환 수 측정','표시 지연 / 가짜 늦은 상태'),
'12/03':('큐 병합 / 최신 정보 유지 확인','탄환 보정 / 동일 초기값 위치 비교','보간 버퍼 / 가짜 지연 비교'),
'12/04':('단절 정리 / 대기 중 종료 재시험','전투 회귀 / 가짜 동시 충돌','입력·UI / 가짜 상태 재시험'),
'12/05':('공동: 두 PC 유실·부하 측정','공동: 보스 탄막·결과 비교','공동: 재접속·표시 지연 시험'),
'12/06':('통신 기능 정리 / 개인 로그 검토','체크리스트 / 개인 시험 대조','검수 시나리오 / 화면 순서 점검'),
'12/07':('송수신 로그 / 기록 누락 확인','보고서 작성 / 시험 기록 대조','시연 영상 / 촬영 결과 확인'),
'12/08':('소스·리소스 패키지 / 파일 확인','최종 보고서 / 작업 결과 대조','PDF·영상 / 열기 및 내용 확인'),
'12/09':('공동: 소스·리소스 최종 확인','공동: 체크리스트·보고서 확인','공동: 팀장의 제출 여부 확인'),
'12/10':('필요 시 통신 검수 대응','필요 시 판정·동기화 검수 대응','필요 시 조작·화면 검수 대응'),
'12/11':('필요 시 통신 검수 대응','필요 시 판정·동기화 검수 대응','필요 시 조작·화면 검수 대응'),
'12/12':('검수 결과·통신 수정 기록','판정·동기화 이력 정리','검수 화면·자료 정리')}
W,H=landscape(A3); c=canvas.Canvas(out,pagesize=(W,H))
style=ParagraphStyle('cell',fontName='Malgun',fontSize=8.3,leading=11.1,textColor=HexColor('#172B46'),wordWrap='CJK')
for month in (11,12):
 c.setFillColor(HexColor('#142D4A'));c.setFont('MalgunBold',21);c.drawString(30,H-40,f'2026년 {month}월  |  개인별 개발·독립 검증 달력')
 c.setFont('Malgun',10);c.setFillColor(HexColor('#52667A'));c.drawString(30,H-60,'김: 김영록  /  정: 정우진  /  이: 이현범     ·     각 항목: 작업 / 혼자 확인할 방법     ·     공동: 실제 통합 시험')
 x0=30;top=H-82;width=W-60;col=width/7;head=27;weeks=calendar.Calendar(firstweekday=6).monthdayscalendar(2026,month);height=(H-150-head)/len(weeks)
 for j,label in enumerate(['일','월','화','수','목','금','토']):
  x=x0+j*col;c.setFillColor(HexColor('#E8EEF5'));c.rect(x,top-head,col,head,fill=1,stroke=0);c.setFillColor(HexColor('#B23A3A') if j==0 else HexColor('#3569A1') if j==6 else HexColor('#263A50'));c.setFont('MalgunBold',10);c.drawCentredString(x+col/2,top-head+8,label)
 for i,week in enumerate(weeks):
  y=top-head-(i+1)*height
  for j,day in enumerate(week):
   x=x0+j*col;c.setFillColor(white);c.setStrokeColor(HexColor('#ABBAC8'));c.rect(x,y,col,height,fill=1,stroke=1)
   if not day:continue
   c.setFillColor(HexColor('#B23A3A') if j==0 else HexColor('#3569A1') if j==6 else HexColor('#25384D'));c.setFont('MalgunBold',10);c.drawString(x+6,y+height-15,str(day))
   values=data.get(f'{month:02}/{day:02}')
   if not values:continue
   segment=(height-22)/3
   for k,v in enumerate(values):
    yy=y+height-22-(k+1)*segment
    if k:c.setStrokeColor(HexColor('#E1E6EB'));c.line(x+3,yy+segment,x+col-3,yy+segment)
    para=Paragraph('<b>'+['김','정','이'][k]+':</b> '+v,style);_,ph=para.wrap(col-10,segment-2);para.drawOn(c,x+5,yy+segment-ph-2)
 c.setFillColor(HexColor('#53667A'));c.setFont('Malgun',8.5);c.drawString(30,28,'가짜 입력은 상대 기능 대신 미리 준비한 시험 데이터입니다. 공통 메시지 형식과 인터페이스는 먼저 합의합니다.')
 c.drawRightString(W-30,28,'2인 협동 규칙과 실제 최종 제출 요건은 팀·수업 안내 확인');c.showPage()
c.save();print(out)
