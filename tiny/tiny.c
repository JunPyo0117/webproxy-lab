/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv) {
  int listenfd, connfd;  // 리슨 소켓, 연결 소켓 
  char hostname[MAXLINE], port[MAXLINE]; // 호스트 네임, 포트 번호 
  socklen_t clientlen;  // 클라이언트 주소 구조체 크기
  struct sockaddr_storage clientaddr; // 클라이언트 주소 정보 저장용

  /* Check command line args */
  // 명령행 인수 개수 확인 (프로그램명 + 포트 = 2개) 잘못 입력되면 사용법 출력
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]); // 지정된 포트에서 클라이언트 연결을 기다리는 리슨 소켓 생성

  while (1) {
    clientlen = sizeof(clientaddr); // 클라이언트 주소 구조체 크기 설정
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // line:netp:tiny:accept // 클라이언트 연결 수락
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0); // 클라이언트 주소를 호스트명과 포트번호로 변환

    printf("Accepted connection from (%s, %s)\n", hostname, port);  // 연결된 클라이언트 정보 출력
    doit(connfd);   // line:netp:tiny:doit  // HTTP 요청 처리 (웹서버 핵심 로직)
    Close(connfd);  // line:netp:tiny:close // 클라이언트와의 연결 종료
    printf("connfd closed\n");
  }
}

// HTTP 트랜잭션 처리 - 웹서버의 핵심 로직
void doit(int fd) {
  int is_static;                    // 정적 파일 여부 (1=정적, 0=동적)
  struct stat sbuf;                 // 파일 정보 저장용 구조체
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];  // HTTP 요청 라인 파싱용
  char filename[MAXLINE], cgiargs[MAXLINE];  // 파일명, CGI 인수
  rio_t rio;                        // 안정적인 I/O를 위한 RIO 구조체

  // 1단계: HTTP 요청 라인 읽기
  Rio_readinitb(&rio, fd);          // RIO 구조체 초기화
  // Rio_readlineb(&rio, buf, MAXLINE); // HTTP 요청 라인 읽기 (예: GET /index.html HTTP/1.1)
  if(!(Rio_readlineb(&rio, buf, MAXLINE))){
    return;
	}
  printf("Request headers:\n");
  printf("%s", buf);                // 서버 콘솔에 요청 라인 출력
  sscanf(buf, "%s %s %s", method, uri, version);  // 요청 라인 파싱 (메소드, URI, 버전) Ex) GET /index.html HTTP/1.1
  
  // 2단계: GET 메소드 확인 (Tiny는 GET만 지원)
  // if (strcasecmp(method, "GET")) {
  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD")) {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;  // GET이 아니면 501 에러 응답 후 종료
  }
  
  // 3단계: HTTP 헤더들 읽기 (Host, Connection 등)
  read_requesthdrs(&rio);

  // 4단계: URI 파싱하여 정적/동적 파일 구분
  is_static = parse_uri(uri, filename, cgiargs);  // URI → 파일명, CGI 인수 분리
  
  // 5단계: 요청된 파일 존재 확인
  if (stat(filename, &sbuf) < 0) {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;  // 파일이 없으면 404 에러 응답 후 종료
  }

  // 6단계: 정적 파일 서비스 경로
  if (is_static) {
    // 파일 권한 확인 (일반 파일이고 읽기 가능한지)
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Not found", "Tiny couldn't read this file");
      return;  // 권한 없으면 403 에러 응답 후 종료
    }
    serve_static(fd, filename, sbuf.st_size, method);  // 정적 파일 전송
  }
  // 7단계: 동적 CGI 서비스 경로
  else {
    // CGI 프로그램 실행 권한 확인
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Not found", "Tiny couldn't run CGI Program");
      return;  // 실행 권한 없으면 403 에러 응답 후 종료
    }
    serve_dynamic(fd, filename, cgiargs, method);  // CGI 프로그램 실행
  }
}

// HTTP 에러 응답 생성 및 클라이언트로 전송
// 매개변수: fd(클라이언트 소켓), cause(에러 원인), errnum(에러 번호), shortmsg(간단한 메시지), longmsg(상세 메시지)
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

// HTTP 요청 헤더들을 읽어서 처리 (현재는 단순히 출력만 함)
// 매개변수: rp(RIO 구조체 포인터 - 클라이언트 소켓과 연결됨)
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

// URI 파싱하여 정적/동적 콘텐츠 구분 및 파일명, CGI 인수 추출
// 매개변수: uri(요청 URI), filename(파일 경로 저장), cgiargs(CGI 인수 저장)
// 반환값: 1=정적 콘텐츠, 0=동적 콘텐츠
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;  // '?' 위치를 찾기 위한 포인터

  // 정적 콘텐츠 처리 (URI에 "cgi-bin"이 없으면 정적 파일)
  if (!(uri, "cgi-bin")) {
    strcpy(cgiargs, "");                    // CGI 인수 비우기 (정적 파일은 인수 없음)
    strcpy(filename, ".");                  // 현재 디렉터리부터 시작
    strcat(filename, uri);                  // "./index.html", "./images/logo.png" 등
    if (uri[strlen(uri)-1] == '/')          // URI가 "/"로 끝나면 (루트 디렉터리)
      strcat(filename, "home.html");        // 기본 페이지 "home.html" 추가
    return 1;                               // 정적 콘텐츠 표시
  }
  // 동적 콘텐츠 처리 (URI에 "cgi-bin"이 있으면 CGI 프로그램)
  else {
    ptr = index(uri, '?');                  // '?' 위치 찾기 (CGI 인수의 시작점)
    if (ptr) {                              // '?'가 있으면 (CGI 인수 존재)
      strcpy(cgiargs, ptr+1);               // '?' 다음 부분을 CGI 인수로 복사 ("15&25")
      *ptr = '\0';                          // URI를 '?' 앞까지만 자르기 ("/cgi-bin/adder")
    }
    else                                    // '?'가 없으면 (CGI 인수 없음)
      strcpy(cgiargs, "");                  // CGI 인수 비우기
    strcpy(filename, ".");                  // 현재 디렉터리부터 시작
    strcat(filename, uri);                  // "./cgi-bin/adder" 등
    return 0;                               // 동적 콘텐츠 표시
  }
}

// 정적 파일(HTML, 이미지 등)을 클라이언트에게 전송
// 매개변수: fd(클라이언트 소켓), filename(파일 경로), filesize(파일 크기)
void serve_static(int fd, char *filename, int filesize, char *method)
{
  int srcfd;                              // 파일 디스크립터
  char *srcp, filetype[MAXLINE], buf[MAXBUF];  // 파일 메모리 포인터, MIME 타입, HTTP 응답 버퍼

  // 1단계: HTTP 응답 헤더 생성 및 전송
  get_filetype(filename, filetype);       // 파일 확장자로부터 MIME 타입 결정
  
  sprintf(buf, "HTTP/1.0 200 OK\r\n");   // 상태 라인 (성공)
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);      // 서버 정보
  sprintf(buf, "%sConnection: close\r\n", buf);            // 연결 종료 알림
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize); // 파일 크기
  sprintf(buf, "%sContent-type: %s\r\n", buf, filetype);   // MIME 타입
  sprintf(buf, "%s\r\n", buf);  // 빈 줄 추가


  Rio_writen(fd, buf, strlen(buf));       // HTTP 헤더 전송
  printf("Response headers:\n");
  printf("%s", buf);                      // 서버 콘솔에 응답 헤더 출력

  if (strcasecmp(method, "GET") == 0) {
    // 2단계: 파일 내용 전송 (메모리 매핑 방식)
    srcfd = Open(filename, O_RDONLY, 0);    // 파일을 읽기 전용으로 열기
    // mmap 방식
    // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);  // 파일을 메모리에 매핑
    // Close(srcfd);                           // 파일 디스크립터 닫기 (메모리 매핑 후 불필요)
    // Rio_writen(fd, srcp, filesize);         // 매핑된 메모리 내용을 클라이언트에 전송
    // Munmap(srcp, filesize);                 // 메모리 매핑 해제
    srcp = malloc(filesize);
    Rio_readn(srcfd, srcp, filesize);
    Rio_writen(fd, srcp, filesize);
    free(srcp);
    Close(srcfd);
  }
}

// 파일 확장자로부터 MIME 타입 결정
// 매개변수: filename(파일 경로), filetype(MIME 타입 저장할 문자열)
void get_filetype(char *filename, char *filetype)
{
    // 파일 확장자를 검사하여 적절한 MIME 타입 설정
  if (strstr(filename, ".html"))          // HTML 파일
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))      // GIF 이미지
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))      // PNG 이미지
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))      // JPEG 이미지
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".jpeg"))     // JPEG 이미지 (확장자 .jpeg)
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".ico"))      // ICO 파일 (favicon)
    strcpy(filetype, "image/x-icon");
  else if (strstr(filename, ".mp4"))      // Mp4 파일 
    strcpy(filetype, "video/mp4");
  else if (strstr(filename, ".mpg"))      // MPG 비디오
    strcpy(filetype, "video/mpg");
  else                                    // 기타 파일 (텍스트로 처리)
    strcpy(filetype, "text/plain");
}

// 동적 콘텐츠 (CGI 프로그램) 실행 및 결과 전송
// 매개변수: fd(클라이언트 소켓), filename(CGI 프로그램 경로), cgiargs(CGI 인수)
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method)
{
  char buf[MAXLINE], *emptylist[] = { NULL };  // HTTP 응답 버퍼, execve용 빈 인수 리스트

  // 1단계: HTTP 응답 헤더 전송 (Content-length 없음 - 동적 생성이므로 크기 모름)
  sprintf(buf, "HTTP/1.0 200 OK\r\n");        // 상태 라인 (성공)
  Rio_writen(fd, buf, strlen(buf));            // 상태 라인 전송
  sprintf(buf, "Server: Tiny Web Server\r\n"); // 서버 정보
  Rio_writen(fd, buf, strlen(buf));            // 서버 정보 전송

  // 2단계: 자식 프로세스 생성하여 CGI 프로그램 실행
  if (Fork() == 0) { /* Child */
    // 환경 변수 이름: "QUERY_STRING", 환경 변수 값: cgiargs (CGI 인수), 덮어쓰기: 1 (기존 값이 있어도 덮어씀)
    setenv("QUERY_STRING", cgiargs, 1);
    // method를 cgi-bin/adder.c에 넘겨주기 위해 환경변수 set
    setenv("REQUEST_METHOD", method, 1);
    Dup2(fd, STDOUT_FILENO); /* Redirect stdout to client */
    Execve(filename, emptylist, environ); /* Run CGI program */
  }
  Wait(NULL);
}
