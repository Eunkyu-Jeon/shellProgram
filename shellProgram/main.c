//
//  main.c
//  shellProgram
//
//  Created by 전은규 on 2023/05/14.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>

#define MAX_LENGTH 1024 // 입력받을 명령어의 최대 길이
#define MAX_ARGS 64 // 명령어에 전달할 인수의 최대 개수

void handle_signal(int signo) {
    printf("\n"); // 개행 문자 출력
    exit(0); // 프로그램 종료
}

int main() {
    char input[MAX_LENGTH];
    char *args[MAX_ARGS];
    int status;
    
    signal(SIGINT, handle_signal); // Ctrl+C 시그널 핸들러 등록
    
    while (1) {
        printf("shell> ");
        fgets(input, MAX_LENGTH, stdin); // 사용자로부터 명령어 입력 받음
        
        if (input[strlen(input) - 1] == '\n') { // 마지막 개행 문자 제거
            input[strlen(input) - 1] = '\0';
        }
        
        if (strcmp(input, "exit") == 0) { // "exit" 명령어가 입력되면 프로그램 종료
            exit(0);
        }
        
        int i = 0;
        args[i] = strtok(input, " "); // 공백 문자를 기준으로 명령어와 인수를 분리
        while (args[i] != NULL) {
            i++;
            args[i] = strtok(NULL, " ");
        }
        
        args[i] = NULL;
        
        
        if (strcmp(args[0], "cd") == 0) { // "cd" 명령어가 입력되면 디렉토리 이동
            if (args[1] == NULL) { // 디렉토리 이름이 입력되지 않은 경우
                printf("Usage: cd <directory>\n");
            } else {
                if (chdir(args[1]) != 0) { // 디렉토리 변경에 실패한 경우
                    perror("chdir failed");
                }
            }
        } else if (strcmp(args[i-1], "&") == 0) { // 백그라운드 실행 기능
            args[i-1] = NULL; // "&" 기호를 인수 목록에서 제거
            
            pid_t pid = fork(); // 자식 프로세스 생성
            
            if (pid < 0) { // fork() 호출 실패 시 에러 메시지 출력
                perror("fork failed");
                exit(1);
            } else if (pid == 0) { // 자식 프로세스에서 실행할 코드
                if (execvp(args[0], args) < 0) { // 명령어 실행
                    perror("execvp failed");
                    exit(1);
                }
            } else { // 부모 프로세스에서 실행할 코드
                printf("Background process ID: %d\n", pid);
                // 백그라운드 프로세스를 대기하지 않고 바로 다음 명령어를 입력받음
            }
        } else { // "cd" 명령어가 아닌 경우 명령어 실행
            pid_t pid = fork(); // 자식 프로세스 생성
            
            if (pid < 0) { // fork() 호출 실패 시 에러 메시지 출력
                perror("fork failed");
                exit(1);
            } else if (pid == 0) { // 자식 프로세스에서 실행할 코드
                // 입출력 리디렉션을 위한 파일 디스크립터
                int fd_in = 0; // 표준 입력
                int fd_out = 1; // 표준 출력
                
                // 입력 리디렉션 처리
                if (args[i - 1] && strcmp(args[i - 1], "<") == 0) {
                    args[i - 1] = NULL; // "<" 기호 제거
                    fd_in = open(args[i], O_RDONLY); // 파일을 읽기 전용으로 열기
                    if (fd_in < 0) {
                        perror("open failed");
                        exit(1);
                        
                    }
                    // 파일 디스크립터 재지정
                    if (dup2(fd_in, STDIN_FILENO) < 0) {
                        perror("dup2 failed");
                        exit(1);
                    }
                }
                
                // 출력 리디렉션 처리
                if (args[i - 1] && strcmp(args[i - 1], ">") == 0) {
                    args[i - 1] = NULL; // ">" 기호 제거
                    fd_out = open(args[i], O_WRONLY | O_CREAT | O_TRUNC, 0644); // 파일을 쓰기 모드로 열기 (파일이 없으면 생성, 있으면 내용 삭제)
                    if (fd_out < 0) {
                        perror("open failed");
                        exit(1);
                    }
                    // 파일 디스크립터 재지정
                    if (dup2(fd_out, STDOUT_FILENO) < 0) {
                        perror("dup2 failed");
                        exit(1);
                    }
                }
                // >> 리디렉션 처리
                if (args[i - 1] && strcmp(args[i - 1], ">>") == 0) {
                    args[i - 1] = NULL; // ">>" 기호 제거
                    fd_out = open(args[i], O_WRONLY | O_CREAT | O_APPEND, 0644); // 파일을 쓰기 모드로 열기 (파일이 없으면 생성, 있으면 내용 뒤에 추가)
                    if (fd_out < 0) {
                        perror("open failed");
                        exit(1);
                    }
                    // 파일 디스크립터 재지정
                    if (dup2(fd_out, STDOUT_FILENO) < 0) {
                        perror("dup2 failed");
                        exit(1);
                    }
                }
                
                // | 파이프처리
                
                int pipefd[2];
                if (args[i - 1] && strcmp(args[i - 1], "|") == 0) {
                    args[i - 1] = NULL; // "|" 기호 제거
                    if (pipe(pipefd) == -1) {
                        perror("pipe failed");
                        exit(1);
                    }
                    
                    pid_t pid2 = fork(); // 두 번째 자식 프로세스 생성
                    
                    if (pid2 < 0) {
                        perror("fork failed");
                        exit(1);
                    } else if (pid2 == 0) { // 두 번째 자식 프로세스에서 실행할 코드 (뒷부분 명령어 실행)
                        close(pipefd[1]); // 파이프의 쓰기 단을 닫음
                        dup2(pipefd[0], STDIN_FILENO); // 파이프의 읽기 단을 표준 입력으로 재지정
                        close(pipefd[0]); // 파이프의 읽기 단을 닫음
                        
                        // 뒷부분 명령어 실행
                        if (execvp(args[i], &args[i]) < 0) {
                            perror("execvp failed");
                            exit(1);
                        }
                    } else { // 부모 프로세스에서 실행할 코드 (앞부분 명령어 실행)
                        close(pipefd[0]); // 파이프의 읽기 단을 닫음
                        dup2(pipefd[1], STDOUT_FILENO); // 파이프의 쓰기 단을 표준 출력으로 재지정
                        close(pipefd[1]); // 파이프의 쓰기 단을 닫음
                    }
                }
                
                // 명령어 실행
                if (execvp(args[0], args) < 0) {
                    perror("execvp failed");
                    exit(1);
                }
                
            } else { // 부모 프로세스에서 실행할 코드
                waitpid(pid, &status, 0); // 자식 프로세스가 종료될 때까지 대기
            }
        }
    }
    
    return 0;
}



