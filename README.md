# TP Systèmes d'Exploitation

Ensemble de travaux pratiques réalisés dans le cadre d'un cours de systèmes d'exploitation.
Les TP couvrent des notions fondamentales : processus, threads, shell, allocation mémoire, etc.

## 📂 Contenu du dépôt

| Dossier / Fichier | Sujet |
|---|---|
| `tp2/` | Mini-shell : implémentation d'un interpréteur de commandes simple (`shell.c`, `myshell`) |
| `tp3/` | Contextes et threads : création et gestion de threads (`tp_ctx.c`, `tp_threads.c`) |
| `tp5/` | Gestion mémoire : allocation dynamique (`hm.c`) |
| `tp6/` | Allocation mémoire personnalisée (`pmalloc.c`) |
| `tp7/` | Suivi de threads / traceur (`tracker.c`, `main_part1.c`, `main_part2.c`) |
| `tp9/` | TP final : résultats et code associé (`tp9.c`, `resultat.txt`) |
| `rapport_tp-1.pdf` | Rapport du TP 1 |
| `rapport_tp.pdf` | Rapport de TP (général) |

## ⚙️ Prérequis

- Un compilateur C (GCC ou Clang)
- `make` (optionnel, selon les TP)
- Un environnement Linux / Unix (les TP utilisent des appels système POSIX : `fork`, `pthread`, `malloc`, etc.)

Installation des outils de compilation (Debian/Ubuntu) :

    sudo apt update
    sudo apt install build-essential

## 🚀 Compilation et exécution

Chaque dossier de TP contient généralement un fichier `.c` principal et parfois un binaire déjà compilé.
Pour compiler manuellement :

    gcc -Wall -Wextra -o nom_du_binaire fichier.c
    ./nom_du_binaire

Exemple pour le TP2 (shell) :

    cd tp2
    gcc -Wall -o myshell shell.c
    ./myshell

## 📊 Résultats et rapports

Les rapports PDF (`rapport_tp-1.pdf` et `rapport_tp.pdf`) détaillent la démarche, les choix d'implémentation et les résultats obtenus pour les TP concernés.
Le fichier `tp9/resultat.txt` contient les sorties ou mesures du TP final.

## 🎯 Objectifs pédagogiques

- Comprendre la création et la gestion de processus (`fork`, `exec`, `wait`)
- Manipuler des threads POSIX (`pthread_create`, synchronisation)
- Implémenter un interpréteur de commandes simple
- Étudier l'allocation mémoire dynamique (`malloc` / `free`, gestion de tas)
- Mettre en pratique des appels système bas niveau

