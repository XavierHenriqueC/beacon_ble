# main/gen_build_time.py
import time
from datetime import datetime
import sys

output_file = sys.argv[1]

# Pega a hora atual (do sistema)
now = int(time.time())

# Gera um arquivo .h com o timestamp em uint64_t
with open(output_file, 'w') as f:
    f.write('// Gerado automaticamente. Não edite manualmente.\n')
    f.write('#pragma once\n')
    f.write(f'#define BUILD_UNIX_TIMESTAMP ((uint64_t){now}ULL)\n')
