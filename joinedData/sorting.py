import os
import pygame
import shutil

folder_path = "brighter" 

def play_and_sort_files(folder_path):
    pygame.init()
    screen = pygame.display.set_mode((640, 480))
    # Set up pygame mixer
    pygame.mixer.init()

    # List all WAV files in the "on" folder
    files = [file for file in os.listdir(folder_path) if file.endswith(".wav")]
    # make this a list not a generator
    files = list(files)

    # Create "good" and "bad" folders if not exist
    good_folder = os.path.join(folder_path, "good")
    bad_folder = os.path.join(folder_path, "bad")
    os.makedirs(good_folder, exist_ok=True)
    os.makedirs(bad_folder, exist_ok=True)

    for file in files:
        file_path = os.path.join(folder_path, file)
        print(f"Progress: {files.index(file) + 1}/{len(files)}")
        print(f"Playing: {file}")

        # Load and play the audio file
        pygame.mixer.music.load(file_path)
        pygame.mixer.music.play()

        # Wait for user input (left or right arrow key)
        while True:
            for event in pygame.event.get():
                if event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_RIGHT: 
                        pygame.mixer.music.stop()
                        pygame.mixer.music.unload()
                        shutil.move(file_path, os.path.join(good_folder, file))
                        print(f"Moved to 'good' folder: {file}")
                        break
                    elif event.key == pygame.K_LEFT:
                        pygame.mixer.music.stop()
                        pygame.mixer.music.unload()
                        shutil.move(file_path, os.path.join(bad_folder, file))
                        print(f"Moved to 'bad' folder: {file}")
                        break
            else:
                continue
            break

if __name__ == "__main__":
    play_and_sort_files(folder_path)