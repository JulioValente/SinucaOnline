# 🎱 SinucaOnline (Julio Ball Pool)

![C](https://img.shields.io/badge/Language-C%20(C99%2B)-00599C?style=for-the-badge&logo=c&logoColor=white)
![TCP/IP](https://img.shields.io/badge/Network-TCP%2FIP%20Sockets-2EA44F?style=for-the-badge)
![SQLite](https://img.shields.io/badge/Database-SQLite3%20Embedded-003B57?style=for-the-badge&logo=sqlite&logoColor=white)
![Security](https://img.shields.io/badge/Security-Diffie--Hellman%20%2B%20SHA--1-D93F0B?style=for-the-badge)

> **Projeto Capstone — Tecnologia de Redes para Desenvolvedores (2026)**  
> Jogo multiplayer 2P de sinuca em rede com arquitetura cliente-servidor autoritativa, sincronização de estado em tempo real, persistência em banco de dados relacional embutido e proteção criptográfica de sessão.

---

## 📋 Sobre o Projeto

O **SinucaOnline** é uma aplicação distribuída desenvolvida integralmente em linguagem **C** (`server.c` e `client.c`) que permite que dois jogadores disputem partidas de bilhar em turnos alternados através de conexões **TCP/IP**. O servidor atua como fonte única da verdade (*Single Source of Truth*), validando regras, processando jogadas, gerenciando múltiplas salas simultâneas e registrando eventos de auditoria.

### 🎯 Regras do Jogo (*Julio Ball Pool*)
* **Início da Partida:** Sempre começa pelo **Player 1** (o primeiro jogador a entrar na sala).
* **Condição de Vitória:** Cada jogador deve primeiro encaçapar a bola correspondente ao seu número atribuído (**1 ponto**) e, em seguida, encaçapar a **bola 8**, totalizando **2 pontos** para vencer. Também vence caso o adversário abandone a partida.
* **Condição de Derrota:** Encaçapar a **bola 8 prematuramente** (antes de derrubar a própria bola numerada) ou desistir da partida.
* **Dinâmica das Jogadas:** Não há restrição de toque na bola adversária; o jogador pode utilizar toda a mesa taticamente, atingir a bola do oponente ou até pontuar para o outro jogador, obedecendo estritamente às regras de pontuação e da bola 8.

---

## 🏗️ Arquitetura do Sistema

O sistema adota uma topologia **Cliente-Servidor centralizada autoritativa** operando na **porta TCP `8888`**:

```text
┌──────────────────────┐                       ┌──────────────────────────────┐
│  Cliente 1..N        │   Sockets TCP/IP      │  Servidor Central            │
│  (client.exe)        │◄─────────────────────►│  (server.exe :8888)          │
│                      │   Diffie-Hellman      │  • Multithreading            │
│  • Interface / UI    │   Payload Delimitado  │  • Validação Autoritativa    │
│  • Envio de Jogadas  │                       │  • Roteamento por Sala       │
└──────────────────────┘                       └───────┬──────────────┬───────┘
                                                       │              │
                                         Consultas SQL │              │ Auditoria
                                      (Prepared Stmts) ▼              ▼
                                            ┌──────────────────┐ ┌────────────────┐
                                            │ SQLite3 Embutido │ │ Logs do Sistema│
                                            │ (sqlite3.c/.h)   │ │ server_log.txt │
                                            └──────────────────┘ └────────────────┘