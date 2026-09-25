# Documentation API Backend – ESPLedManager

Le backend **ESPLedManager** est un serveur REST écrit en C++ (utilisant la bibliothèque Crow). Il permet de gérer dynamiquement la configuration de traitement d'images destinées à une matrice de LED, ainsi que de prévisualiser en temps réel le résultat généré.

* **Base URL** : `http://localhost:18080` *(ou l'IP/domaine de votre serveur)*
* **Formats supportés** : JSON pour la configuration, `multipart/form-data` ou binaire brut pour l'envoi d'images.

## Requirements: 
Voici la section **Prérequis / Requirements** à intégrer au début de votre fichier `README.md` ou de la documentation du projet.

---

## Prérequis (Requirements)

Pour compiler et exécuter le serveur backend **ESPLedManager**, votre environnement de développement doit disposer des outils et dépendances suivants :

### 1. Outils de build et compilateur

* **Compilateur C++** : Support de **C++20** (ou plus récent)
* `g++` (v9+) ou `clang++` (v10+)


* **Système de build** : **[xmake](https://xmake.io/?utm_source=gemini)** (v2.5+)
* *Installation rapide (Linux/macOS)* : `curl -fsSL https://xmake.io/shget.text | bash`



---

### 2. Dépendances & Bibliothèques C++

Les dépendances C++ principales gérées par le projet (ou via le gestionnaire de paquets de `xmake`) :

| Bibliothèque                                                                       | Version requise | Rôle                                                         |
|------------------------------------------------------------------------------------|-----------------|--------------------------------------------------------------|
| **[Crow](https://www.google.com/search?q=https://crowcpp.org/&utm_source=gemini)** | `v1.0+`         | Framework web C++ (routes HTTP, support multipart, REST API) |
| **[nlohmann/json](https://github.com/nlohmann/json?utm_source=gemini)**            | `v3.10+`        | Analyse et sérialisation JSON pour les configurations        |
| **[Asio](https://think-async.com/Asio/?utm_source=gemini)** / **Boost.Asio**       | -               | Moteur réseau asynchrone utilisé sous-jacent par Crow        |
| **`pthread` / `threads**`                                                          | -               | Support du multithreading HTTP                               |

---

### 3. Installation et Démarrage rapide

1. **Cloner le projet** :
```bash
git clone <url-du-repo-espledmanager>
cd ESPLedManager

```


2. **Fichier de configuration initial** :
   Assurez-vous qu'un fichier `config.json` valide est présent à la racine du projet (ou un fichier par défaut sera créé au premier lancement).
3. **Compilation et Lancement** :
```bash
# Compiler le projet
xmake

# Lancer le serveur backend depuis la racine du projet
xmake run -w .

```



Le serveur sera alors accessible à l'adresse `http://localhost:18080` par défaut.

---

## Endpoints API

### 1. Récupérer la configuration actuelle

Permet de charger les paramètres enregistrés en mémoire au chargement de l'application web.

* **URL** : `/api/config`
* **Méthode** : `GET`
* **Headers** : Aucun requis

#### Exemple de réponse (`200 OK`) :

```json
{
  "matrix": {
    "width": 16,
    "height": 15,
    "max_palette_colors": 16,
    "enable_serpentine_layout": true
  },
  "options": {
    "fit_mode": "LETTERBOX",
    "color_mode": "QUANTIZED_KMEANS",
    "background_color": [0, 0, 0],
    "brightness": 1.0,
    "contrast": 1.1,
    "gamma": 1.0,
    "rotation_deg": 0,
    "flip_horizontal": false,
    "flip_vertical": false
  }
}

```

---

### 2. Mettre à jour la configuration

Permet de modifier les paramètres de traitement (luminosité, rotation, dimensions, etc.). Les modifications prennent effet immédiatement en mémoire et sont automatiquement sauvegardées sur le disque backend.

* **URL** : `/api/config`
* **Méthode** : `POST`
* **Headers** : `Content-Type: application/json`

#### Corps de la requête (`Body JSON`) :

> **Note** : Vous pouvez envoyer un objet partiel contenant uniquement les champs à modifier.

```json
{
  "matrix": {
    "width": 32,
    "height": 32
  },
  "options": {
    "brightness": 1.2,
    "contrast": 1.0,
    "rotation_deg": 90,
    "fit_mode": "CROP"
  }
}

```

#### Valeurs possibles pour les énumérations :

| Champ | Type | Valeurs acceptées | Description |
| --- | --- | --- | --- |
| `fit_mode` | `string` | `"LETTERBOX"`, `"CROP"`, `"STRETCH"` | Mode d'ajustement à la taille de la matrice. |
| `color_mode` | `string` | `"QUANTIZED_KMEANS"`, `"GRAYSCALE"`, `"RGB565"` | Traitement/Réduction des couleurs. |
| `rotation_deg` | `int` | `0`, `90`, `180`, `270` | Angle de rotation de l'image. |
| `background_color` | `array[int]` | `[R, G, B]` (0 à 255) | Couleur de fond pour le mode `LETTERBOX`. |

#### Réponses :

* **`200 OK`** : `"Configuration mise à jour avec succès."`
* **`400 Bad Request`** : `"JSON invalide."`

---

### 3. Traiter et prévisualiser une image

Envoie une image au serveur. Le backend applique la configuration courante (taille de la matrice, filtres, rotation, etc.) et renvoie directement l'image PNG résultante.

* **URL** : `/`
* **Méthode** : `POST`
* **Headers** : `Content-Type: multipart/form-data` *(ou image brute)*

#### Option A : Formulaire `multipart/form-data` (Recommandé)

Envoyez le fichier sous le nom de champ `image` ou comme fichier joint.

#### Option B : Corps binaire brut

Envoyez directement le buffer binaire de l'image dans le corps de la requête.

#### Réponse :

* **`200 OK`**
* **Header** : `Content-Type: image/png`
* **Body** : Données binaires du fichier PNG généré.


* **`400 Bad Request`** : `"Aucune image reçue."` ou `"Impossible de décoder l'image transmise."`

---

Voici les exemples **curl** complets à ajouter à la documentation :

---

### Exemples d'utilisation avec `curl`

#### 1. Récupérer la configuration (`GET /api/config`)

```bash
curl -X GET http://localhost:18080/api/config

```

---

#### 2. Mettre à jour des paramètres (`POST /api/config`)
**Tout ajuster en une seule requête :**

```bash
curl -X POST http://localhost:18080/api/config \
     -H "Content-Type: application/json" \
     -d '{
       "matrix": {
         "width": 32,
         "height": 32,
         "max_palette_colors": 12,
         "enable_serpentine_layout": false
       },
       "options": {
         "brightness": 1.1,
         "contrast": 1.2,
         "gamma": 0.9,
         "rotation_deg": 180,
         "flip_horizontal": true,
         "flip_vertical": false,
         "fit_mode": "CROP",
         "color_mode": "QUANTIZED_KMEANS",
         "background_color": [10, 10, 10]
       }
     }'
```
**Ajuster la luminosité, le contraste et la rotation :**

```bash
curl -X POST http://localhost:18080/api/config \
     -H "Content-Type: application/json" \
     -d '{
       "options": {
         "brightness": 1.2,
         "contrast": 1.1,
         "rotation_deg": 90
       }
     }'

```

**Changer le mode d'ajustement et la couleur de fond (Letterbox) :**

```bash
curl -X POST http://localhost:18080/api/config \
     -H "Content-Type: application/json" \
     -d '{
       "options": {
         "fit_mode": "LETTERBOX",
         "background_color": [255, 0, 0]
       }
     }'

```

---

#### 3. Envoyer une image et enregistrer le rendu (`POST /`)

**Option A : Via un formulaire `multipart/form-data` (Recommandé)**

```bash
curl -X POST http://localhost:18080/ \
     -F "image=@/chemin/vers/mon_image.jpg" \
     --output rendu_matrice.png

```

**Option B : Via l'envoi du binaire brut dans le corps de la requête**

```bash
curl -X POST http://localhost:18080/ \
     -H "Content-Type: application/octet-stream" \
     --data-binary "@/chemin/vers/mon_image.jpg" \
     --output rendu_matrice.png
```

Pour ajouter un choix entre la restitution d'une **image PNG** ou d'un **tableau JSON de la matrice de pixels**, il convient de récupérer le champ booléen (par exemple `return_matrix` ou `output=matrix`) depuis le formulaire multipart ou les paramètres de requête (*query params*).

Vous pouvez spécifier le paramètre `return_matrix` soit via le **URL Query Parameter**, soit via le **Form-Data multipart** :

#### Option 1 : Via l'URL (Query Param)

```bash
curl -X POST "http://localhost:18080/?return_matrix=true" \
     -F "image=@/chemin/vers/mon_image.jpg"

```

#### Option 2 : Via le formulaire multipart (`-F`)

```bash
curl -X POST http://localhost:18080/ \
     -F "image=@/chemin/vers/mon_image.jpg" \
     -F "return_matrix=true"

```

#### Exemple de réponse JSON si `return_matrix=true` :

```json
{
  "width": 16,
  "height": 16,
  "pixels": [
    { "r": 255, "g": 0, "b": 0 },
    { "r": 0, "g": 255, "b": 0 },
    ...
  ]
}

```
---

## Exemples d'intégration Frontend (JavaScript / Fetch)

### Récupérer les paramètres (`GET`)

```javascript
async function loadConfig() {
  try {
    const response = await fetch('http://localhost:18080/api/config');
    const config = await response.json();
    console.log('Configuration actuelle :', config);
    return config;
  } catch (error) {
    console.error('Erreur lors du chargement de la config :', error);
  }
}

```

### Mettre à jour un paramètre (`POST`)

```javascript
async function updateBrightness(newBrightness) {
  try {
    const response = await fetch('http://localhost:18080/api/config', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        options: {
          brightness: newBrightness
        }
      }),
    });
    const message = await response.text();
    console.log(message);
  } catch (error) {
    console.error('Erreur lors de la mise à jour :', error);
  }
}

```

### Envoyer une image et afficher le résultat (`POST`)

```javascript
async function processAndPreviewImage(fileInput) {
  const file = fileInput.files[0];
  if (!file) return;

  const formData = new FormData();
  formData.append('image', file);

  try {
    const response = await fetch('http://localhost:18080/', {
      method: 'POST',
      body: formData,
    });

    if (!response.ok) throw new Error('Échec du traitement');

    // Conversion de la réponse binaire PNG en URL affichable dans une balise <img />
    const blob = await response.blob();
    const imageUrl = URL.createObjectURL(blob);
    
    document.getElementById('preview-img').src = imageUrl;
  } catch (error) {
    console.error('Erreur lors du traitement d\'image :', error);
  }
}

```