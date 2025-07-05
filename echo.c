#include "csapp.h"

// 클라이언트와 연결된 소켓에서 메시지를 읽고 그대로 다시 보내는 에코 함수
void echo(int connfd) {
    size_t n;                    // 읽은 바이트 수
    char buf[MAXLINE];           // 클라이언트 메시지를 저장할 버퍼
    rio_t rio;                   // 안정적인 I/O를 위한 RIO 구조체

    // RIO 구조체를 소켓 디스크립터와 연결하여 초기화
    Rio_readinitb(&rio, connfd);
    
    // 클라이언트로부터 한 줄씩 읽어서 처리 (EOF까지 반복)
    while ((n=Rio_readlineb(&rio, buf, MAXLINE)) != 0)
    {   
        // 받은 바이트 수를 서버 콘솔에 출력
        printf("server received %d bytes\n", (int)n);
        // 받은 메시지를 클라이언트에게 그대로 전송 (에코)
        Rio_writen(connfd, buf, n);
    }
}