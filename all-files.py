from pathlib import Path


app_dir = Path('.').resolve()


with open('all_code.txt', 'w') as f:
    blacklist_dirs = ['node_modules', '.git', '__pycache__', 'build', 'dist', 'vendor', 'venv', '.venv', 'CleanupFolder', 'old_code', 'dados-publicos-parquet']
    for p in app_dir.rglob('*'):
        if p.is_file() and p.suffix in ['.c', '.cpp', '.h', '.hpp', '.txt']:
            if any(blacklist_dir in p.parts for blacklist_dir in blacklist_dirs):
                continue
            f.write(f"path: {p}\n")
            f.write("```\n")
            f.write(p.read_text())
            f.write("```\n\n")