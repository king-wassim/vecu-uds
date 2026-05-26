# vecu-uds — Script de démo entretien

> Démo de **5 à 7 minutes** maximum. Le but : prouver en peu de temps
> que tu comprends UDS, ISO-TP, le multi-threading POSIX et la chaîne
> d'outils embarqué/Linux. Garde un débit calme, ne te précipite pas.

---

## Avant l'entretien — préparation 5 minutes

### 1. Préparer les fenêtres

Ouvre **Windows Terminal** et splitte-le en **3 panneaux** :

```
+---------------------+---------------------+
| Fenêtre 1: ECU      | Fenêtre 2: tester   |
| (vecu_uds tourne)   | (diag_tool.py)      |
|                     +---------------------+
|                     | Fenêtre 3: monitor  |
|                     | (curses temps réel) |
+---------------------+---------------------+
```

### 2. Lancer la chaîne complète (tape AVANT l'entretien)

```bash
# Vérifier les modules kernel
sudo modprobe vcan can-isotp can_raw 2>/dev/null
ip link show vcan0 2>/dev/null || (sudo ip link add dev vcan0 type vcan && sudo ip link set up vcan0)

# Rebuild propre
cd ~/projects/vecu-uds
cmake --build build -j > /dev/null && echo "Build OK"

# Activer le venv Python dans la 2ème fenêtre
cd ~/projects/vecu-uds/client && source .venv/bin/activate
```

### 3. Ouvre dans ton navigateur (en arrière-plan, pour montrer si demandé)

- https://github.com/king-wassim/vecu-uds (le repo public avec badge CI ✅)
- Le fichier `docs/comparison.md` (pour les limites assumées)
- Le diagramme Mermaid en haut du README (sur GitHub il est rendu directement)

---

## Le script de démo (à dire à voix haute)

### ▶ ÉTAPE 1 — Pitch (30 sec)

**Tu dis :**

> "Je vais te montrer **vecu-uds**, un projet que j'ai écrit en 21 jours.
> C'est un **ECU virtuel automobile** en C/POSIX, qui implémente
> **UDS — la norme ISO 14229** — par-dessus **ISO-TP — ISO 15765-2** —
> sur Linux SocketCAN.
>
> L'objectif était de me confronter concrètement à la stack de
> diagnostic embarqué qu'on retrouve chez tous les équipementiers."

**Tu montres :** la page GitHub du repo (badge CI ✅, README en haut).

---

### ▶ ÉTAPE 2 — Architecture en 1 minute

**Tu dis :**

> "L'ECU tourne en **4 threads POSIX** qui se parlent par une file
> producteur/consommateur sous mutex.
>
> - Un **thread RX** qui lit les messages UDS complets depuis un
>   socket `CAN_ISOTP` du kernel.
> - Un **thread dispatcher** qui prend la requête, l'envoie au bon
>   handler — j'ai une table de 8 services UDS — et renvoie la
>   réponse.
> - Un **thread app** à 10 Hz qui simule un moteur — RPM, vitesse,
>   coolant — pour alimenter les DIDs dynamiques.
> - Un **thread S3** qui implémente le **timer S3 d'ISO 14229** :
>   si le tester se tait pendant 5 secondes en session étendue,
>   l'ECU retombe en session par défaut. C'est ce qui empêche un
>   ECU de rester déverrouillé indéfiniment."

**Tu montres :** le **diagramme Mermaid** dans le README (déjà ouvert).

---

### ▶ ÉTAPE 3 — Démo live (3-4 min) — la partie qui compte

#### 3a. Lancer l'ECU (Fenêtre 1)

```bash
./build/src/vecu_uds vcan0
```

**Tu dis :**
> "L'ECU démarre, écoute sur `vcan0`, ID 0x7E0 pour les requêtes,
> 0x7E8 pour les réponses. C'est l'addressing 11-bit classique
> ISO 15765-4."

Sortie attendue :
```
vecu-uds v1.0.0 — starting on vcan0 (rx=0x7E0 tx=0x7E8)
vecu-uds ready. Ctrl+C to stop.
```

---

#### 3b. Premier service — Read VIN (Fenêtre 2)

```bash
python3 diag_tool.py read-vin
```

**Tu dis :**
> "Première requête UDS : **ReadDataByIdentifier service 0x22**,
> DID 0xF190 — le **VIN** du véhicule. Côté wire, c'est une SF
> ISO-TP simple en aller, et un FF + 2 CFs au retour parce que la
> réponse fait 20 octets — donc on a aussi prouvé que le
> multi-frame ISO-TP marche."

Sortie attendue :
```
VIN: VF1234567890ABCDE
```

---

#### 3c. Démarrer le monitor curses (Fenêtre 3)

```bash
python3 diag_tool.py monitor
```

**Tu dis :**
> "Là je lance un **monitor temps réel** : c'est ma fenêtre 3.
> Il lit 3 DIDs dynamiques toutes les 200 ms — RPM, vitesse,
> coolant — et tu vois les valeurs **bouger en live** parce que
> mon thread app oscille les valeurs comme le ferait un moteur."

L'écran affiche en boucle :
```
+----- vECU monitor -----+
| RPM     :  1366        |
| Speed   : 109 km/h     |
| Coolant :  77 C        |
+------------------------+
```

**Laisse-le tourner en fond pendant la suite.**

---

#### 3d. Lire les DTCs (Fenêtre 2)

```bash
python3 diag_tool.py read-dtc
```

**Tu dis :**
> "**Service 0x19 — ReadDTCInformation**. L'ECU a deux DTCs en
> mémoire que j'ai seedés pour la démo : **P0301** raté
> d'allumage cylindre 1, et **P0420** efficacité catalyseur.
> Le format des codes est SAE J2012."

Sortie attendue :
```
Active DTCs (2):
  P0301 — Cylinder 1 Misfire   [confirmed]
  P0420 — Catalyst Efficiency  [confirmed]
```

---

#### 3e. Montrer une NRC — la preuve qu'on connaît UDS

```bash
python3 diag_tool.py write-name "WASSIM"
```

**Tu dis :**
> "Maintenant je tente d'écrire le nom du propriétaire **sans
> avoir débloqué la sécurité**. L'ECU doit refuser."

Sortie attendue :
```
ECU responded with NRC: securityAccessDenied (0x33)
```

**Tu dis :**
> "Et là tu vois la **NRC 0x33 — securityAccessDenied**. C'est
> exactement le bon code de retour défini par ISO 14229 quand un
> service est gardé par SecurityAccess et que le tester n'est pas
> unlock. Le ECU n'a pas écrit, il a renvoyé la negative response
> propre."

---

#### 3f. Le seed/key — la partie la plus impressionnante

```bash
python3 diag_tool.py unlock --level 1
```

**Tu dis :**
> "**Service 0x27 — SecurityAccess**. L'ECU me génère une
> **seed aléatoire de 4 octets**, je calcule la **clé** côté
> tester, je la renvoie, l'ECU vérifie. Si c'est bon, il déverrouille
> le niveau 1.
>
> Algo de démo : `key = seed XOR 0xDEADBEEF` — sur un vrai ECU
> ce serait un HSM avec une crypto signée par le constructeur,
> mais le **state machine** est identique."

Sortie attendue :
```
Seed: 0x35636373 -> Key: 0xEBCEDD9C
Security unlocked (level 1).
```

---

#### 3g. Maintenant l'écriture passe

```bash
python3 diag_tool.py write-name "WASSIM"
```

**Tu dis :**
> "Et maintenant que la sécurité est débloquée, le même service
> 0x2E passe. L'ECU écrit, et je relis derrière pour confirmer."

Sortie attendue :
```
Owner name written: 'WASSIM'
Confirmed: 'WASSIM'
```

---

#### 3h. Bonus — le timer S3 (si tu as 30 sec en plus)

**Tu dis :**
> "Si je laisse passer 5 secondes sans envoyer de TesterPresent,
> l'ECU va automatiquement retomber en Default Session et relock
> la sécurité — c'est le S3 timer d'ISO 14229."

**Laisse 6-7 secondes** en silence. Dans la Fenêtre 1, l'ECU affiche :
```
[S3] timeout — reverting to default session
```

**Tu dis :**
> "Voilà, l'ECU s'est auto-protégé. Si maintenant je retentais le
> write-name, je retomberais sur NRC 0x33."

---

### ▶ ÉTAPE 4 — Montrer le code (1 min)

**Ouvre VS Code ou `cat` sur 2 fichiers clés :**

```bash
# Le dispatcher UDS — au cœur du projet
code ~/projects/vecu-uds/src/uds.c

# La table des handlers (montre les 8 services)
```

**Tu dis :**
> "Côté code, le dispatcher est une simple **table de fonctions**
> indexée par SID — c'est lisible et c'est exactement comme ça
> qu'on le ferait dans un AUTOSAR. Chaque handler a la même
> signature : il lit la requête, applique les checks de session
> et de sécurité, et écrit la réponse."

**Puis montre le runtime multi-thread :**

```bash
code ~/projects/vecu-uds/src/main.c
```

**Tu dis :**
> "Et la fonction `main` qui orchestre les 4 threads, avec un
> shutdown propre via `sigaction` + `pthread_cond_broadcast`
> pour ne laisser aucun thread bloqué."

---

### ▶ ÉTAPE 5 — Limites assumées (30 sec) — IMPORTANT

**Tu dis :**
> "Je suis honnête sur les limites — tout est documenté dans
> `docs/comparison.md`. Je sais que ce projet :
>
> - **n'a pas de WCET garanti** : c'est du Linux best-effort,
>   pas un OSEK avec déclaration de tâches
> - **n'a pas de bootloader** : services 0x34/0x36/0x37 absents
> - **n'a pas de crypto réelle** : le seed/key est démo
> - **n'est pas ISO 26262** : pas d'ASIL, pas de FMEDA
>
> Mais le **state machine UDS** est correct, l'**ISO-TP** est
> respecté, et la **discipline de validation** — un pytest par
> NRC, exactement ce que je faisais pendant mon stage chez KPIT
> — est là."

**Tu ouvres :** le tableau de `docs/comparison.md` une seconde,
visuel rapide.

---

## Questions probables et réponses

### "Pourquoi avoir réécrit ISO-TP from scratch puis utilisé le kernel ?"

> "Deux raisons. **Pédagogique** : je voulais vraiment comprendre
> SF/FF/CF/FC, le rôle de Block Size, STmin, le timeout N_Cr. La
> meilleure façon est d'écrire le code. **Pragmatique** : pour
> le runtime, lire et écrire depuis deux threads différents sur
> le **même socket** ne pose pas problème avec `can-isotp` du
> kernel — read et write sont sérialisés par le module. Avec
> mon ISO-TP custom, il aurait fallu que je gère la mutex
> moi-même. Le kernel fait le travail mieux que moi."

### "Pourquoi 4 threads et pas 1 boucle événementielle ?"

> "Un vrai ECU AUTOSAR a plusieurs tâches OSEK, pas une seule
> boucle. Le modèle multi-tâches est plus proche de la réalité.
> Et ça me force à exercer la **discipline de threading POSIX** :
> ordre de lock documenté, mutex par ressource, producer/consumer,
> shutdown propre — toutes choses qu'on demande en entretien."

### "Quelle est la chose la plus difficile que tu as faite ?"

> "Le **shutdown propre**. Avoir 4 threads qui peuvent être
> bloqués sur 3 endroits différents (`read` socket, `cond_wait`
> queue, `nanosleep`), et les faire tous sortir proprement sur
> SIGINT sans deadlock ni double-free, c'est non trivial. J'ai
> mis un `SO_RCVTIMEO` d'1 seconde sur le socket, un flag atomic
> `sig_atomic_t` partagé, et un `cond_broadcast` à la sortie de
> la queue. **Helgrind** ne se plaint pas."

### "Et après, qu'est-ce que tu ferais ?"

> "**v2 roadmap** dans le README :
> 1. Implémenter les services 0x34/0x36/0x37 — RequestDownload,
>    TransferData, TransferExit — pour faire un mini bootloader
> 2. Porter le dispatcher sur **FreeRTOS** avec WCET mesurée
> 3. Ajouter **DoIP** — UDS sur IP — pour qu'on accède au même
>    dispatcher depuis CAN ou Ethernet
> 4. Conformité **OBD-II / SAE J1979** sur les PIDs"

---

## Checklist juste avant l'entretien

- [ ] Modules CAN chargés (`lsmod | grep can` → can_isotp visible)
- [ ] `vcan0` up (`ip link show vcan0` → UP, LOWER_UP)
- [ ] Build à jour (`cmake --build build -j` ne renvoie rien à faire)
- [ ] `vecu_uds` PAS en train de tourner avant la démo (sinon Ctrl+C)
- [ ] venv Python activé dans la fenêtre 2
- [ ] Police du terminal **assez grande** pour être lisible en visio (16+)
- [ ] Onglet GitHub du repo ouvert dans le navigateur (badge CI vert)
- [ ] Verre d'eau à côté
- [ ] **Respirer**, parler **lentement**

## Si quelque chose plante en live

Reste calme. Tu dis :
> "Ça arrive — laisse-moi débugger 30 secondes."

Tu Ctrl+C l'ECU, tu relances :
```bash
pkill vecu_uds 2>/dev/null
sudo modprobe vcan can-isotp 2>/dev/null
./build/src/vecu_uds vcan0
```

Et tu enchaînes. **Un recruteur préfère voir comment tu débugges
calmement plutôt qu'un script millimétré qui ne casse jamais.**
