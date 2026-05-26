# vecu-uds — Runbook (comment lancer le projet)

> Mémo perso. Ouvre ce fichier à chaque session WSL pour ne plus rien chercher.
> Chemins : code WSL = `~/projects/vecu-uds`, index Windows = `Documents\17-automotive embedded systems(vECU-UDS)`.

---

## 0. Pré-requis kernel (UNE FOIS — déjà fait)

Le kernel WSL custom est dans `C:\Users\wassi\wsl-kernel\bzImage-can`, déclaré dans `C:\Users\wassi\.wslconfig`. Si tu refais un PC ou supprimes le fichier, recompile avec `CONFIG_CAN`, `CONFIG_CAN_RAW`, `CONFIG_CAN_VCAN`, `CONFIG_CAN_ISOTP`.

**Modules persistants** (à faire une seule fois, après ça ils chargent au boot WSL) :
```bash
echo -e "vcan\ncan_raw\ncan-isotp" | sudo tee /etc/modules-load.d/can.conf
```

---

## 1. Démarrage d'une session WSL (CHECK rapide ~10 s)

Ouvre **Terminal Windows** → onglet WSL Ubuntu-22.04, puis :

```bash
# 1.1 Charger les modules CAN si pas encore fait (silencieux si déjà chargés)
sudo modprobe vcan can-isotp can_raw

# 1.2 Vérifier (tu dois voir can_isotp, can, vcan, can_dev, can_raw)
lsmod | grep can

# 1.3 Créer vcan0 si absent
ip link show vcan0 2>/dev/null || (sudo ip link add dev vcan0 type vcan && sudo ip link set up vcan0)

# 1.4 Test ultra-rapide CAN (optionnel)
candump vcan0 &
cansend vcan0 123#DEADBEEF
# tu dois voir : vcan0  123   [4]  DE AD BE EF
kill %1
```

Si **n'importe laquelle** de ces étapes échoue → relis section 0 (kernel manquant).

---

## 2. Build C (CMake)

```bash
cd ~/projects/vecu-uds

# Build complet Debug (par défaut)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j

# OU build Release optimisé
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# Tests unitaires C (rapides, pas de vcan requis)
ctest --test-dir build --output-on-failure
# attendu : 3/3 tests passed (uds_smoke, can_io_smoke, isotp_smoke)
```

Binaire produit : `./build/src/vecu_uds`

---

## 3. Lancer l'ECU virtuel

**Fenêtre WSL n°1** (l'ECU) :
```bash
cd ~/projects/vecu-uds
./build/src/vecu_uds vcan0
```

Attendu :
```
vecu-uds v1.0.0 — starting on vcan0 (rx=0x7E0 tx=0x7E8)
vecu-uds ready. Ctrl+C to stop.
```

Laisse cette fenêtre tourner. Pour stopper : **Ctrl+C** (clean shutdown).

---

## 4. Parler à l'ECU avec le client Python

**Fenêtre WSL n°2** (le tester) :

```bash
cd ~/projects/vecu-uds/client

# Première fois seulement : créer le venv + installer les deps
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt pytest

# Sessions suivantes : juste activer le venv
source .venv/bin/activate
```

**Commandes du diag_tool :**

```bash
python3 diag_tool.py read-vin
# VIN: VF1234567890ABCDE

python3 diag_tool.py read-serial
# Serial: ECU-DEMO-001

python3 diag_tool.py read-dtc
# DTC list (status filter 0xFF):
#   P0301 — Cylinder 1 Misfire   [confirmed]
#   P0420 — Catalyst Efficiency  [confirmed]

python3 diag_tool.py clear-dtc

python3 diag_tool.py unlock --level 1
# Unlocked level 1

python3 diag_tool.py write-name "WASSIM"
# Owner written: WASSIM
# (note : nécessite unlock préalable + extended session)

python3 diag_tool.py monitor
# UI curses temps réel : RPM, vitesse, coolant
# quitter : q
```

---

## 5. Tests d'intégration Python (pytest)

L'ECU doit **NE PAS** tourner — pytest le lance lui-même via fixture.

```bash
cd ~/projects/vecu-uds/client
source .venv/bin/activate
pytest tests/ -v
# ~20 cas, <10 s
```

Si un test rate → regarde l'erreur, c'est probablement une incompatibilité d'API udsoncan à patcher.

---

## 6. Docker (optionnel — démo conteneurisée)

```bash
cd ~/projects/vecu-uds

# Build et lance l'ECU dans un conteneur (partage vcan0 host)
docker compose up --build vecu

# Dans une autre fenêtre : appel diag depuis conteneur
docker compose run --rm diag read-vin
```

Pré-requis : `vcan0` déjà up sur l'hôte (section 1.3).

---

## 7. Dépannage rapide

| Symptôme | Cause probable | Fix |
|---|---|---|
| `socket: Address family not supported by protocol` | modules CAN pas chargés | `sudo modprobe vcan can-isotp can_raw` |
| `modprobe: FATAL: Module can-isotp not found` | kernel WSL sans `CONFIG_CAN_ISOTP` | recompiler kernel (section 0) |
| `Failed to open CAN_ISOTP socket on 'vcan0'` | vcan0 inexistant ou down | `sudo ip link add dev vcan0 type vcan && sudo ip link set up vcan0` |
| `ModuleNotFoundError: No module named 'isotp'` | venv pas activé | `source .venv/bin/activate` |
| `./diag_tool.py: Permission denied` | exécutable absent | `chmod +x diag_tool.py` ou utiliser `python3 diag_tool.py …` |
| `pytest: command not found` | pas installé dans le venv | `pip install pytest` |
| ECU répond NRC 0x7F | service interdit dans la session actuelle | passer en extended : `python3 diag_tool.py` doit faire `change_session(3)` (déjà fait dans le tool) |
| ECU répond NRC 0x33 | security pas débloquée | `python3 diag_tool.py unlock --level 1` AVANT le write |
| ECU répond NRC 0x35 | mauvaise clé seed/key | revoir l'algo `key = seed XOR 0xDEADBEEF` |
| Process zombie `vecu_uds` qui occupe vcan0 | reste d'un kill bâclé | `pkill vecu_uds` |

---

## 8. Workflow type "je veux tester un truc"

```bash
# Fenêtre 1 — toujours dans ~/projects/vecu-uds
sudo modprobe vcan can-isotp can_raw 2>/dev/null
ip link show vcan0 || (sudo ip link add dev vcan0 type vcan && sudo ip link set up vcan0)
cmake --build build -j        # rebuild si tu as modifié du C
./build/src/vecu_uds vcan0    # lance l'ECU

# Fenêtre 2 — interagir
cd ~/projects/vecu-uds/client && source .venv/bin/activate
python3 diag_tool.py read-vin
python3 diag_tool.py monitor

# Pour relancer après modif C :
# Ctrl+C dans Fenêtre 1, refaire cmake --build build -j, relancer le binaire.
```

---

## 9. Commit Git (quand tu es content du résultat)

```bash
cd ~/projects/vecu-uds
git status
git add -A
git commit -m "feat: <ce que tu as fait>"
git log --oneline -5
```

**Ne pas pousser sur GitHub avant** : le repo public, c'est pour la démo finale du J21.
