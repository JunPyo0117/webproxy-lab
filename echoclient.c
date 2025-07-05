#include "csapp.h"

// 에코 서버에 메시지를 보내고 응답을 받아 출력하는 클라이언트
int main(int argc, char **argv) {
    int clientfd;  // 서버와 연결된 소켓 디스크립터
    char *host, *port, buf[MAXLINE]; // 서버 호스트, 포트, 메시지 버퍼
    rio_t rio; // 서버와의 안정적인 I/O를 위한 RIO 구조체

    // 명령행 인수 개수 확인 (프로그램명 + 호스트 + 포트 = 3개)
    if (argc != 3) {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        exit(0);
    }

    // 명령행 인수에서 호스트와 포트 정보 가져오기
    host = argv[1];
    port = argv[2];

    // 서버와 연결 설정
    clientfd = Open_clientfd(host, port);
    // RIO 구조체를 소켓과 연결하여 초기화
    Rio_readinitb(&rio, clientfd);

    // 사용자 입력을 받아 서버에 전송하고 응답을 받는 메인 루프
    while (Fgets(buf, MAXLINE, stdin) != NULL) {
        Rio_writen(clientfd, buf, strlen(buf)); // 서버에 메시지 전송
        Rio_readlineb(&rio, buf, MAXLINE); // 서버 응답 받기
        Fputs(buf, stdout); // 응답을 화면에 출력
    }
    
    // 연결 종료
    Close(clientfd);
    exit(0);
}