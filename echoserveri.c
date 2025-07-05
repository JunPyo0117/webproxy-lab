#include "csapp.h"

// 클라이언트로부터 받은 메시지를 그대로 돌려보내는 에코 함수 (선언)
void echo(int connfd);

// 에코 서버 메인 함수
int main(int argc, char **argv) {
    int listenfd, connfd;                   // 리슨 소켓, 연결된 클라이언트 소켓
    socklen_t clientlen;                    // 클라이언트 주소 구조체 크기
    struct sockaddr_storage clientaddr;    // 클라이언트 주소 정보 저장
    char client_hostname[MAXLINE], client_port[MAXLINE]; // 클라이언트 호스트명, 포트번호

    // 명령행 인수 개수 확인 (프로그램명 + 포트 = 2개)
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    // 지정된 포트에서 클라이언트 연결을 기다리는 리슨 소켓 생성
    listenfd = Open_listenfd(argv[1]);
    
    // 무한 루프로 클라이언트 연결 처리
    while (1)
    {
        clientlen = sizeof(struct sockaddr_storage); // 클라이언트 주소 구조체 크기 설정
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // 클라이언트 연결 요청 수락
        Getnameinfo((SA *)&clientaddr, clientlen,client_hostname , MAXLINE, client_port, MAXLINE, 0); // 클라이언트의 호스트명과 포트번호 추출
        printf("Connected to (%s, %s)\n", client_hostname, client_port); // 연결된 클라이언트 정보 출력
        echo(connfd); // 에코 서비스 제공
        Close(connfd); // 클라이언트와의 연결 종료
    }
    exit(0);
}