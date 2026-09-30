import asyncio
import sys
import pygame
import websockets

WIDTH = 32
HEIGHT = 32
PIXEL_SIZE = 16
SERPENTINE = False

pygame.init()

FONT = None
try:
    pygame.font.init()
    FONT = pygame.font.SysFont("Consolas", 12, bold=True)
except Exception:
    pass

screen = pygame.display.set_mode((WIDTH * PIXEL_SIZE, HEIGHT * PIXEL_SIZE))
pygame.display.set_caption("Émulateur Matrice LED ESP32")


class EmulatorState:

    def __init__(self):
        self.delay = 0.05
        self.paused = False
        self.show_grid = True
        self.last_frame = None
        self.is_running = True  # Drapeau pour contrôler la fermeture propre


state = EmulatorState()


def render(data, state: EmulatorState):
    screen.fill((10, 10, 10))

    if data:
        idx = 0
        for y in range(HEIGHT):
            for x in range(WIDTH):
                sx = WIDTH - 1 - x if (SERPENTINE and y % 2 == 1) else x
                if idx + 2 < len(data):
                    r, g, b = data[idx], data[idx + 1], data[idx + 2]
                    idx += 3
                    rect = (
                        sx * PIXEL_SIZE,
                        y * PIXEL_SIZE,
                        PIXEL_SIZE - (1 if state.show_grid else 0),
                        PIXEL_SIZE - (1 if state.show_grid else 0),
                    )
                    pygame.draw.rect(screen, (r, g, b), rect)

    if FONT:
        status_str = "PAUSE" if state.paused else f"Délai: {int(state.delay*1000)}ms"
        grid_str = "Grille: ON" if state.show_grid else "Grille: OFF"
        info_text = (
            f"{status_str} | {grid_str} | [▲/▼] Vitesse | [ESPACE] Pause | [ESC/Q]"
            " Quitter"
        )

        text_surface = FONT.render(info_text, True, (255, 255, 255))
        bg_rect = text_surface.get_rect(topleft=(5, 5))
        pygame.draw.rect(screen, (0, 0, 0, 180), bg_rect.inflate(8, 4))
        screen.blit(text_surface, (5, 5))

    pygame.display.flip()


async def run_emulator():
    uri = "ws://127.0.0.1:18080/esp/ws"
    print(f"Connexion à {uri}...")

    try:
        async with websockets.connect(uri) as ws:
            print("Connecté au serveur Rust ! En attente de frames...")

            while state.is_running:
                # 1. Gestion des événements clavier
                for event in pygame.event.get():
                    if event.type == pygame.QUIT:
                        state.is_running = False

                    elif event.type == pygame.KEYDOWN:
                        if event.key in (pygame.K_ESCAPE, pygame.K_q):
                            state.is_running = False

                        elif event.key == pygame.K_SPACE:
                            state.paused = not state.paused

                        elif event.key == pygame.K_UP:
                            state.delay = max(0.005, state.delay - 0.01)

                        elif event.key == pygame.K_DOWN:
                            state.delay = min(0.5, state.delay + 0.01)

                        elif event.key == pygame.K_g:
                            state.show_grid = not state.show_grid
                            render(state.last_frame, state)

                if not state.is_running:
                    break

                # 2. Réception du flux binaire
                try:
                    msg = await asyncio.wait_for(ws.recv(), timeout=0.01)
                    if isinstance(msg, bytes):
                        state.last_frame = msg
                        render(msg, state)

                        # Pause non bloquante
                        while state.paused and state.is_running:
                            for event in pygame.event.get():
                                if event.type == pygame.QUIT or (
                                        event.type == pygame.KEYDOWN
                                        and event.key in (pygame.K_ESCAPE, pygame.K_q)
                                ):
                                    state.is_running = False
                                    state.paused = False
                                elif (
                                        event.type == pygame.KEYDOWN
                                        and event.key == pygame.K_SPACE
                                ):
                                    state.paused = False
                                    render(state.last_frame, state)
                            await asyncio.sleep(0.05)

                        if state.delay > 0 and state.is_running:
                            await asyncio.sleep(state.delay)

                        if state.is_running:
                            await ws.send("ACK")

                except asyncio.TimeoutError:
                    pass

                await asyncio.sleep(0.001)

    except (websockets.exceptions.ConnectionClosed, ConnectionRefusedError) as e:
        print(f"Connexion fermée/impossible : {e}")
    finally:
        print("\nFermeture propre de Pygame...")
        pygame.quit()


if __name__ == "__main__":
    try:
        asyncio.run(run_emulator())
    except KeyboardInterrupt:
        pass