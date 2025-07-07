/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1=0, n2=0;

  /* Extract the two arguments */
  if ((buf = getenv("QUERY_STRING")) != NULL) {
    p = strchr(buf, '&');
    *p = '\0';
    strcpy(arg1, buf);
    strcpy(arg2, p+1);
    n1 = atoi(arg1);
    n2 = atoi(arg2);
  }

  /* Make the response body */
  sprintf(content, "QUERY_STRING=%s", buf);
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n2, n1 + n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);
  
  // // 1. sprintf로 content 버퍼를 초기화합니다.
  // sprintf(content, "Welcome to add.com: ");
  
  // // 2. strcat으로 문자열을 안전하게 이어붙입니다.
  // strcat(content, "THE Internet addition portal.\r\n<p>");
  
  // // 3. 임시 버퍼를 사용해 포맷팅된 문자열을 만든 뒤 이어붙입니다.
  // char temp[MAXLINE];
  // sprintf(temp, "The answer is: %d + %d = %d\r\n<p>", n1, n2, n1 + n2);
  // strcat(content, temp);
  // strcat(content, "Thanks for visiting!\r\n");

  /* Generate the HTTP response */
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  printf("%s", content);
  fflush(stdout);

  exit(0);
}
/* $end adder */
