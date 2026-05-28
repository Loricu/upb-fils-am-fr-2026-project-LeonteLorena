# Synthétiseur Visuel Etch-a-Sketch

| | |
|-|-|
|`Auteur` | Leonte Lorena

## Description

Ce projet consiste à créer un synthétiseur visuel interactif inspiré du jeu classique Etch-a-Sketch. L'utilisateur utilise deux potentiomètres pour dessiner une courbe sur un écran OLED — l'axe X représente le temps/la séquence et l'axe Y représente la fréquence/la hauteur de la note. Une fois le dessin terminé, un bouton permet de « jouer » l'image de gauche à droite : le microcontrôleur parcourt la trace et un buzzer passif génère les notes musicales correspondantes en temps réel. Un autre bouton permet d'effacer l'écran.

Une fusion originale entre une mini console rétro et un séquenceur musical.

## Motivation

Ce projet combine deux choses rarement associées : le dessin et la musique. Le format Etch-a-Sketch est immédiatement intuitif — n'importe qui peut le prendre en main et commencer à expérimenter — tandis que la lecture musicale ajoute une dimension créative et surprenante. C'est une façon ludique d'explorer comment des motifs visuels peuvent être traduits en son, et une introduction concrète aux systèmes embarqués, aux entrées analogiques et à la génération audio en temps réel.

## Architecture

### Schéma fonctionnel

<!-- Assurez-vous que le chemin vers l'image est correct -->
![Schéma fonctionnel](schematics/block_diagram.png)

### Schéma électrique

![Schéma électrique](schematics/kicad_schematic.png)

### Composants

| Composant | Rôle | Prix |
|-----------|------|------|
| Arduino Nano V3 (CH340) | Microcontrôleur — exécute toute la logique, lit les entrées, pilote l'écran et le buzzer | ~25 RON |
| Écran OLED 0.96" I2C (SSD1306, 128×64 px) | Surface de dessin — affiche la courbe en temps réel | ~20 RON |
| Potentiomètre rotatif 10kΩ (×2) | Contrôle du curseur X et Y — les deux « molettes » | ~5 RON chacun |
| Buzzer passif | Sortie audio — génère des notes musicales à hauteur variable via `tone()` | ~3 RON |
| Bouton poussoir (×3) | Contrôles : jouer, effacer et activer/désactiver le dessin | ~1 RON chacun |
| Breadboard (400–830 points) | Base de prototypage sans soudure | ~10 RON |
| Fils Dupont (jumper wires) | Connexion de tous les composants | ~7 RON |

### Bibliothèques

| Bibliothèque | Description | Utilisation |
|--------------|-------------|-------------|
| [Adafruit SSD1306](https://github.com/adafruit/Adafruit_SSD1306) | Pilote pour écrans OLED basés sur SSD1306 | Affichage des pixels et effacement de l'écran |
| [Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library) | Bibliothèque graphique de base | Fonctions de dessin primitives utilisées par la bibliothèque SSD1306 |

## Journal

<!-- Notez votre progression chaque semaine -->

### Semaine 6 - 12 Mai

### Semaine 7 - 19 Mai

### Semaine 20 - 26 Mai

## Liens de référence

[Bibliothèque Adafruit SSD1306](https://github.com/adafruit/Adafruit_SSD1306)

[Référence Arduino tone()](https://www.arduino.cc/reference/en/language/functions/advanced-io/tone/)

[Brochage Arduino Nano](https://projecthub.arduino.cc/PaoloP74/another-arduino-nano-pinout-36b2a3)
