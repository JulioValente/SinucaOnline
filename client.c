#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <winsock2.h>
#include <windows.h>
#include <math.h>

#pragma comment(lib, "ws2_32.lib")

#define WIDTH 60
#define HEIGHT 25
#define TABLE_X1 10
#define TABLE_Y1 5
#define TABLE_X2 50
#define TABLE_Y2 20

typedef struct {
    int action;
    char name[32];
    int room_id;
    char password[32];
    int key;
} ClientMsg;

typedef struct {
    int type; 
    char message[64];
    int game_status; 
    float bx[4], by[4];
    int b_active[4];
    int m_x, m_y;
    int current_player; 
    int winner; 
    char name1[32], name2[32];
    int score1, score2;
    int is_host;
} ServerMsg;

ServerMsg gameState;
SOCKET sock;
int running = 1;

void hideCursor() {
    HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO info;
    info.dwSize = 100; info.bVisible = FALSE;
    SetConsoleCursorInfo(consoleHandle, &info);
}

void setCursorPosition(int x, int y) {
    COORD coord = { (short)x, (short)y };
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

void draw() {
    char screen[HEIGHT][WIDTH + 1];

    for(int y = 0; y < HEIGHT; y++) {
        for(int x = 0; x < WIDTH; x++) { screen[y][x] = ' '; }
        screen[y][WIDTH] = '\0';
    }

    for(int x = 0; x < WIDTH; x++) { screen[0][x] = '#'; screen[HEIGHT-1][x] = '#'; }
    for(int y = 0; y < HEIGHT; y++) { screen[y][0] = '#'; screen[y][WIDTH-1] = '#'; }
    for(int x = TABLE_X1; x <= TABLE_X2; x++) { screen[TABLE_Y1][x] = 'X'; screen[TABLE_Y2][x] = 'X'; }
    for(int y = TABLE_Y1; y <= TABLE_Y2; y++) { screen[y][TABLE_X1] = 'X'; screen[y][TABLE_X2] = 'X'; }

    screen[TABLE_Y1+1][TABLE_X1+1] = '@'; screen[TABLE_Y1+1][TABLE_X1+2] = '@';
    screen[TABLE_Y1+2][TABLE_X1+1] = '@'; screen[TABLE_Y1+2][TABLE_X1+2] = '@';
    screen[TABLE_Y1+1][TABLE_X2-2] = '@'; screen[TABLE_Y1+1][TABLE_X2-1] = '@';
    screen[TABLE_Y1+2][TABLE_X2-2] = '@'; screen[TABLE_Y1+2][TABLE_X2-1] = '@';
    screen[TABLE_Y2-2][TABLE_X1+1] = '@'; screen[TABLE_Y2-2][TABLE_X1+2] = '@';
    screen[TABLE_Y2-1][TABLE_X1+1] = '@'; screen[TABLE_Y2-1][TABLE_X1+2] = '@';
    screen[TABLE_Y2-2][TABLE_X2-2] = '@'; screen[TABLE_Y2-2][TABLE_X2-1] = '@';
    screen[TABLE_Y2-1][TABLE_X2-2] = '@'; screen[TABLE_Y2-1][TABLE_X2-1] = '@';

    char sym[4] = {'O', '1', '2', '8'};
    for(int i = 0; i < 4; i++) {
        if(!gameState.b_active[i]) continue;
        int bx = (int)round(gameState.bx[i]);
        int by = (int)round(gameState.by[i]);
        if(bx >= 0 && bx < WIDTH && by >= 0 && by < HEIGHT) screen[by][bx] = sym[i];
    }

    if(gameState.game_status == 1) {
        if(gameState.m_y >= 0 && gameState.m_y < HEIGHT && gameState.m_x >= 0 && gameState.m_x < WIDTH) {
            screen[gameState.m_y][gameState.m_x] = 'M';
        }
    }

    setCursorPosition(0, 0);
    
    printf("HOST: %-15s (Bola 1) | GUEST: %-15s (Bola 2)                \n", gameState.name1, gameState.name2);
    printf("PLACAR - %-15s: %d vitorias | %-15s: %d vitorias                   \n", gameState.name1, gameState.score1, gameState.name2, gameState.score2);
    
    if(gameState.game_status == 0) printf(">> AGUARDANDO UM JOGADOR ENTRAR... <<                                     \n");
    else if(gameState.game_status == 3) printf(">> FIM DE JOGO! VENCEDOR: %-15s <<                                  \n", gameState.winner == 1 ? gameState.name1 : gameState.name2);
    else printf("TURNO ATUAL: %-15s                                                          \n", gameState.current_player == 1 ? gameState.name1 : gameState.name2);

    for(int y = 0; y < HEIGHT; y++) puts(screen[y]);
    
    if(gameState.game_status == 3) {
        if(gameState.is_host) printf("Voce e o host. Pressione 'R' para REINICIAR o jogo.                       \n");
        else printf("Aguarde o host reiniciar a partida.                                        \n");
    } else {
        printf("Setas: Mirar (M) | Espaco: Tacada. (Apenas no seu turno)                       \n");
    }
}

DWORD WINAPI ReceiveThread(LPVOID arg) {
    while(running) {
        int res = recv(sock, (char*)&gameState, sizeof(gameState), 0);
        if(res <= 0) {
            system("cls");
            printf("Conexao com o servidor perdida!\n");
            running = 0;
            break;
        }
        if(gameState.type == 1) {
            printf("\nERRO DO SERVIDOR: %s\n", gameState.message);
            running = 0;
            break;
        }
        draw();
    }
    return 0;
}

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    sock = socket(AF_INET, SOCK_STREAM, 0);

    char server_ip[64];
    int server_port;
    ClientMsg msg;
    memset(&msg, 0, sizeof(msg));

    printf("                                        $$\\                                        \n");
    printf("                                        \\__|                                       \n");
    printf("        ____                   $$$$$$$\\ $$\\ $$$$$$$\\  $$\\   $$\\  $$$$$$$\\ $$$$$$\\  \n");
    printf("    ,dP9CGG88@b,              $$  _____|$$ |$$  __$$\\ $$ |  $$ |$$  _____|\\____$$\\ \n");
    printf("  ,IP  _   Y888@@b,           \\$$$$$$\\  $$ |$$ |  $$ |$$ |  $$ |$$ /      $$$$$$$ |\n");
    printf(" dIi  (_)   G8888@b            \\____$$\\ $$ |$$ |  $$ |$$ |  $$ |$$ |     $$  __$$ |\n");
    printf("dCII  (_)   G8888@@b          $$$$$$$  |$$ |$$ |  $$ |\\$$$$$$  |\\$$$$$$$\\\\$$$$$$$ |\n");
    printf("GCCIi     ,GG8888@@@          \\_______/ \\__|\\__|  \\__| \\______/  \\_______|\\_______|\n");
    printf("GGCCCCCCCGGG88888@@@          \n");
    printf("GGGGCCCGGGG88888@@@@...                           $$\\ $$\\                     \n");
    printf("Y8GGGGGG8888888@@@@P.....                         $$ |\\__|                    \n");
    printf(" Y88888888888@@@@@P......      $$$$$$\\  $$$$$$$\\  $$ |$$\\ $$$$$$$\\   $$$$$$\\  \n");
    printf(" `Y8888888@@@@@@@P'......     $$  __$$\\ $$  __$$\\ $$ |$$ |$$  __$$\\ $$  __$$\\ \n");
    printf("    `@@@@@@@@@P'.......       $$ /  $$ |$$ |  $$ |$$ |$$ |$$ |  $$ |$$$$$$$$ |\n");
    printf("        \"\"\"\"........          $$ |  $$ |$$ |  $$ |$$ |$$ |$$ |  $$ |$$   ____|\n");
    printf("                              \\$$$$$$  |$$ |  $$ |$$ |$$ |$$ |  $$ |\\$$$$$$$\\ \n");
    printf("                               \\______/ \\__|  \\__|\\__|\\__|\\__|  \\__| \\_______|\n\n\n");
    printf("Digite o IP do servidor (ex: 127.0.0.1): ");
    scanf("%63s", server_ip);
    printf("Digite a porta do servidor (ex: 8888): ");
    scanf("%d", &server_port);

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(server_ip); 
    server.sin_port = htons(server_port);

    printf("\nConectando ao servidor...\n");
    if(connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        printf("A conexao falhou! Nao foi possivel encontrar o servidor em %s:%d\n", server_ip, server_port);
        system("pause");
        return 1;
    }

    while(1) {
        system("cls");
        printf("                                        $$\\                                        \n");
        printf("                                        \\__|                                       \n");
        printf("        ____                   $$$$$$$\\ $$\\ $$$$$$$\\  $$\\   $$\\  $$$$$$$\\ $$$$$$\\  \n");
        printf("    ,dP9CGG88@b,              $$  _____|$$ |$$  __$$\\ $$ |  $$ |$$  _____|\\____$$\\ \n");
        printf("  ,IP  _   Y888@@b,           \\$$$$$$\\  $$ |$$ |  $$ |$$ |  $$ |$$ /      $$$$$$$ |\n");
        printf(" dIi  (_)   G8888@b            \\____$$\\ $$ |$$ |  $$ |$$ |  $$ |$$ |     $$  __$$ |\n");
        printf("dCII  (_)   G8888@@b          $$$$$$$  |$$ |$$ |  $$ |\\$$$$$$  |\\$$$$$$$\\\\$$$$$$$ |\n");
        printf("GCCIi     ,GG8888@@@          \\_______/ \\__|\\__|  \\__| \\______/  \\_______|\\_______|\n");
        printf("GGCCCCCCCGGG88888@@@          \n");
        printf("GGGGCCCGGGG88888@@@@...                           $$\\ $$\\                     \n");
        printf("Y8GGGGGG8888888@@@@P.....                         $$ |\\__|                    \n");
        printf(" Y88888888888@@@@@P......      $$$$$$\\  $$$$$$$\\  $$ |$$\\ $$$$$$$\\   $$$$$$\\  \n");
        printf(" `Y8888888@@@@@@@P'......     $$  __$$\\ $$  __$$\\ $$ |$$ |$$  __$$\\ $$  __$$\\ \n");
        printf("    `@@@@@@@@@P'.......       $$ /  $$ |$$ |  $$ |$$ |$$ |$$ |  $$ |$$$$$$$$ |\n");
        printf("        \"\"\"\"........          $$ |  $$ |$$ |  $$ |$$ |$$ |$$ |  $$ |$$   ____|\n");
        printf("                              \\$$$$$$  |$$ |  $$ |$$ |$$ |$$ |  $$ |\\$$$$$$$\\ \n");
        printf("                               \\______/ \\__|  \\__|\\__|\\__|\\__|  \\__| \\_______|\n\n\n");
        printf("=== MENU DE AUTENTICACAO ===\n");
        printf("1 - Fazer Login\n");
        printf("2 - Registrar nova conta\n");
        printf("Escolha: ");
        scanf("%d", &msg.action);

        if(msg.action == 1) msg.action = 5; 
        else if(msg.action == 2) msg.action = 4; 
        else continue;

        printf("Nome de Usuario: ");
        scanf("%31s", msg.name);
        printf("Senha: ");
        scanf("%31s", msg.password);

        send(sock, (char*)&msg, sizeof(msg), 0);
        recv(sock, (char*)&gameState, sizeof(gameState), 0);

        if(gameState.type == 3) {
            printf("\n%s\n", gameState.message);
            Sleep(1000);
            break; 
        } else {
            printf("\nErro: %s\n", gameState.message);
            system("pause");
        }
    }

    while(1) {
        system("cls");
        printf("                                        $$\\                                        \n");
        printf("                                        \\__|                                       \n");
        printf("        ____                   $$$$$$$\\ $$\\ $$$$$$$\\  $$\\   $$\\  $$$$$$$\\ $$$$$$\\  \n");
        printf("    ,dP9CGG88@b,              $$  _____|$$ |$$  __$$\\ $$ |  $$ |$$  _____|\\____$$\\ \n");
        printf("  ,IP  _   Y888@@b,           \\$$$$$$\\  $$ |$$ |  $$ |$$ |  $$ |$$ /      $$$$$$$ |\n");
        printf(" dIi  (_)   G8888@b            \\____$$\\ $$ |$$ |  $$ |$$ |  $$ |$$ |     $$  __$$ |\n");
        printf("dCII  (_)   G8888@@b          $$$$$$$  |$$ |$$ |  $$ |\\$$$$$$  |\\$$$$$$$\\\\$$$$$$$ |\n");
        printf("GCCIi     ,GG8888@@@          \\_______/ \\__|\\__|  \\__| \\______/  \\_______|\\_______|\n");
        printf("GGCCCCCCCGGG88888@@@          \n");
        printf("GGGGCCCGGGG88888@@@@...                           $$\\ $$\\                     \n");
        printf("Y8GGGGGG8888888@@@@P.....                         $$ |\\__|                    \n");
        printf(" Y88888888888@@@@@P......      $$$$$$\\  $$$$$$$\\  $$ |$$\\ $$$$$$$\\   $$$$$$\\  \n");
        printf(" `Y8888888@@@@@@@P'......     $$  __$$\\ $$  __$$\\ $$ |$$ |$$  __$$\\ $$  __$$\\ \n");
        printf("    `@@@@@@@@@P'.......       $$ /  $$ |$$ |  $$ |$$ |$$ |$$ |  $$ |$$$$$$$$ |\n");
        printf("        \"\"\"\"........          $$ |  $$ |$$ |  $$ |$$ |$$ |$$ |  $$ |$$   ____|\n");
        printf("                              \\$$$$$$  |$$ |  $$ |$$ |$$ |$$ |  $$ |\\$$$$$$$\\ \n");
        printf("                               \\______/ \\__|  \\__|\\__|\\__|\\__|  \\__| \\_______|\n\n\n");
        printf("=== BEM-VINDO, %s! ===\n", msg.name);
        printf("1 - Criar uma nova Sala\n");
        printf("2 - Entrar em uma Sala Existente\n");
        printf("Escolha: ");
        scanf("%d", &msg.action); 
        
        if(msg.action != 1 && msg.action != 2) continue;

        printf("Numero da Sala (Ex: 1, 2...): ");
        scanf("%d", &msg.room_id);
        printf("Senha da Sala: ");
        scanf("%31s", msg.password);

        send(sock, (char*)&msg, sizeof(msg), 0);
        recv(sock, (char*)&gameState, sizeof(gameState), 0);

        if(gameState.type == 1) { 
            printf("\nERRO: %s\n", gameState.message);
            system("pause");
        } else {
            break;
        }
    }

    hideCursor();
    system("cls");
    CreateThread(NULL, 0, ReceiveThread, NULL, 0, NULL);

    while(running) {
        if(_kbhit()) {
            int ch = _getch();
            if(ch == 0 || ch == 224) { 
                ch = _getch(); 
                msg.key = ch;
            } else if(ch == ' ' || ch == 'r' || ch == 'R') {
                msg.key = ch;
                if(ch == 'R') msg.key = 'r';
            } else {
                continue;
            }
            msg.action = 3;
            send(sock, (char*)&msg, sizeof(msg), 0);
        }
        Sleep(10);
    }
    
    system("pause");
    return 0;
}