# vecu-uds

Un projet C implémentant une pile UDS (Unified Diagnostic Services) et des utilitaires CAN pour l'application Vecu.

## Table des matières
- **But :** Présentation rapide du projet
- **Statut :** Etat actuel (build, tests)
- **Prérequis :** Outils nécessaires
- **Build :** Instructions pour compiler
- **Tests :** Exécuter la suite de tests
- **Structure :** Arborescence principale du dépôt
- **Contribuer :** Comment contribuer
- **Licence :** Informations de licence

## But
Ce dépôt contient une implémentation UDS (fichiers `src/uds.c`, `include/uds.h`, `include/uds_types.h`) et des modules CAN/ISO-TP (`src/can_io.c`, `src/isotp.c`) destinés à piloter et tester les communications diagnostiques sur réseau CAN.

## Statut
- Compilation CMake: configurée (dossier `build/` présent)
- Tests unitaires: présents sous `tests/` (ex : `test_uds_smoke`, `test_can_io_smoke`)

## Prérequis
- GNU toolchain (gcc, make)
- CMake >= 3.16
- Outils de test: CTest (fourni par CMake)
- Optionnel: environnement Linux/WSL pour les scripts `setup_vcan.sh`

## Build
Depuis la racine du projet :

```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

Les exécutables se trouvent ensuite dans `build/` et `src/` selon la configuration CMake.

## Tests
Pour lancer les tests :

```bash
cd build
ctest --output-on-failure
```

Ou exécuter les binaires de test directement depuis `build/tests/`.

## Structure du dépôt
- `src/` : sources C
- `include/` : fichiers d'en-tête publics
- `tests/` : tests unitaires et de fumée
- `build/` : sortie CMake
- `docs/`, `scripts/`, `tools/` : documentation et utilitaires

## Contribuer
- Ouvrir une issue pour discuter des changements
- Fournir des commits atomiques et descriptifs
- Ajouter/mettre à jour les tests pour toute fonctionnalité critique

## Licence
Veuillez consulter le fichier `LICENSE` si présent, sinon demander au mainteneur du projet.

---
Si vous voulez, je peux aussi :
- ajouter une section "Exemples d'utilisation" avec commandes d'exécution
- créer un petit guide pour lancer un réseau CAN virtuel (`setup_vcan.sh`)

