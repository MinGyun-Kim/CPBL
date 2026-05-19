#include "auth.h"
#include "board.h"
#include "db.h"
#include "mypage.h"
#include "message.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void run_admin_interface(MYSQL *conn);
void view_clubs_by_category(MYSQL *conn, const char *logged_id);

// ─────────────────────────────────────────────
// 동아리장 신청 (스텁)
// ─────────────────────────────────────────────
void apply_club_leader(MYSQL *conn, const char *logged_id) {
  printf("\n=== 동아리장 및 동아리 신청 ===\n");

  char club_name[100];
  int category_id;
  char purpose[300];

  printf("동아리 이름: ");
  scanf("%s", club_name);

  // 카테고리 로딩
  printf("\n[카테고리 목록]\n");
  if (mysql_query(conn,
                  "SELECT category_id, category_name FROM club_categories")) {
    printf("카테고리 로딩 실패: %s\n", mysql_error(conn));
    return;
  }
  MYSQL_RES *res = mysql_store_result(conn);
  MYSQL_ROW row;
  while ((row = mysql_fetch_row(res))) {
    printf("%s. %s\n", row[0], row[1]);
  }
  mysql_free_result(res);

  printf("카테고리 번호 선택: ");
  scanf("%d", &category_id);

  printf("동아리의 목적 (300자 이내): ");
  getchar(); // 버퍼 비우기 (개행 문자 제거)
  fgets(purpose, sizeof(purpose), stdin);
  purpose[strcspn(purpose, "\n")] = 0; // 끝에 붙은 개행 문자 제거

  // 신청 일자 생성 (<time.h> 사용)
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  char apply_date[50];
  strftime(apply_date, sizeof(apply_date), "%Y-%m-%d %H:%M:%S", t);

  // DB에 삽입 (status는 기본값 '대기'로 저장)
  char query[1024];
  sprintf(query,
          "INSERT INTO clubs (club_name, leader_id, category_id, purpose, "
          "status, apply_date) "
          "VALUES ('%s', '%s', %d, '%s', '대기', '%s')",
          club_name, logged_id, category_id, purpose, apply_date);

  if (mysql_query(conn, query)) {
    printf("신청에 실패했습니다: %s\n", mysql_error(conn));
  } else {
    char msg_query[1024];
    sprintf(msg_query, "INSERT INTO messages (sender_id, receiver_id, title, content, sent_at) VALUES ('SYSTEM', '%s', '[시스템] 동아리장 신청 접수 완료', '신청하신 동아리장 신청이 성공적으로 접수되었습니다. 현재 상태는 [대기]입니다.', '%s')",
            logged_id, apply_date);
    mysql_query(conn, msg_query);

    printf("\n동아리장 및 동아리 신청이 성공적으로 접수되었습니다!\n");
    printf("- 리더 ID: %s (자동 기입됨)\n", logged_id);
    printf("- 신청 일자: %s\n", apply_date);
    printf("- 승인 상태: 대기중 (관리자 승인 대기)\n");
    printf("※ 신청 결과 알림이 메시지함으로 전송되었습니다.\n");
  }
}

// ─────────────────────────────────────────────
// 카테고리별 동아리 목록 보기
// ─────────────────────────────────────────────
void view_clubs_by_category(MYSQL *conn, const char *logged_id) {
  printf("\n=== 동아리 카테고리 목록 ===\n");
  if (mysql_query(conn,
                  "SELECT category_id, category_name FROM club_categories")) {
    printf("카테고리 로딩 실패: %s\n", mysql_error(conn));
    return;
  }
  MYSQL_RES *res = mysql_store_result(conn);
  MYSQL_ROW row;
  while ((row = mysql_fetch_row(res))) {
    printf("%s. %s\n", row[0], row[1]);
  }
  mysql_free_result(res);

  int category_id;
  printf("\n조회할 카테고리 번호 선택 (0. 뒤로가기): ");
  scanf("%d", &category_id);

  if (category_id == 0)
    return;

  char query[512];
  sprintf(query, 
          "SELECT club_id, club_name, leader_id, purpose FROM clubs "
          "WHERE category_id = %d AND status = '승인'", 
          category_id);

  if (mysql_query(conn, query)) {
    printf("동아리 목록 로딩 실패: %s\n", mysql_error(conn));
    return;
  }

  MYSQL_RES *club_res = mysql_store_result(conn);
  printf("\n=== 해당 카테고리의 동아리 목록 ===\n");
  if (mysql_num_rows(club_res) == 0) {
    printf("현재 이 카테고리에 승인된 동아리가 없습니다.\n");
    mysql_free_result(club_res);
    return;
  }
  
  MYSQL_ROW club_row;
  while ((club_row = mysql_fetch_row(club_res))) {
    printf("[%s] 동아리명: %s (리더: %s)\n", club_row[0], club_row[1], club_row[2]);
    printf("  목적: %s\n\n", club_row[3]);
  }
  mysql_free_result(club_res);

  int join_club_id;
  printf("가입 신청할 동아리의 ID 번호를 입력하세요 (0: 뒤로가기): ");
  scanf("%d", &join_club_id);
  if (join_club_id > 0) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char apply_date[50];
    strftime(apply_date, sizeof(apply_date), "%Y-%m-%d %H:%M:%S", t);

    char join_query[512];
    sprintf(join_query, "INSERT INTO join_requests (club_id, user_id, status, apply_date) VALUES (%d, '%s', '대기', '%s')", join_club_id, logged_id, apply_date);
    if (mysql_query(conn, join_query)) {
      printf("가입 신청 실패: %s\n", mysql_error(conn));
    } else {
      printf("가입 신청이 완료되었습니다! (동아리장의 승인을 기다려주세요)\n");
    }
  }
}

// ─────────────────────────────────────────────
// 메인 화면 (로그인 후 진입)
// ─────────────────────────────────────────────
void home_screen(MYSQL *conn, const char *logged_id) {
  int choice;

  while (1) {
    printf("\n============================\n");
    printf("  메인 메뉴\n");
    printf("============================\n");
    printf("1. 동아리 목록 (카테고리별)\n");
    printf("2. 전공 동아리 게시판\n");
    printf("3. 마이페이지\n");
    printf("4. 동아리장 및 동아리 신청\n");
    printf("5. 메시지함\n");
    printf("0. 로그아웃\n");
    printf("============================\n");
    printf("입력: ");
    scanf("%d", &choice);

    switch (choice) {
    case 1:
      view_clubs_by_category(conn, logged_id);
      break;
    case 2:
      major_club_board(conn);
      break;
    case 3:
      my_page(conn, logged_id);
      break;
    case 4:
      apply_club_leader(conn, logged_id);
      break;
    case 5:
      message_box_menu(conn, logged_id);
      break;
    case 0:
      printf("로그아웃 합니다.\n");
      return;
    default:
      printf("잘못된 입력입니다.\n");
    }
  }
}

// ─────────────────────────────────────────────
// main
// ─────────────────────────────────────────────
int main() {
  MYSQL *conn;
  init_db(&conn);

  int choice;
  char logged_id[50];

  while (1) {
    printf("\n=== 시작 화면 ===\n");
    printf("1. 로그인\n");
    printf("2. 회원가입\n");
    printf("0. 종료\n");
    printf("입력: ");
    scanf("%d", &choice);

    if (choice == 1) {
      int login_status = login_screen(conn, logged_id);
      if (login_status == 1) {
        home_screen(conn, logged_id);
      } else if (login_status == 2) {
        run_admin_interface(conn);
      }
    } else if (choice == 2) {
      register_screen(conn);
    } else if (choice == 0) {
      printf("프로그램을 종료합니다.\n");
      break;
    } else {
      printf("잘못된 입력입니다.\n");
    }
  }

  close_db(conn);
  return 0;
}
