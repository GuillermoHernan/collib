#!/usr/bin/env python3
import sys
import csv
from collections import defaultdict
import re

def parse_benchmark_file(filename):
    """Parsea UN SOLO fichero de benchmark y extrae los datos CSV"""
    data = {}
    
    with open(filename, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    
    csv_start = False
    
    for line in lines:
        line = line.strip()
        
        # Inicio CSV
        if line == '--- CSV ---':
            csv_start = True
            continue
            
        # FIN CSV (lo paramos aquí)
        if csv_start and line.startswith('---'):
            break
            
        # Header (skip)
        if csv_start and line.startswith('operation;config;size;time_ms'):
            continue
            
        # Datos válidos
        if csv_start and ';' in line and not line.startswith('#'):
            parts = line.split(';')
            if len(parts) == 4:
                op, config, size, time_str = parts
                try:
                    time_ms = float(time_str)
                    key = (op, config, int(size))
                    data[key] = time_ms
                except ValueError:
                    continue
    
    return data


def get_unique_configs(data1, data2):
    """Obtiene todas las combinaciones únicas de operation/config/size"""
    all_keys = set(data1.keys()) | set(data2.keys())
    return sorted(all_keys, key=lambda x: (x[0], x[1], x[2]))

def get_colored_diff(diff, time_baseline):
    """Porcentaje alineado perfectamente (14 chars fijos)"""
    if time_baseline == 0:
        rel_pct = 0.0
    else:
        rel_pct = (diff / time_baseline) * 100
    
    # Formatear SIEMPRE a 14 caracteres primero
    pct_str = f"{rel_pct:>10.1f}%"
    
    if abs(rel_pct) < 2.0:  # Neutro <2%
        return f"{pct_str:>14}"
    elif rel_pct < 0:  # Verde
        return f"\033[32m{pct_str:>14}\033[0m"
    else:  # Rojo
        return f"\033[31m{pct_str:>14}\033[0m"


        
def print_results_table(diffs, data1, operations, configs, sizes):
    """Imprime tabla con colores usando print individual por celda"""
    
    for operation in sorted(operations):
        print(f"\n# Operation: {operation}")
        print()
        
        # Header
        print(f"{'Configuration':<25}", end='')
        for size in sizes:
            print(f"{size:>14}", end='')
        print()
        print('-' * 110)  # Separador
        
        # Filas por config
        for config in sorted(configs):
            # Nombre de config
            print(f"{config:<25}", end='')
            
            # Cada celda individual con su propio print
            for size in sizes:
                key = (operation, config, size)
                diff = diffs.get(key, 0.0)
                baseline = data1.get(key, 0.0)
                print(get_colored_diff(diff, baseline), end='')
            
            print()  # Nueva línea


def main():
    if len(sys.argv) != 3:
        print("Uso: python3 compare_benchmarks.py file1.txt file2.txt")
        print("  file1: baseline (referencia)")
        print("  file2: nueva versión (se calcularán diferencias file2-file1)")
        sys.exit(1)
    
    file1, file2 = sys.argv[1], sys.argv[2]
    
    print(f"# Comparación: {file2} vs {file1} (diferencias = nuevo - baseline)")
    print()
    
    # Parsear ficheros
    data1 = parse_benchmark_file(file1)
    data2 = parse_benchmark_file(file2)
    
    # Calcular diferencias
    diffs = {}
    all_keys = get_unique_configs(data1, data2)
    
    for key in all_keys:
        t1 = data1.get(key, 0.0)
        t2 = data2.get(key, 0.0)
        diffs[key] = t2 - t1
    
    # Extraer operaciones, configs y sizes únicas para tablas
    operations = set()
    configs = set()
    sizes = set()
    
    for (op, config, size) in all_keys:
        operations.add(op)
        configs.add(config)
        sizes.add(int(size))
    
    sizes = sorted(sizes)
    
    # PRIMERO CSV (como el original)
    print("\n--- CSV ---")
    print("operation;config;size;diff_ms")
    for key in sorted(all_keys):
        op, config, size = key
        diff = diffs[key]
        print(f"{op};{config};{size};{diff:.4f}")
    
    print()
    
    # DESPUÉS tablas formateadas con colores
    print_results_table(diffs, data1, operations, configs, sizes)

if __name__ == "__main__":
    main()
