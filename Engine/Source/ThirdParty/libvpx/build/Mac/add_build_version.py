import logging
from pathlib import Path

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(message)s')
logger = logging.getLogger(__name__)

# Assuming we'll always just run this from the root of the source folder
search_dir = Path('.')  

# Define the line to be added at the top of each ASM file
line_to_add = '[buildversion minos="14.0.0" sdk="15.1.0"]\n'

asm_files = list(search_dir.rglob('*.asm'))

def process_file(asm_file):
    try:
        with asm_file.open('r+', encoding='utf-8') as file:
            content = file.read()

            if not content.startswith(line_to_add):
                file.seek(0, 0)
                file.write(line_to_add + content)
                logger.info(f"Added build version to: {asm_file}")
            else:
                logger.info(f"Build version already present in: {asm_file}")
    
    except PermissionError:
        logger.error(f"Permission error with {asm_file}. Skipping.")
    except IOError as e:
        logger.error(f"IO error with {asm_file}: {e}. Skipping.")
    except Exception as e:
        logger.error(f"Unexpected error with {asm_file}: {e}. Skipping.")

# Run the script
if __name__ == "__main__":
    if asm_files:
        for asm_file in asm_files:
            process_file(asm_file)
    else:
        logger.info("No .asm files found in the directory.")
