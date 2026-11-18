#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <winsock2.h>
#include <windows.h>
#include <math.h>
#include "sqlite3.h"
#include <time.h>
#include <stdarg.h>

#pragma comment(lib, "ws2_32.lib")

#define MAX_ROOMS 10
#define WIDTH 60
#define HEIGHT 25
#define TABLE_X1 10
#define TABLE_Y1 5
#define TABLE_X2 50
#define TABLE_Y2 20

#define ROTL32(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

sqlite3 *db;

//Gera hash SHA-1 em formato hexadecimal
void sha1_hash(const char *input, char output[41]) {
    uint32_t h0 = 0x67452301;
    uint32_t h1 = 0xEFCDAB89;
    uint32_t h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476;
    uint32_t h4 = 0xC3D2E1F0;

    size_t orig_len = strlen(input);
    uint64_t bit_len = (uint64_t)orig_len * 8;

    size_t padded_len = orig_len + 1;
    while (padded_len % 64 != 56) {
        padded_len++;
    }
    padded_len += 8;

    uint8_t *msg = (uint8_t *)calloc(padded_len, 1);
    memcpy(msg, input, orig_len);
    msg[orig_len] = 0x80;

    for (int i = 0; i < 8; i++) {
        msg[padded_len - 8 + i] = (uint8_t)((bit_len >> ((7 - i) * 8)) & 0xFF);
    }

    for (size_t offset = 0; offset < padded_len; offset += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++) {
            w[i] = ((uint32_t)msg[offset + i * 4] << 24) |
                   ((uint32_t)msg[offset + i * 4 + 1] << 16) |
                   ((uint32_t)msg[offset + i * 4 + 2] << 8) |
                   ((uint32_t)msg[offset + i * 4 + 3]);
        }
        for (int i = 16; i < 80; i++) {
            w[i] = ROTL32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }

        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;

        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }

            uint32_t temp = ROTL32(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = ROTL32(b, 30);
            b = a;
            a = temp;
        }

        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }

    free(msg);
    sprintf(output, "%08x%08x%08x%08x%08x", h0, h1, h2, h3, h4);
}

void server_log(const char *format, ...) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);

    va_list args;
    va_start(args, format);
    
    printf("[%s] ", time_str);
    vprintf(format, args);
    
    FILE *f = fopen("server_log.txt", "a");
    if(f) {
        fprintf(f, "[%s] ", time_str);
        vfprintf(f, format, args);
        fclose(f);
    }
    va_end(args);
}

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

typedef struct {
    int active;
    int id;
    char password[32];
    SOCKET p1, p2;
    char name1[32], name2[32];
    int score1, score2;
    int game_status;
    float bx[4], by[4], vx[4], vy[4];
    int b_active[4];
    int m_x, m_y;
    int current_player;
    int winner;
} Room;

typedef struct {
    SOCKET client_sock;
    char ip[64];
} ClientData;

Room rooms[MAX_ROOMS];

void initDB() {
    if(sqlite3_open("sinuca.db", &db)) {
        server_log("[ERRO] Nao foi possivel abrir BD: %s\n", sqlite3_errmsg(db));
        exit(0);
    }
    const char *sql = "CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY AUTOINCREMENT, username TEXT UNIQUE, password TEXT, wins INTEGER DEFAULT 0, games_played INTEGER DEFAULT 0);";
    sqlite3_exec(db, sql, 0, 0, 0);
    server_log("[SISTEMA] Banco de dados SQLite carregado com sucesso!\n");
}

void updateStats(const char* username, int is_winner) {
    sqlite3_stmt *stmt;
    const char *query = is_winner ? 
        "UPDATE users SET wins = wins + 1, games_played = games_played + 1 WHERE username = ?" :
        "UPDATE users SET games_played = games_played + 1 WHERE username = ?";
    
    if(sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void initBalls(Room *r) {
    r->bx[0] = 20; r->by[0] = 12.5; r->b_active[0] = 1;
    r->bx[1] = 35; r->by[1] = 12.5; r->b_active[1] = 1;
    r->bx[2] = 38; r->by[2] = 11.5; r->b_active[2] = 1;
    r->bx[3] = 38; r->by[3] = 13.5; r->b_active[3] = 1;
    for(int i=0; i<4; i++) { r->vx[i] = 0; r->vy[i] = 0; }
    r->m_x = (int)r->bx[0] - 6; r->m_y = (int)r->by[0];
}

int isPocket(float x, float y) {
    int ix = (int)round(x), iy = (int)round(y);
    if((ix==TABLE_X1+1 || ix==TABLE_X1+2) && (iy==TABLE_Y1+1 || iy==TABLE_Y1+2)) return 1;
    if((ix==TABLE_X2-2 || ix==TABLE_X2-1) && (iy==TABLE_Y1+1 || iy==TABLE_Y1+2)) return 1;
    if((ix==TABLE_X1+1 || ix==TABLE_X1+2) && (iy==TABLE_Y2-2 || iy==TABLE_Y2-1)) return 1;
    if((ix==TABLE_X2-2 || ix==TABLE_X2-1) && (iy==TABLE_Y2-2 || iy==TABLE_Y2-1)) return 1;
    return 0;
}

void updatePhysics(Room *r) {
    int moving = 0;
    for(int i = 0; i < 4; i++) {
        if(!r->b_active[i]) continue;
        r->bx[i] += r->vx[i]; r->by[i] += r->vy[i];
        r->vx[i] *= 0.96; r->vy[i] *= 0.96;

        if(fabs(r->vx[i]) < 0.03) r->vx[i] = 0;
        if(fabs(r->vy[i]) < 0.03) r->vy[i] = 0;
        if(r->vx[i] != 0 || r->vy[i] != 0) moving = 1;

        if(isPocket(r->bx[i], r->by[i])) {
            r->b_active[i] = 0; r->vx[i] = 0; r->vy[i] = 0;
            if(i == 0) { 
                r->b_active[0] = 1; r->bx[0] = 20; r->by[0] = 12.5; 
                server_log("[PARTIDA] Sala %d: '%s' encacapou a Branca (Falta)\n", r->id, r->current_player == 1 ? r->name1 : r->name2);
            }
            else if(i == 3) {
                server_log("[PARTIDA] Sala %d: Bola 8 encacapada!\n", r->id);
                if(r->current_player == 1) r->winner = (r->b_active[1] == 1) ? 2 : 1;
                else r->winner = (r->b_active[2] == 1) ? 1 : 2;
                
                if(r->winner == 1) r->score1++; else r->score2++;
                r->game_status = 3;
                server_log("[PARTIDA] Sala %d: Vencedor: '%s'. Atualizando Banco.\n", r->id, r->winner == 1 ? r->name1 : r->name2);
                updateStats(r->name1, r->winner == 1);
                updateStats(r->name2, r->winner == 2);
            }
            continue;
        }

        if(r->bx[i] <= TABLE_X1+1) { r->bx[i] = TABLE_X1+1; r->vx[i] *= -1; }
        if(r->bx[i] >= TABLE_X2-1) { r->bx[i] = TABLE_X2-1; r->vx[i] *= -1; }
        if(r->by[i] <= TABLE_Y1+1) { r->by[i] = TABLE_Y1+1; r->vy[i] *= -1; }
        if(r->by[i] >= TABLE_Y2-1) { r->by[i] = TABLE_Y2-1; r->vy[i] *= -1; }
    }

    for(int i=0; i<4; i++) {
        if(!r->b_active[i]) continue;
        for(int j=i+1; j<4; j++) {
            if(!r->b_active[j]) continue;
            float dx = r->bx[j] - r->bx[i], dy = r->by[j] - r->by[i];
            float dist = sqrt(dx*dx + dy*dy);
            if(dist < 1.0f && dist > 0.001f) {
                float overlap = 1.0f - dist, nx = dx/dist, ny = dy/dist;
                r->bx[i] -= nx * overlap / 2.0f; r->by[i] -= ny * overlap / 2.0f;
                r->bx[j] += nx * overlap / 2.0f; r->by[j] += ny * overlap / 2.0f;
                float p = (r->vx[i]-r->vx[j])*nx + (r->vy[i]-r->vy[j])*ny;
                if(p > 0) { r->vx[i] -= p*nx; r->vy[i] -= p*ny; r->vx[j] += p*nx; r->vy[j] += p*ny; }
            }
        }
    }

    if(!moving && r->game_status == 2) {
        r->game_status = 1;
        r->current_player = (r->current_player == 1) ? 2 : 1;
        r->m_x = (int)r->bx[0] - 5; r->m_y = (int)r->by[0];
    }
}

void broadcastState(Room *r) {
    ServerMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = 2;
    msg.game_status = r->game_status;
    msg.current_player = r->current_player;
    msg.winner = r->winner;
    msg.score1 = r->score1; msg.score2 = r->score2;
    strcpy(msg.name1, r->name1); strcpy(msg.name2, r->name2);
    msg.m_x = r->m_x; msg.m_y = r->m_y;
    for(int i=0; i<4; i++) { msg.bx[i] = r->bx[i]; msg.by[i] = r->by[i]; msg.b_active[i] = r->b_active[i]; }

    if(r->p1) { msg.is_host = 1; send(r->p1, (char*)&msg, sizeof(msg), 0); }
    if(r->p2) { msg.is_host = 0; send(r->p2, (char*)&msg, sizeof(msg), 0); }
}

DWORD WINAPI RoomThread(LPVOID arg) {
    Room *r = (Room*)arg;
    ClientMsg cmsg;
    
    while(r->active) {
        fd_set readfds;
        struct timeval tv = {0, 0};
        FD_ZERO(&readfds);
        if(r->p1) FD_SET(r->p1, &readfds);
        if(r->p2) FD_SET(r->p2, &readfds);

        int ready = select(0, &readfds, NULL, NULL, &tv);
        if(ready > 0) {
            // VERIFICA JOGADOR 1
            if(r->p1 && FD_ISSET(r->p1, &readfds)) {
                int res = recv(r->p1, (char*)&cmsg, sizeof(cmsg), 0);
                if(res <= 0) { // HOST DESCONECTOU
                    server_log("[SALA %d] HOST '%s' desconectou.\n", r->id, r->name1);
                    if(r->game_status == 1 || r->game_status == 2) {
                        updateStats(r->name1, 0); updateStats(r->name2, 1);
                    }
                    closesocket(r->p1);
                    r->p1 = 0;
                    
                    if(r->p2) {
                        server_log("[SALA %d] '%s' agora e o novo HOST.\n", r->id, r->name2);
                        r->p1 = r->p2;
                        r->p2 = 0;
                        strcpy(r->name1, r->name2);
                        strcpy(r->name2, "Aguardando...");
                        r->score1 = r->score2;
                        r->score2 = 0;
                        r->current_player = 1;
                        r->game_status = 0;
                        initBalls(r);
                    }
                } else if(r->game_status == 1 && r->current_player == 1) {
                    if(cmsg.key == 72 && r->m_y > 1) r->m_y--;
                    if(cmsg.key == 80 && r->m_y < HEIGHT - 2) r->m_y++;
                    if(cmsg.key == 75 && r->m_x > 1) r->m_x--;
                    if(cmsg.key == 77 && r->m_x < WIDTH - 2) r->m_x++;
                    if(cmsg.key == 32) {
                        r->vx[0] = (r->bx[0] - r->m_x) * 0.15; r->vy[0] = (r->by[0] - r->m_y) * 0.15;
                        r->game_status = 2;
                    }
                } else if(r->game_status == 3 && cmsg.key == 'r') {
                    server_log("[PARTIDA] Sala %d: '%s' reiniciou a partida.\n", r->id, r->name1);
                    initBalls(r); r->game_status = 1; r->winner = 0; r->current_player = 1;
                }
            }

            if(r->p2 && FD_ISSET(r->p2, &readfds)) {
                int res = recv(r->p2, (char*)&cmsg, sizeof(cmsg), 0);
                if(res <= 0) { 
                    server_log("[SALA %d] GUEST '%s' desconectou.\n", r->id, r->name2);
                    if(r->game_status == 1 || r->game_status == 2) {
                        updateStats(r->name1, 1); updateStats(r->name2, 0);
                    }
                    closesocket(r->p2);
                    r->p2 = 0;
                    strcpy(r->name2, "Aguardando...");
                    r->score2 = 0;
                    r->current_player = 1;
                    r->game_status = 0;
                    initBalls(r);
                } else if(r->game_status == 1 && r->current_player == 2) {
                    if(cmsg.key == 72 && r->m_y > 1) r->m_y--;
                    if(cmsg.key == 80 && r->m_y < HEIGHT - 2) r->m_y++;
                    if(cmsg.key == 75 && r->m_x > 1) r->m_x--;
                    if(cmsg.key == 77 && r->m_x < WIDTH - 2) r->m_x++;
                    if(cmsg.key == 32) {
                        r->vx[0] = (r->bx[0] - r->m_x) * 0.15; r->vy[0] = (r->by[0] - r->m_y) * 0.15;
                        r->game_status = 2;
                    }
                }
            }
        }

        if(r->game_status == 2) updatePhysics(r);
        if(!r->p1 && !r->p2) { r->active = 0; break; } 
        broadcastState(r);
        Sleep(30); 
    }
    server_log("[SALA %d] Encerrada (Vazia).\n", r->id);
    return 0;
}

DWORD WINAPI ClientSetupThread(LPVOID arg) {
    ClientData *cd = (ClientData*)arg;
    SOCKET client = cd->client_sock;
    char client_ip[64];
    strcpy(client_ip, cd->ip);
    free(cd);

    ClientMsg msg;
    ServerMsg reply;
    memset(&reply, 0, sizeof(reply));
    char current_username[32] = {0};

    server_log("[REDE] Conexao estabelecida do IP: %s\n", client_ip);

    while(1) {
        if(recv(client, (char*)&msg, sizeof(msg), 0) <= 0) {
            closesocket(client);
            return 0; 
        }

        if(msg.action == 4) { // REGISTER COM HASH SHA-1
            char hashed_pass[41];
            sha1_hash(msg.password, hashed_pass);

            sqlite3_stmt *stmt;
            const char* sql = "INSERT INTO users (username, password) VALUES (?, ?)";
            if(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, msg.name, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 2, hashed_pass, -1, SQLITE_TRANSIENT);
                if(sqlite3_step(stmt) == SQLITE_DONE) {
                    reply.type = 3; strcpy(reply.message, "Conta registrada com sucesso!");
                    strcpy(current_username, msg.name);
                    server_log("[AUTH] Cadastro de '%s' (SHA-1) [IP: %s].\n", msg.name, client_ip);
                } else {
                    reply.type = 1; strcpy(reply.message, "O nome de usuario ja existe.");
                    server_log("[AUTH] Falha de cadastro (Ja existe) [IP: %s].\n", client_ip);
                }
                sqlite3_finalize(stmt);
            }
            send(client, (char*)&reply, sizeof(reply), 0);
        }
        else if(msg.action == 5) { // LOGIN COM HASH SHA-1
            char hashed_pass[41];
            sha1_hash(msg.password, hashed_pass);

            sqlite3_stmt *stmt;
            const char* sql = "SELECT id FROM users WHERE username = ? AND password = ?";
            if(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, msg.name, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 2, hashed_pass, -1, SQLITE_TRANSIENT);
                if(sqlite3_step(stmt) == SQLITE_ROW) {
                    reply.type = 3; strcpy(reply.message, "Login efetuado com sucesso!");
                    strcpy(current_username, msg.name);
                    server_log("[AUTH] Usuario '%s' logou [IP: %s].\n", msg.name, client_ip);
                } else {
                    reply.type = 1; strcpy(reply.message, "Usuario ou senha incorretos.");
                    server_log("[AUTH] Falha de login [IP: %s].\n", client_ip);
                }
                sqlite3_finalize(stmt);
            }
            send(client, (char*)&reply, sizeof(reply), 0);
        }
        else if(msg.action == 1) { // CREATE ROOM
            int created = 0;
            for(int i = 0; i < MAX_ROOMS; i++) {
                if(!rooms[i].active) {
                    rooms[i].active = 1; rooms[i].id = msg.room_id;
                    strcpy(rooms[i].password, msg.password);
                    strcpy(rooms[i].name1, current_username);
                    strcpy(rooms[i].name2, "Aguardando...");
                    rooms[i].p1 = client; rooms[i].p2 = 0;
                    rooms[i].score1 = 0; rooms[i].score2 = 0;
                    rooms[i].game_status = 0; rooms[i].current_player = 1;
                    initBalls(&rooms[i]);
                    
                    CreateThread(NULL, 0, RoomThread, &rooms[i], 0, NULL);
                    created = 1;
                    server_log("[ROOM] Sala %d criada por '%s'. Aguardando oponente...\n", msg.room_id, current_username);
                    break;
                }
            }
            if(created) return 0;
            else {
                reply.type = 1; strcpy(reply.message, "Servidor lotado!");
                send(client, (char*)&reply, sizeof(reply), 0);
            }
        }
        else if(msg.action == 2) { // JOIN ROOM
            int joined = 0;
            for(int i = 0; i < MAX_ROOMS; i++) {
                if(rooms[i].active && rooms[i].id == msg.room_id) {
                    if(strcmp(rooms[i].password, msg.password) != 0) {
                        reply.type = 1; strcpy(reply.message, "Senha da sala incorreta.");
                    } else if(rooms[i].p2 != 0) {
                        reply.type = 1; strcpy(reply.message, "A sala ja esta cheia.");
                    } else {
                        rooms[i].p2 = client;
                        strcpy(rooms[i].name2, current_username);
                        rooms[i].game_status = 1; 
                        joined = 1;
                        server_log("[ROOM] '%s' entrou na sala %d.\n", current_username, msg.room_id);
                        server_log("[PARTIDA] Iniciada Sala %d: %s vs %s\n", msg.room_id, rooms[i].name1, rooms[i].name2);
                    }
                    break;
                }
            }
            if(joined) return 0;
            else {
                if(reply.type != 1) { reply.type = 1; strcpy(reply.message, "Sala nao encontrada."); }
                send(client, (char*)&reply, sizeof(reply), 0);
            }
        }
    }
}

int main() {
    initDB();
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8888);

    bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_fd, 5);
    server_log("[SISTEMA] Servidor de Sinuca rodando na porta 8888...\n");

    while(1) {
        struct sockaddr_in client_info;
        int addr_len = sizeof(client_info);
        SOCKET client = accept(server_fd, (struct sockaddr*)&client_info, &addr_len);
        
        if(client != INVALID_SOCKET) {
            ClientData *cd = (ClientData*)malloc(sizeof(ClientData));
            cd->client_sock = client;
            strcpy(cd->ip, inet_ntoa(client_info.sin_addr));
            CreateThread(NULL, 0, ClientSetupThread, (LPVOID)cd, 0, NULL);
        }
    }
    return 0;
}