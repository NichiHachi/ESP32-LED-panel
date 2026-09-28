import os
from PIL import Image

def images_to_gif(input_folder, output_gif_path, duration=100, loop=0):
    """
    Convertit toutes les images d'un dossier en un fichier GIF animé.

    :param input_folder: Dossier contenant les images.
    :param output_gif_path: Chemin du fichier GIF de sortie.
    :param duration: Durée de chaque frame en millisecondes (ex: 100ms = 10 fps).
    :param loop: 0 pour une boucle infinie, N pour répéter N fois.
    """
    # 1. Lister et trier les images du dossier
    valid_extensions = ('.png', '.jpg', '.jpeg', '.bmp')
    image_files = [
        f for f in sorted(os.listdir(input_folder))
        if f.lower().endswith(valid_extensions)
    ]

    if not image_files:
        print(f"Aucune image trouvée dans {input_folder}")
        return

    # 2. Charger les images en RGBA pour préserver les couleurs/transparences
    frames = []
    for file_name in image_files:
        file_path = os.path.join(input_folder, file_name)
        img = Image.open(file_path).convert("RGBA")
        frames.append(img)

    # 3. Sauvegarder en GIF
    frames[0].save(
        output_gif_path,
        save_all=True,
        append_images=frames[1:],
        duration=duration,
        loop=loop,
        disposition=2 # Nettoie la frame précédente (évite les superpositions)
    )

    print(f"GIF créé avec succès : {output_gif_path} ({len(frames)} frames)")

if __name__ == "__main__":
    # Configuration
    DOSSIER_IMAGES = "./"  # Dossier où se trouvent vos images
    GIF_SORTIE = "kevin.gif"      # Fichier GIF généré
    DUREE_FRAME_MS = 100             # 100ms par image (10 images/sec)

    images_to_gif(DOSSIER_IMAGES, GIF_SORTIE, duration=DUREE_FRAME_MS)