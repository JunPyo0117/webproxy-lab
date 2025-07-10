#include <stdio.h>
#include "csapp.h"

void doit(int fd);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *hostname, char *port, char *path);
void forward_request(int serverfd, char *method, char *path, char *version, rio_t *client_rio, char *hostname, char *port);
void forward_response(int clientfd, int serverfd);

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

int main(int argc, char **argv) {
  int listenfd, connfd;
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  // 명령행 인수 검증
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]); // 지정된 포트에서 클라이언트 연결을 기다리는 리슨 소켓 생성

  while (1) {
    clientlen = sizeof(clientaddr); // 클라이언트 주소 구조체 크기 설정
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // 클라이언트 연결 수락

    doit(connfd);   // HTTP 요청 처리 (웹서버 핵심 로직)
    Close(connfd);  // 클라이언트와의 연결 종료
    printf("connfd closed\n");
  }
}

void doit(int fd) {
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char hostname[MAXLINE], port[MAXLINE], path[MAXLINE];
  rio_t rio;
  int serverfd;

  // TODO 1: RIO 구조체 초기화
  Rio_readinitb(&rio, fd);
  
  // TODO 2: HTTP 요청 라인 읽기
  if (!(Rio_readlineb(&rio, buf, MAXLINE))) {
    return;
  }
  
  // TODO 3: 요청 라인 파싱 (method, uri, version)
  sscanf(buf, "%s %s %s", method, uri, version);
  
  // TODO 4: GET/HEAD 메소드만 허용
  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD")) {
    clienterror(fd, method, "501", "Not implemented", "Proxy does not implement this method");
    return;
  }
  
  // TODO 5: HTTP 헤더들 읽기 (read_requesthdrs 함수 호출)
  read_requesthdrs(&rio);
  
  // TODO 6: URI 파싱 (parse_uri 함수 호출)
  if (parse_uri(uri, hostname, port, path) < 0) {
    clienterror(fd, uri, "400", "Bad Request", "Invalid URI format");
    return;
  }
  
  // TODO 7: 원본 서버에 연결
  serverfd = Open_clientfd(hostname, port);
  if (serverfd < 0) {
    clienterror(fd, hostname, "502", "Bad Gateway", "Could not connect to origin server");
    return;
  }
  
  // TODO 8: 원본 서버로 요청 전달
  forward_request(serverfd, method, path, version, &rio, hostname, port);
  
  // TODO 9: 원본 서버 응답을 클라이언트로 전달
  forward_response(fd, serverfd);
  
  // TODO 10: 서버 연결 종료
  Close(serverfd);
}

int parse_uri(char *uri, char *hostname, char *port, char *path)
{
  // http://localhost:8000
  char *ptr=uri;

  // TODO 1 http:// 분리
  if (strncmp(uri, "http://", 7) == 0) {
    ptr += 7;
  }

  // TODO 3 /기본 경로 home.html 
  char *path_ptr = strchr(ptr, '/');  // 경로 위치 찾기
  if (path_ptr) {
    strcpy(path, path_ptr);         // 경로만 복사
    *path_ptr = '\0';               // 호스트명과 분리
  } else {
    strcpy(path, "/");              // 기본 경로
  }

  // TODO 2 :8000 분리 포트가 없으면 기본 80
  char *port_ptr = strchr(ptr, ':');
  if (port_ptr) {
    strcpy(port, port_ptr + 1);
    *port_ptr = '\0';
  } else {
    strcpy(port, "80");
  }

  strcpy(hostname, ptr); // 추가: hostname 복사

  printf("URI: %s\n", uri);
  printf("Parsed: hostname=%s, port=%s, path=%s\n", hostname, port, path);

  return 0;
}

void forward_request(int serverfd, char *method, char *path, char *version, rio_t *rio, char *hostname, char *port){
  char buf[MAXLINE];
  int n;

  // TODO 1: 요청 라인 생성 및 전송
  sprintf(buf, "%s %s HTTP/%s\r\n", method, path, version);
  Rio_writen(serverfd, buf, strlen(buf));

  // TODO 2: Host 헤더 생성 및 전송
  sprintf(buf, "Host: %s:%s\r\n", hostname, port);
  Rio_writen(serverfd, buf, strlen(buf));

  // TODO 3: Connection 헤더 생성 및 전송
  sprintf(buf, "Connection: close\r\n");
  Rio_writen(serverfd, buf, strlen(buf));

  // TODO 4: User-Agent 헤더 전송
  Rio_writen(serverfd, user_agent_hdr, strlen(user_agent_hdr));

  // TODO 5 헤더 끝:
  Rio_writen(serverfd, "\r\n", 2);

}

void forward_response(int clientfd, int serverfd){
  rio_t rio;
  char buf[MAXLINE];
  int n;               // 읽은 바이트 수 저장용
  
  Rio_readinitb(&rio, serverfd);
  
  while ((n = Rio_readnb(&rio, buf, MAXLINE)) > 0)
  {
    Rio_writen(clientfd, buf, n);
  }
}

// 기존 tiny.c 함수
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
  char buf[MAXLINE], body[MAXBUF];  // HTTP 헤더용 버퍼, HTML 바디용 버퍼

  // 1단계: HTML 에러 페이지 생성
  sprintf(body, "<html><title>Tiny Error</title>");                         // HTML 제목
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);                   // 흰색 배경의 바디 시작
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);                   // 에러 번호와 간단한 메시지 (예: 404: Not found)
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);                  // 상세 메시지와 원인 (예: Tiny couldn't find this file: /nonexistent.html)
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);           // 서버 정보 푸터
  sprintf(body, "%s</body></html>\r\n", body);

  // 2단계: HTTP 응답 헤더 생성 및 전송
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);                    // 상태 라인 (예: HTTP/1.0 404 Not found)
  Rio_writen(fd, buf, strlen(buf));                                         // 상태 라인 전송
  sprintf(buf, "Content-type: text/html\r\n");                             // 콘텐츠 타입 헤더
  Rio_writen(fd, buf, strlen(buf));                                         // 콘텐츠 타입 전송
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));           // 콘텐츠 길이 헤더 + 빈 줄
  Rio_writen(fd, buf, strlen(buf));                                         // 콘텐츠 길이 전송
  
  // 3단계: HTML 에러 페이지 바디 전송
  Rio_writen(fd, body, strlen(body));                                       // 에러 페이지 전송
}

// 기존 tiny.c 함수
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];  // 헤더 라인을 저장할 버퍼

  // 첫 번째 헤더 라인 읽기
  Rio_readlineb(rp, buf, MAXLINE);
  
  // 빈 줄("\r\n")이 나올 때까지 헤더들을 계속 읽기
  // HTTP에서 헤더의 끝은 빈 줄로 표시됨
  while(strcmp(buf, "\r\n")) {           // 빈 줄이 아니면 계속 반복
    Rio_readlineb(rp, buf, MAXLINE);     // 다음 헤더 라인 읽기
    printf("%s", buf);                   // 서버 콘솔에 헤더 내용 출력 (디버깅용)
  }
  return;  // 모든 헤더 읽기 완료
}