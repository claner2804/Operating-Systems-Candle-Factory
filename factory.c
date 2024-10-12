#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>

#define MAX_WAX 10
#define MAX_WICKS 10
#define MAX_CANDLES 5
#define WORKERS 6

// Lagerstruktur, die den Stand der Ressourcen enthält
typedef struct {
    int wax;
    int wicks;
    int candles;
    pthread_mutex_t lock;
    pthread_cond_t wax_cond;
    pthread_cond_t wick_cond;
    pthread_cond_t candle_cond;
} Storage;

Storage storage = {MAX_WAX, MAX_WICKS, 0, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER};
bool running = true;

void signal_handler(int signum) {
    printf("\n[Signal Handler] Signal %d empfangen. Beende die Simulation...\n", signum);
    running = false;
    pthread_mutex_lock(&storage.lock);
    pthread_cond_broadcast(&storage.wax_cond);
    pthread_cond_broadcast(&storage.wick_cond);
    pthread_cond_broadcast(&storage.candle_cond);
    pthread_mutex_unlock(&storage.lock);
}

void* createCandle(void* arg) {
    while (running) {
        pthread_mutex_lock(&storage.lock);

        // Warten, bis genug Wachs vorhanden ist
        while (storage.wax < 3 && running) {
            printf("[Worker] Nicht genug Wachs. Warte auf Nachfüllung...\n");
            pthread_cond_wait(&storage.wax_cond, &storage.lock);
        }

        // Warten, bis genug Dochte vorhanden ist
        while (storage.wicks < 1 && running) {
            printf("[Worker] Nicht genug Dochte. Warte auf Nachfüllung...\n");
            pthread_cond_wait(&storage.wick_cond, &storage.lock);
        }

        if (!running) {
            pthread_mutex_unlock(&storage.lock);
            break;
        }

        // Ressourcen entnehmen
        storage.wax -= 3;
        storage.wicks -= 1;
        printf("[Worker] Beginne mit der Herstellung einer Kerze. Verbleibendes Wachs: %d, Verbleibende Dochte: %d\n", storage.wax, storage.wicks);
        pthread_mutex_unlock(&storage.lock);

        // Eine Sekunde schlafen, um die Herstellung der Kerze zu simulieren
        usleep(1000000);

        pthread_mutex_lock(&storage.lock);
        // Warten, bis Platz für eine weitere Kerze im Lager ist
        while (storage.candles >= MAX_CANDLES && running) {
            printf("[Worker] Kein Platz für neue Kerzen. Gehe Kaffee trinken...\n");
            pthread_cond_wait(&storage.candle_cond, &storage.lock);
        }

        if (!running) {
            pthread_mutex_unlock(&storage.lock);
            break;
        }

        // Kerze ins Lager bringen
        storage.candles++;
        printf("[Worker] Kerze hergestellt und ins Lager gebracht. Aktuelle Kerzenanzahl: %d\n", storage.candles);
        pthread_cond_signal(&storage.candle_cond);
        pthread_mutex_unlock(&storage.lock);
    }
    return NULL;
}

void manageStorage(int signum) {
    static int counter = 0;
    pthread_mutex_lock(&storage.lock);

    if (counter % 2 == 0) {
        // Wachs nachfüllen
        if (storage.wax < MAX_WAX) {
            storage.wax = MAX_WAX;
            printf("[Manager] Wachs nachgefüllt. Aktueller Wachsstand: %d\n", storage.wax);
            pthread_cond_broadcast(&storage.wax_cond);
        }
    }

    if (counter % 3 == 0) {
        // Dochte nachfüllen
        if (storage.wicks < MAX_WICKS) {
            storage.wicks = MAX_WICKS;
            printf("[Manager] Dochte nachgefüllt. Aktueller Dochtestand: %d\n", storage.wicks);
            pthread_cond_broadcast(&storage.wick_cond);
        }
    }

    if (counter % 4 == 0) {
        // Kerzenlager leeren
        if (storage.candles > 0) {
            storage.candles = 0;
            printf("[Manager] Kerzenlager geleert. Aktuelle Kerzenanzahl: %d\n", storage.candles);
            pthread_cond_broadcast(&storage.candle_cond);
        }
    }

    counter++;
    pthread_mutex_unlock(&storage.lock);
    alarm(5);
}

int main() {
    signal(SIGINT, signal_handler);
    signal(SIGALRM, manageStorage);

    pthread_t workers[WORKERS];

    // Arbeiter-Threads erstellen
    for (int i = 0; i < WORKERS; i++) {
        if (pthread_create(&workers[i], NULL, createCandle, NULL) != 0) {
            perror("Fehler bei pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    // Alarm für die Verwaltung des Lagers setzen
    alarm(5);

    // Warten, bis das Programm beendet wird
    while (running) {
        pause();
    }

    // Auf alle Arbeiter-Threads warten
    for (int i = 0; i < WORKERS; i++) {
        pthread_join(workers[i], NULL);
    }

    // Ressourcen freigeben
    pthread_mutex_destroy(&storage.lock);
    pthread_cond_destroy(&storage.wax_cond);
    pthread_cond_destroy(&storage.wick_cond);
    pthread_cond_destroy(&storage.candle_cond);

    printf("[Main] Programm beendet.\n");
    return 0;
}
