#include "message.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int is_club_leader(MYSQL *conn, const char *logged_id) {
  char query[256];
  sprintf(query, "SELECT club_id FROM clubs WHERE leader_id = '%s'", logged_id);
  mysql_query(conn, query);
  MYSQL_RES *res = mysql_store_result(conn);
  int is_leader = (mysql_num_rows(res) > 0);
  mysql_free_result(res);
  return is_leader;
}

void send_system_message(MYSQL *conn, const char *receiver_id, const char *title, const char *content) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  char sent_at[50];
  strftime(sent_at, sizeof(sent_at), "%Y-%m-%d %H:%M:%S", t);

  char query[1024];
  sprintf(query, "INSERT INTO messages (sender_id, receiver_id, title, content, sent_at) VALUES ('SYSTEM', '%s', '%s', '%s', '%s')",
          receiver_id, title, content, sent_at);
  mysql_query(conn, query);
}

void message_box_menu(MYSQL *conn, const char *logged_id) {
  int choice;
  while (1) {
    int leader = is_club_leader(conn, logged_id);
    printf("\n============================\n");
    printf("  메시지함 (%s)\n", logged_id);
    printf("============================\n");
    printf("1. 받은 메시지함\n");
    printf("2. 보낸 메시지함\n");
    printf("3. 메시지 보내기\n");
    if (leader) {
      printf("4. 동아리 가입 신청 관리 (동아리장 전용)\n");
    }
    printf("0. 뒤로\n");
    printf("============================\n");
    printf("입력: ");
    scanf("%d", &choice);

    switch (choice) {
      case 1: view_received_messages(conn, logged_id); break;
      case 2: view_sent_messages(conn, logged_id); break;
      case 3: send_message(conn, logged_id); break;
      case 4: 
        if (leader) manage_join_requests(conn, logged_id);
        else printf("잘못된 입력입니다.\n");
        break;
      case 0: return;
      default: printf("잘못된 입력입니다.\n");
    }
  }
}

void view_received_messages(MYSQL *conn, const char *logged_id) {
  while (1) {
    char query[512];
    sprintf(query, "SELECT msg_id, sender_id, title, sent_at, is_read FROM messages WHERE receiver_id = '%s' ORDER BY msg_id DESC", logged_id);
    if (mysql_query(conn, query)) {
      printf("메시지 로딩 실패: %s\n", mysql_error(conn));
      return;
    }

    MYSQL_RES *res = mysql_store_result(conn);
    printf("\n=== 받은 메시지함 ===\n");
    if (mysql_num_rows(res) == 0) {
      printf("도착한 메시지가 없습니다.\n");
      mysql_free_result(res);
      return;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
      const char *read_status = (strcmp(row[4], "1") == 0) ? "읽음" : "안읽음";
      printf("[%s] ID: %s | 보낸사람: %s | 제목: %s | 날짜: %s\n", read_status, row[0], row[1], row[2], row[3]);
    }
    mysql_free_result(res);

    int msg_id;
    printf("\n읽을 메시지 ID 선택 (0: 뒤로): ");
    scanf("%d", &msg_id);
    if (msg_id == 0) return;

    // View message details
    sprintf(query, "SELECT sender_id, title, content, sent_at FROM messages WHERE msg_id = %d AND receiver_id = '%s'", msg_id, logged_id);
    mysql_query(conn, query);
    res = mysql_store_result(conn);
    if (mysql_num_rows(res) > 0) {
      row = mysql_fetch_row(res);
      printf("\n--- 메시지 상세 ---\n");
      printf("보낸사람: %s\n", row[0]);
      printf("날짜: %s\n", row[3]);
      printf("제목: %s\n", row[1]);
      printf("내용: %s\n", row[2]);
      printf("-------------------\n");

      // Mark as read
      char update_query[128];
      sprintf(update_query, "UPDATE messages SET is_read = TRUE WHERE msg_id = %d", msg_id);
      mysql_query(conn, update_query);

      char sender[50];
      strcpy(sender, row[0]);
      mysql_free_result(res);

      int action;
      printf("1. 답장하기  2. 삭제하기  0. 목록으로\n입력: ");
      scanf("%d", &action);
      if (action == 1) {
        if (strcmp(sender, "SYSTEM") == 0) {
          printf("시스템 메시지에는 답장할 수 없습니다.\n");
        } else {
          char reply_title[100], reply_content[500];
          printf("제목: ");
          while (getchar() != '\n');
          fgets(reply_title, sizeof(reply_title), stdin);
          reply_title[strcspn(reply_title, "\n")] = '\0';
          printf("내용: ");
          fgets(reply_content, sizeof(reply_content), stdin);
          reply_content[strcspn(reply_content, "\n")] = '\0';
          
          time_t now = time(NULL);
          struct tm *t = localtime(&now);
          char sent_at[50];
          strftime(sent_at, sizeof(sent_at), "%Y-%m-%d %H:%M:%S", t);

          char insert_query[1024];
          sprintf(insert_query, "INSERT INTO messages (sender_id, receiver_id, title, content, sent_at) VALUES ('%s', '%s', '%s', '%s', '%s')",
                  logged_id, sender, reply_title, reply_content, sent_at);
          if (mysql_query(conn, insert_query)) {
            printf("답장 실패: %s\n", mysql_error(conn));
          } else {
            printf("답장을 보냈습니다!\n");
          }
        }
      } else if (action == 2) {
        char del_query[128];
        sprintf(del_query, "DELETE FROM messages WHERE msg_id = %d", msg_id);
        if (mysql_query(conn, del_query)) {
          printf("삭제 실패: %s\n", mysql_error(conn));
        } else {
          printf("메시지가 삭제되었습니다.\n");
        }
      }
    } else {
      printf("존재하지 않거나 권한이 없는 메시지입니다.\n");
      mysql_free_result(res);
    }
  }
}

void view_sent_messages(MYSQL *conn, const char *logged_id) {
  while (1) {
    char query[512];
    sprintf(query, "SELECT msg_id, receiver_id, title, sent_at, is_read FROM messages WHERE sender_id = '%s' ORDER BY msg_id DESC", logged_id);
    if (mysql_query(conn, query)) {
      printf("메시지 로딩 실패: %s\n", mysql_error(conn));
      return;
    }

    MYSQL_RES *res = mysql_store_result(conn);
    printf("\n=== 보낸 메시지함 ===\n");
    if (mysql_num_rows(res) == 0) {
      printf("보낸 메시지가 없습니다.\n");
      mysql_free_result(res);
      return;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
      const char *read_status = (strcmp(row[4], "1") == 0) ? "읽음" : "안읽음";
      printf("[%s] ID: %s | 받는사람: %s | 제목: %s | 날짜: %s\n", read_status, row[0], row[1], row[2], row[3]);
    }
    mysql_free_result(res);

    int msg_id;
    printf("\n확인할 메시지 ID 선택 (0: 뒤로): ");
    scanf("%d", &msg_id);
    if (msg_id == 0) return;

    sprintf(query, "SELECT receiver_id, title, content, sent_at FROM messages WHERE msg_id = %d AND sender_id = '%s'", msg_id, logged_id);
    mysql_query(conn, query);
    res = mysql_store_result(conn);
    if (mysql_num_rows(res) > 0) {
      row = mysql_fetch_row(res);
      printf("\n--- 메시지 상세 ---\n");
      printf("받는사람: %s\n", row[0]);
      printf("날짜: %s\n", row[3]);
      printf("제목: %s\n", row[1]);
      printf("내용: %s\n", row[2]);
      printf("-------------------\n");
      mysql_free_result(res);
      
      int action;
      printf("1. 삭제하기  0. 목록으로\n입력: ");
      scanf("%d", &action);
      if (action == 1) {
        char del_query[128];
        sprintf(del_query, "DELETE FROM messages WHERE msg_id = %d", msg_id);
        if (mysql_query(conn, del_query)) {
          printf("삭제 실패: %s\n", mysql_error(conn));
        } else {
          printf("메시지가 삭제되었습니다.\n");
        }
      }
    } else {
      printf("존재하지 않거나 권한이 없는 메시지입니다.\n");
      mysql_free_result(res);
    }
  }
}

void send_message(MYSQL *conn, const char *sender_id) {
  char receiver_id[50];
  char title[100];
  char content[500];

  printf("\n=== 메시지 보내기 ===\n");
  printf("받는 사람 ID: ");
  scanf("%s", receiver_id);

  // Check if receiver exists
  char check_query[128];
  sprintf(check_query, "SELECT id FROM users WHERE id = '%s'", receiver_id);
  mysql_query(conn, check_query);
  MYSQL_RES *res = mysql_store_result(conn);
  if (mysql_num_rows(res) == 0) {
    printf("존재하지 않는 사용자입니다.\n");
    mysql_free_result(res);
    return;
  }
  mysql_free_result(res);

  printf("제목: ");
  while (getchar() != '\n');
  fgets(title, sizeof(title), stdin);
  title[strcspn(title, "\n")] = '\0';
  
  printf("내용: ");
  fgets(content, sizeof(content), stdin);
  content[strcspn(content, "\n")] = '\0';

  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  char sent_at[50];
  strftime(sent_at, sizeof(sent_at), "%Y-%m-%d %H:%M:%S", t);

  char query[1024];
  sprintf(query, "INSERT INTO messages (sender_id, receiver_id, title, content, sent_at) VALUES ('%s', '%s', '%s', '%s', '%s')",
          sender_id, receiver_id, title, content, sent_at);
  if (mysql_query(conn, query)) {
    printf("메시지 전송 실패: %s\n", mysql_error(conn));
  } else {
    printf("메시지가 성공적으로 전송되었습니다!\n");
  }
}

void manage_join_requests(MYSQL *conn, const char *logged_id) {
  while (1) {
    char query[512];
    sprintf(query, 
            "SELECT jr.request_id, jr.user_id, c.club_name, jr.apply_date, jr.status "
            "FROM join_requests jr "
            "JOIN clubs c ON jr.club_id = c.club_id "
            "WHERE c.leader_id = '%s' AND jr.status = '대기'", logged_id);
    if (mysql_query(conn, query)) {
      printf("가입 신청 로딩 실패: %s\n", mysql_error(conn));
      return;
    }

    MYSQL_RES *res = mysql_store_result(conn);
    printf("\n=== 동아리 가입 신청 관리 ===\n");
    if (mysql_num_rows(res) == 0) {
      printf("현재 대기 중인 가입 신청이 없습니다.\n");
      mysql_free_result(res);
      return;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
      printf("신청ID: %s | 신청자: %s | 동아리: %s | 날짜: %s\n", row[0], row[1], row[2], row[3]);
    }
    mysql_free_result(res);

    int req_id;
    printf("\n처리할 신청 ID 선택 (0: 뒤로): ");
    scanf("%d", &req_id);
    if (req_id == 0) return;

    // Verify ownership and get user_id, club_name
    sprintf(query, 
            "SELECT jr.user_id, c.club_name FROM join_requests jr "
            "JOIN clubs c ON jr.club_id = c.club_id "
            "WHERE jr.request_id = %d AND c.leader_id = '%s'", req_id, logged_id);
    mysql_query(conn, query);
    res = mysql_store_result(conn);
    if (mysql_num_rows(res) == 0) {
      printf("유효하지 않은 신청 ID입니다.\n");
      mysql_free_result(res);
      continue;
    }
    row = mysql_fetch_row(res);
    char applicant_id[50];
    char club_name[100];
    strcpy(applicant_id, row[0]);
    strcpy(club_name, row[1]);
    mysql_free_result(res);

    int action;
    printf("1. 승인  2. 거절  0. 취소\n입력: ");
    scanf("%d", &action);
    if (action == 1) {
      sprintf(query, "UPDATE join_requests SET status = '승인' WHERE request_id = %d", req_id);
      mysql_query(conn, query);
      
      char title[100], content[500];
      sprintf(title, "[시스템] '%s' 동아리 가입 승인", club_name);
      sprintf(content, "축하합니다! '%s' 동아리 가입 신청이 승인되었습니다.", club_name);
      send_system_message(conn, applicant_id, title, content);
      printf("가입을 승인했습니다.\n");
    } else if (action == 2) {
      sprintf(query, "UPDATE join_requests SET status = '거절' WHERE request_id = %d", req_id);
      mysql_query(conn, query);
      
      char title[100], content[500];
      sprintf(title, "[시스템] '%s' 동아리 가입 거절", club_name);
      sprintf(content, "아쉽게도 '%s' 동아리 가입 신청이 거절되었습니다.", club_name);
      send_system_message(conn, applicant_id, title, content);
      printf("가입을 거절했습니다.\n");
    }
  }
}
